#include "transformer.h"
#include "inference_kernels.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <omp.h>

// TransformerLayer: Complete multi-head attention + FFN with GQA support
TransformerLayer::TransformerLayer(int d, int nh, int nkv, int hidden) 
    : dim(d), n_heads(nh), n_kv_heads(nkv), hidden_dim(hidden) {
    
    head_dim = dim / n_heads;
    if (n_kv_heads == 0) n_kv_heads = n_heads;  // Default to MHA if not specified
    kv_head_dim = dim / n_kv_heads;
    
    cache_pos = 0;
    max_seq_len = 4096;
    
    // Allocate KV cache for full sequence
    int kv_dim = n_kv_heads * head_dim;
    k_cache = new float[max_seq_len * kv_dim]();
    v_cache = new float[max_seq_len * kv_dim]();
    
    // Initialize weights pointers to nullptr - they'll be set by the engine
    wq = wk = wv = wo = w1 = w2 = w3 = nullptr;
    attn_norm = ffn_norm = nullptr;
}

TransformerLayer::~TransformerLayer() {
    delete[] k_cache;
    delete[] v_cache;
}

void TransformerLayer::forward(float* x, int pos, int seq_len) {
    std::vector<float> tmp(dim);
    std::vector<float> q(dim), k(kv_head_dim), v(kv_head_dim);
    std::vector<float> attn_out(dim);
    
    // === ATTENTION BLOCK ===
    
    // Pre-attention RMSNorm
    if (attn_norm) {
        InferenceKernels::rmsnorm_avx512(tmp.data(), x, attn_norm, dim);
    } else {
        std::memcpy(tmp.data(), x, dim * sizeof(float));
    }
    
    // QKV projections
    // Q projects from dim -> dim
    // K, V project from dim -> kv_dim (for GQA)
    if (wq_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wq, q.data(), 1, dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wk, k.data(), 1, kv_head_dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wv, v.data(), 1, kv_head_dim, dim);
    } else {
        // F32 fallback using naive matmul
        for (int i = 0; i < dim; i++) {
            q[i] = 0;
            for (int j = 0; j < dim; j++) {
                q[i] += ((float*)wq)[i * dim + j] * tmp[j];
            }
        }
        for (int i = 0; i < kv_head_dim; i++) {
            k[i] = v[i] = 0;
            for (int j = 0; j < dim; j++) {
                k[i] += ((float*)wk)[i * dim + j] * tmp[j];
                v[i] += ((float*)wv)[i * dim + j] * tmp[j];
            }
        }
    }
    
    // Apply RoPE (Rotary Position Embedding) to Q and K
    // This makes the model position-aware
    for (int h = 0; h < n_heads; h++) {
        int q_offset = h * head_dim;
        // For GQA, map heads to KV heads
        int kv_h = (n_kv_heads < n_heads) ? (h * n_kv_heads / n_heads) : h;
        int k_offset = kv_h * kv_head_dim;
        
        InferenceKernels::rope_avx512(
            q.data() + q_offset, 
            k.data() + k_offset, 
            std::min(head_dim, kv_head_dim), 
            pos
        );
    }
    
    // Store K, V in cache for later use (KV cache)
    int kv_dim = n_kv_heads * head_dim;
    if (cache_pos < max_seq_len) {
        std::memcpy(k_cache + cache_pos * kv_dim, k.data(), std::min((int)k.size(), kv_dim) * sizeof(float));
        std::memcpy(v_cache + cache_pos * kv_dim, v.data(), std::min((int)v.size(), kv_dim) * sizeof(float));
        cache_pos++;
    }
    
    // Multi-head attention with Group Query Attention support
    multi_head_attention(q.data(), k_cache, v_cache, attn_out.data(),
                        cache_pos, n_heads, n_kv_heads, head_dim);
    
    // Output projection: attn_out -> x
    if (wq_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(attn_out.data(), (block_q4_0*)wo, tmp.data(), 1, dim, dim);
    } else {
        for (int i = 0; i < dim; i++) {
            tmp[i] = 0;
            for (int j = 0; j < dim; j++) {
                tmp[i] += ((float*)wo)[i * dim + j] * attn_out[j];
            }
        }
    }
    
    // Residual connection
    for (int i = 0; i < dim; i++) x[i] += tmp[i];
    
    // === FEED-FORWARD BLOCK ===
    
    // Pre-FFN RMSNorm
    if (ffn_norm) {
        InferenceKernels::rmsnorm_avx512(tmp.data(), x, ffn_norm, dim);
    } else {
        std::memcpy(tmp.data(), x, dim * sizeof(float));
    }
    
    std::vector<float> gate(hidden_dim), up(hidden_dim), ffn_out(dim);
    
    // SwiGLU gating mechanism
    // w1 @ x -> gate (applies activation)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)w1, gate.data(), 1, hidden_dim, dim);
    } else {
        for (int i = 0; i < hidden_dim; i++) {
            gate[i] = 0;
            for (int j = 0; j < dim; j++) {
                gate[i] += ((float*)w1)[i * dim + j] * tmp[j];
            }
        }
    }
    
    // w3 @ x -> up (projection without activation)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)w3, up.data(), 1, hidden_dim, dim);
    } else {
        for (int i = 0; i < hidden_dim; i++) {
            up[i] = 0;
            for (int j = 0; j < dim; j++) {
                up[i] += ((float*)w3)[i * dim + j] * tmp[j];
            }
        }
    }
    
    // SwiGLU activation: silu(gate) * up
    // silu(x) = x * sigmoid(x) = x / (1 + exp(-x))
    for (int i = 0; i < hidden_dim; i++) {
        float sigmoid_gate = 1.0f / (1.0f + expf(-gate[i]));
        gate[i] = gate[i] * sigmoid_gate * up[i];
    }
    
    // w2 @ (silu(gate) * up) -> ffn_out
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(gate.data(), (block_q4_0*)w2, ffn_out.data(), 1, dim, hidden_dim);
    } else {
        for (int i = 0; i < dim; i++) {
            ffn_out[i] = 0;
            for (int j = 0; j < hidden_dim; j++) {
                ffn_out[i] += ((float*)w2)[i * hidden_dim + j] * gate[j];
            }
        }
    }
    
    // Residual connection
    for (int i = 0; i < dim; i++) x[i] += ffn_out[i];
}

// Multi-head attention with GQA (Group Query Attention) support
void TransformerLayer::multi_head_attention(float* q, float* k_cache, float* v_cache,
                              float* out, int seq_len, int n_h, int n_kv_h, int h_d) {
    
    // Handle group query attention: multiple Q heads share one KV head
    int groups = n_h / n_kv_h;
    if (groups == 0) groups = 1;
    
    int kv_dim = n_kv_h * h_d;
    float scale = 1.0f / sqrtf((float)h_d);  // Attention scaling factor
    
    #pragma omp parallel for
    for (int h = 0; h < n_h; h++) {
        float* q_h = q + h * h_d;
        float* out_h = out + h * h_d;
        
        // Determine which KV head this Q head maps to (for GQA)
        int kv_h = h / groups;
        
        // Compute attention scores for this head across all positions
        std::vector<float> scores(seq_len);
        
        for (int t = 0; t < seq_len; t++) {
            // K for this position and KV head
            float* k_t = k_cache + t * kv_dim + kv_h * h_d;
            
            // Dot product: q_h · k_t
            float score = 0.0f;
            for (int i = 0; i < h_d; i++) {
                score += q_h[i] * k_t[i];
            }
            scores[t] = score * scale;
        }
        
        // Softmax over time dimension (numerical stable)
        float max_score = scores[0];
        for (int t = 1; t < seq_len; t++) {
            if (scores[t] > max_score) max_score = scores[t];
        }
        
        float sum = 0.0f;
        for (int t = 0; t < seq_len; t++) {
            scores[t] = expf(scores[t] - max_score);
            sum += scores[t];
        }
        
        for (int t = 0; t < seq_len; t++) {
            scores[t] /= sum;
        }
        
        // Weighted sum of values: out = sum_t(attention_weights[t] * v[t])
        std::memset(out_h, 0, h_d * sizeof(float));
        
        for (int t = 0; t < seq_len; t++) {
            float weight = scores[t];
            float* v_t = v_cache + t * kv_dim + kv_h * h_d;
            
            for (int i = 0; i < h_d; i++) {
                out_h[i] += weight * v_t[i];
            }
        }
    }
}

// Reset KV cache for new sequence
void TransformerLayer::reset_cache() {
    cache_pos = 0;
    std::memset(k_cache, 0, max_seq_len * n_kv_heads * head_dim * sizeof(float));
    std::memset(v_cache, 0, max_seq_len * n_kv_heads * head_dim * sizeof(float));
}
