#include "rawrxd_transformer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <iostream>
#ifdef __AVX512F__
#include <immintrin.h>
#endif

// C++ Implementations of Kernels (Ensuring Real Logic Execution)
#ifdef __AVX512F__
void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N) {
    #pragma omp parallel for collapse(2)
    for (uint64_t i = 0; i < M; i++) {
        for (uint64_t j = 0; j < N; j++) {
            __m512 sum_vec = _mm512_setzero_ps();
            uint64_t k = 0;
            for (; k + 15 < K; k += 16) {
                __m512 a_vec = _mm512_loadu_ps(A + i * K + k);
                __m512 b_vec = _mm512_loadu_ps(B + j * K + k);
                sum_vec = _mm512_fmadd_ps(a_vec, b_vec, sum_vec);
            }
            float sum = _mm512_reduce_add_ps(sum_vec);
            for (; k < K; k++) {
                sum += A[i * K + k] * B[j * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}
#else
void MatrixMultiply_AVX512(const float* A, const float* B, float* C, uint64_t M, uint64_t K, uint64_t N) {
    #pragma omp parallel for collapse(2)
    for (uint64_t i = 0; i < M; i++) {
        for (uint64_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (uint64_t k = 0; k < K; k++) {
                sum += A[i * K + k] * B[j * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}
#endif

#ifdef __AVX512F__
void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps) {
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
#else
void RMSNorm_AVX512(float* out, const float* in, const float* weight, int size, float eps) {
    float ss = 0.0f;
    for (int i = 0; i < size; i++) {
        ss += in[i] * in[i];
    }
    ss /= size;
    ss += eps;
    float inv_rms = 1.0f / sqrtf(ss);
    for (int i = 0; i < size; i++) {
        out[i] = in[i] * weight[i] * inv_rms;
    }
}
#endif

#ifdef __AVX512F__
void Softmax_AVX512(float* x, int size) {
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
#else
void Softmax_AVX512(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}
#endif

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

#ifdef __AVX512F__
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
#else
void Silu_AVX512(float* x, int size) {
    for (int i = 0; i < size; i++) {
        float val = x[i];
        x[i] = val / (1.0f + expf(-val));
    }
}
#endif

// AVX-512 optimized dot product
#ifdef __AVX512F__
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
#else
float DotProduct_AVX512(const float* a, const float* b, int size) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}
#endif

// AVX-512 optimized vector addition with scalar multiplier
#ifdef __AVX512F__
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
#else
void VectorAddScaled_AVX512(float* out, const float* in, float scale, int size) {
    for (int i = 0; i < size; i++) {
        out[i] += scale * in[i];
    }
}
#endif

// AVX-512 optimized vector addition
#ifdef __AVX512F__
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
#else
void VectorAdd_AVX512(float* out, const float* a, const float* b, int size) {
    for (int i = 0; i < size; i++) {
        out[i] = a[i] + b[i];
    }
}
#endif


void RawrXDTransformer::Initialize(VkDevice device, VkPhysicalDevice physDevice, Config cfg, RawrXDModelLoader* loader) {
    this->device = device;
    this->config = cfg;
    this->loader = loader;
    
    // Initialize KV Cache — use seq_len if n_ctx wasn't set
    int ctx = config.n_ctx > 0 ? config.n_ctx : (config.seq_len > 0 ? config.seq_len : 2048);
    // Use n_kv_heads dimension for KV cache (GQA/MQA support)
    int kv_dim = (config.n_kv_heads > 0 ? config.n_kv_heads : config.n_heads) * (config.dim / config.n_heads);
    size_t kv_size = (size_t)config.n_layers * ctx * kv_dim;
    printf("[RawrXD] KV cache: %zu floats (%.1f MB per cache)\n", kv_size, kv_size * 4.0 / 1e6);
    kv_cache_k.resize(kv_size, 0.0f);
    kv_cache_v.resize(kv_size, 0.0f);
    
    // Precompute RoPE tables if needed (usually just done on fly in kernels)
    printf("[RawrXD] Transformer Initialized. AVX-512 Kernels Linked.\n");
}

std::vector<float> RawrXDTransformer::Forward(const std::vector<uint32_t>& tokens, int start_pos) {
    if (tokens.empty()) return {};
    
    int T = tokens.size();
    int current_pos = start_pos;
    
    // GQA dimensions
    int dim = config.dim;
    int n_heads = config.n_heads;
    int n_kv_heads = config.n_kv_heads > 0 ? config.n_kv_heads : n_heads;
    int head_dim = dim / n_heads;
    int kv_dim = n_kv_heads * head_dim;  // K/V projection output size
    int heads_per_kv = n_heads / n_kv_heads;  // GQA repetition factor
    
    static bool printed_config = false;
    if (!printed_config) {
        printf("[Forward] GQA: dim=%d heads=%d kv_heads=%d head_dim=%d kv_dim=%d hidden=%d\n",
               dim, n_heads, n_kv_heads, head_dim, kv_dim, config.hidden_dim);
        printed_config = true;
    }
    
    // Current hidden state
    std::vector<float> x(dim);
    
    for (int t = 0; t < T; t++) {
        uint32_t token = tokens[t];
        
        // 1. Embedding lookup
        float* emb_w = loader->GetTensor("token_embd.weight");
        if (!emb_w) emb_w = loader->GetTensor("model.embed_tokens.weight");
        if (!emb_w) { printf("[Forward] FATAL: Missing token_embd.weight\n"); return {}; }
        
        if (token >= (uint32_t)config.vocab_size) {
            printf("[Forward] WARN: token %u >= vocab_size %d, clamping\n", token, config.vocab_size);
            token = config.vocab_size - 1;
        }
        
        memcpy(x.data(), emb_w + token * dim, dim * sizeof(float));
        
        // 2. Transformer Layers
        for (int l = 0; l < config.n_layers; l++) {
            std::vector<float> residual = x;
            
            // --- ATTENTION ---
            std::string prefix = "blk." + std::to_string(l) + ".";
            
            float* attn_norm = loader->GetTensor(prefix + "attn_norm.weight");
            if (!attn_norm) { printf("[Forward] FATAL: Missing %sattn_norm.weight\n", prefix.c_str()); return {}; }
            RMSNorm_AVX512(x.data(), x.data(), attn_norm, dim, config.rms_norm_eps);
            
            float* wq = loader->GetTensor(prefix + "attn_q.weight");
            float* wk = loader->GetTensor(prefix + "attn_k.weight");
            float* wv = loader->GetTensor(prefix + "attn_v.weight");
            float* wo = loader->GetTensor(prefix + "attn_output.weight");
            if (!wq || !wk || !wv || !wo) {
                printf("[Forward] FATAL: Missing attn weights layer %d (q=%p k=%p v=%p o=%p)\n",
                       l, (void*)wq, (void*)wk, (void*)wv, (void*)wo);
                return {};
            }
            
            // Q: dim → dim, K: dim → kv_dim, V: dim → kv_dim
            std::vector<float> q(dim), k(kv_dim), v(kv_dim);
            MatrixMultiply_AVX512(x.data(), wq, q.data(), 1, dim, dim);
            MatrixMultiply_AVX512(x.data(), wk, k.data(), 1, dim, kv_dim);
            MatrixMultiply_AVX512(x.data(), wv, v.data(), 1, dim, kv_dim);
            
            // RoPE — apply separately for Q (n_heads) and K (n_kv_heads)
            RoPE_AVX512(q.data(), nullptr, current_pos + t, head_dim, n_heads);
            RoPE_AVX512(k.data(), nullptr, current_pos + t, head_dim, n_kv_heads);
            
            // KV Cache Update — store kv_dim per position
            size_t cache_offset = (size_t)l * config.n_ctx * kv_dim + (size_t)(current_pos + t) * kv_dim;
            if (cache_offset + kv_dim <= kv_cache_k.size()) {
                memcpy(kv_cache_k.data() + cache_offset, k.data(), kv_dim * sizeof(float));
                memcpy(kv_cache_v.data() + cache_offset, v.data(), kv_dim * sizeof(float));
            }
            
            // Multi-head attention with GQA
            std::vector<float> att_out(dim, 0.0f);
            int seq_len = current_pos + t + 1;
            
            for (int h = 0; h < n_heads; h++) {
                int kv_h = h / heads_per_kv;  // Which KV head this Q head uses
                
                std::vector<float> scores(seq_len);
                float* q_head = q.data() + h * head_dim;
                
                for (int p = 0; p < seq_len; p++) {
                    size_t k_off = (size_t)l * config.n_ctx * kv_dim + (size_t)p * kv_dim + kv_h * head_dim;
                    float* k_past = kv_cache_k.data() + k_off;
                    float score = DotProduct_AVX512(q_head, k_past, head_dim);
                    scores[p] = score / sqrtf((float)head_dim);
                }
                
                Softmax_AVX512(scores.data(), seq_len);
                
                float* out_head = att_out.data() + h * head_dim;
                for (int p = 0; p < seq_len; p++) {
                    size_t v_off = (size_t)l * config.n_ctx * kv_dim + (size_t)p * kv_dim + kv_h * head_dim;
                    float* v_past = kv_cache_v.data() + v_off;
                    VectorAddScaled_AVX512(out_head, v_past, scores[p], head_dim);
                }
            }
            
            // Output projection: dim → dim
            std::vector<float> attn_final(dim);
            MatrixMultiply_AVX512(att_out.data(), wo, attn_final.data(), 1, dim, dim);
            
            // Residual add
            VectorAdd_AVX512(x.data(), residual.data(), attn_final.data(), dim);
            
            // --- FFN (SwiGLU) ---
            residual = x;
            std::string ffn_prefix = prefix + "ffn_";
            
            float* ffn_norm = loader->GetTensor(prefix + "ffn_norm.weight");
            if (!ffn_norm) { printf("[Forward] FATAL: Missing %sffn_norm.weight\n", prefix.c_str()); return {}; }
            RMSNorm_AVX512(x.data(), x.data(), ffn_norm, dim, config.rms_norm_eps);
            
            float* w1 = loader->GetTensor(ffn_prefix + "gate.weight");
            float* w2 = loader->GetTensor(ffn_prefix + "down.weight");
            float* w3 = loader->GetTensor(ffn_prefix + "up.weight");
            if (!w1 || !w2 || !w3) {
                printf("[Forward] FATAL: Missing FFN weights layer %d (gate=%p down=%p up=%p)\n",
                       l, (void*)w1, (void*)w2, (void*)w3);
                return {};
            }
            
            int hdim = config.hidden_dim;
            std::vector<float> h1(hdim), h3(hdim);
            
            MatrixMultiply_AVX512(x.data(), w1, h1.data(), 1, dim, hdim);
            MatrixMultiply_AVX512(x.data(), w3, h3.data(), 1, dim, hdim);
            
            // SiLU(gate) * up
            Silu_AVX512(h1.data(), hdim);
#ifdef __AVX512F__
            {
                int i = 0;
                for (; i + 15 < hdim; i += 16) {
                    __m512 h1v = _mm512_loadu_ps(h1.data() + i);
                    __m512 h3v = _mm512_loadu_ps(h3.data() + i);
                    _mm512_storeu_ps(h1.data() + i, _mm512_mul_ps(h1v, h3v));
                }
                for (; i < hdim; i++) h1[i] *= h3[i];
            }
#else
            for (int i = 0; i < hdim; i++) h1[i] *= h3[i];
#endif
            
            // Down projection: hidden_dim → dim
            std::vector<float> final_ffn(dim);
            MatrixMultiply_AVX512(h1.data(), w2, final_ffn.data(), 1, hdim, dim);
            
            VectorAdd_AVX512(x.data(), residual.data(), final_ffn.data(), dim);
        }
    }
    
    // Final norm + output projection
    float* out_norm = loader->GetTensor("output_norm.weight");
    if (!out_norm) { printf("[Forward] FATAL: Missing output_norm.weight\n"); return {}; }
    RMSNorm_AVX512(x.data(), x.data(), out_norm, dim, config.rms_norm_eps);
    
    float* w_out = loader->GetTensor("output.weight");
    if (!w_out) { printf("[Forward] FATAL: Missing output.weight\n"); return {}; }
    if (config.vocab_size <= 0) { printf("[Forward] FATAL: vocab_size=%d\n", config.vocab_size); return {}; }
    
    std::vector<float> logits(config.vocab_size);
    MatrixMultiply_AVX512(x.data(), w_out, logits.data(), 1, dim, config.vocab_size);
    
    return logits;
}
