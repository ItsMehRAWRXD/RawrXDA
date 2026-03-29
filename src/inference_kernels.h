// Inference kernel declarations — AVX2/AVX-512 optimized matrix operations
// These are implemented in MASM assembly (tasks.asm) or C++ fallback paths.
#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Quantized matrix-vector multiply (Q4_K format, AVX2 path)
void rawrxd_gemv_q4k_avx2(
    const void* weights,      // Q4_K quantized weight block
    const float* input,       // Input vector (fp32)
    float* output,            // Output vector (fp32)
    int64_t rows,
    int64_t cols
);

// FP32 matrix multiply kernel (AVX2)
void rawrxd_sgemm_avx2(
    const float* A,
    const float* B,
    float* C,
    int64_t M, int64_t N, int64_t K
);

// Softmax in-place (AVX2 accelerated)
void rawrxd_softmax_avx2(float* data, int64_t len);

// RMS normalization kernel
void rawrxd_rmsnorm_avx2(
    float* output,
    const float* input,
    const float* weight,
    int64_t size,
    float eps
);

// SiLU activation (x * sigmoid(x))
void rawrxd_silu_avx2(float* data, int64_t len);

// Rope (rotary position embedding) apply
void rawrxd_rope_avx2(
    float* data,
    int64_t seq_len,
    int64_t n_dims,
    int64_t n_heads,
    float theta_base
);

#ifdef __cplusplus
}
#endif
