// q8_0_neon.cc — ARM-NEON Q8_0 GEMM kernel
// Phase 5: Port AVX2 Q8_0 kernel to ARM64 for Apple Silicon parity
// Target: ≥4× speedup on M1/M2/M3 vs scalar

#include <arm_neon.h>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" void matmul_kernel_neon(float* A, float* B, float* C, int N, int M, int K, bool accumulate = false);

// Q8_0 unpack: 64×64 int8 tile → FP32 with scale
extern "C" void ggml_q8_0_unpack_64x64_neon(const int8_t* q8, float* fp32, float scale) {
    constexpr int N = 64;
    constexpr int K = 64;
    const float32x4_t vscale = vdupq_n_f32(scale);
    
    for (int k = 0; k < K; ++k) {
        const int8_t* q8_row = q8 + k * N;
        float* fp32_row = fp32 + k * N;
        
        // Process 16 int8 values at a time
        for (int n = 0; n < N; n += 16) {
            // Load 16×int8
            int8x16_t q8_vec = vld1q_s8(q8_row + n);
            
            // Split into two 8×int8 vectors for widening
            int8x8_t lo = vget_low_s8(q8_vec);
            int8x8_t hi = vget_high_s8(q8_vec);
            
            // Widen int8 → int16
            int16x8_t lo_i16 = vmovl_s8(lo);
            int16x8_t hi_i16 = vmovl_s8(hi);
            
            // Widen int16 → int32 (4 vectors of 4×int32)
            int32x4_t v0 = vmovl_s16(vget_low_s16(lo_i16));
            int32x4_t v1 = vmovl_s16(vget_high_s16(lo_i16));
            int32x4_t v2 = vmovl_s16(vget_low_s16(hi_i16));
            int32x4_t v3 = vmovl_s16(vget_high_s16(hi_i16));
            
            // Convert int32 → float and scale
            float32x4_t f0 = vmulq_f32(vcvtq_f32_s32(v0), vscale);
            float32x4_t f1 = vmulq_f32(vcvtq_f32_s32(v1), vscale);
            float32x4_t f2 = vmulq_f32(vcvtq_f32_s32(v2), vscale);
            float32x4_t f3 = vmulq_f32(vcvtq_f32_s32(v3), vscale);
            
            // Store 16 floats
            vst1q_f32(fp32_row + n + 0, f0);
            vst1q_f32(fp32_row + n + 4, f1);
            vst1q_f32(fp32_row + n + 8, f2);
            vst1q_f32(fp32_row + n + 12, f3);
        }
    }
}

// Q8_0 GEMM: C = A × B, where B is Q8_0 quantized
extern "C" void ggml_gemm_q8_0_neon(int M, int N, int K,
                                    const float* A, const int8_t* Bq8, float scale, float* C) {
    constexpr int TM = 64;
    constexpr int TN = 64;
    constexpr int TK = 64;
    thread_local static float Btile[TM * TN];

    for (int i0 = 0; i0 < M; i0 += TM) {
        int Mb = (i0 + TM <= M) ? TM : (M - i0);
        for (int j0 = 0; j0 < N; j0 += TN) {
            int Nb = (j0 + TN <= N) ? TN : (N - j0);
            
            // Zero output tile
            for (int ii = 0; ii < Mb; ++ii) {
                std::memset(C + (i0 + ii) * N + j0, 0, sizeof(float) * Nb);
            }
            
            for (int k0 = 0; k0 < K; k0 += TK) {
                int Kb = (k0 + TK <= K) ? TK : (K - k0);
                
                // Extract Q8_0 panel and unpack to FP32
                alignas(16) int8_t q8_panel[TN * TK] = {0};
                for (int kk = 0; kk < Kb; ++kk) {
                    std::memcpy(&q8_panel[kk * TN], &Bq8[(k0 + kk) * N + j0], Nb);
                }
                
                ggml_q8_0_unpack_64x64_neon(q8_panel, Btile, scale);
                
                // Blocked GEMM with NEON FMA
                std::vector<float> Ablk(Mb * Kb);
                std::vector<float> Bblk(Kb * Nb);
                std::vector<float> Cblk(Mb * Nb);
                
                for (int ii = 0; ii < Mb; ++ii) {
                    std::memcpy(&Ablk[ii * Kb], A + (i0 + ii) * K + k0, sizeof(float) * Kb);
                }
                for (int kk = 0; kk < Kb; ++kk) {
                    std::memcpy(&Bblk[kk * Nb], &Btile[kk * TN], sizeof(float) * Nb);
                }
                
                matmul_kernel_neon(Ablk.data(), Bblk.data(), Cblk.data(), Mb, Kb, Nb, false);
                
                // Accumulate into C
                for (int ii = 0; ii < Mb; ++ii) {
                    float* Cd = C + (i0 + ii) * N + j0;
                    const float* Cs = &Cblk[ii * Nb];
                    for (int jj = 0; jj < Nb; ++jj) Cd[jj] += Cs[jj];
                }
            }
        }
    }
}

// Runtime dispatcher (Apple Silicon has NEON by default, but check for dot product extension)
static inline bool cpu_has_neon_rt() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return true;  // NEON is guaranteed on ARM64
#else
    return false;
#endif
}

static inline bool cpu_has_dotprod_rt() {
#if defined(__ARM_FEATURE_DOTPROD)
    return true;  // Dot product extension available
#else
    return false;
#endif
}

extern "C" void ggml_gemm_q8_0(int M, int N, int K,
                               const float* A, const int8_t* Bq8, float scale, float* C) {
    if (cpu_has_neon_rt()) {
        ggml_gemm_q8_0_neon(M, N, K, A, Bq8, scale, C);
        return;
    }
    
    // Scalar fallback
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
