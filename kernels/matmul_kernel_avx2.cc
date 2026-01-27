#include <immintrin.h>
#include <cstring>

// Core implementation (C linkage for compatibility)
extern "C" void matmul_kernel_avx2_impl(const float* A, const float* B, float* C, int M, int N, int K, bool accumulate) {
#if defined(__AVX2__)
    // Zero initialize C if not accumulating
    if (!accumulate) {
        for (int i = 0; i < M; ++i) {
            std::memset(C + i * N, 0, sizeof(float) * N);
        }
    }

    for (int i = 0; i < M; ++i) {
        for (int k = 0; k < K; ++k) {
            __m256 a_vec = _mm256_set1_ps(A[i * K + k]);

            int j = 0;
            for (; j + 8 <= N; j += 8) {
                __m256 b_vec = _mm256_loadu_ps(B + k * N + j);
                __m256 c_vec = _mm256_loadu_ps(C + i * N + j);
                c_vec = _mm256_fmadd_ps(a_vec, b_vec, c_vec);
                _mm256_storeu_ps(C + i * N + j, c_vec);
            }
            for (; j < N; ++j) {
                C[i * N + j] += A[i * K + k] * B[k * N + j];
            }
        }
    }
#else
    // Fallback scalar implementation
    if (!accumulate) {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < K; ++k) {
                    sum += A[i * K + k] * B[k * N + j];
                }
                C[i * N + j] = sum;
            }
        }
    } else {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                for (int k = 0; k < K; ++k) {
                    C[i * N + j] += A[i * K + k] * B[k * N + j];
                }
            }
        }
    }
#endif
}

// C++ overload for new code with accumulate parameter
void matmul_kernel_avx2(const float* A, const float* B, float* C, int M, int N, int K, bool accumulate) {
    matmul_kernel_avx2_impl(A, B, C, M, N, K, accumulate);
}

// Legacy C wrapper for old code (non-const pointers, no accumulate)
extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K) {
    matmul_kernel_avx2_impl(A, B, C, N, M, K, false);
}
