#include "inference_kernels.h"
#include <immintrin.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

// Q4_0 block size = 32 (16 weights in 8 bytes + 2 bytes for delta)
#define QK4_0 32

// Q4_0 dequantization with AVX2
static inline float fp32_from_bits(uint32_t w) {
    union { uint32_t i; float f; } u;
    u.i = w;
    return u.f;
}

// Implementation of InferenceKernels::matmul_q4_0_fused
void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y,
                                        int n, int m, int k) {
    // y[n x m] = x[n x k] @ w[k x m]
    // For each output element
    for (int i = 0; i < n; i++) {
        const float* row_x = x + i * k;
        
        for (int j = 0; j < m; j++) {
            float sum = 0.0f;
            
            // Dequantize and multiply one row
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

void InferenceKernels::softmax(float* x, int n) {
    // Find max for numerical stability
    float max_val = -1e9f;
    for (int i = 0; i < n; i++) {
        max_val = std::max(max_val, x[i]);
    }
    
    // Compute exp
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    
    // Normalize
    float inv_sum = 1.0f / sum;
    for (int i = 0; i < n; i++) {
        x[i] *= inv_sum;
    }
}

void InferenceKernels::rope_forward(float* x, int n, int d, int pos) {
    for (int i = 0; i < d; i += 2) {
        float theta = std::pow(10000.0f, -float(i) / d);
        float m_theta = pos * theta;
        float cos_theta = std::cos(m_theta);
        float sin_theta = std::sin(m_theta);
        
        float x0 = x[i];
        float x1 = x[i + 1];
        
        x[i] = x0 * cos_theta - x1 * sin_theta;
        x[i + 1] = x0 * sin_theta + x1 * cos_theta;
    }
}

void InferenceKernels::attention_forward(float* q, float* k, float* v, float* out,
                       int seq_len, int head_dim, int num_heads) {
    // Simplified attention: for each head
    float* head_q = q;
    float* head_k = k;
    float* head_v = v;
    float* head_out = out;
    
    for (int h = 0; h < num_heads; h++) {
        // Compute Q @ K^T / sqrt(d)
        float scale = 1.0f / std::sqrt(float(head_dim));
        
        std::vector<float> scores(seq_len * seq_len);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                float dot = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    dot += head_q[i * head_dim + d] * head_k[j * head_dim + d];
                }
                scores[i * seq_len + j] = dot * scale;
            }
        }
        
        // Apply softmax per row
        for (int i = 0; i < seq_len; i++) {
            softmax(&scores[i * seq_len], seq_len);
        }
        
        // Compute attention output
        for (int i = 0; i < seq_len; i++) {
            for (int d = 0; d < head_dim; d++) {
                float val = 0.0f;
                for (int j = 0; j < seq_len; j++) {
                    val += scores[i * seq_len + j] * head_v[j * head_dim + d];
                }
                head_out[i * head_dim + d] = val;
            }
        }
        
        head_q += seq_len * head_dim;
        head_k += seq_len * head_dim;
        head_v += seq_len * head_dim;
        head_out += seq_len * head_dim;
    }
}

void InferenceKernels::ffn_forward(float* x, const float* w1, const float* w2,
                 float* tmp, int n, int d, int hidden_d) {
    // x @ W1 -> tmp[hidden_d]
    for (int i = 0; i < hidden_d; i++) {
        tmp[i] = 0.0f;
        for (int j = 0; j < d; j++) {
            tmp[i] += x[j] * w1[j * hidden_d + i];
        }
    }
    
    // GELU activation
    for (int i = 0; i < hidden_d; i++) {
        float t = tmp[i];
        tmp[i] = t * 0.5f * (1.0f + std::tanh(0.7978845608f * (t + 0.044715f * t * t * t)));
    }
    
    // tmp @ W2 -> x[d]
    for (int i = 0; i < d; i++) {
        x[i] = 0.0f;
        for (int j = 0; j < hidden_d; j++) {
            x[i] += tmp[j] * w2[j * d + i];
        }
    }
}

void InferenceKernels::rms_norm(float* x, const float* weight, float eps, int n) {
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        sum_sq += x[i] * x[i];
    }
    
    float scale = 1.0f / std::sqrt(sum_sq / n + eps);
    for (int i = 0; i < n; i++) {
        x[i] = (x[i] * scale) * weight[i];
    }
}

void print_tensor_stats(const float* x, const char* name, int n) {
    float min_val = x[0], max_val = x[0], mean = 0.0f;
    for (int i = 0; i < n; i++) {
        min_val = std::min(min_val, x[i]);
        max_val = std::max(max_val, x[i]);
        mean += x[i];
    }
    mean /= n;
    
    printf("[%s] min=%.4f max=%.4f mean=%.4f size=%d\n", name, min_val, max_val, mean, n);
}

} // namespace InferenceKernels_Impl

// Wrapper functions that delegate to the implementation namespace
namespace InferenceKernels {

void matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y,
                       int n, int m, int k) {
    return InferenceKernels_Impl::matmul_q4_0_fused(x, w, y, n, m, k);
}
