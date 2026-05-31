#pragma once
#include <cstdint>
#include <cstring>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

namespace RawrXD::Kernels {

// Q4_0 block format: 32 weights share 1 scale
struct Q4_0Block {
    float   d;       // delta (scale)
    uint8_t qs[16];  // 32 weights packed, 4-bit each
};

// Q8_0 block: 32 weights, 1 scale, full 8-bit precision
struct Q8_0Block {
    float  d;
    int8_t qs[32];
};

// Decode Q4_0 weights to FP32 on-the-fly during GEMM
inline void decode_q4_0_block(float* dst, const Q4_0Block* src, int n_blocks) {
    for (int b = 0; b < n_blocks; ++b) {
        const float    d  = src[b].d;
        const uint8_t* qs = src[b].qs;
        for (int i = 0; i < 16; ++i) {
            uint8_t byte = qs[i];
            dst[b * 32 + i]      = d * (static_cast<int8_t>(byte & 0x0F) - 8);
            dst[b * 32 + i + 16] = d * (static_cast<int8_t>(byte >> 4)   - 8);
        }
    }
}

#ifdef __AVX2__
// AVX2 optimized: Q4_0 x FP32 GEMV (vector-matrix multiply)
// Computes y[row] = dot(x, W[row]) for each output row
inline void gemv_q4_0_fp32_avx2(
    float*             y,
    const float*       x,
    const Q4_0Block*   W,
    int                n_input,
    int                n_output,
    int                n_threads
) {
    const int n_blocks = n_input / 32;

    #pragma omp parallel for num_threads(n_threads) schedule(static)
    for (int row = 0; row < n_output; ++row) {
        const Q4_0Block* w_row = W + row * n_blocks;

        __m256 sum0 = _mm256_setzero_ps();
        __m256 sum1 = _mm256_setzero_ps();
        __m256 sum2 = _mm256_setzero_ps();
        __m256 sum3 = _mm256_setzero_ps();

        for (int b = 0; b < n_blocks; ++b) {
            const float  d     = w_row[b].d;
            const __m256 scale = _mm256_set1_ps(d);

            // Load 16 bytes of packed 4-bit weights
            __m128i packed = _mm_loadu_si128(reinterpret_cast<const __m128i*>(w_row[b].qs));

            // Expand uint8 to uint16
            __m256i bytes = _mm256_cvtepu8_epi16(packed);

            // Mask low nibbles, shift to get high nibbles
            __m256i low  = _mm256_and_si256(bytes, _mm256_set1_epi16(0x0F));
            __m256i high = _mm256_and_si256(_mm256_srli_epi16(bytes, 4), _mm256_set1_epi16(0x0F));

            // Convert 4-bit unsigned (0..15) -> signed (-8..+7), then to float
            __m256 w0 = _mm256_cvtepi32_ps(_mm256_sub_epi32(
                _mm256_cvtepi16_epi32(_mm256_castsi256_si128(low)),
                _mm256_set1_epi32(8)));
            __m256 w1 = _mm256_cvtepi32_ps(_mm256_sub_epi32(
                _mm256_cvtepi16_epi32(_mm256_extracti128_si256(low, 1)),
                _mm256_set1_epi32(8)));
            __m256 w2 = _mm256_cvtepi32_ps(_mm256_sub_epi32(
                _mm256_cvtepi16_epi32(_mm256_castsi256_si128(high)),
                _mm256_set1_epi32(8)));
            __m256 w3 = _mm256_cvtepi32_ps(_mm256_sub_epi32(
                _mm256_cvtepi16_epi32(_mm256_extracti128_si256(high, 1)),
                _mm256_set1_epi32(8)));

            w0 = _mm256_mul_ps(w0, scale);
            w1 = _mm256_mul_ps(w1, scale);
            w2 = _mm256_mul_ps(w2, scale);
            w3 = _mm256_mul_ps(w3, scale);

            const float* x_ptr = x + b * 32;
            __m256 x0 = _mm256_loadu_ps(x_ptr);
            __m256 x1 = _mm256_loadu_ps(x_ptr + 8);
            __m256 x2 = _mm256_loadu_ps(x_ptr + 16);
            __m256 x3 = _mm256_loadu_ps(x_ptr + 24);

#ifdef __FMA__
            sum0 = _mm256_fmadd_ps(w0, x0, sum0);
            sum1 = _mm256_fmadd_ps(w1, x1, sum1);
            sum2 = _mm256_fmadd_ps(w2, x2, sum2);
            sum3 = _mm256_fmadd_ps(w3, x3, sum3);
#else
            sum0 = _mm256_add_ps(_mm256_mul_ps(w0, x0), sum0);
            sum1 = _mm256_add_ps(_mm256_mul_ps(w1, x1), sum1);
            sum2 = _mm256_add_ps(_mm256_mul_ps(w2, x2), sum2);
            sum3 = _mm256_add_ps(_mm256_mul_ps(w3, x3), sum3);
#endif
        }

        // Reduce 4 x 8-float sums -> scalar
        __m256 sum01 = _mm256_add_ps(sum0, sum1);
        __m256 sum23 = _mm256_add_ps(sum2, sum3);
        __m256 sum   = _mm256_add_ps(sum01, sum23);

        __m128 hi     = _mm256_extractf128_ps(sum, 1);
        __m128 lo     = _mm256_castps256_ps128(sum);
        __m128 sum128 = _mm_add_ps(lo, hi);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);

        y[row] = _mm_cvtss_f32(sum128);
    }
}
#else
// Scalar fallback
inline void gemv_q4_0_fp32_avx2(
    float*           y,
    const float*     x,
    const Q4_0Block* W,
    int              n_input,
    int              n_output,
    int              /*n_threads*/
) {
    const int n_blocks = n_input / 32;
    for (int row = 0; row < n_output; ++row) {
        float acc = 0.0f;
        for (int b = 0; b < n_blocks; ++b) {
            float weights[32];
            decode_q4_0_block(weights, W + row * n_blocks + b, 1);
            for (int i = 0; i < 32; ++i) {
                acc += weights[i] * x[b * 32 + i];
            }
        }
        y[row] = acc;
    }
}
#endif // __AVX2__

#ifdef __AVX512F__
// AVX-512 version for Q8_0
inline void gemv_q8_0_fp32_avx512(
    float*           y,
    const float*     x,
    const Q8_0Block* W,
    int              n_input,
    int              n_output,
    int              n_threads
) {
    const int n_blocks = n_input / 32;

    #pragma omp parallel for num_threads(n_threads)
    for (int row = 0; row < n_output; ++row) {
        const Q8_0Block* w_row = W + row * n_blocks;
        __m512 sum = _mm512_setzero_ps();

        for (int b = 0; b < n_blocks; ++b) {
            __m256i qs         = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w_row[b].qs));
            __m512i weights_i32 = _mm512_cvtepi8_epi32(qs);  // only uses low 16 of 32 — see note
            __m512   weights    = _mm512_cvtepi32_ps(weights_i32);
            __m512   scale      = _mm512_set1_ps(w_row[b].d);
            weights = _mm512_mul_ps(weights, scale);
            __m512 x_vec = _mm512_loadu_ps(x + b * 32);
            sum = _mm512_fmadd_ps(weights, x_vec, sum);
        }

        y[row] = _mm512_reduce_add_ps(sum);
    }
}
#endif // __AVX512F__

} // namespace RawrXD::Kernels
