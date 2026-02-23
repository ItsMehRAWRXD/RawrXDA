#include <immintrin.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

extern "C" void ggml_gemm_q4_0(int M, int N, int K, const float* A, const uint8_t* Bq4, float scale, float* C);

static void gemm_q4_0_scalar(int M, int N, int K, const float* A, const uint8_t* Bq4, float scale, float* C) {
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                int idx = k * N + j;
                int byteIndex = idx >> 1;
                bool hi = (idx & 1) != 0;
                uint8_t byte = Bq4[byteIndex];
                int8_t w4 = hi ? ((byte >> 4) & 0xF) : (byte & 0xF);
                float w = (float)(w4 - 8) * scale;
                sum += A[i * K + k] * w;
            }
            C[i * N + j] = sum;
        }
    }
}

static void pack_q4_0(int K, int N, const float* Bfp32, float scale, uint8_t* Bq4) {
    for (int k = 0; k < K; ++k) {
        for (int j = 0; j < N; j += 2) {
            float w0 = Bfp32[k * N + j] / scale;
            float w1 = (j + 1 < N) ? (Bfp32[k * N + j + 1] / scale) : 0.0f;
            int v0 = std::clamp((int)std::round(w0) + 8, 0, 15);
            int v1 = std::clamp((int)std::round(w1) + 8, 0, 15);
            int byteIndex = (k * N + j) >> 1;
            Bq4[byteIndex] = (uint8_t)((v1 << 4) | v0);
        }
    }
}

int main() {
    const int M = 64;
    const int K = 128;
    const int N = 64;

    std::vector<float> A(M * K);
    std::vector<float> Bfp32(K * N);
    std::vector<uint8_t> Bq4((K * N + 1) / 2);
    std::vector<float> Cref(M * N);
    std::vector<float> Copt(M * N);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (auto& v : A) v = dist(rng);
    for (auto& v : Bfp32) v = std::round(dist(rng) * 7.0f);

    float scale = 0.5f;
    pack_q4_0(K, N, Bfp32.data(), scale, Bq4.data());

    gemm_q4_0_scalar(M, N, K, A.data(), Bq4.data(), scale, Cref.data());
    ggml_gemm_q4_0(M, N, K, A.data(), Bq4.data(), scale, Copt.data());

    float max_abs = 0.0f;
    for (int i = 0; i < M * N; ++i) max_abs = std::max(max_abs, std::abs(Cref[i] - Copt[i]));
    std::printf("Max abs diff: %.6f\n", max_abs);

    const int iters = 100;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        gemm_q4_0_scalar(M, N, K, A.data(), Bq4.data(), scale, Cref.data());
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_scalar = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        ggml_gemm_q4_0(M, N, K, A.data(), Bq4.data(), scale, Copt.data());
    }
    t1 = std::chrono::high_resolution_clock::now();
    double ms_opt = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double speedup = ms_scalar / ms_opt;
    std::printf("Scalar: %.2f ms  Opt(AVX2): %.2f ms  Speedup: %.2fx\n", ms_scalar, ms_opt, speedup);

    if (speedup >= 1.8) {
        std::puts("✅ END-TO-END: >= 1.8× speedup achieved");
        return 0;
    } else {
        std::puts("❌ END-TO-END: below 1.8× target");
        return 1;
    }
}
