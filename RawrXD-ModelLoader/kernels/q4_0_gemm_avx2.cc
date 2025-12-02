#include <immintrin.h>
#include <cstdint>
#include <vector>
#include <cstring>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale);
extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K, bool accumulate = false);

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

static inline bool cpu_has_avx2_rt() {
#if defined(_MSC_VER)
    int cpuInfo[4] = {0};
    __cpuidex(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
  #if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
  #else
    return false;
  #endif
#else
    return false;
#endif
}

extern "C" void ggml_gemm_q4_0_avx2(int M, int N, int K, const float* A, const uint8_t* Bq4, float scale, float* C) {
#if !defined(__AVX2__)
    gemm_q4_0_scalar(M, N, K, A, Bq4, scale, C);
#else
    constexpr int TM = 64;
    constexpr int TN = 64;
    constexpr int TK = 64;
    thread_local static float Btile[TM * TN];

    for (int i0 = 0; i0 < M; i0 += TM) {
        int Mb = (i0 + TM <= M) ? TM : (M - i0);
        for (int j0 = 0; j0 < N; j0 += TN) {
            int Nb = (j0 + TN <= N) ? TN : (N - j0);
            for (int ii = 0; ii < Mb; ++ii) {
                std::memset(C + (i0 + ii) * N + j0, 0, sizeof(float) * Nb);
            }
            for (int k0 = 0; k0 < K; k0 += TK) {
                int Kb = (k0 + TK <= K) ? TK : (K - k0);
                alignas(16) uint8_t q4_panel[(TN * TK) / 2] = {0};
                for (int kk = 0; kk < Kb; ++kk) {
                    for (int jj = 0; jj < Nb; ++jj) {
                        int src_idx = (k0 + kk) * N + (j0 + jj);
                        int src_byte = src_idx >> 1;
                        bool src_hi = (src_idx & 1) != 0;
                        uint8_t byte = Bq4[src_byte];
                        uint8_t nib = src_hi ? (byte >> 4) & 0xF : (byte & 0xF);
                        int dst_idx = kk * TN + jj;
                        int dst_byte = dst_idx >> 1;
                        bool dst_hi = (dst_idx & 1) != 0;
                        if (dst_hi) q4_panel[dst_byte] = (q4_panel[dst_byte] & 0x0F) | (nib << 4);
                        else        q4_panel[dst_byte] = (q4_panel[dst_byte] & 0xF0) | (nib);
                    }
                }
                q4_0_unpack_64x64(q4_panel, Btile, scale);
                std::vector<float> Ablk(Mb * Kb);
                std::vector<float> Bblk(Kb * Nb);
                std::vector<float> Cblk(Mb * Nb);
                for (int ii = 0; ii < Mb; ++ii) {
                    std::memcpy(&Ablk[ii * Kb], A + (i0 + ii) * K + k0, sizeof(float) * Kb);
                }
                for (int kk2 = 0; kk2 < Kb; ++kk2) {
                    std::memcpy(&Bblk[kk2 * Nb], &Btile[kk2 * TN], sizeof(float) * Nb);
                }
                matmul_kernel_avx2(Ablk.data(), Bblk.data(), Cblk.data(), Mb, Kb, Nb, false);
                for (int ii = 0; ii < Mb; ++ii) {
                    float* Cd = C + (i0 + ii) * N + j0;
                    const float* Cs = &Cblk[ii * Nb];
                    for (int jj = 0; jj < Nb; ++jj) Cd[jj] += Cs[jj];
                }
            }
        }
    }
#endif
}

extern "C" void ggml_gemm_q4_0(int M, int N, int K,
                                const float* A, const uint8_t* Bq4, float scale, float* C) {
#if defined(__AVX2__)
    if (cpu_has_avx2_rt()) {
        ggml_gemm_q4_0_avx2(M, N, K, A, Bq4, scale, C);
        return;
    }
#endif
    gemm_q4_0_scalar(M, N, K, A, Bq4, scale, C);
}
