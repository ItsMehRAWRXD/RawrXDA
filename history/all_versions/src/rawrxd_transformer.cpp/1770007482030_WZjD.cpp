#include "rawrxd_transformer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <immintrin.h>

// Link to ASM kernels
extern "C" void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N);
extern "C" void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps);
extern "C" void Softmax_AVX512(float* x, int size);
extern "C" void RoPE_AVX512(float* q, float* k, int pos, int head_dim, int num_heads);
extern "C" void Silu_AVX512(float* x, int size);

void RawrXDTransformer::Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader* loader) {
    this->device = device;
    this->config = cfg;
    this->loader = loader;
    
    // Initialize KV Cache
    int kv_size = config.n_layers * config.n_ctx * config.dim; // Simplified
    kv_cache_k.resize(kv_size);
    kv_cache_v.resize(kv_size);
    
    // Precompute RoPE tables if needed (usually just done on fly in kernels)
    printf("[RawrXD] Transformer Initialized. AVX-512 Kernels Linked.\n");
}

std::vector<float> RawrXDTransformer::Forward(const std::vector<uint32_t>& tokens, int start_pos) {
    if (tokens.empty()) return {};
    
    // Logic for single token generation (inference mode)
    // For prompt processing, we would loop over tokens.
    // Here we assume tokens contains the PROMPT, and we process it.
    
    int T = tokens.size();
    int current_pos = start_pos;
    
    // Current hidden state
    std::vector<float> x(config.dim);
    
    // Process context (Prefill)
    // In a real optimized engine, we batch this. 
    // Here we iterate to simulate correct causal masking flow.
    
    for (int t = 0; t < T; t++) {
        uint32_t token = tokens[t];
        
        // 1. Embedding
        // Fetch from loader
        float* emb_w = loader->GetTensor("token_embd.weight");
        if (!emb_w) { printf("Missing token_embd.weight\n"); return {}; }
        
        // Copy embedding to x
        memcpy(x.data(), emb_w + token * config.dim, config.dim * sizeof(float));
        
        // 2. Transformer Blocks
        for (int l = 0; l < config.n_layers; l++) {
            std::vector<float> residual = x;
            
            // --- ATTENTION START ---
            
            // RMS Norm
            std::string prefix = "blk." + std::to_string(l) + ".";
            float* attn_norm = loader->GetTensor(prefix + "attn_norm.weight");
            RMSNorm_AVX512(x.data(), x.data(), attn_norm, config.dim, config.rms_norm_eps);
            
            // QKV Projections
            // In GGUF these are often merged or separate. Assuming separate for clarity or merged.
            // Llama 2/3: wq, wk, wv.
            float* wq = loader->GetTensor(prefix + "attn_q.weight");
            float* wk = loader->GetTensor(prefix + "attn_k.weight");
            float* wv = loader->GetTensor(prefix + "attn_v.weight");
            float* wo = loader->GetTensor(prefix + "attn_output.weight");
            
            int head_dim = config.dim / config.n_heads;
            std::vector<float> q(config.dim), k(config.dim), v(config.dim);
            
            MatrixMultiply_AVX512(x.data(), wq, q.data(), 1, config.dim, config.dim);
            MatrixMultiply_AVX512(x.data(), wk, k.data(), 1, config.dim, config.dim); // Support GQA later
            MatrixMultiply_AVX512(x.data(), wv, v.data(), 1, config.dim, config.dim); // Support GQA later
            
            // RoPE
            RoPE_AVX512(q.data(), k.data(), current_pos + t, head_dim, config.n_heads);
            
            // KV Cache Update
            // Store k, v into cache at [layer][pos]
            // Implementation specific offset math
            int cache_offset = (l * config.n_ctx + (current_pos + t)) * config.dim;
            memcpy(kv_cache_k.data() + cache_offset, k.data(), config.dim * sizeof(float));
            memcpy(kv_cache_v.data() + cache_offset, v.data(), config.dim * sizeof(float));
            
            // Attention Calculation (Q * K^T)
            // Simplified for single head loop to show correct logic
            std::vector<float> att_out(config.dim);
            
            // Multi-head attention loop (parallelize via OpenMP usually)
            #pragma omp parallel for
            for (int h = 0; h < config.n_heads; h++) {
                // Score current query against all past keys
                std::vector<float> scores(config.n_ctx); // Max context
                float* q_head = q.data() + h * head_dim;
                
                for (int p = 0; p <= (current_pos + t); p++) {
                    int k_offset = (l * config.n_ctx + p) * config.dim + h * head_dim;
                    float* k_past = kv_cache_k.data() + k_offset;
                    
                    // Dot product
                    float score = 0.0f;
                    for (int i=0; i<head_dim; i++) score += q_head[i] * k_past[i];
                    score /= sqrtf((float)head_dim);
                    scores[p] = score;
                }
                
                // Softmax
                Softmax_AVX512(scores.data(), (current_pos + t) + 1);
                
                // Weighted sum of Values
                float* out_head = att_out.data() + h * head_dim;
                std::fill(out_head, out_head + head_dim, 0.0f);
                
                for (int p = 0; p <= (current_pos + t); p++) {
                    int v_offset = (l * config.n_ctx + p) * config.dim + h * head_dim;
                    float* v_past = kv_cache_v.data() + v_offset;
                    float w = scores[p];
                    
                    for (int i=0; i<head_dim; i++) out_head[i] += w * v_past[i];
                }
            }
            
            // Linear Output
            std::vector<float> attn_final(config.dim);
            MatrixMultiply_AVX512(att_out.data(), wo, attn_final.data(), 1, config.dim, config.dim);
            
            // Residual Add
            for(int i=0; i<config.dim; i++) x[i] = residual[i] + attn_final[i];
            
            // --- ATTENTION END ---
            
            // --- FFN START ---
            residual = x;
            std::string ffn_prefix = prefix + "ffn_";
            float* ffn_norm = loader->GetTensor(prefix + "ffn_norm.weight");
            RMSNorm_AVX512(x.data(), x.data(), ffn_norm, config.dim, config.rms_norm_eps);
            
            float* w1 = loader->GetTensor(ffn_prefix + "gate.weight");
            float* w2 = loader->GetTensor(ffn_prefix + "down.weight");
            float* w3 = loader->GetTensor(ffn_prefix + "up.weight");
            
            // Hidden dim is usually w1.shape[0] (or [1] depending on transpose)
            // Hardcoded guess based on ptr math if not available, or assume loader handles it?
            // For now, assume w1 maps dim -> hidden_dim
            // Standard Llama: 4096 -> 11008
             
            // We need temporary buffers for hidden states.
            // Since we can't easily query dimensions in this rigid C++ block without the map,
            // we'll assume a safe upper bound or dynamic resize based on model config.
            // Let's assume hidden_dim = (dim * 8) / 3 refined aligned (standard SwiGLU ratio)
            int hidden_dim = (config.dim * 8) + 2*config.dim; // Roughly
             // Actually, lets trust the pointer exists and use a vector that resizes?
             // No, MatrixMultiply needs size. 
             // We rely on config.hidden_dim which should be populated by loader.
            
            std::vector<float> h1(config.hidden_dim);
            std::vector<float> h3(config.hidden_dim);
            
            MatrixMultiply_AVX512(x.data(), w1, h1.data(), 1, config.dim, config.hidden_dim);
            MatrixMultiply_AVX512(x.data(), w3, h3.data(), 1, config.dim, config.hidden_dim);
            
            // SiLU(h1) * h3
            Silu_AVX512(h1.data(), config.hidden_dim);
            for(int i=0; i<config.hidden_dim; i++) h1[i] *= h3[i];
            
            // Down proj
            std::vector<float> final_ffn(config.dim);
            MatrixMultiply_AVX512(h1.data(), w2, final_ffn.data(), 1, config.hidden_dim, config.dim);
            
            // Residual Add
            for(int i=0; i<config.dim; i++) x[i] = residual[i] + final_ffn[i];
            
            // --- FFN END ---
        }
    }
    
    // Final Norm
    float* out_norm = loader->GetTensor("output_norm.weight");
    RMSNorm_AVX512(x.data(), x.data(), out_norm, config.dim, config.rms_norm_eps);
    
    // Output Heads
    float* w_out = loader->GetTensor("output.weight");
    std::vector<float> logits(config.vocab_size);
    MatrixMultiply_AVX512(x.data(), w_out, logits.data(), 1, config.dim, config.vocab_size);
    
    return logits;
}
