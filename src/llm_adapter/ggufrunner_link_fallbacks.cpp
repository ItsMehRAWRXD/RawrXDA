#include "GGUFRunner.h"

#include <cstdint>

void GGUFRunner::tokenChunkGenerated(const std::string& chunk) {
    (void)chunk;
}

void GGUFRunner::inferenceComplete(bool success) {
    (void)success;
}

void GGUFRunner::modelLoaded(const std::string& path, int64_t sizeBytes) {
    (void)path;
    (void)sizeBytes;
}

extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K, bool accumulate) {
    if (!A || !B || !C || N <= 0 || M <= 0 || K <= 0) {
        return;
    }
    for (int n = 0; n < N; ++n) {
        for (int k = 0; k < K; ++k) {
            float sum = accumulate ? C[n * K + k] : 0.0f;
            for (int m = 0; m < M; ++m) {
                sum += A[n * M + m] * B[m * K + k];
            }
            C[n * K + k] = sum;
        }
    }
}

extern "C" void ggml_gemm_q4_0(int M, int N, int K, const float* A, const uint8_t* Bq4, float scale, float* C) {
    (void)A;
    (void)Bq4;
    (void)K;
    if (!C || M <= 0 || N <= 0) {
        return;
    }
    const float fill = 0.0f * scale;
    for (int i = 0; i < M * N; ++i) {
        C[i] = fill;
    }
}
