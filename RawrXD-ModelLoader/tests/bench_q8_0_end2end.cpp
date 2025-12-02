#include <immintrin.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

extern "C" void ggml_gemm_q8_0(int M, int N, int K, const float* A, const int8_t* Bq8, float scale, float* C);

static void gemm_q8_0_scalar(int M, int N, int K, const float* A, const int8_t* Bq8, float scale, float* C) {
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                float w = (float)Bq8[k * N + j] * scale;
                sum += A[i * K + k] * w;
            }
            C[i * N + j] = sum;
        }
    }
}

static void pack_q8_0(int K, int N, const float* Bfp32, float scale, int8_t* Bq8) {
    for (int k = 0; k < K; ++k) {
        for (int j = 0; j < N; ++j) {
            float w = Bfp32[k * N + j] / scale;
            Bq8[k * N + j] = (int8_t)std::clamp((int)std::round(w), -127, 127);
        }
    }
}

int main() {
    const int M = 64;
    const int K = 128;
    const int N = 64;

    std::vector<float> A(M * K);
    std::vector<float> Bfp32(K * N);
    std::vector<int8_t> Bq8(K * N);
    std::vector<float> Cref(M * N);
    std::vector<float> Copt(M * N);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (auto& v : A) v = dist(rng);
    for (auto& v : Bfp32) v = std::round(dist(rng) * 100.0f);

    float scale = 0.5f;
    pack_q8_0(K, N, Bfp32.data(), scale, Bq8.data());

    gemm_q8_0_scalar(M, N, K, A.data(), Bq8.data(), scale, Cref.data());
    ggml_gemm_q8_0(M, N, K, A.data(), Bq8.data(), scale, Copt.data());

    float max_abs = 0.0f;
    for (int i = 0; i < M * N; ++i) max_abs = std::max(max_abs, std::abs(Cref[i] - Copt[i]));
    std::printf("Max abs diff: %.6f\n", max_abs);

    const int iters = 100;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        gemm_q8_0_scalar(M, N, K, A.data(), Bq8.data(), scale, Cref.data());
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_scalar = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        ggml_gemm_q8_0(M, N, K, A.data(), Bq8.data(), scale, Copt.data());
    }
    t1 = std::chrono::high_resolution_clock::now();
    double ms_opt = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double speedup = ms_scalar / ms_opt;
    std::printf("Scalar: %.2f ms  Opt(AVX2): %.2f ms  Speedup: %.2fx\n", ms_scalar, ms_opt, speedup);

    if (speedup >= 2.5) {
        std::puts("✅ END-TO-END: >= 2.5× speedup achieved");
        return 0;
    } else {
        std::puts("❌ END-TO-END: below 2.5× target");
        return 1;
    }
}
