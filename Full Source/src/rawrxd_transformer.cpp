#include "rawrxd_transformer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <immintrin.h>

// C++ Implementations of Kernels (Ensuring Real Logic Execution)
void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N) {
    // A: (M, K), B: (K, N) [Standard matrix multiplication], C: (M, N)
    // Note: B is transposed in the original code, but we'll assume standard layout
    // Parallelize over M (batch) and N (outputs)
    #pragma omp parallel for collapse(2)
    for (uint64_t i = 0; i < M; i++) {
        for (uint64_t j = 0; j < N; j++) {
            __m512 sum_vec = _mm512_setzero_ps();
            uint64_t k = 0;
            // Vectorize the inner loop with AVX-512 (16 floats per iteration)
            for (; k + 15 < K; k += 16) {
                __m512 a_vec = _mm512_loadu_ps(A + i * K + k);
                __m512 b_vec = _mm512_loadu_ps(B + j * K + k);
                sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
            }
            // Handle remaining elements
            float sum = _mm512_reduce_add_ps(sum_vec);
            for (; k < K; k++) {
                sum += A[i * K + k] * B[j * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}

void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps) {
    // Compute sum of squares
    __m512 sum_vec = _mm512_setzero_ps();
    int i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 in_vec = _mm512_loadu_ps(in + i);
        sum_vec = _mm512_fmadd_ps(in_vec, in_vec, sum_vec);
    }
    float ss = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++) {
        ss += in[i] * in[i];
    }
    ss /= size;
    ss += eps;
    float inv_rms = 1.0f / sqrtf(ss);

    // Apply normalization
    __m512 inv_rms_vec = _mm512_set1_ps(inv_rms);
    i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 in_vec = _mm512_loadu_ps(in + i);
        __m512 weight_vec = _mm512_loadu_ps(weight + i);
        __m512 out_vec = _mm512_mul_ps(_mm512_mul_ps(in_vec, weight_vec), inv_rms_vec);
        _mm512_storeu_ps(out + i, out_vec);
    }
    for (; i < size; i++) {
        out[i] = in[i] * weight[i] * inv_rms;
    }
}

void Softmax_AVX512(float* x, int size) {
    // Find max value
    __m512 max_vec = _mm512_loadu_ps(x);
    int i = 16;
    for (; i + 15 < size; i += 16) {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        max_vec = _mm512_max_ps(max_vec, curr_vec);
    }
    float max_val = _mm512_reduce_max_ps(max_vec);
    for (; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }

    // Compute exp(x - max) and sum
    __m512 max_val_vec = _mm512_set1_ps(max_val);
    __m512 sum_vec = _mm512_setzero_ps();
    i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        curr_vec = _mm512_sub_ps(curr_vec, max_val_vec);
        curr_vec = _mm512_exp_ps(curr_vec);
        _mm512_storeu_ps(x + i, curr_vec);
        sum_vec = _mm512_add_ps(sum_vec, curr_vec);
    }
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }

    // Normalize
    __m512 sum_inv_vec = _mm512_set1_ps(1.0f / sum);
    i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        curr_vec = _mm512_mul_ps(curr_vec, sum_inv_vec);
        _mm512_storeu_ps(x + i, curr_vec);
    }
    for (; i < size; i++) {
        x[i] /= sum;
    }
}

void RoPE_AVX512(float* q, float* k, int pos, int head_dim, int num_heads) {
    // Simple scalar implementation of RoPE
    for (int h = 0; h < num_heads; h++) {
        for (int i = 0; i < head_dim; i += 2) {
            float theta = powf(10000.0f, -float(i) / head_dim);
            float alpha = pos * theta;
            float cos_a = cosf(alpha);
            float sin_a = sinf(alpha);
            
            float* q_ptr = q + h*head_dim + i;
            float* k_ptr = k + h*head_dim + i;
            
            float q0 = q_ptr[0];
            float q1 = q_ptr[1];
            q_ptr[0] = q0 * cos_a - q1 * sin_a;
            q_ptr[1] = q0 * sin_a + q1 * cos_a;
            
            if (k) { // Support cases where k is rotated separately
                float k0 = k_ptr[0];
                float k1 = k_ptr[1];
                k_ptr[0] = k0 * cos_a - k1 * sin_a;
                k_ptr[1] = k0 * sin_a + k1 * cos_a;
            }
        }
    }
}

void Silu_AVX512(float* x, int size) {
    int i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 val_vec = _mm512_loadu_ps(x + i);
        __m512 neg_val_vec = _mm512_sub_ps(_mm512_setzero_ps(), val_vec);
        __m512 exp_vec = _mm512_exp_ps(neg_val_vec);
        __m512 one_vec = _mm512_set1_ps(1.0f);
        __m512 denom_vec = _mm512_add_ps(one_vec, exp_vec);
        __m512 result_vec = _mm512_div_ps(val_vec, denom_vec);
        _mm512_storeu_ps(x + i, result_vec);
    }
    for (; i < size; i++) {
        float val = x[i];
        x[i] = val / (1.0f + expf(-val));
    }
}

// AVX-512 optimized dot product
float DotProduct_AVX512(const float* a, const float* b, int size) {
    __m512 sum_vec = _mm512_setzero_ps();
    int i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 a_vec = _mm512_loadu_ps(a + i);
        __m512 b_vec = _mm512_loadu_ps(b + i);
        sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
    }
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// AVX-512 optimized vector addition with scalar multiplier
void VectorAddScaled_AVX512(float* out, const float* in, float scale, int size) {
    __m512 scale_vec = _mm512_set1_ps(scale);
    int i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 out_vec = _mm512_loadu_ps(out + i);
        __m512 in_vec = _mm512_loadu_ps(in + i);
        __m512 scaled_vec = _mm512_mul_ps(in_vec, scale_vec);
        out_vec = _mm512_add_ps(out_vec, scaled_vec);
        _mm512_storeu_ps(out + i, out_vec);
    }
    for (; i < size; i++) {
        out[i] += scale * in[i];
    }
}

// AVX-512 optimized vector addition
void VectorAdd_AVX512(float* out, const float* a, const float* b, int size) {
    int i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 a_vec = _mm512_loadu_ps(a + i);
        __m512 b_vec = _mm512_loadu_ps(b + i);
        __m512 sum_vec = _mm512_add_ps(a_vec, b_vec);
        _mm512_storeu_ps(out + i, sum_vec);
    }
    for (; i < size; i++) {
        out[i] = a[i] + b[i];
    }
}


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
                    float score = DotProduct_AVX512(q_head, k_past, head_dim);
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
                    VectorAddScaled_AVX512(out_head, v_past, w, head_dim);
                }
            }
            
            // Linear Output
            std::vector<float> attn_final(config.dim);
            MatrixMultiply_AVX512(att_out.data(), wo, attn_final.data(), 1, config.dim, config.dim);
            
            // Residual Add
            VectorAdd_AVX512(x.data(), residual.data(), attn_final.data(), config.dim);
            
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
            // Element-wise multiplication
            int i = 0;
            for (; i + 15 < config.hidden_dim; i += 16) {
                __m512 h1_vec = _mm512_loadu_ps(h1.data() + i);
                __m512 h3_vec = _mm512_loadu_ps(h3.data() + i);
                __m512 result_vec = _mm512_mul_ps(h1_vec, h3_vec);
                _mm512_storeu_ps(h1.data() + i, result_vec);
            }
            for (; i < config.hidden_dim; i++) {
                h1[i] *= h3[i];
            }
            
            // Down proj
            std::vector<float> final_ffn(config.dim);
            MatrixMultiply_AVX512(h1.data(), w2, final_ffn.data(), 1, config.hidden_dim, config.dim);
            
            // Residual Add
            VectorAdd_AVX512(x.data(), residual.data(), final_ffn.data(), config.dim);
            
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
