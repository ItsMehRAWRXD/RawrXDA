// win32ide_symbol_impls_H.cpp -- RawrXD IDE debug agentic symbol implementations

#include <cstdint>
#include <cstring>
#include <immintrin.h>

extern "C" void ggml_gemm_q4_0(int M, int N, int K,
                                const float* A,
                                const uint8_t* Bq4,
                                float scale,
                                float* C) {
    if (!A || !Bq4 || !C || M <= 0 || N <= 0 || K <= 0) {
        return;
    }

    std::memset(C, 0, static_cast<size_t>(M) * static_cast<size_t>(N) * sizeof(float));

    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k) {
                const int linear = n * K + k;
                const uint8_t packed = Bq4[linear >> 1];
                const int nibble = (linear & 1) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
                const float b = static_cast<float>(nibble - 8) * scale;
                acc += A[m * K + k] * b;
            }
            C[m * N + n] = acc;
        }
    }
}

extern "C" void matmul_kernel_avx2(float* A,
                                     float* B,
                                     float* C,
                                     int N,
                                     int M,
                                     int K,
                                     bool accumulate) {
    if (!A || !B || !C || N <= 0 || M <= 0 || K <= 0) {
        return;
    }

    if (!accumulate) {
        std::memset(C, 0, static_cast<size_t>(M) * static_cast<size_t>(N) * sizeof(float));
    }

    for (int i = 0; i < M; ++i) {
        float* c_row = C + static_cast<size_t>(i) * N;
        const float* a_row = A + static_cast<size_t>(i) * K;

        for (int k = 0; k < K; ++k) {
            const __m256 va = _mm256_set1_ps(a_row[k]);
            const float* b_row = B + static_cast<size_t>(k) * N;
            int j = 0;

            for (; j <= N - 8; j += 8) {
                __m256 vc = _mm256_loadu_ps(c_row + j);
                __m256 vb = _mm256_loadu_ps(b_row + j);
                vc = _mm256_fmadd_ps(va, vb, vc);
                _mm256_storeu_ps(c_row + j, vc);
            }
            for (; j < N; ++j) {
                c_row[j] += a_row[k] * b_row[j];
            }
        }
    }
}
