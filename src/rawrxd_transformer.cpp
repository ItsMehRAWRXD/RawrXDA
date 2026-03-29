#include "rawrxd_transformer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <limits>
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
    if (!out || !in || !weight || size <= 0) return;
    if (!std::isfinite(eps) || eps <= 0.0f) eps = 1e-5f;

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
    if (!std::isfinite(ss) || ss <= 0.0f) ss = eps;
    float inv_rms = 1.0f / sqrtf(ss);
    if (!std::isfinite(inv_rms) || inv_rms <= 0.0f) inv_rms = 1.0f;

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
    if (!out || !in || !weight || size <= 0) return;
    if (!std::isfinite(eps) || eps <= 0.0f) eps = 1e-5f;

    float ss = 0.0f;
    for (int i = 0; i < size; i++) {
        ss += in[i] * in[i];
    }
    ss /= size;
    ss += eps;
    if (!std::isfinite(ss) || ss <= 0.0f) ss = eps;
    float inv_rms = 1.0f / sqrtf(ss);
    if (!std::isfinite(inv_rms) || inv_rms <= 0.0f) inv_rms = 1.0f;
    for (int i = 0; i < size; i++) {
        out[i] = in[i] * weight[i] * inv_rms;
    }
}
#endif

// =============================================================================
// Portable AVX-512 exp approximation — replaces SVML _mm512_exp_ps
// 6th-order minimax polynomial on [-87.3, 88.7], ~1e-6 relative error.
// Works on ANY AVX-512F capable compiler (GCC, Clang, MSVC) without SVML.
// =============================================================================
#ifdef __AVX512F__
static inline __m512 _rawrxd_exp_ps_avx512(__m512 x) {
    // Clamp to prevent overflow/underflow
    const __m512 hi = _mm512_set1_ps(88.3762626647949f);
    const __m512 lo = _mm512_set1_ps(-87.3365447504f);
    x = _mm512_min_ps(x, hi);
    x = _mm512_max_ps(x, lo);

    // exp(x) = 2^(x * log2(e))  →  2^(n + f)  where n=floor, f=fraction
    const __m512 log2e = _mm512_set1_ps(1.44269504088896341f);
    const __m512 half  = _mm512_set1_ps(0.5f);
    __m512 t = _mm512_fmadd_ps(x, log2e, half);  // t = x*log2e + 0.5

    // n = floor(t)
    __m512 n = _mm512_roundscale_ps(t, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);

    // f = x - n * ln(2)
    const __m512 c1 = _mm512_set1_ps(0.693359375f);     // ln(2) upper
    const __m512 c2 = _mm512_set1_ps(-2.12194440e-4f);  // ln(2) lower (Cody-Waite)
    __m512 f = _mm512_fnmadd_ps(n, c1, x);
    f = _mm512_fnmadd_ps(n, c2, f);

    // Polynomial approximation of 2^f - 1 on [0, ln2)
    const __m512 p0 = _mm512_set1_ps(1.0f);
    const __m512 p1 = _mm512_set1_ps(1.0f);
    const __m512 p2 = _mm512_set1_ps(0.5f);
    const __m512 p3 = _mm512_set1_ps(0.1666666666666f);
    const __m512 p4 = _mm512_set1_ps(0.0416666666666f);
    const __m512 p5 = _mm512_set1_ps(0.0083333333333f);
    const __m512 p6 = _mm512_set1_ps(0.0013888888888f);

    __m512 y = _mm512_fmadd_ps(p6, f, p5);
    y = _mm512_fmadd_ps(y, f, p4);
    y = _mm512_fmadd_ps(y, f, p3);
    y = _mm512_fmadd_ps(y, f, p2);
    y = _mm512_fmadd_ps(y, f, p1);
    y = _mm512_fmadd_ps(y, f, p0);

    // Scale by 2^n: convert n to integer, shift into float exponent bits
    __m512i ni = _mm512_cvtps_epi32(n);
    ni = _mm512_slli_epi32(ni, 23);                 // Shift into exponent field
    __m512 pow2n = _mm512_castsi512_ps(_mm512_add_epi32(ni, _mm512_set1_epi32(0x3F800000)));

    return _mm512_mul_ps(y, pow2n);
}

void Softmax_AVX512(float* x, int size) {
    if (!x || size <= 0) return;
    if (size == 1) { x[0] = 1.0f; return; }
    for (int i = 0; i < size; ++i) {
        if (!std::isfinite(x[i])) x[i] = -1e9f;
    }

    float max_val = x[0];
    int i = 1;
    if (size >= 16) {
        __m512 max_vec = _mm512_loadu_ps(x);
        i = 16;
        for (; i + 15 < size; i += 16) {
            __m512 curr_vec = _mm512_loadu_ps(x + i);
            max_vec = _mm512_max_ps(max_vec, curr_vec);
        }
        max_val = _mm512_reduce_max_ps(max_vec);
    }
    for (; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }

    __m512 max_val_vec = _mm512_set1_ps(max_val);
    __m512 sum_vec = _mm512_setzero_ps();
    i = 0;
    for (; i + 15 < size; i += 16) {
        __m512 curr_vec = _mm512_loadu_ps(x + i);
        curr_vec = _mm512_sub_ps(curr_vec, max_val_vec);
        curr_vec = _rawrxd_exp_ps_avx512(curr_vec);  // Portable replacement
        _mm512_storeu_ps(x + i, curr_vec);
        sum_vec = _mm512_add_ps(sum_vec, curr_vec);
    }
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    if (!std::isfinite(sum) || sum <= 0.0f) {
        const float uni = 1.0f / static_cast<float>(size);
        for (int j = 0; j < size; ++j) x[j] = uni;
        return;
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
    if (!x || size <= 0) return;
    if (size == 1) { x[0] = 1.0f; return; }
    for (int i = 0; i < size; ++i) {
        if (!std::isfinite(x[i])) x[i] = -1e9f;
    }
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    if (!std::isfinite(sum) || sum <= 0.0f) {
        const float uni = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; ++i) x[i] = uni;
        return;
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
            float* k_ptr = k ? (k + h * head_dim + i) : nullptr;
            
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
        __m512 exp_vec = _rawrxd_exp_ps_avx512(neg_val_vec);  // Portable replacement
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
    kv_cache_pos.assign(static_cast<size_t>(config.n_layers) * static_cast<size_t>(ctx), -1);
    
    // Precompute RoPE tables if needed (usually just done on fly in kernels)
    printf("[RawrXD] Transformer Initialized. AVX-512 Kernels Linked.\n");
}

std::vector<float> RawrXDTransformer::Forward(const std::vector<uint32_t>& tokens, int start_pos) {
    if (tokens.empty()) return {};

    if (!loader) {
        printf("[Forward] FATAL: loader is null\n");
        return {};
    }

    if (config.dim <= 0 || config.n_layers <= 0 || config.hidden_dim <= 0 || config.n_heads <= 0) {
        printf("[Forward] FATAL: invalid config (dim=%d layers=%d hidden=%d heads=%d)\n",
               config.dim, config.n_layers, config.hidden_dim, config.n_heads);
        return {};
    }

    const int dim = config.dim;
    const int n_heads = config.n_heads;
    if (dim % n_heads != 0) {
        printf("[Forward] FATAL: dim %d not divisible by n_heads %d\n", dim, n_heads);
        return {};
    }

    const int cache_ctx = std::max(1, config.n_ctx > 0 ? config.n_ctx : (config.seq_len > 0 ? config.seq_len : 2048));
    const int max_tokens_per_call = std::max(1, std::min(cache_ctx, config.seq_len > 0 ? config.seq_len : cache_ctx));
    const int T = std::min(static_cast<int>(tokens.size()), max_tokens_per_call);
    if (static_cast<int>(tokens.size()) > T) {
        printf("[Forward] WARN: truncating token batch %zu -> %d\n", tokens.size(), T);
    }
    int64_t current_pos = static_cast<int64_t>(std::max(0, start_pos));

    int n_kv_heads = config.n_kv_heads > 0 ? config.n_kv_heads : n_heads;
    n_kv_heads = std::max(1, std::min(n_kv_heads, n_heads));
    while (n_heads % n_kv_heads != 0 && n_kv_heads > 1) {
        --n_kv_heads;
    }
    if (n_heads % n_kv_heads != 0) {
        printf("[Forward] FATAL: unable to map n_heads=%d to n_kv_heads=%d\n", n_heads, n_kv_heads);
        return {};
    }

    const int head_dim = dim / n_heads;
    if (head_dim <= 0) {
        printf("[Forward] FATAL: invalid head_dim=%d\n", head_dim);
        return {};
    }

    const int kv_dim = n_kv_heads * head_dim;
    const int heads_per_kv = std::max(1, n_heads / n_kv_heads);

    const size_t layers_u = static_cast<size_t>(config.n_layers);
    const size_t ctx_u = static_cast<size_t>(cache_ctx);
    const size_t kv_u = static_cast<size_t>(kv_dim);
    if (layers_u == 0 || ctx_u == 0 || kv_u == 0 ||
        layers_u > (std::numeric_limits<size_t>::max() / ctx_u) ||
        (layers_u * ctx_u) > (std::numeric_limits<size_t>::max() / kv_u)) {
        printf("[Forward] FATAL: KV cache size overflow risk (layers=%zu ctx=%zu kv=%zu)\n",
               layers_u, ctx_u, kv_u);
        return {};
    }
    const size_t expected_cache = layers_u * ctx_u * kv_u;
    if (kv_cache_k.size() < expected_cache || kv_cache_v.size() < expected_cache) {
        printf("[Forward] WARN: KV cache resized (%zu -> %zu)\n", kv_cache_k.size(), expected_cache);
        kv_cache_k.assign(expected_cache, 0.0f);
        kv_cache_v.assign(expected_cache, 0.0f);
        kv_cache_pos.assign(layers_u * ctx_u, -1);
    } else if (kv_cache_pos.size() < (layers_u * ctx_u)) {
        kv_cache_pos.assign(layers_u * ctx_u, -1);
    }
    
    static bool printed_config = false;
    if (!printed_config) {
        printf("[Forward] GQA: dim=%d heads=%d kv_heads=%d head_dim=%d kv_dim=%d hidden=%d\n",
               dim, n_heads, n_kv_heads, head_dim, kv_dim, config.hidden_dim);
        printed_config = true;
    }
    
    // Current hidden state and reusable per-layer buffers.
    std::vector<float> x(dim);
    std::vector<float> residual(dim);
    std::vector<float> q(dim), k(kv_dim), v(kv_dim);
    std::vector<float> att_out(dim), attn_final(dim);
    std::vector<float> h1(config.hidden_dim), h3(config.hidden_dim), final_ffn(dim);
    std::vector<float> scores(cache_ctx);
    std::vector<uint8_t> score_valid(cache_ctx, 0);
    
    for (int t = 0; t < T; t++) {
        uint32_t token = tokens[t];
        
        // 1. Embedding lookup
        float* emb_w = loader->GetTensor("token_embd.weight");
        if (!emb_w) emb_w = loader->GetTensor("model.embed_tokens.weight");
        if (!emb_w) { printf("[Forward] FATAL: Missing token_embd.weight\n"); return {}; }
        
        if (config.vocab_size <= 0) {
            printf("[Forward] FATAL: vocab_size=%d\n", config.vocab_size);
            return {};
        }
        if (token >= static_cast<uint32_t>(config.vocab_size)) {
            printf("[Forward] WARN: token %u >= vocab_size %d, clamping\n", token, config.vocab_size);
            token = static_cast<uint32_t>(config.vocab_size - 1);
        }
        
        memcpy(x.data(), emb_w + static_cast<size_t>(token) * static_cast<size_t>(dim), static_cast<size_t>(dim) * sizeof(float));
        
        // 2. Transformer Layers
        for (int l = 0; l < config.n_layers; l++) {
            residual = x;
            
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
            MatrixMultiply_AVX512(x.data(), wq, q.data(), 1, dim, dim);
            MatrixMultiply_AVX512(x.data(), wk, k.data(), 1, dim, kv_dim);
            MatrixMultiply_AVX512(x.data(), wv, v.data(), 1, dim, kv_dim);
            
            // RoPE — apply separately for Q (n_heads) and K (n_kv_heads)
            RoPE_AVX512(q.data(), nullptr, current_pos + t, head_dim, n_heads);
            RoPE_AVX512(k.data(), nullptr, current_pos + t, head_dim, n_kv_heads);
            
            // KV Cache Update with ring-buffer indexing.
            const int64_t abs_pos = current_pos + static_cast<int64_t>(t);
            const int slot = static_cast<int>(abs_pos % static_cast<int64_t>(cache_ctx));
            const size_t layer_base = static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) * static_cast<size_t>(kv_dim);
            const size_t cache_offset = layer_base + static_cast<size_t>(slot) * static_cast<size_t>(kv_dim);
            memcpy(kv_cache_k.data() + cache_offset, k.data(), static_cast<size_t>(kv_dim) * sizeof(float));
            memcpy(kv_cache_v.data() + cache_offset, v.data(), static_cast<size_t>(kv_dim) * sizeof(float));
            kv_cache_pos[static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) + static_cast<size_t>(slot)] = abs_pos;
            
            // Multi-head attention with GQA
            std::fill(att_out.begin(), att_out.end(), 0.0f);
            const int64_t seq_len_total = abs_pos + 1;
            const int attn_len = static_cast<int>(std::min<int64_t>(seq_len_total, static_cast<int64_t>(cache_ctx)));
            const int64_t window_start = seq_len_total - static_cast<int64_t>(attn_len);
            const float inv_scale = 1.0f / sqrtf(static_cast<float>(head_dim));
            
            for (int h = 0; h < n_heads; h++) {
                const int kv_h = std::min(n_kv_heads - 1, h / heads_per_kv);
                const float* q_head = q.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                int valid_count = 0;

                for (int p = 0; p < attn_len; p++) {
                    const int64_t abs_p = window_start + static_cast<int64_t>(p);
                    const int p_slot = static_cast<int>(abs_p % static_cast<int64_t>(cache_ctx));
                    const size_t pos_idx = static_cast<size_t>(l) * static_cast<size_t>(cache_ctx) + static_cast<size_t>(p_slot);
                    if (kv_cache_pos[pos_idx] != abs_p) {
                        scores[p] = -1e9f;
                        score_valid[p] = 0;
                        continue;
                    }
                    const size_t k_off = layer_base +
                                         static_cast<size_t>(p_slot) * static_cast<size_t>(kv_dim) +
                                         static_cast<size_t>(kv_h) * static_cast<size_t>(head_dim);
                    const float* k_past = kv_cache_k.data() + k_off;
                    float score = DotProduct_AVX512(q_head, k_past, head_dim);
                    float scaled = std::isfinite(score) ? (score * inv_scale) : -1e9f;
                    scores[p] = std::max(-80.0f, std::min(80.0f, scaled));
                    score_valid[p] = 1;
                    ++valid_count;
                }
                if (valid_count == 0) {
                    float* out_head = att_out.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                    std::fill(out_head, out_head + head_dim, 0.0f);
                    continue;
                }
                
                Softmax_AVX512(scores.data(), attn_len);
                for (int p = 0; p < attn_len; ++p) {
                    if (!std::isfinite(scores[p])) scores[p] = 0.0f;
                }
                
                float* out_head = att_out.data() + static_cast<size_t>(h) * static_cast<size_t>(head_dim);
                for (int p = 0; p < attn_len; p++) {
                    if (!score_valid[p]) continue;
                    const int64_t abs_p = window_start + static_cast<int64_t>(p);
                    const int p_slot = static_cast<int>(abs_p % static_cast<int64_t>(cache_ctx));
                    const size_t v_off = layer_base +
                                         static_cast<size_t>(p_slot) * static_cast<size_t>(kv_dim) +
                                         static_cast<size_t>(kv_h) * static_cast<size_t>(head_dim);
                    const float* v_past = kv_cache_v.data() + v_off;
                    VectorAddScaled_AVX512(out_head, v_past, scores[p], head_dim);
                }
            }
            
            // Output projection: dim → dim
            MatrixMultiply_AVX512(att_out.data(), wo, attn_final.data(), 1, dim, dim);
            
            // Residual add
            VectorAdd_AVX512(x.data(), residual.data(), attn_final.data(), dim);
            for (int i = 0; i < dim; ++i) {
                if (!std::isfinite(x[i])) x[i] = 0.0f;
            }
            
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
            MatrixMultiply_AVX512(h1.data(), w2, final_ffn.data(), 1, hdim, dim);
            
            VectorAdd_AVX512(x.data(), residual.data(), final_ffn.data(), dim);
            for (int i = 0; i < dim; ++i) {
                if (!std::isfinite(x[i])) x[i] = 0.0f;
            }
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
    for (int i = 0; i < config.vocab_size; ++i) {
        if (!std::isfinite(logits[i])) logits[i] = -std::numeric_limits<float>::max();
    }
    
    return logits;
}
