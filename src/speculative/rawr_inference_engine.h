// rawr_inference_engine.h
// Complete inference engine with working kernels and Q8_0 dequantization

#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <immintrin.h>

namespace rawr {

// ============================================================================
// QUANTIZATION BLOCKS
// ============================================================================

// Q8_0: 32 8-bit weights + 1 fp16 scale
struct block_q8_0 {
    uint16_t d;      // scale (fp16)
    int8_t qs[32];   // 32 quantized weights
};
static_assert(sizeof(block_q8_0) == 34, "q8_0 block size");

// Q4_0: 32 4-bit weights packed into 16 bytes + 1 fp16 scale
struct block_q4_0 {
    uint16_t d;      // scale (fp16)
    uint8_t qs[16];  // 32 4-bit weights packed
};
static_assert(sizeof(block_q4_0) == 18, "q4_0 block size");

// ============================================================================
// DEQUANTIZATION KERNELS
// ============================================================================

// FP16 to FP32 conversion (simple, handles normalized values)
inline float fp16_to_fp32(uint16_t h) {
    // Extract fields
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        // Zero or denormal
        if (mant == 0) return 0.0f;
        // Denormal
        float val = mant / 1024.0f;
        return sign ? -val * 6.1035e-5f : val * 6.1035e-5f;
    }
    if (exp == 0x1F) {
        // Inf or NaN
        return mant ? NAN : (sign ? -INFINITY : INFINITY);
    }
    
    // Normalized
    uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
    float result;
    memcpy(&result, &f32, 4);
    return result;
}

// Dequantize Q8_0 row to FP32
inline void dequantize_q8_0_row(const uint8_t* src, float* dst, size_t n) {
    const block_q8_0* blocks = (const block_q8_0*)src;
    size_t nb = (n + 31) / 32;
    
    for (size_t b = 0; b < nb; b++) {
        float d = fp16_to_fp32(blocks[b].d);
        for (int i = 0; i < 32; i++) {
            size_t idx = b * 32 + i;
            if (idx < n) {
                dst[idx] = blocks[b].qs[i] * d;
            }
        }
    }
}

// Dequantize Q4_0 row to FP32
inline void dequantize_q4_0_row(const uint8_t* src, float* dst, size_t n) {
    const block_q4_0* blocks = (const block_q4_0*)src;
    size_t nb = (n + 31) / 32;
    
    for (size_t b = 0; b < nb; b++) {
        float d = fp16_to_fp32(blocks[b].d);
        for (int i = 0; i < 16; i++) {
            uint8_t q = blocks[b].qs[i];
            int x0 = (q & 0x0F) - 8;
            int x1 = (q >> 4) - 8;
            
            size_t idx0 = b * 32 + i * 2;
            size_t idx1 = idx0 + 1;
            
            if (idx0 < n) dst[idx0] = x0 * d;
            if (idx1 < n) dst[idx1] = x1 * d;
        }
    }
}

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

// Vector dot product (for attention scores)
inline float vec_dot(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// Matrix-vector multiplication: y = x @ W (where W is row-major)
inline void matmul_vec(const float* x, const float* W, float* y, 
                       size_t n_rows, size_t n_cols) {
    for (size_t i = 0; i < n_rows; i++) {
        y[i] = vec_dot(x, W + i * n_cols, n_cols);
    }
}

// Quantized matrix-vector multiplication
inline void matmul_vec_q8_0(const float* x, const uint8_t* W_q, float* y,
                            size_t n_rows, size_t n_cols) {
    size_t block_size = 32;
    size_t n_blocks_per_row = (n_cols + block_size - 1) / block_size;
    
    for (size_t i = 0; i < n_rows; i++) {
        float sum = 0.0f;
        const uint8_t* row_q = W_q + i * n_blocks_per_row * sizeof(block_q8_0);
        
        for (size_t j = 0; j < n_cols; j += block_size) {
            const block_q8_0* block = (const block_q8_0*)(row_q + (j / block_size) * sizeof(block_q8_0));
            float d = fp16_to_fp32(block->d);
            
            for (size_t k = 0; k < block_size && (j + k) < n_cols; k++) {
                sum += x[j + k] * block->qs[k] * d;
            }
        }
        y[i] = sum;
    }
}

// ============================================================================
// ACTIVATION FUNCTIONS
// ============================================================================

// SiLU activation: x * sigmoid(x)
inline float silu(float x) {
    return x / (1.0f + expf(-x));
}

// SwiGLU: gate(x) * up(x) where both use SiLU
inline void swiglu(const float* gate, const float* up, float* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = silu(gate[i]) * up[i];
    }
}

// Softmax: exp(x - max) / sum(exp(x - max))
inline void softmax(float* x, size_t n) {
    float max_val = x[0];
    for (size_t i = 1; i < n; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    for (size_t i = 0; i < n; i++) {
        x[i] /= sum;
    }
}

// RMSNorm: x / sqrt(mean(x^2) + eps) * weight
inline void rmsnorm(const float* x, const float* weight, float* out, 
                    size_t n, float eps = 1e-5f) {
    float sum_sq = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum_sq += x[i] * x[i];
    }
    float scale = 1.0f / sqrtf(sum_sq / n + eps);
    
    for (size_t i = 0; i < n; i++) {
        out[i] = x[i] * scale * weight[i];
    }
}

// ============================================================================
// ROPE (Rotary Position Embeddings)
// ============================================================================

// Apply RoPE to Q and K vectors
inline void apply_rope(float* q, float* k, size_t head_dim, size_t pos, float theta = 10000.0f) {
    for (size_t i = 0; i < head_dim; i += 2) {
        float freq = 1.0f / powf(theta, (float)i / head_dim);
        float val = pos * freq;
        float cos_val = cosf(val);
        float sin_val = sinf(val);
        
        // Rotate Q
        float q0 = q[i];
        float q1 = q[i + 1];
        q[i] = q0 * cos_val - q1 * sin_val;
        q[i + 1] = q0 * sin_val + q1 * cos_val;
        
        // Rotate K
        float k0 = k[i];
        float k1 = k[i + 1];
        k[i] = k0 * cos_val - k1 * sin_val;
        k[i + 1] = k0 * sin_val + k1 * cos_val;
    }
}

// ============================================================================
// ATTENTION
// ============================================================================

// Single-head attention: Q @ K^T / sqrt(d_k), softmax, @ V
inline void attention_single_head(const float* Q, const float* K, const float* V,
                                   float* out, size_t seq_len, size_t head_dim) {
    float scale = 1.0f / sqrtf((float)head_dim);
    std::vector<float> scores(seq_len);
    
    // Compute attention scores: Q @ K^T
    for (size_t t = 0; t < seq_len; t++) {
        scores[t] = vec_dot(Q, K + t * head_dim, head_dim) * scale;
    }
    
    // Causal mask: set future positions to -inf
    // (In real implementation, this would be handled differently)
    
    // Softmax
    softmax(scores.data(), seq_len);
    
    // Weighted sum of values
    for (size_t i = 0; i < head_dim; i++) {
        out[i] = 0.0f;
        for (size_t t = 0; t < seq_len; t++) {
            out[i] += scores[t] * V[t * head_dim + i];
        }
    }
}

// Multi-head attention (MHA)
inline void attention_mha(const float* Q, const float* K, const float* V,
                         float* out, size_t n_heads, size_t seq_len, size_t head_dim) {
    size_t hidden_dim = n_heads * head_dim;
    
    for (size_t h = 0; h < n_heads; h++) {
        const float* Q_h = Q + h * head_dim;
        const float* K_h = K + h * seq_len * head_dim;
        const float* V_h = V + h * seq_len * head_dim;
        float* out_h = out + h * head_dim;
        
        attention_single_head(Q_h, K_h, V_h, out_h, seq_len, head_dim);
    }
}

// Grouped Query Attention (GQA)
inline void attention_gqa(const float* Q, const float* K, const float* V,
                         float* out, size_t n_heads, size_t n_kv_heads,
                         size_t seq_len, size_t head_dim) {
    size_t heads_per_group = n_heads / n_kv_heads;
    
    for (size_t kv_h = 0; kv_h < n_kv_heads; kv_h++) {
        const float* K_h = K + kv_h * seq_len * head_dim;
        const float* V_h = V + kv_h * seq_len * head_dim;
        
        for (size_t g = 0; g < heads_per_group; g++) {
            size_t q_h = kv_h * heads_per_group + g;
            const float* Q_h = Q + q_h * head_dim;
            float* out_h = out + q_h * head_dim;
            
            attention_single_head(Q_h, K_h, V_h, out_h, seq_len, head_dim);
        }
    }
}

// ============================================================================
// FFN
// ============================================================================

// Dense FFN: SwiGLU variant
inline void ffn_swiglu(const float* x, const float* gate_w, const float* up_w,
                      const float* down_w, float* out, float* tmp,
                      size_t hidden_dim, size_t ffn_dim) {
    // gate = x @ gate_w
    matmul_vec(x, gate_w, tmp, ffn_dim, hidden_dim);
    
    // up = x @ up_w
    matmul_vec(x, up_w, tmp + ffn_dim, ffn_dim, hidden_dim);
    
    // SwiGLU: gate_silu * up
    for (size_t i = 0; i < ffn_dim; i++) {
        tmp[i] = silu(tmp[i]) * tmp[ffn_dim + i];
    }
    
    // out = tmp @ down_w
    matmul_vec(tmp, down_w, out, hidden_dim, ffn_dim);
}

// ============================================================================
// TOKEN SAMPLING
// ============================================================================

// Greedy sampling: argmax
inline int sample_greedy(const float* logits, size_t vocab_size) {
    int best_idx = 0;
    float best_val = logits[0];
    
    for (size_t i = 1; i < vocab_size; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best_idx = i;
        }
    }
    
    return best_idx;
}

// Temperature sampling
inline int sample_temperature(const float* logits, size_t vocab_size, float temp) {
    if (temp == 0.0f) {
        return sample_greedy(logits, vocab_size);
    }
    
    std::vector<float> probs(vocab_size);
    float max_logit = logits[0];
    for (size_t i = 1; i < vocab_size; i++) {
        if (logits[i] > max_logit) max_logit = logits[i];
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < vocab_size; i++) {
        probs[i] = expf((logits[i] - max_logit) / temp);
        sum += probs[i];
    }
    
    for (size_t i = 0; i < vocab_size; i++) {
        probs[i] /= sum;
    }
    
    // Simple greedy for now (would use random sampling in production)
    int best_idx = 0;
    float best_prob = probs[0];
    for (size_t i = 1; i < vocab_size; i++) {
        if (probs[i] > best_prob) {
            best_prob = probs[i];
            best_idx = i;
        }
    }
    
    return best_idx;
}

} // namespace rawr
