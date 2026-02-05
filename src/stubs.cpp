#include "engine/inference_kernels.h"
#include <iostream>
#include <vector>
#include <string>

// Removed register_rawr_inference (defined in rawr_engine.cpp)
void register_sovereign_engines() {}

namespace Diagnostics {
    void error(const std::string& title, const std::string& message) {
        std::cerr << "[ERROR] " << title << ": " << message << std::endl;
    }
}

// InferenceKernels Stubs
void InferenceKernels::softmax_avx512(float* x, int n) {}
void InferenceKernels::rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps) {}
void InferenceKernels::rope_avx512(float* q, float* k, int head_dim, int pos, float theta, float scale) {}
void InferenceKernels::gelu_avx512(float* x, int n) {}
void InferenceKernels::matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K) {}
void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y, int n, int m, int k) {}