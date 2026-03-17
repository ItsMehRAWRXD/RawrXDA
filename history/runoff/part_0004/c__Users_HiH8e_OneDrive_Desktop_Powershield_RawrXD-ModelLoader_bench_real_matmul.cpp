// Real-world matmul benchmark: tests actual inference dimensions
// Typical LLaMA attention: (seq_len × hidden_dim) × (hidden_dim × heads × head_dim)
#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>
#include <immintrin.h>

// Production matmul with transpose optimization
void matmul_avx2(const float* A, const float* B, float* C, int N, int M, int K) {
#ifdef __AVX2__
    // Transpose B: B^T is K×M instead of M×K
    std::vector<float> BT(K * M);
    for (int k = 0; k < M; ++k) {
        for (int j = 0; j < K; ++j) {
            BT[j * M + k] = B[k * K + j];
        }
    }
    
    // AVX2 SIMD: process 8 floats at a time
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            __m256 sum = _mm256_setzero_ps();
            int k = 0;
            
            for (; k + 7 < M; k += 8) {
                __m256 a = _mm256_loadu_ps(&A[i * M + k]);
                __m256 b = _mm256_loadu_ps(&BT[j * M + k]);
                sum = _mm256_fmadd_ps(a, b, sum);
            }
            
            float result[8];
            _mm256_storeu_ps(result, sum);
            float s = result[0] + result[1] + result[2] + result[3] +
                      result[4] + result[5] + result[6] + result[7];
            
            for (; k < M; ++k) s += A[i * M + k] * BT[j * M + k];
            
            C[i * K + j] = s;
        }
    }
#else
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            float s = 0.0f;
            for (int k = 0; k < M; ++k) s += A[i * M + k] * B[k * K + j];
            C[i * K + j] = s;
        }
    }
#endif
}

int main() {
    // Realistic LLaMA-70B dimensions (seq_len=512, hidden=8192)
    // Q×K^T for attention scores: (512×8192) × (8192×512)
    const int seq_len = 512;
    const int hidden = 8192;
    
    std::vector<float> A(seq_len * hidden, 0.1f);
    std::vector<float> B(hidden * seq_len, 0.1f);
    std::vector<float> C(seq_len * seq_len);
    
    // Warmup
    matmul_avx2(A.data(), B.data(), C.data(), seq_len, hidden, seq_len);
    
    // Benchmark: 10 iterations
    const int iters = 10;
    auto t0 = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iters; ++i) {
        matmul_avx2(A.data(), B.data(), C.data(), seq_len, hidden, seq_len);
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    
    std::cout << ms << "\n";
    
    return 0;
}
