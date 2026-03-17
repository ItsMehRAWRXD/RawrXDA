#include "inference_kernels.h"
#include <immintrin.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cstdio>

#define QK4_0 32

static inline float fp32_from_bits(uint32_t w) {
    union { uint32_t i; float f; } u;
    u.i = w;
    return u.f;
}

void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y,
                                        int n, int m, int k) {
    for (int i = 0; i < n; i++) {
        const float* row_x = x + i * k;
        for (int j = 0; j < m; j++) {
            float sum = 0.0f;
            const int blocks = (k + QK4_0 - 1) / QK4_0;
            const block_q4_0* wj = w + j * blocks;
            
            int idx = 0;
            for (int b = 0; b < blocks && idx < k; b++) {
                const float d = fp32_from_bits(wj[b].d);
                const uint8_t* qs = wj[b].qs;
                for (int l = 0; l < 16 && idx < k; l++) {
                    const uint8_t q = qs[l];
                    float wl0 = ((q & 0x0F) - 8.0f) * d;
                    float wl1 = ((q >> 4) - 8.0f) * d;
                    sum += row_x[idx] * wl0;
                    idx++;
                    if (idx < k) {
                        sum += row_x[idx] * wl1;
                        idx++;
                    }
                }
            }
            y[i * m + j] = sum;
        }
    }
}

void InferenceKernels::gelu_avx512(float* x, int n) {
    // GELU: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    for (int i = 0; i < n; i++) {
        float t = x[i];
        x[i] = t * 0.5f * (1.0f + std::tanh(0.7978845608f * (t + 0.044715f * t * t * t)));
    }
}

void InferenceKernels::softmax_avx512(float* x, int n) {
    float max_val = -1e9f;
    for (int i = 0; i < n; i++) {
        max_val = std::max(max_val, x[i]);
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    float inv_sum = 1.0f / sum;
    for (int i = 0; i < n; i++) {
        x[i] *= inv_sum;
    }
}

void InferenceKernels::rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps) {
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        sum_sq += x[i] * x[i];
    }
    float scale = 1.0f / std::sqrt(sum_sq / n + eps);
    for (int i = 0; i < n; i++) {
        o[i] = (x[i] * scale) * weight[i];
    }
}

void InferenceKernels::rope_avx512(float* q, float* k, int head_dim, int pos, float theta, float scale) {
    for (int i = 0; i < head_dim; i += 2) {
        float freq = std::pow(theta, -float(i) / head_dim);
        float m_theta = pos * freq;
        float cos_theta = std::cos(m_theta);
        float sin_theta = std::sin(m_theta);
        
        if (q) {
            float q0 = q[i] * scale;
            float q1 = q[i + 1] * scale;
            q[i] = q0 * cos_theta - q1 * sin_theta;
            q[i + 1] = q0 * sin_theta + q1 * cos_theta;
        }
        
        if (k) {
            float k0 = k[i] * scale;
            float k1 = k[i + 1] * scale;
            k[i] = k0 * cos_theta - k1 * sin_theta;
            k[i + 1] = k0 * sin_theta + k1 * cos_theta;
        }
    }
}

void InferenceKernels::matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K) {
    // Simplified F16 matmul - in production would use vectorized instructions
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                // F16->F32 conversion (simplified)
                union { uint16_t u; float f; } a, b;
                a.u = A[i * K + k];
                b.u = B[k * N + j];
                sum += a.f * b.f;
            }
            C[i * N + j] = sum;
        }
    }
}
