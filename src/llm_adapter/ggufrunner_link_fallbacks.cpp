#include "GGUFRunner.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace {
std::mutex g_runnerFallbackMutex;
std::string g_lastLoadedPath;
std::atomic<uint64_t> g_tokenChunkCount{0};
std::atomic<uint64_t> g_tokenBytes{0};
std::atomic<uint64_t> g_inferenceSuccessCount{0};
std::atomic<uint64_t> g_inferenceFailureCount{0};
std::atomic<int64_t> g_lastLoadedSize{0};
}

void GGUFRunner::tokenChunkGenerated(const std::string& chunk) {
    g_tokenChunkCount.fetch_add(1, std::memory_order_relaxed);
    g_tokenBytes.fetch_add(static_cast<uint64_t>(chunk.size()), std::memory_order_relaxed);
}

void GGUFRunner::inferenceComplete(bool success) {
    if (success) {
        g_inferenceSuccessCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_inferenceFailureCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void GGUFRunner::modelLoaded(const std::string& path, int64_t sizeBytes) {
    std::lock_guard<std::mutex> lock(g_runnerFallbackMutex);
    g_lastLoadedPath = path;
    g_lastLoadedSize.store(sizeBytes, std::memory_order_relaxed);
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
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k) {
                const int linear = k * N + n;
                const uint8_t packed = Bq4[linear / 2];
                const int nibble = (linear & 1) ? (packed >> 4) : (packed & 0x0F);
                const float b = static_cast<float>(nibble - 8) * scale;
                acc += A[m * K + k] * b;
            }
            C[m * N + n] = acc;
        }
    }
}
