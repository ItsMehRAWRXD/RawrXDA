#include "inference_kernels.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <omp.h>

extern "C" {
    void matmul_f16_avx512_masm(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K);
    void rmsnorm_avx512_masm(float* o, const float* x, const float* weight, int n, float eps);
}

void InferenceKernels::matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C,
                       int M, int N, int K) {
    matmul_f16_avx512_masm(A, B, C, M, N, K);
}

void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y,
                       int n, int m, int k) {
    int blocks_per_row = (k + 31) / 32;
    
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            float sum = 0.0f;
            
            for (int b = 0; b < blocks_per_row; b++) {
                const block_q4_0& block = w[j * blocks_per_row + b];
                float scale = block.d;
                
                for (int q = 0; q < 16; q++) {
                    uint8_t byte_val = block.qs[q];
                    
                    int8_t w_low = (byte_val & 0x0F) - 8;
                    int8_t w_high = ((byte_val >> 4) & 0x0F) - 8;
                    
                    int idx_low = b * 32 + q * 2;
                    int idx_high = idx_low + 1;
                    
                    if (idx_low < k) {
                        sum += x[i * k + idx_low] * w_low * scale;
                    }
                    if (idx_high < k) {
                        sum += x[i * k + idx_high] * w_high * scale;
                    }
                }
            }
            y[i * m + j] = sum;
        }
    }
}

void InferenceKernels::gelu_avx512(float* x, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        float x_val = x[i];
        float x_cu = x_val * x_val * x_val;
        float inner = 0.7978845608f * (x_val + 0.044715f * x_cu);
        float tanh_val = tanhf(inner);
        x[i] = x_val * 0.5f * (1.0f + tanh_val);
    }
}

void InferenceKernels::softmax_avx512(float* x, int n) {
    if (n <= 0) return;
    float max_val = x[0];
    for (int i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];
    
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    float inv_sum = 1.0f / (sum + 1e-9f);
    for (int i = 0; i < n; i++) {
        x[i] *= inv_sum;
    }
}

void InferenceKernels::rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps) {
    rmsnorm_avx512_masm(o, x, weight, n, eps);
}


void InferenceKernels::rope_avx512(float* q, float* k, int head_dim, int pos, float theta, float scale) {
     for (int i = 0; i < head_dim; i += 2) {
        float freq = 1.0f / powf(theta, (float)i / head_dim);
        float val = pos * freq;
        float cos_val = cosf(val);
        float sin_val = sinf(val);
        
        if (q) {
            float q0 = q[i];
            float q1 = q[i+1];
            q[i] = (q0 * cos_val - q1 * sin_val) * scale;
            q[i+1] = (q0 * sin_val + q1 * cos_val) * scale;
        }
        if (k) {
            float k0 = k[i];
            float k1 = k[i+1];
            k[i] = (k0 * cos_val - k1 * sin_val) * scale;
            k[i+1] = (k0 * sin_val + k1 * cos_val) * scale;
        }
    }
}

void InferenceKernels::matmul_f32(const float* A, const float* B, float* C, int M, int N, int K) {
    #pragma omp parallel for collapse(2)
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += A[m * K + k] * B[n * K + k]; // Careful: B might be transposed or not?
                // Standard convention: C(m,n) += A(m,k) * B(k,n)
                // BUT GGUF usually stores weights as transposed (output dim first).
                // If A is weights (M, K) and B is input (K, N), then A[m*K + k] is correct if row-major.
                // In my Eval code: A is weights (vocab, embed), B is input (embed, 1).
                // So M=vocab, K=embed, N=1.
                // B is simple vector x[k].
                // So A[m * K + k] * B[k*1 + n] (where n=0) => A[m*K+k] * B[k].
                // Code above assumes this structure: B[n*K+k] looks like B is (N, K).
                // If B is x, it is (K, 1). So B[0*K + k] => B[k].
                // Wait.
                // Standard MatMul (A: MxK, B: KxN) -> C: MxN
                // C[m, n] = Sum_k (A[m, k] * B[k, n])
                // My loop: A[m*K+k] which is row-major. B[n*K+k] imply B is (N, K)?
                // Ah, the matmul_f16 implementation used B[n*K+k]. This means B was expected to be transposed (N, K).
                // Or maybe the kernel assumes B is (N, K).
                // Let's check usage in f16.
                // If I use standard matmul, B should be accessed as B[k * N + n].
                // But for LLM inference (A*x), B is usually a vector (N=1). B[k*1 + 0] => B[k].
                // So let's stick to standard multiplication logic for safety.
            }
             // Let's fix the logic to be safe for standard layout
             // Actually, to match f16 implementation's signature:
             // It uses B[n*K+k]. This implies B is expected to be Transposed (N, K).
             // If N=1, B is (1, K).
             // But x is (K, 1).
             // If I pass x as B, and say it is (1, K), it works?
             // No, x is K elements from 0 to K-1.
             // If N=1, B layout for (1, K) is K consecutive floats. Same as (K, 1).
             // So B[0*K + k] ==> B[k]. This works.
             // BUT if N > 1, this kernel expects B to be (N, K) (transposed).
             // This is common in optimizations.
             // I'll stick to the pattern used in f16_avx512 for consistency.
             // M=vocab, N=1, K=embed.
        }
    }
}
