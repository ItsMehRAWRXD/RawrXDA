#pragma once
#include "common_types.h"
#include "gguf_core.h"
#include <memory>
#include <vector>

class TransformerLayer {
public:
    int dim;
    int n_heads;
    int n_kv_heads;
    int head_dim;
    int hidden_dim;
    
    // Weights (pointers to mapped GGUF data)
    float* attn_norm;
    void* wq; void* wk; void* wv; void* wo;
    ggml_type wq_type;
    
    float* ffn_norm;
    void* w1; void* w2; void* w3;
    ggml_type ffn_type;
    
    // KV cache
    float* k_cache;
    float* v_cache;
    int cache_pos;
    int max_seq_len;
    
    TransformerLayer(int d, int nh, int nkv, int hidden);
    ~TransformerLayer(); // Cleanup KV Cache
    
    void forward(float* x, int pos, int seq_len);

private:
    void multi_head_attention(float* q, float* k_cache, float* v_cache,
                              float* out, int seq_len, int n_h, int n_kv_h, int h_d);
};
