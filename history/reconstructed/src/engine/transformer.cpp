#include "transformer.h"
#include "inference_kernels.h"
#include <cstring>
#include <cmath>
#include <vector>

TransformerLayer::TransformerLayer(int d, int nh, int nkv, int hidden) 
    : dim(d), n_heads(nh), n_kv_heads(nkv), hidden_dim(hidden) {
    head_dim = dim / n_heads;
    cache_pos = 0;
    
    // Allocate KV cache
    // In a real scenario, this memory would be managed by the engine context
    // For this snippet, we allocate here.
    max_seq_len = 4096;
    int kv_dim = n_kv_heads * head_dim;
    k_cache = new float[max_seq_len * kv_dim]();
    v_cache = new float[max_seq_len * kv_dim]();
}

TransformerLayer::~TransformerLayer() {
    delete[] k_cache;
    delete[] v_cache;
}

void TransformerLayer::forward(float* x, int pos, int seq_len) {
    std::vector<float> tmp(dim);
    std::vector<float> q(dim), k(dim), v(dim);
    std::vector<float> attn_out(dim);
    
    // --- Self Attention ---
    InferenceKernels::rmsnorm_avx512(tmp.data(), x, attn_norm, dim);
    
    // QKV projections
    if (wq_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wq, q.data(), 1, dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wk, k.data(), 1, dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)wv, v.data(), 1, dim, dim);
    } else {
        // F32 fallback
        for (int i = 0; i < dim; i++) {
            q[i] = k[i] = v[i] = 0;
            for (int j = 0; j < dim; j++) {
                q[i] += ((float*)wq)[i * dim + j] * tmp[j];
                k[i] += ((float*)wk)[i * dim + j] * tmp[j];
                v[i] += ((float*)wv)[i * dim + j] * tmp[j];
            }
        }
    }
    
    // Apply RoPE to Q and K
    for (int h = 0; h < n_heads; h++) {
        InferenceKernels::rope_avx512(q.data() + h * head_dim, k.data() + h * head_dim, 
                   head_dim, pos);
    }
    
    // Update KV cache
    int kv_dim = n_kv_heads * head_dim;
    memcpy(k_cache + cache_pos * kv_dim, k.data(), kv_dim * sizeof(float));
    memcpy(v_cache + cache_pos * kv_dim, v.data(), kv_dim * sizeof(float));
    cache_pos++;
    
    // Multi-head attention
    multi_head_attention(q.data(), k_cache, v_cache, attn_out.data(),
                        pos + 1, n_heads, n_kv_heads, head_dim);
    
    // Output projection
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
    
    // Residual
    for (int i = 0; i < dim; i++) x[i] += tmp[i];
    
    // --- FFN (SwiGLU) ---
    InferenceKernels::rmsnorm_avx512(tmp.data(), x, ffn_norm, dim);
    
    std::vector<float> gate(hidden_dim), up(hidden_dim), ffn_out(dim);
    
    // w1 @ x (gate)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)w1, gate.data(), 1, hidden_dim, dim);
    }
    
    // w3 @ x (up)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp.data(), (block_q4_0*)w3, up.data(), 1, hidden_dim, dim);
    }
    
    // SwiGLU: silu(gate) * up
    for (int i = 0; i < hidden_dim; i++) {
        float silu = gate[i] * (1.0f / (1.0f + expf(-gate[i])));
        gate[i] = silu * up[i];
    }
    
    // w2 @ result
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(gate.data(), (block_q4_0*)w2, ffn_out.data(), 1, dim, hidden_dim);
    }
    
    // Residual
    for (int i = 0; i < dim; i++) x[i] += ffn_out[i];
}

void TransformerLayer::multi_head_attention(float* q, float* k_cache, float* v_cache,
                              float* out, int seq_len, int n_h, int n_kv_h, int h_d) {
    int kv_h_d = n_kv_h * h_d;
    
    #pragma omp parallel for
    for (int h = 0; h < n_h; h++) {
        float* q_h = q + h * h_d;
        float* out_h = out + h * h_d;
        
        // Scores for this head
        std::vector<float> scores(seq_len);
        
        int kv_h = h / (n_h / n_kv_h); // Map to KV head (GQA)
        
        for (int t = 0; t < seq_len; t++) {
            float* k_t = k_cache + t * kv_h_d + kv_h * h_d;
            float score = 0;
            for (int i = 0; i < h_d; i++) {
                score += q_h[i] * k_t[i];
            }
            scores[t] = score / sqrtf(h_d);
        }
        
        // Softmax
        InferenceKernels::softmax_avx512(scores.data(), seq_len);
        
        // Weighted sum of values
        for (int i = 0; i < h_d; i++) out_h[i] = 0;
        for (int t = 0; t < seq_len; t++) {
            float* v_t = v_cache + t * kv_h_d + kv_h * h_d;
            for (int i = 0; i < h_d; i++) {
                out_h[i] += scores[t] * v_t[i];
            }
        }
    }
}
