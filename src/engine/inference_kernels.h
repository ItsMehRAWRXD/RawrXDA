#pragma once
#include "common_types.h"
#include <cstdint>
#include <immintrin.h>

// ============================================================================
// fast_exp_avx2 — Exposed for use by transformer.cpp SwiGLU and other modules
// Schraudolph-style 4th-order polynomial exp approximation in AVX2
// Monotonic, clamped to [-88.37, +88.37], max relative error < 1e-4
// ============================================================================
static inline __m256 fast_exp_avx2_shared(__m256 x) {
    const __m256 max_val = _mm256_set1_ps(88.3762626647949f);
    const __m256 min_val = _mm256_set1_ps(-88.3762626647949f);
    x = _mm256_min_ps(x, max_val);
    x = _mm256_max_ps(x, min_val);
    const __m256 log2e = _mm256_set1_ps(1.4426950408889634f);
    const __m256 half   = _mm256_set1_ps(0.5f);
    __m256 t = _mm256_fmadd_ps(x, log2e, half);
    __m256 t_floor = _mm256_floor_ps(t);
    __m256 f = _mm256_sub_ps(t, t_floor);
    f = _mm256_sub_ps(f, half);
    __m256i ni = _mm256_cvtps_epi32(t_floor);
    ni = _mm256_add_epi32(ni, _mm256_set1_epi32(127));
    ni = _mm256_slli_epi32(ni, 23);
    __m256 pow2n = _mm256_castsi256_ps(ni);
    const __m256 ln2   = _mm256_set1_ps(0.6931471805599453f);
    const __m256 c2    = _mm256_set1_ps(0.24022650695910072f);
    const __m256 c3    = _mm256_set1_ps(0.05550410866482158f);
    const __m256 one   = _mm256_set1_ps(1.0f);
    __m256 poly = _mm256_fmadd_ps(c3, f, c2);
    poly = _mm256_fmadd_ps(poly, f, ln2);
    poly = _mm256_fmadd_ps(poly, f, one);
    return _mm256_mul_ps(pow2n, poly);
}

class InferenceKernels {
public:
    static void matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K);
    static void matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y, int n, int m, int k);
    static void gelu_avx512(float* x, int n);
    static void softmax_avx512(float* x, int n);
    static void rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps = 1e-6f);
    static void rope_avx512(float* q, float* k, int head_dim, int pos, float theta = 10000.0f, float scale = 1.0f);
    
    // ====================================================================
    // Flash-Attention v2 — Tiled attention with O(N) memory
    // Processes Q×K in tiles of BLOCK_SIZE to avoid materializing full NxN
    // score matrix. Online softmax with running max/sum correction.
    // ====================================================================
    static void flash_attention_v2(
        const float* Q, const float* K_cache, const float* V_cache,
        float* output,
        int seq_len,         // number of KV entries
        int logical_start_pos, // absolute token index for seq element 0 (ring-buffer chronology)
        int n_heads,         // query heads
        int n_kv_heads,      // KV heads (GQA support)
        int head_dim,        // dimension per head
        int max_seq_len,     // ring buffer capacity
        int BLOCK_SIZE = 64  // tile size for K/V blocks
    );
    
    // ====================================================================
    // Fused MLP Kernel — gate_proj + SiLU + up_proj + elementwise mul
    // Single pass through memory: minimizes cache thrashing for FFN
    // gate = SiLU(W1 @ x) * (W3 @ x), then output = W2 @ gate
    // This function fuses the first part: SiLU(gate) * up
    // ====================================================================
    static void fused_silu_mul_avx2(float* gate, const float* up, int n);
    
    // ====================================================================
    // KV Cache Quantization Helpers — FP32 ↔ int8 with per-head scales
    // Reduces KV cache bandwidth by 4x (FP32 → int8)
    // ====================================================================
    static void quantize_kv_fp32_to_int8(const float* src, int8_t* dst, float* scale, int n);
    static void dequantize_kv_int8_to_fp32(const int8_t* src, float* dst, float scale, int n);
};
