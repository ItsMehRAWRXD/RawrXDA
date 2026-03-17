#include "transformer.h"
#include "inference_kernels.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <immintrin.h>

// ============================================================================
// TransformerLayer — Zero-Alloc Forward Pass
// ============================================================================
// Rules:
// - NO std::vector construction in forward() or multi_head_attention()
// - ALL scratch buffers pre-allocated in constructor
// - KV cache uses ring-buffer indexing for sliding window
// - AVX2 dot products in attention scoring and value accumulation
// ============================================================================

TransformerLayer::TransformerLayer(int d, int nh, int nkv, int hidden) 
    : dim(d), n_heads(nh), n_kv_heads(nkv), hidden_dim(hidden) {
    head_dim = dim / n_heads;
    cache_pos = 0;
    
    // Allocate KV cache
    max_seq_len = 4096;
    int kv_dim = n_kv_heads * head_dim;
    k_cache = new float[max_seq_len * kv_dim]();
    v_cache = new float[max_seq_len * kv_dim]();
    
    // Pre-allocate ALL scratch buffers — these persist for the layer's lifetime
    scratch_tmp.resize(dim, 0.0f);
    scratch_q.resize(dim, 0.0f);
    scratch_k.resize(dim, 0.0f);
    scratch_v.resize(dim, 0.0f);
    scratch_attn_out.resize(dim, 0.0f);
    scratch_gate.resize(hidden_dim, 0.0f);
    scratch_up.resize(hidden_dim, 0.0f);
    scratch_ffn_out.resize(dim, 0.0f);
    scratch_scores.resize(max_seq_len, 0.0f);
}

TransformerLayer::~TransformerLayer() {
    delete[] k_cache;
    delete[] v_cache;
}

void TransformerLayer::forward(float* x, int pos, int seq_len) {
    // Use pre-allocated scratch — ZERO heap allocations from here on
    float* tmp      = scratch_tmp.data();
    float* q        = scratch_q.data();
    float* k        = scratch_k.data();
    float* v        = scratch_v.data();
    float* attn_out = scratch_attn_out.data();
    
    // --- Self Attention ---
    InferenceKernels::rmsnorm_avx512(tmp, x, attn_norm, dim);
    
    // QKV projections
    if (wq_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp, (block_q4_0*)wq, q, 1, dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp, (block_q4_0*)wk, k, 1, dim, dim);
        InferenceKernels::matmul_q4_0_fused(tmp, (block_q4_0*)wv, v, 1, dim, dim);
    } else {
        // F32 fallback — still no allocations, uses pre-allocated buffers
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
        InferenceKernels::rope_avx512(q + h * head_dim, k + h * head_dim, 
                   head_dim, pos);
    }
    
    // Update KV cache (sliding window: wrap around at max_seq_len)
    int kv_dim = n_kv_heads * head_dim;
    int cache_idx = cache_pos % max_seq_len;  // Ring buffer indexing
    memcpy(k_cache + cache_idx * kv_dim, k, kv_dim * sizeof(float));
    memcpy(v_cache + cache_idx * kv_dim, v, kv_dim * sizeof(float));
    cache_pos++;
    
    // Multi-head attention — uses scratch_scores internally
    int effective_len = std::min(cache_pos, max_seq_len);
    multi_head_attention(q, k_cache, v_cache, attn_out,
                        effective_len, n_heads, n_kv_heads, head_dim);
    
    // Output projection
    if (wq_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(attn_out, (block_q4_0*)wo, tmp, 1, dim, dim);
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
    
    // --- FFN (SwiGLU) --- using pre-allocated scratch_gate, scratch_up, scratch_ffn_out
    InferenceKernels::rmsnorm_avx512(tmp, x, ffn_norm, dim);
    
    float* gate    = scratch_gate.data();
    float* up      = scratch_up.data();
    float* ffn_out = scratch_ffn_out.data();
    
    // w1 @ x (gate)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp, (block_q4_0*)w1, gate, 1, hidden_dim, dim);
    }
    
    // w3 @ x (up)
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(tmp, (block_q4_0*)w3, up, 1, hidden_dim, dim);
    }
    
    // SwiGLU: silu(gate) * up — AVX2 vectorized
    {
        const __m256 one = _mm256_set1_ps(1.0f);
        int i = 0;
        for (; i + 7 < hidden_dim; i += 8) {
            __m256 g = _mm256_loadu_ps(gate + i);
            __m256 u = _mm256_loadu_ps(up + i);
            
            // SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
            // Fast approximation: use the exp from inference_kernels
            // For now, scalar sigmoid inside — still no allocs
            // TODO: inline fast_exp_avx2 here for full vectorization
            alignas(32) float g_tmp[8], result[8];
            _mm256_store_ps(g_tmp, g);
            for (int j = 0; j < 8; j++) {
                float sigmoid = 1.0f / (1.0f + expf(-g_tmp[j]));
                result[j] = g_tmp[j] * sigmoid;
            }
            __m256 silu = _mm256_load_ps(result);
            _mm256_storeu_ps(gate + i, _mm256_mul_ps(silu, u));
        }
        for (; i < hidden_dim; i++) {
            float silu = gate[i] * (1.0f / (1.0f + expf(-gate[i])));
            gate[i] = silu * up[i];
        }
    }
    
    // w2 @ result
    if (ffn_type == GGML_TYPE_Q4_0) {
        InferenceKernels::matmul_q4_0_fused(gate, (block_q4_0*)w2, ffn_out, 1, dim, hidden_dim);
    }
    
    // Residual
    for (int i = 0; i < dim; i++) x[i] += ffn_out[i];
}

// ============================================================================
// Multi-Head Attention — AVX2 dot products, pre-allocated score buffer
// ============================================================================
void TransformerLayer::multi_head_attention(float* q, float* k_cache, float* v_cache,
                              float* out, int seq_len, int n_h, int n_kv_h, int h_d) {
    int kv_h_d = n_kv_h * h_d;
    
    // Zero output (pre-allocated, just clear)
    memset(out, 0, n_h * h_d * sizeof(float));
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < n_h; h++) {
        float* q_h = q + h * h_d;
        float* out_h = out + h * h_d;
        
        // Use thread-local portion of scratch_scores
        // Since OMP threads process different heads, we need per-thread scores
        // For simplicity with the current design, allocate on stack if small
        // Stack allocation is FREE (just pointer adjust, no heap)
        float scores_stack[4096];  // max_seq_len fits in stack (16KB)
        float* scores = (seq_len <= 4096) ? scores_stack : scratch_scores.data();
        
        int kv_h = h / (n_h / std::max(n_kv_h, 1)); // Map to KV head (GQA)
        
        // Score computation: dot(q_h, k_t) / sqrt(h_d)
        float scale = 1.0f / sqrtf((float)h_d);
        
        for (int t = 0; t < seq_len; t++) {
            // Ring-buffer aware indexing
            int cache_t = t % max_seq_len;
            float* k_t = k_cache + cache_t * kv_h_d + kv_h * h_d;
            
            // AVX2 dot product
            __m256 acc = _mm256_setzero_ps();
            int i = 0;
            for (; i + 7 < h_d; i += 8) {
                __m256 vq = _mm256_loadu_ps(q_h + i);
                __m256 vk = _mm256_loadu_ps(k_t + i);
                acc = _mm256_fmadd_ps(vq, vk, acc);
            }
            // Horizontal sum
            __m128 lo = _mm256_castps256_ps128(acc);
            __m128 hi = _mm256_extractf128_ps(acc, 1);
            lo = _mm_add_ps(lo, hi);
            __m128 shuf = _mm_movehdup_ps(lo);
            __m128 sums = _mm_add_ps(lo, shuf);
            shuf = _mm_movehl_ps(shuf, sums);
            sums = _mm_add_ss(sums, shuf);
            float score = _mm_cvtss_f32(sums);
            
            // Scalar tail
            for (; i < h_d; i++) {
                score += q_h[i] * k_t[i];
            }
            scores[t] = score * scale;
        }
        
        // Softmax over scores
        InferenceKernels::softmax_avx512(scores, seq_len);
        
        // Weighted sum of values — AVX2 FMA
        for (int i = 0; i < h_d; i++) out_h[i] = 0;
        for (int t = 0; t < seq_len; t++) {
            int cache_t = t % max_seq_len;
            float* v_t = v_cache + cache_t * kv_h_d + kv_h * h_d;
            float s = scores[t];
            
            if (s < 1e-8f) continue;  // Skip near-zero scores
            
            __m256 vs = _mm256_set1_ps(s);
            int i = 0;
            for (; i + 7 < h_d; i += 8) {
                __m256 vo = _mm256_loadu_ps(out_h + i);
                __m256 vv = _mm256_loadu_ps(v_t + i);
                _mm256_storeu_ps(out_h + i, _mm256_fmadd_ps(vs, vv, vo));
            }
            for (; i < h_d; i++) {
                out_h[i] += s * v_t[i];
            }
        }
    }
}
