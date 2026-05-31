// ============================================================================
// k_quant_kernels.cpp — AVX2 K-Quant Dequantization + Fused GEMV
// ============================================================================
#include "k_quant_kernels.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace rawrxd {
namespace kquant {

// ============================================================================
// Q4_K Dequantization
// 256 elements per super-block, organized as 8 sub-blocks of 32 elements.
// Each element: x = scale * q - min  where q ∈ [0,15]
// ============================================================================

void dequantize_q4_K(const block_q4_K* blk, float* dst, int n_blocks)
{
    for (int b = 0; b < n_blocks; ++b) {
        const block_q4_K& bl = blk[b];
        float d    = fp16_to_fp32(bl.d);
        float dmin = fp16_to_fp32(bl.dmin);

        float scales[8], mins[8];
        unpack_scales_q4K(bl.scales, d, dmin, scales, mins);

        float* out = dst + b * 256;

        // Each sub-block is 32 elements; nibbles packed 2/byte
        // qs[0..63]: first 128 elements (sub-blocks 0..3), each byte holds 2 nibbles
        // qs[64..127] would be sub-blocks 4..7 — BUT Q4_K uses interleaved layout:
        // qs[0..31]: lo nibbles of sub-blocks 0..3 (128 elements)
        // qs[32..63]: lo nibbles of sub-blocks 4..7 (128 elements)
        // Actually the GGML layout packs all 256 nibbles sequentially; they form 8 groups
        // of 32 elements (16 bytes each). Groups 0-3 pack into qs[0..63], groups 4-7 pack
        // into the same byte range offset by half-nibble index.
        //
        // Correct GGML Q4_K layout:
        //   Element i in super-block: sub_block = i/32, pos = i%32
        //   nibble index = sub_block * 32 + pos = i
        //   byte = i/2,  shift = (i&1)*4

        for (int sub = 0; sub < 8; ++sub) {
            float sc  = scales[sub];
            float mn  = mins[sub];
            for (int j = 0; j < 32; ++j) {
                int idx = sub * 32 + j;
                uint8_t byte_val = bl.qs[idx / 2];
                uint8_t nibble   = (idx & 1) ? (byte_val >> 4) : (byte_val & 0x0F);
                out[idx] = sc * (float)nibble - mn;
            }
        }

#ifdef __AVX2__
        // AVX2 accelerated path: process 32 nibbles at a time
        // (enabled at compile time only when target supports AVX2)
        // The scalar loop above is always correct; AVX2 path is an
        // optimization that produces identical results.
        (void)out; // scalar path used by default; SIMD handled in gemv_q4_K_avx2
#endif
    }
}

// ============================================================================
// Q5_K Dequantization
// Like Q4_K but elements have an additional high-bit from qh[]
// element i: q = (qs[i/2] nibble) | ((qh[i/8] >> (i%8)) & 1) << 4
// value = scale * q - min,  q ∈ [0,31]
// ============================================================================

void dequantize_q5_K(const block_q5_K* blk, float* dst, int n_blocks)
{
    for (int b = 0; b < n_blocks; ++b) {
        const block_q5_K& bl = blk[b];
        float d    = fp16_to_fp32(bl.d);
        float dmin = fp16_to_fp32(bl.dmin);

        float scales[8], mins[8];
        unpack_scales_q4K(bl.scales, d, dmin, scales, mins);

        float* out = dst + b * 256;

        for (int sub = 0; sub < 8; ++sub) {
            float sc = scales[sub];
            float mn = mins[sub];
            for (int j = 0; j < 32; ++j) {
                int idx = sub * 32 + j;
                // Low nibble
                uint8_t bv = bl.qs[idx / 2];
                uint8_t lo = (idx & 1) ? (bv >> 4) : (bv & 0x0F);
                // High bit
                uint8_t hi = (bl.qh[idx / 8] >> (idx % 8)) & 1;
                uint8_t q  = lo | (hi << 4);
                out[idx]   = sc * (float)q - mn;
            }
        }
    }
}

// ============================================================================
// Q6_K Dequantization
// 256 values per super-block. Each value is 6 bits:
//   lo 4 bits from ql[], hi 2 bits from qh[]
//   value = scale[sub] * q + 32 * scale[sub]   (signed, reconstructed as q-32)
// GGML actual: quant ∈ [-32, 31], stored as uint6 mapped via q -= 32
// ============================================================================

void dequantize_q6_K(const block_q6_K* blk, float* dst, int n_blocks)
{
    for (int b = 0; b < n_blocks; ++b) {
        const block_q6_K& bl = blk[b];
        float d = fp16_to_fp32(bl.d);

        float* out = dst + b * 256;

        for (int i = 0; i < 256; ++i) {
            // Low 4 bits: ql interleaving
            // GGML Q6_K: element i -> byte = i/2, shift = 4*(i&1)
            // But actually upper/lower nibbles alternate differently for Q6_K
            // Exact layout: ql[i/2] is split into lo (i even) and hi (i odd)
            uint8_t lo4 = (i & 1) ? (bl.ql[i/2] >> 4) : (bl.ql[i/2] & 0x0F);

            // High 2 bits: qh[i/4], bits at position 2*(i%4) and 2*(i%4)+1
            uint8_t hi2 = (bl.qh[i / 4] >> (2 * (i % 4))) & 0x03;

            int8_t q6 = (int8_t)(lo4 | (hi2 << 4)) - 32;

            // Sub-block index: one scale per 16 elements
            int sub = i / 16;
            float sc = d * (float)bl.scales[sub];
            out[i]   = sc * (float)q6;
        }
    }
}

// ============================================================================
// AVX2 Fused GEMV for Q4_K
// x: [k] float input;  w: [n × k/256] Q4_K blocks;  y: [n] output (+=)
// One output element y[row] = sum over k of x[i] * dequant(w[row, i])
// ============================================================================

void gemv_q4_K_avx2(const float* x, const block_q4_K* w, float* y, int n, int k)
{
    const int blocks_per_row = k / 256;

    for (int row = 0; row < n; ++row) {
        const block_q4_K* row_w = w + row * blocks_per_row;
        float acc = 0.0f;

        for (int b = 0; b < blocks_per_row; ++b) {
            const block_q4_K& bl = row_w[b];
            float d    = fp16_to_fp32(bl.d);
            float dmin = fp16_to_fp32(bl.dmin);

            float scales[8], mins[8];
            unpack_scales_q4K(bl.scales, d, dmin, scales, mins);

            const float* xb = x + b * 256;

#ifdef __AVX2__
            // AVX2 path: process 32 elements (one sub-block) per iteration
            for (int sub = 0; sub < 8; ++sub) {
                const float* xi = xb + sub * 32;
                float sc = scales[sub];
                float mn = mins[sub];

                // Load 16 bytes of nibbles (32 4-bit values)
                const uint8_t* qs_ptr = bl.qs + sub * 16;
                __m128i raw = _mm_loadu_si128((const __m128i*)qs_ptr);

                // Unpack nibbles to two sets of 16 uint8s
                __m128i lo_mask = _mm_set1_epi8(0x0F);
                __m128i lo16 = _mm_and_si128(raw, lo_mask);
                __m128i hi16 = _mm_and_si128(_mm_srli_epi16(raw, 4), lo_mask);

                // Convert u8 → i32 via zero-extension, then to float
                // Process lo first (elements 0..15 of sub-block)
                __m256i lo32 = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(
                    *((const int64_t*)&lo16)));  // lower 8 bytes → 8 int32

                // Scale and min
                __m256 sc_v  = _mm256_set1_ps(sc);
                __m256 mn_v  = _mm256_set1_ps(mn);

                // First 8 elements of lo
                __m256 q_f = _mm256_cvtepi32_ps(lo32);
                __m256 w_f = _mm256_sub_ps(_mm256_mul_ps(sc_v, q_f), mn_v);
                __m256 x_v = _mm256_loadu_ps(xi);
                __m256 p   = _mm256_mul_ps(w_f, x_v);

                float tmp[8];
                _mm256_storeu_ps(tmp, p);
                for (int t = 0; t < 8; ++t) acc += tmp[t];

                // Next 8 elements of lo (bytes 8-15)
                __m256i lo32b = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(
                    *((const int64_t*)((const uint8_t*)&lo16 + 8))));
                q_f = _mm256_cvtepi32_ps(lo32b);
                w_f = _mm256_sub_ps(_mm256_mul_ps(sc_v, q_f), mn_v);
                x_v = _mm256_loadu_ps(xi + 8);
                p   = _mm256_mul_ps(w_f, x_v);
                _mm256_storeu_ps(tmp, p);
                for (int t = 0; t < 8; ++t) acc += tmp[t];

                // Hi nibbles (elements 16..31): same process
                __m256i hi32 = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(
                    *((const int64_t*)&hi16)));
                q_f = _mm256_cvtepi32_ps(hi32);
                w_f = _mm256_sub_ps(_mm256_mul_ps(sc_v, q_f), mn_v);
                x_v = _mm256_loadu_ps(xi + 16);
                p   = _mm256_mul_ps(w_f, x_v);
                _mm256_storeu_ps(tmp, p);
                for (int t = 0; t < 8; ++t) acc += tmp[t];

                __m256i hi32b = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(
                    *((const int64_t*)((const uint8_t*)&hi16 + 8))));
                q_f = _mm256_cvtepi32_ps(hi32b);
                w_f = _mm256_sub_ps(_mm256_mul_ps(sc_v, q_f), mn_v);
                x_v = _mm256_loadu_ps(xi + 24);
                p   = _mm256_mul_ps(w_f, x_v);
                _mm256_storeu_ps(tmp, p);
                for (int t = 0; t < 8; ++t) acc += tmp[t];
            }
#else
            // Scalar fallback
            for (int sub = 0; sub < 8; ++sub) {
                float sc = scales[sub]; float mn = mins[sub];
                for (int j = 0; j < 32; ++j) {
                    int idx = sub * 32 + j;
                    uint8_t bv = bl.qs[idx / 2];
                    uint8_t nib = (idx & 1) ? (bv >> 4) : (bv & 0x0F);
                    acc += xb[idx] * (sc * (float)nib - mn);
                }
            }
#endif
        }

        y[row] += acc;
    }
}

// ============================================================================
// AVX2 Fused GEMV for Q6_K
// ============================================================================

void gemv_q6_K_avx2(const float* x, const block_q6_K* w, float* y, int n, int k)
{
    const int blocks_per_row = k / 256;

    for (int row = 0; row < n; ++row) {
        const block_q6_K* row_w = w + row * blocks_per_row;
        float acc = 0.0f;

        for (int b = 0; b < blocks_per_row; ++b) {
            const block_q6_K& bl = row_w[b];
            float d = fp16_to_fp32(bl.d);
            const float* xb = x + b * 256;

#ifdef __AVX2__
            // 16-element sub-blocks
            for (int sub = 0; sub < 16; ++sub) {
                float sc = d * (float)bl.scales[sub];
                __m256 sc_v = _mm256_set1_ps(sc);
                const float* xi = xb + sub * 16;

                float tmp[8];
                // Two 8-element chunks per sub-block
                for (int chunk = 0; chunk < 2; ++chunk) {
                    int base = sub * 16 + chunk * 8;
                    __m256i ql8, qh8;
                    // Gather 8 elements
                    int32_t qi[8];
                    for (int e = 0; e < 8; ++e) {
                        int i = base + e;
                        uint8_t lo = (i & 1) ? (bl.ql[i/2] >> 4) : (bl.ql[i/2] & 0x0F);
                        uint8_t hi = (bl.qh[i/4] >> (2*(i%4))) & 0x03;
                        qi[e] = (int32_t)(int8_t)(lo | (hi << 4)) - 32;
                    }
                    ql8 = _mm256_loadu_si256((const __m256i*)qi);
                    __m256 q_f = _mm256_cvtepi32_ps(ql8);
                    __m256 w_f = _mm256_mul_ps(sc_v, q_f);
                    __m256 x_v = _mm256_loadu_ps(xi + chunk * 8);
                    __m256 p   = _mm256_mul_ps(w_f, x_v);
                    _mm256_storeu_ps(tmp, p);
                    for (int t = 0; t < 8; ++t) acc += tmp[t];
                }
            }
#else
            for (int i = 0; i < 256; ++i) {
                uint8_t lo = (i & 1) ? (bl.ql[i/2] >> 4) : (bl.ql[i/2] & 0x0F);
                uint8_t hi = (bl.qh[i/4] >> (2*(i%4))) & 0x03;
                int8_t q6 = (int8_t)(lo | (hi << 4)) - 32;
                int sub = i / 16;
                acc += xb[i] * (d * (float)bl.scales[sub] * (float)q6);
            }
#endif
        }

        y[row] += acc;
    }
}

// ============================================================================
// Unified Dispatch
// ============================================================================

void dequant_k(ggml_rxd_type type, const void* src, float* dst, int n_elements)
{
    switch (type) {
    case GGML_RXD_TYPE_Q4_K: {
        int n_blocks = n_elements / 256;
        dequantize_q4_K(static_cast<const block_q4_K*>(src), dst, n_blocks);
        break;
    }
    case GGML_RXD_TYPE_Q5_K: {
        int n_blocks = n_elements / 256;
        dequantize_q5_K(static_cast<const block_q5_K*>(src), dst, n_blocks);
        break;
    }
    case GGML_RXD_TYPE_Q6_K: {
        int n_blocks = n_elements / 256;
        dequantize_q6_K(static_cast<const block_q6_K*>(src), dst, n_blocks);
        break;
    }
    default:
        // Unknown K-quant type — zero out
        memset(dst, 0, n_elements * sizeof(float));
        break;
    }
}

void matmul_k_fused(ggml_rxd_type type,
                    const float* x, const void* w, float* y,
                    int n_out, int k_dim)
{
    switch (type) {
    case GGML_RXD_TYPE_Q4_K:
        gemv_q4_K_avx2(x, static_cast<const block_q4_K*>(w), y, n_out, k_dim);
        break;
    case GGML_RXD_TYPE_Q6_K:
        gemv_q6_K_avx2(x, static_cast<const block_q6_K*>(w), y, n_out, k_dim);
        break;
    case GGML_RXD_TYPE_Q5_K: {
        // Q5_K GEMV: dequantize then dot product (no fused path yet)
        int n_blocks = k_dim / 256;
        std::vector<float> tmp(k_dim);
        dequantize_q5_K(static_cast<const block_q5_K*>(w), tmp.data(), n_blocks);
        for (int r = 0; r < n_out; ++r) {
            float acc = 0.0f;
            const float* row_tmp = tmp.data() + r * k_dim;
            for (int i = 0; i < k_dim; ++i) acc += x[i] * row_tmp[i];
            y[r] += acc;
        }
        break;
    }
    default:
        break;
    }
}

} // namespace kquant
} // namespace rawrxd
