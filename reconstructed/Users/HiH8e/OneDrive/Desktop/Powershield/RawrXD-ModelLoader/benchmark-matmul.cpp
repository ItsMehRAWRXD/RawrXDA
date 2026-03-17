// Simple matrix multiplication benchmark
// Compile with: cl /O2 /std:c++17 benchmark-matmul.cpp (scalar)
//               cl /O2 /arch:AVX2 /std:c++17 benchmark-matmul.cpp (AVX2)

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>

#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#include <immintrin.h>
#define USE_AVX2 1
#endif

void matmul_scalar(const float* A, const float* B, float* C, int N, int M, int K) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            float s = 0.0f;
            for (int k = 0; k < M; ++k) {
                s += A[i * M + k] * B[k * K + j];
            }
            C[i * K + j] = s;
        }
    }
}

#ifdef USE_AVX2
void matmul_avx2(const float* A, const float* B, float* C, int N, int M, int K) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            __m256 sum = _mm256_setzero_ps();
            int k = 0;
            
            // Vectorized loop
            for (; k + 7 < M; k += 8) {
                __m256 a = _mm256_loadu_ps(&A[i * M + k]);
                __m256 b = _mm256_set_ps(
                    B[(k+7) * K + j], B[(k+6) * K + j],
                    B[(k+5) * K + j], B[(k+4) * K + j],
                    B[(k+3) * K + j], B[(k+2) * K + j],
                    B[(k+1) * K + j], B[k * K + j]
                );
                sum = _mm256_fmadd_ps(a, b, sum);
            }
            
            // Horizontal reduction
            float result[8];
            _mm256_storeu_ps(result, sum);
            float s = result[0] + result[1] + result[2] + result[3] +
                      result[4] + result[5] + result[6] + result[7];
            
            // Scalar tail
            for (; k < M; ++k) {
                s += A[i * M + k] * B[k * K + j];
            }
            
            C[i * K + j] = s;
        }
    }
}
#endif

int main(int argc, char** argv) {
    // Typical LLM dimensions: embedDim × embedDim
    const int N = 1;      // Batch size (single token)
    const int M = 4096;   // Embedding dimension (e.g., LLaMA-7B)
    const int K = 4096;   // Output dimension
    
    std::cout << "Matrix Multiplication Benchmark\n";
    std::cout << "================================\n";
    std::cout << "Dimensions: " << N << " × " << M << " @ " << M << " × " << K << "\n";
    std::cout << "Size: " << (N*M + M*K + N*K) * sizeof(float) / 1024.0 / 1024.0 << " MB\n\n";
    
    // Allocate matrices
    std::vector<float> A(N * M);
    std::vector<float> B(M * K);
    std::vector<float> C_scalar(N * K);
    std::vector<float> C_simd(N * K);
    
    // Initialize with random data
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& v : A) v = dist(rng);
    for (auto& v : B) v = dist(rng);
    
    // Warm-up
    matmul_scalar(A.data(), B.data(), C_scalar.data(), N, M, K);
    
    // Benchmark scalar
    const int iterations = 10;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        matmul_scalar(A.data(), B.data(), C_scalar.data(), N, M, K);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto scalar_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    
    std::cout << "Scalar:  " << scalar_ms << " ms/iteration\n";
    
#ifdef USE_AVX2
    // Benchmark AVX2
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        matmul_avx2(A.data(), B.data(), C_simd.data(), N, M, K);
    }
    end = std::chrono::high_resolution_clock::now();
    auto avx2_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    
    std::cout << "AVX2:    " << avx2_ms << " ms/iteration\n";
    std::cout << "Speedup: " << scalar_ms / avx2_ms << "×\n\n";
    
    // Verify correctness
    double max_diff = 0.0;
    for (size_t i = 0; i < C_scalar.size(); ++i) {
        double diff = std::abs(C_scalar[i] - C_simd[i]);
        max_diff = std::max(max_diff, diff);
    }
    std::cout << "Max difference: " << max_diff << " (should be ~0 for bit-exact)\n";
    
    if (max_diff < 1e-3) {
        std::cout << "✓ Results match!\n";
    } else {
        std::cout << "⚠ Results differ (expected due to rounding)\n";
    }
#else
    std::cout << "AVX2:    Not compiled (use /arch:AVX2)\n";
#endif
    
    return 0;
}
