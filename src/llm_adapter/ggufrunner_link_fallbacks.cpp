#include "GGUFRunner.h"

#include <atomic>
#include <cstdint>

namespace {
std::atomic<uint64_t> g_tokenChunks{0};
std::atomic<uint64_t> g_inferenceSuccess{0};
std::atomic<uint64_t> g_inferenceFailure{0};
std::atomic<int64_t> g_lastModelBytes{0};
}

void GGUFRunner::tokenChunkGenerated(const std::string& chunk) {
    if (!chunk.empty()) {
        g_tokenChunks.fetch_add(1, std::memory_order_relaxed);
    }
}

void GGUFRunner::inferenceComplete(bool success) {
    if (success) {
        g_inferenceSuccess.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_inferenceFailure.fetch_add(1, std::memory_order_relaxed);
    }
}

void GGUFRunner::modelLoaded(const std::string& path, int64_t sizeBytes) {
    if (!path.empty()) {
        g_lastModelBytes.store(sizeBytes, std::memory_order_relaxed);
    }
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
    if (!A || !Bq4 || !C || M <= 0 || N <= 0 || K <= 0) {
        return;
    }

    // Fallback decode for packed Q4 data (two signed 4-bit values per byte).
    // Layout assumption: B matrix is KxN in row-major quantized element order.
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                const int qIndex = k * N + n;
                const uint8_t packed = Bq4[qIndex >> 1];
                const uint8_t nibble = (qIndex & 1) ? static_cast<uint8_t>(packed >> 4) : static_cast<uint8_t>(packed & 0x0F);
                const int signedQ = static_cast<int>(nibble) - 8;
                const float b = static_cast<float>(signedQ) * scale;
                sum += A[m * K + k] * b;
            }
            C[m * N + n] = sum;
        }
    }
}
