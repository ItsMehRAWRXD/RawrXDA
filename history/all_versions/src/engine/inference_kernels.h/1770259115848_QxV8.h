#pragma once
#include "common_types.h"
#include <cstdint>

class InferenceKernels {
public:
    static void matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K);
    static void matmul_f32(const float* A, const float* B, float* C, int M, int N, int K);
    static void matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y, int n, int m, int k);
    static void gelu_avx512(float* x, int n);
    static void softmax_avx512(float* x, int n);
    static void rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps = 1e-6f);
    static void rope_avx512(float* q, float* k, int head_dim, int pos, float theta = 10000.0f, float scale = 1.0f);
};
