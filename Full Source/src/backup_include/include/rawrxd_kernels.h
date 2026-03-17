// rawrxd_kernels.h - C++20 to MASM x64 bridge (extern "C" only)
#pragma once
#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// SGEMM: C = A * B, A[M*K], B[K*N], C[M*N]
void __cdecl RawrXD_InferenceCore_SGEMM_AVX2(
    const float* __restrict A,
    const float* __restrict B,
    float* __restrict C,
    int M, int N, int K);

// Flash Attention v2: O = softmax(Q*K.T/sqrt(d)) * V
void __cdecl RawrXD_FlashAttention_AVX512(
    const float* __restrict Q,
    const float* __restrict K,
    const float* __restrict V,
    float* __restrict O,
    int N, int d, float scale);

// Dequantization
void __cdecl RawrXD_Dequant_Q4_0_AVX2(
    const void* __restrict blocks,
    float* __restrict output,
    int n_blocks);

void __cdecl RawrXD_Dequant_Q4_K_AVX2(
    const void* __restrict blocks,
    float* __restrict output,
    int n_blocks);

#ifdef __cplusplus
}
#endif

// C++20 wrapper namespace
namespace RawrXD { namespace Kernels {
inline void SGEMM(const float* A, const float* B, float* C, int M, int N, int K) {
    RawrXD_InferenceCore_SGEMM_AVX2(A, B, C, M, N, K);
}
inline void FlashAttention(const float* Q, const float* K, const float* V,
    float* O, int N, int d, float scale = 0.0f) {
    if (scale == 0.0f) scale = 1.0f / sqrtf(static_cast<float>(d));
    RawrXD_FlashAttention_AVX512(Q, K, V, O, N, d, scale);
}
}}
