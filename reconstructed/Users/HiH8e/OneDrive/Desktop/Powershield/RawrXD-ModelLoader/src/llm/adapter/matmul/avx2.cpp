#include <immintrin.h>
#include <cstddef>

extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K)
{
    if (!A || !B || !C) {
        return;
    }

    // Basic guard: if AVX2 unavailable at compile time, fall back to scalar path.
#if !defined(__AVX2__)
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k) {
                sum += A[i * M + k] * B[k * K + j];
            }
            C[i * K + j] = sum;
        }
    }
    return;
#else
    // Simple AVX2 implementation processing 8 columns at a time.
    for (int row = 0; row < N; ++row) {
        float* cRow = C + row * K;
        const float* aRow = A + row * M;

        int col = 0;
        for (; col <= K - 8; col += 8) {
            __m256 acc = _mm256_setzero_ps();
            for (int k = 0; k < M; ++k) {
                __m256 bVec = _mm256_loadu_ps(B + k * K + col);
                __m256 aVal = _mm256_set1_ps(aRow[k]);
                acc = _mm256_fmadd_ps(aVal, bVec, acc);
            }
            _mm256_storeu_ps(cRow + col, acc);
        }
        // Remainder columns scalar
        for (; col < K; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k) {
                sum += aRow[k] * B[k * K + col];
            }
            cRow[col] = sum;
        }
    }
#endif
}
