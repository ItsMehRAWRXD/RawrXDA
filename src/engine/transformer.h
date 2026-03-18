#pragma once
#include "common_types.h"
#include "gguf_core.h"
#include <cstdint>
#include <memory>
#include <vector>

class TransformerLayer {
public:
    int dim;
    int n_heads;
    int n_kv_heads;
    int head_dim;
    int hidden_dim;
    
    // Weights (pointers to mapped GGUF data — not owned)
    float* attn_norm;
    void* wq; void* wk; void* wv; void* wo;
    ggml_type wq_type;
    
    float* ffn_norm;
    void* w1; void* w2; void* w3;
    ggml_type ffn_type;
    
    // ================================================================
    // KV Cache — Dual mode: int8 quantized (primary) + FP32 (legacy)
    // int8 reduces bandwidth 4x: 128-head * 128-dim * 4096-seq = 256MB → 64MB
    // Per-position scale factor stored alongside for dequant-on-read
    // ================================================================
    
    // int8 quantized KV cache (primary path — 4x bandwidth reduction)
    int8_t* k_cache_q8;        // [max_seq_len * kv_dim] int8 quantized keys
    int8_t* v_cache_q8;        // [max_seq_len * kv_dim] int8 quantized values
    float*  k_cache_scales;    // [max_seq_len] per-position absmax scale for K
    float*  v_cache_scales;    // [max_seq_len] per-position absmax scale for V
    bool    use_quantized_kv;  // Toggle: true = int8, false = legacy FP32
    
    // Legacy FP32 KV cache (kept for backward compat + F32 fallback path)
    float* k_cache;
    float* v_cache;
    int cache_pos;
    int64_t total_tokens_seen;
    int max_seq_len;
    
    // ================================================================
    // Pre-allocated scratch buffers — ZERO heap allocs in forward()
    // Allocated once in constructor, reused every call
    // ================================================================
    std::vector<float> scratch_tmp;      // [dim]
    std::vector<float> scratch_q;        // [dim]
    std::vector<float> scratch_k;        // [dim]
    std::vector<float> scratch_v;        // [dim]
    std::vector<float> scratch_attn_out; // [dim]
    std::vector<float> scratch_gate;     // [hidden_dim]
    std::vector<float> scratch_up;       // [hidden_dim]
    std::vector<float> scratch_ffn_out;  // [dim]
    std::vector<float> scratch_scores;   // [max_seq_len] — reused per head
    std::vector<float> scratch_k_dequant; // [kv_dim] — for dequanting one position's K
    std::vector<float> scratch_v_dequant; // [kv_dim] — for dequanting one position's V
    
    TransformerLayer(int d, int nh, int nkv, int hidden);
    ~TransformerLayer(); // Cleanup KV Cache
    
    void forward(float* x, int pos, int seq_len);

private:
    void multi_head_attention(float* q, float* k_cache, float* v_cache,
                              float* out, int seq_len, int logical_start_pos,
                              int n_h, int n_kv_h, int h_d);
    
    // Flash-Attention v2 path — uses quantized KV cache
    void multi_head_attention_flash(float* q, float* out, int seq_len,
                                    int logical_start_pos,
                                    int n_h, int n_kv_h, int h_d);
};
