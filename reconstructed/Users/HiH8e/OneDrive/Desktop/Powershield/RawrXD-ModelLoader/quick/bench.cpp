#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>
#include <immintrin.h>

// Cache-blocked GEMM with 128×128 tiles, 8×8 micro-kernel
constexpr int MC = 128, KC = 128, NC = 128;

// Scalar micro-kernel: optimized with better cache access
void gemm_micro_scalar(int mr, int nr, int kc, const float* A, int lda, 
                       const float* B, int ldb, float* C, int ldc) {
    // Process 4 rows at a time for better register reuse
    for (int i = 0; i < mr; i += 4) {
        int m_bound = (i + 4 <= mr) ? 4 : (mr - i);
        for (int j = 0; j < nr; ++j) {
            float sum0 = C[(i+0)*ldc + j];
            float sum1 = (m_bound > 1) ? C[(i+1)*ldc + j] : 0;
            float sum2 = (m_bound > 2) ? C[(i+2)*ldc + j] : 0;
            float sum3 = (m_bound > 3) ? C[(i+3)*ldc + j] : 0;
            
            for (int k = 0; k < kc; ++k) {
                float b_val = B[k*ldb + j];
                sum0 += A[(i+0)*lda + k] * b_val;
                if (m_bound > 1) sum1 += A[(i+1)*lda + k] * b_val;
                if (m_bound > 2) sum2 += A[(i+2)*lda + k] * b_val;
                if (m_bound > 3) sum3 += A[(i+3)*lda + k] * b_val;
            }
            
            C[(i+0)*ldc + j] = sum0;
            if (m_bound > 1) C[(i+1)*ldc + j] = sum1;
            if (m_bound > 2) C[(i+2)*ldc + j] = sum2;
            if (m_bound > 3) C[(i+3)*ldc + j] = sum3;
        }
    }
}

#ifdef __AVX2__
// AVX2 micro-kernel: process 8 columns at once with 8 accumulators
void gemm_micro_avx2(int mr, int nr, int kc, const float* A, int lda,
                     const float* B, int ldb, float* C, int ldc) {
    for (int i = 0; i < mr; ++i) {
        __m256 c0 = _mm256_loadu_ps(&C[i*ldc + 0]);
        
        for (int k = 0; k < kc; ++k) {
            __m256 a = _mm256_set1_ps(A[i*lda + k]);
            __m256 b0 = _mm256_loadu_ps(&B[k*ldb + 0]);
            c0 = _mm256_fmadd_ps(a, b0, c0);
        }
        
        _mm256_storeu_ps(&C[i*ldc + 0], c0);
    }
}
#endif

void gemm_blocked(int M, int N, int K, const float* A, const float* B, float* C) {
    std::memset(C, 0, M * N * sizeof(float));
    
    for (int ii = 0; ii < M; ii += MC) {
        int mb = std::min(MC, M - ii);
        for (int jj = 0; jj < N; jj += NC) {
            int nb = std::min(NC, N - jj);
            for (int kk = 0; kk < K; kk += KC) {
                int kb = std::min(KC, K - kk);
                
                #ifdef __AVX2__
                // Process 8 columns at a time with AVX2
                int j = 0;
                for (; j + 8 <= nb; j += 8) {
                    gemm_micro_avx2(mb, 8, kb, &A[ii*K + kk], K,
                                   &B[kk*N + jj + j], N, &C[ii*N + jj + j], N);
                }
                // Handle remaining columns
                if (j < nb) {
                    gemm_micro_scalar(mb, nb - j, kb, &A[ii*K + kk], K,
                                    &B[kk*N + jj + j], N, &C[ii*N + jj + j], N);
                }
                #else
                gemm_micro_scalar(mb, nb, kb, &A[ii*K + kk], K,
                                &B[kk*N + jj], N, &C[ii*N + jj], N);
                #endif
            }
        }
    }
}

int main() {
    const int M = 4096, N = 4096, K = 4096, iters = 1;  // Reduced to 1 iteration for testing
    
    // Allocate aligned memory for better performance
    std::vector<float> A(M * K, 0.5f);
    std::vector<float> B(K * N, 0.5f);
    std::vector<float> C(M * N, 0.0f);
    
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iters; ++iter) {
        gemm_blocked(M, N, K, A.data(), B.data(), C.data());
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    std::cout << ms << "\n";
    
    return 0;
}
