// ============================================================================
// k_quant_kernels.h — AVX2/AVX-512 K-Quant Dequantization Kernels
// ============================================================================
// Implements dequantization for GGML K-quant formats used by Mixtral, Llama-3,
// Qwen, Phi-3 and most GGUF models distributed as Q4_K_M / Q5_K_M / Q6_K_M.
//
// Format reference (matches llama.cpp / ggml layout exactly):
//
//   Q4_K — 144 bytes / 256 elements (two super-blocks of 128):
//     d      : float16  — super-block scale
//     dmin   : float16  — super-block min
//     scales[12] : uint8 — 6-bit quantized sub-block scales (packed)
//     qs[64] : uint8    — 4-bit quantized weights (packed nibbles)
//
//   Q5_K — 176 bytes / 256 elements:
//     d, dmin : float16
//     scales[12] : uint8
//     qh[32]  : uint8  — high bit of each 5-bit value (1 bit/element)
//     qs[64]  : uint8  — low 4 bits of each 5-bit value
//
//   Q6_K — 210 bytes / 256 elements:
//     ql[128] : uint8  — low 4 bits of each 6-bit value
//     qh[64]  : uint8  — high 2 bits packed (4 per byte)
//     scales[16]: int8 — per-16-element scale
//     d       : float16 — super-block scale
//
// Dispatch:
//   dequant_k(type, src, dst, nelems) — unified entry point
//   matmul_q4_K_avx2()               — fused dequant + multivec GEMV
//
// Pattern: no exceptions, no heap allocation in hot paths.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include "common_types.h"
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace rawrxd {
namespace kquant {

// ---------------------------------------------------------------------------
// Block structs — must match GGML layout byte-for-byte
// ---------------------------------------------------------------------------
#pragma pack(push, 1)

// Q4_K super-block: 256 weights, 144 bytes
struct block_q4_K {
    uint16_t d;         // fp16 super-block scale
    uint16_t dmin;      // fp16 super-block min
    uint8_t  scales[12];// 6-bit sub-scales (8 sub-blocks × 6 bits = 48 bits, packed)
    uint8_t  qs[128];    // 4-bit quantized values packed (256 nibbles in 128 bytes — 64 bytes low + 64 bytes high)
};
static_assert(sizeof(block_q4_K) == 144, "block_q4_K size mismatch");

// Q5_K super-block: 256 weights, 176 bytes
struct block_q5_K {
    uint16_t d;          // fp16 scale
    uint16_t dmin;       // fp16 min
    uint8_t  scales[12]; // 6-bit sub-scales (same packing as Q4_K)
    uint8_t  qh[32];     // high bits: bit i of element j is (qh[j/8] >> (j%8)) & 1
    uint8_t  qs[128];     // low 4 bits
};
static_assert(sizeof(block_q5_K) == 176, "block_q5_K size mismatch");

// Q6_K super-block: 256 weights, 210 bytes
struct block_q6_K {
    uint8_t  ql[128];    // low 4 bits of each 6-bit weight (256 values → 128 bytes)
    uint8_t  qh[64];     // high 2 bits packed (4 per byte, 256 values → 64 bytes)
    int8_t   scales[16]; // per-16-element scale factors
    uint16_t d;          // fp16 super-block scale
};
static_assert(sizeof(block_q6_K) == 210, "block_q6_K size mismatch");

#pragma pack(pop)

// ---------------------------------------------------------------------------
// fp16→fp32 helpers (scalar, no intrinsics needed for scale loads)
// ---------------------------------------------------------------------------
inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign     = (h >> 15) & 0x1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    if (exponent == 0) {
        if (mantissa == 0) return sign ? -0.0f : 0.0f;
        // Denormal
        float val = (float)mantissa / 1024.0f / 16.0f;
        return sign ? -val : val;
    }
    if (exponent == 31) {
        { uint32_t nb = 0x7FC00000u|(mantissa<<13); float nv; memcpy(&nv,&nb,4); return mantissa ? nv : (sign ? -1e38f : 1e38f); } // NaN/Inf
    }
    uint32_t bits = (sign << 31) | ((exponent + 112) << 23) | (mantissa << 13);
    float f; memcpy(&f, &bits, 4); return f;
}

// ---------------------------------------------------------------------------
// Sub-scale unpacking — Q4_K / Q5_K scale layout
// Each sub-block (32 elements) has two 6-bit values packed into the scales[]:
//   scales[0..7]  : low 6 bits of scale for sub-blocks 0..7
//   scales[8..11] : high 2 bits for sub-blocks 0..7 packed 2/byte
// ---------------------------------------------------------------------------
inline void unpack_scales_q4K(const uint8_t* scales_raw, float d, float dmin,
                               float* scale_out, float* min_out, int n_sub = 8)
{
    // Decode 6-bit scales packed into 12 bytes for 8 sub-blocks
    uint8_t sc[8], mn[8];
    for (int i = 0; i < 4; ++i) {
        sc[i]   = (scales_raw[i]     ) & 0x3F;
        mn[i]   = (scales_raw[i + 4] ) & 0x3F;
        sc[i+4] = (scales_raw[i]     ) >> 6 | ((scales_raw[i + 8] & 0x0F) << 2);
        mn[i+4] = (scales_raw[i + 4] ) >> 6 | ((scales_raw[i + 8] >> 4  ) << 2);
    }
    for (int i = 0; i < n_sub; ++i) {
        scale_out[i] = d    * (float)sc[i];
        min_out[i]   = dmin * (float)mn[i];
    }
}

// ---------------------------------------------------------------------------
// Dequantize Q4_K — 256 values → float array
// ---------------------------------------------------------------------------
void dequantize_q4_K(const block_q4_K* blk, float* dst, int n_blocks);

// ---------------------------------------------------------------------------
// Dequantize Q5_K — 256 values → float array
// ---------------------------------------------------------------------------
void dequantize_q5_K(const block_q5_K* blk, float* dst, int n_blocks);

// ---------------------------------------------------------------------------
// Dequantize Q6_K — 256 values → float array
// ---------------------------------------------------------------------------
void dequantize_q6_K(const block_q6_K* blk, float* dst, int n_blocks);

// ---------------------------------------------------------------------------
// Fused GEMV — dequant + dot product (single output row)
// x:   [k]      float input vector
// w:   [n×k/256] packed blocks — one row per output element
// y:   [n]      output accumulators (+=)
// n, k: dimensions
// ---------------------------------------------------------------------------
void gemv_q4_K_avx2(const float* x, const block_q4_K* w, float* y, int n, int k);
void gemv_q6_K_avx2(const float* x, const block_q6_K* w, float* y, int n, int k);

// ---------------------------------------------------------------------------
// Unified dispatch — matches ggml_rxd_type enum from common_types.h
// ---------------------------------------------------------------------------
void dequant_k(ggml_rxd_type type, const void* src, float* dst, int n_elements);

// ---------------------------------------------------------------------------
// Fused dispatch — pick the right GEMV for the weight type
// Used by sovereign_engines / pyre_compute dispatch table
// ---------------------------------------------------------------------------
void matmul_k_fused(ggml_rxd_type type,
                    const float* x, const void* w, float* y,
                    int n_out, int k_dim);

} // namespace kquant
} // namespace rawrxd
