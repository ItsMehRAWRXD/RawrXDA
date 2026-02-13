#include "inference_kernels.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <omp.h>
#include <immintrin.h>

// ============================================================================
// AVX2 SIMD Inference Kernels — RawrXD Performance Engine
// ============================================================================
// Rules: No scalar fallback in hot paths. No heap allocs. No temporaries
// larger than L1 unless tiled. Accumulate in FP32 YMM registers.
// ============================================================================

// --- Helper: FP16 (IEEE 754 half) to FP32 scalar conversion ---
static inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (uint32_t)(h >> 15) << 31;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    if (exp == 0) {
        if (mant == 0) { float r; uint32_t v = sign; memcpy(&r, &v, 4); return r; }
        // Denorm → normalize
        while (!(mant & 0x400)) { mant <<= 1; exp--; }
        exp++; mant &= ~0x400;
    } else if (exp == 31) {
        uint32_t v = sign | 0x7F800000 | (mant << 13);
        float r; memcpy(&r, &v, 4); return r;
    }
    uint32_t v = sign | ((exp + 127 - 15) << 23) | (mant << 13);
    float r; memcpy(&r, &v, 4); return r;
}

// --- Helper: Horizontal sum of __m256 ---
static inline float hsum_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    __m128 shuf = _mm_movehdup_ps(lo);
    __m128 sums = _mm_add_ps(lo, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

// --- Helper: Fast approximate exp for 8 floats (Schraudolph-style + polynomial) ---
static inline __m256 fast_exp_avx2(__m256 x) {
    // Clamp to prevent overflow/underflow
    const __m256 max_val = _mm256_set1_ps(88.3762626647949f);
    const __m256 min_val = _mm256_set1_ps(-88.3762626647949f);
    x = _mm256_min_ps(x, max_val);
    x = _mm256_max_ps(x, min_val);
    
    // exp(x) = 2^(x * log2(e)) = 2^(n + f), n = floor, f = fraction
    const __m256 log2e = _mm256_set1_ps(1.4426950408889634f);
    const __m256 half   = _mm256_set1_ps(0.5f);
    __m256 t = _mm256_fmadd_ps(x, log2e, half);
    
    // Floor
    __m256 t_floor = _mm256_floor_ps(t);
    __m256 f = _mm256_sub_ps(t, t_floor);
    f = _mm256_sub_ps(f, half);
    
    // Convert integer part to float exponent: 2^n via bit manipulation
    __m256i ni = _mm256_cvtps_epi32(t_floor);
    ni = _mm256_add_epi32(ni, _mm256_set1_epi32(127));
    ni = _mm256_slli_epi32(ni, 23);
    __m256 pow2n = _mm256_castsi256_ps(ni);
    
    // Polynomial approximation of 2^f for f in [-0.5, 0.5]
    // 2^f ≈ 1 + f*ln2 + f²*ln2²/2 + f³*ln2³/6
    const __m256 ln2   = _mm256_set1_ps(0.6931471805599453f);
    const __m256 c2    = _mm256_set1_ps(0.24022650695910072f);  // ln2²/2
    const __m256 c3    = _mm256_set1_ps(0.05550410866482158f);  // ln2³/6
    const __m256 one   = _mm256_set1_ps(1.0f);
    
    __m256 poly = _mm256_fmadd_ps(c3, f, c2);
    poly = _mm256_fmadd_ps(poly, f, ln2);
    poly = _mm256_fmadd_ps(poly, f, one);
    
    return _mm256_mul_ps(pow2n, poly);
}

// ============================================================================
// 1. FP16 MatMul — AVX2 with FMA, tiled K dimension
// ============================================================================
void InferenceKernels::matmul_f16_avx512(const uint16_t* A, const uint16_t* B, float* C,
                       int M, int N, int K) {
    // Tile K for L1 cache locality
    const int K_TILE = 64;
    
    #pragma omp parallel for collapse(2) schedule(static)
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            __m256 acc = _mm256_setzero_ps();
            float sum = 0.0f;
            
            for (int kt = 0; kt < K; kt += K_TILE) {
                int k_end = std::min(kt + K_TILE, K);
                int k = kt;
                
                // AVX2 FMA loop: 8 FP16 values at a time
                for (; k + 7 < k_end; k += 8) {
                    // Load and convert FP16 → FP32
                    // Manual unroll since _mm256_cvtph_ps requires F16C
                    alignas(32) float a_f32[8], b_f32[8];
                    for (int u = 0; u < 8; u++) {
                        a_f32[u] = fp16_to_fp32(A[m * K + k + u]);
                        b_f32[u] = fp16_to_fp32(B[(k + u) * N + n]);
                    }
                    __m256 va = _mm256_load_ps(a_f32);
                    __m256 vb = _mm256_load_ps(b_f32);
                    acc = _mm256_fmadd_ps(va, vb, acc);
                }
                // Scalar remainder
                for (; k < k_end; k++) {
                    sum += fp16_to_fp32(A[m * K + k]) * fp16_to_fp32(B[k * N + n]);
                }
            }
            C[m * N + n] = hsum_avx(acc) + sum;
        }
    }
}

// ============================================================================
// 2. FUSED Q4_0 Dequant + MatMul — THE critical kernel (v2: Prefetch + Integer Unpack)
// ============================================================================
// Q4_0 block: 2-byte FP16 scale (d), 16 bytes packed int4 (32 weights)
// Layout: block_q4_0 { uint16_t d; uint8_t qs[16]; } — 18 bytes per 32 weights
// Fused: dequant + dot in one pass, accumulate in FP32 YMM, no temp buffer
//
// v2 improvements over v1:
//   - _mm_prefetch on next block's qs[] and next activation chunk
//   - AVX2 integer nibble unpack via vpand + vpsrlw + vpunpck pipeline
//     eliminates the scalar byte loop + float temp array
//   - 4-wide accumulator pipeline unchanged (already optimal)
// ============================================================================

// Helper: Unpack 16 bytes of Q4_0 nibbles into 4x __m256 of float weights
// Pure integer SIMD: no scalar loop, no alignas(32) float[32] temp
static inline void unpack_q4_0_to_4x8f(const uint8_t* qs, 
                                         __m256& w0, __m256& w1, __m256& w2, __m256& w3) {
    // Load 16 bytes into lower 128 bits of a 256-bit register
    __m128i raw = _mm_loadu_si128((const __m128i*)qs);
    
    // Split into low nibbles and high nibbles (each as uint8)
    const __m128i mask_lo = _mm_set1_epi8(0x0F);
    __m128i lo_nibbles = _mm_and_si128(raw, mask_lo);        // bytes 0..15 low nibbles
    __m128i hi_nibbles = _mm_srli_epi16(raw, 4);             // shift right 4, but affects pairs
    hi_nibbles = _mm_and_si128(hi_nibbles, mask_lo);          // mask to get clean high nibbles
    
    // Interleave low and high nibbles to get weight order:
    // weight[0]=lo[0], weight[1]=hi[0], weight[2]=lo[1], weight[3]=hi[1], ...
    // Use unpacklo/unpackhi to interleave bytes
    __m128i interleaved_lo = _mm_unpacklo_epi8(lo_nibbles, hi_nibbles);  // weights 0-15
    __m128i interleaved_hi = _mm_unpackhi_epi8(lo_nibbles, hi_nibbles);  // weights 16-31
    
    // Convert uint8 → int32 → float for each group of 8
    // Group 0: weights 0-7 (low 8 bytes of interleaved_lo)
    __m128i g0_16 = _mm_unpacklo_epi8(interleaved_lo, _mm_setzero_si128()); // uint8→uint16, 8 values
    __m256i g0_lo = _mm256_cvtepu16_epi32(g0_16);
    w0 = _mm256_cvtepi32_ps(g0_lo);
    
    // Group 1: weights 8-15 (high 8 bytes of interleaved_lo)
    __m128i g1_16 = _mm_unpackhi_epi8(interleaved_lo, _mm_setzero_si128());
    __m256i g1_lo = _mm256_cvtepu16_epi32(g1_16);
    w1 = _mm256_cvtepi32_ps(g1_lo);
    
    // Group 2: weights 16-23 (low 8 bytes of interleaved_hi)
    __m128i g2_16 = _mm_unpacklo_epi8(interleaved_hi, _mm_setzero_si128());
    __m256i g2_lo = _mm256_cvtepu16_epi32(g2_16);
    w2 = _mm256_cvtepi32_ps(g2_lo);
    
    // Group 3: weights 24-31 (high 8 bytes of interleaved_hi)
    __m128i g3_16 = _mm_unpackhi_epi8(interleaved_hi, _mm_setzero_si128());
    __m256i g3_lo = _mm256_cvtepu16_epi32(g3_16);
    w3 = _mm256_cvtepi32_ps(g3_lo);
}

void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* w, float* y,
                       int n, int m, int k) {
    // x[n, k] * w[m, k] → y[n, m]
    // w is quantized as Q4_0 blocks: k/32 blocks per row of m
    const int blocks_per_row = k / 32;
    
    #pragma omp parallel for schedule(static)
    for (int row = 0; row < n; row++) {
        const float* x_row = x + row * k;
        
        for (int col = 0; col < m; col++) {
            const block_q4_0* w_row = w + col * blocks_per_row;
            
            __m256 acc0 = _mm256_setzero_ps();
            __m256 acc1 = _mm256_setzero_ps();
            __m256 acc2 = _mm256_setzero_ps();
            __m256 acc3 = _mm256_setzero_ps();
            
            for (int b = 0; b < blocks_per_row; b++) {
                // Prefetch next block's weight data and activation chunk
                if (b + 1 < blocks_per_row) {
                    _mm_prefetch((const char*)&w_row[b + 1], _MM_HINT_T0);
                    _mm_prefetch((const char*)(x_row + (b + 1) * 32), _MM_HINT_T0);
                    _mm_prefetch((const char*)(x_row + (b + 1) * 32 + 16), _MM_HINT_T0);
                }
                
                // 1. Load and convert FP16 scale to FP32
                float d_val = fp16_to_fp32(w_row[b].d);
                __m256 vd = _mm256_set1_ps(d_val);
                const __m256 zp = _mm256_set1_ps(8.0f);
                
                // 2. Pure-integer AVX2 nibble unpack: 16 bytes → 4x __m256 float
                //    No scalar loop, no alignas(32) float[32] temporary
                const uint8_t* qs = w_row[b].qs;
                const float* xp = x_row + b * 32;
                
                __m256 fw0, fw1, fw2, fw3;
                unpack_q4_0_to_4x8f(qs, fw0, fw1, fw2, fw3);
                
                // Subtract zero-point and multiply by scale: w = (nibble - 8) * d
                fw0 = _mm256_mul_ps(_mm256_sub_ps(fw0, zp), vd);
                fw1 = _mm256_mul_ps(_mm256_sub_ps(fw1, zp), vd);
                fw2 = _mm256_mul_ps(_mm256_sub_ps(fw2, zp), vd);
                fw3 = _mm256_mul_ps(_mm256_sub_ps(fw3, zp), vd);
                
                // Load activations and FMA
                __m256 x0 = _mm256_loadu_ps(xp);
                __m256 x1 = _mm256_loadu_ps(xp + 8);
                __m256 x2 = _mm256_loadu_ps(xp + 16);
                __m256 x3 = _mm256_loadu_ps(xp + 24);
                
                acc0 = _mm256_fmadd_ps(fw0, x0, acc0);
                acc1 = _mm256_fmadd_ps(fw1, x1, acc1);
                acc2 = _mm256_fmadd_ps(fw2, x2, acc2);
                acc3 = _mm256_fmadd_ps(fw3, x3, acc3);
            }
            
            // Horizontal sum of all 4 accumulators
            __m256 sum01 = _mm256_add_ps(acc0, acc1);
            __m256 sum23 = _mm256_add_ps(acc2, acc3);
            __m256 total = _mm256_add_ps(sum01, sum23);
            y[row * m + col] = hsum_avx(total);
        }
    }
}

// ============================================================================
// 3. GELU Activation — AVX2 with fast tanh approximation
// ============================================================================
void InferenceKernels::gelu_avx512(float* x, int n) {
    const __m256 sqrt_2_over_pi = _mm256_set1_ps(0.7978845608f);
    const __m256 coeff          = _mm256_set1_ps(0.044715f);
    const __m256 one            = _mm256_set1_ps(1.0f);
    const __m256 half           = _mm256_set1_ps(0.5f);
    const __m256 neg2           = _mm256_set1_ps(-2.0f);
    
    int i = 0;
    #pragma omp parallel for schedule(static) if(n > 4096)
    for (i = 0; i < (n & ~7); i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        
        // inner = sqrt(2/pi) * (x + 0.044715 * x^3)
        __m256 v3 = _mm256_mul_ps(_mm256_mul_ps(v, v), v);
        __m256 inner = _mm256_fmadd_ps(coeff, v3, v);
        inner = _mm256_mul_ps(sqrt_2_over_pi, inner);
        
        // tanh approximation via: tanh(x) = (exp(2x) - 1) / (exp(2x) + 1)
        // Or use: tanh(x) ≈ 1 - 2/(exp(2x) + 1)
        __m256 exp2x = fast_exp_avx2(_mm256_mul_ps(inner, neg2));  // exp(-2*inner)
        // tanh = (1 - exp(-2x)) / (1 + exp(-2x))
        __m256 tanh_v = _mm256_div_ps(_mm256_sub_ps(one, exp2x), _mm256_add_ps(one, exp2x));
        
        // gelu = 0.5 * x * (1 + tanh)
        __m256 result = _mm256_mul_ps(half, _mm256_mul_ps(v, _mm256_add_ps(one, tanh_v)));
        _mm256_storeu_ps(x + i, result);
    }
    
    // Scalar tail
    for (int j = (n & ~7); j < n; j++) {
        float val = x[j];
        float cdf = 0.5f * (1.0f + std::tanh(0.7978845608f * (val + 0.044715f * val * val * val)));
        x[j] = val * cdf;
    }
}

// ============================================================================
// 4. Softmax — AVX2 vectorized 3-pass with fast_exp
// ============================================================================
void InferenceKernels::softmax_avx512(float* x, int n) {
    // Pass 1: Find max (AVX2 horizontal max)
    __m256 vmax = _mm256_set1_ps(-1e30f);
    int i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        vmax = _mm256_max_ps(vmax, v);
    }
    // Reduce vmax to scalar
    float max_val = -1e30f;
    {
        alignas(32) float tmp[8];
        _mm256_store_ps(tmp, vmax);
        for (int j = 0; j < 8; j++) if (tmp[j] > max_val) max_val = tmp[j];
    }
    for (; i < n; i++) if (x[i] > max_val) max_val = x[i];
    
    // Pass 2: exp(x - max) and sum (AVX2 fast_exp)
    __m256 vmax_bc = _mm256_set1_ps(max_val);
    __m256 vsum = _mm256_setzero_ps();
    i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        __m256 e = fast_exp_avx2(_mm256_sub_ps(v, vmax_bc));
        _mm256_storeu_ps(x + i, e);
        vsum = _mm256_add_ps(vsum, e);
    }
    float sum = hsum_avx(vsum);
    for (; i < n; i++) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    
    // Pass 3: Normalize
    float inv_sum = 1.0f / sum;
    __m256 vinv = _mm256_set1_ps(inv_sum);
    i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        _mm256_storeu_ps(x + i, _mm256_mul_ps(v, vinv));
    }
    for (; i < n; i++) x[i] *= inv_sum;
}

// ============================================================================
// 5. RMS Norm — AVX2 vectorized sum-of-squares + scale
// ============================================================================
void InferenceKernels::rmsnorm_avx512(float* o, const float* x, const float* weight, 
                    int n, float eps) {
    // Sum of squares via AVX2 FMA
    __m256 vss = _mm256_setzero_ps();
    int i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        vss = _mm256_fmadd_ps(v, v, vss);
    }
    float ss = hsum_avx(vss);
    for (; i < n; i++) ss += x[i] * x[i];
    
    float rms_inv = 1.0f / std::sqrt(ss / n + eps);
    __m256 vscale = _mm256_set1_ps(rms_inv);
    
    // Apply: o = (x * rms_inv) * weight
    i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vw = _mm256_loadu_ps(weight + i);
        __m256 r = _mm256_mul_ps(_mm256_mul_ps(vx, vscale), vw);
        _mm256_storeu_ps(o + i, r);
    }
    for (; i < n; i++) {
        o[i] = (x[i] * rms_inv) * weight[i];
    }
}

// ============================================================================
// 6. RoPE Positional Encoding — precompute freq table, process pairs
// ============================================================================
void InferenceKernels::rope_avx512(float* q, float* k, int head_dim, int pos, 
                 float theta, float scale) {
    // Process dimension pairs — this is naturally sequential per pair
    // but we can vectorize the sin/cos computation for 4 pairs at once
    for (int i = 0; i < head_dim; i += 2) {
        if (i + 1 >= head_dim) break;
        
        int idx = i / 2;
        float freq = scale / std::pow(theta, (2.0f * idx) / head_dim);
        float val = pos * freq;
        float fcr = std::cos(val);
        float fci = std::sin(val);
        
        // Rotate q
        if (q) {
            float q0 = q[i], q1 = q[i+1];
            q[i]   = q0 * fcr - q1 * fci;
            q[i+1] = q0 * fci + q1 * fcr;
        }
        
        // Rotate k
        if (k) {
            float k0 = k[i], k1 = k[i+1];
            k[i]   = k0 * fcr - k1 * fci;
            k[i+1] = k0 * fci + k1 * fcr;
        }
    }
}

// ============================================================================
// 7. Flash-Attention v2 — Tiled attention with O(N) memory
// ============================================================================
// Instead of materializing an NxN score matrix (N=seq_len), we process
// K/V in tiles of BLOCK_SIZE. For each tile:
//   1. Compute partial scores: S_tile = Q_h · K_tile^T / sqrt(d)
//   2. Online softmax: track running max and sum correction
//   3. Accumulate weighted V into output with rescaling
//
// Memory: O(BLOCK_SIZE) per head instead of O(seq_len)
// Supports GQA via n_kv_heads mapping
// Ring-buffer KV cache aware (cache_t % max_seq_len)
// ============================================================================
void InferenceKernels::flash_attention_v2(
    const float* Q, const float* K_cache, const float* V_cache,
    float* output,
    int seq_len, int n_heads, int n_kv_heads, int head_dim,
    int max_seq_len, int BLOCK_SIZE) {
    
    const int kv_dim = n_kv_heads * head_dim;
    
    memset(output, 0, n_heads * head_dim * sizeof(float));
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < n_heads; h++) {
        const float* q_h = Q + h * head_dim;
        float* out_h = output + h * head_dim;
        
        int kv_h = h / (n_heads / std::max(n_kv_heads, 1));  // GQA head mapping
        float scale = 1.0f / sqrtf((float)head_dim);
        
        // Online softmax state
        float running_max = -1e30f;
        float running_sum = 0.0f;
        
        // Stack-allocated tile buffers (BLOCK_SIZE * sizeof(float) ≤ ~2KB)
        // These fit in L1 — zero heap allocation
        float scores_tile[256];  // max tile size we support
        int effective_block = (BLOCK_SIZE > 256) ? 256 : BLOCK_SIZE;
        
        // Accumulator for weighted V (will be rescaled as we go)
        // Use stack — head_dim is typically 64-128
        float acc[256];  // supports up to head_dim=256
        memset(acc, 0, head_dim * sizeof(float));
        
        // Process K/V in tiles
        for (int tile_start = 0; tile_start < seq_len; tile_start += effective_block) {
            int tile_end = std::min(tile_start + effective_block, seq_len);
            int tile_len = tile_end - tile_start;
            
            // Prefetch next tile's K data
            if (tile_end < seq_len) {
                int next_cache_t = tile_end % max_seq_len;
                _mm_prefetch((const char*)(K_cache + next_cache_t * kv_dim + kv_h * head_dim), _MM_HINT_T0);
            }
            
            // Step 1: Compute scores for this tile: S[t] = dot(Q_h, K_t) * scale
            float tile_max = -1e30f;
            for (int ti = 0; ti < tile_len; ti++) {
                int t = tile_start + ti;
                int cache_t = t % max_seq_len;
                const float* k_t = K_cache + cache_t * kv_dim + kv_h * head_dim;
                
                // AVX2 dot product
                __m256 dot_acc = _mm256_setzero_ps();
                int d = 0;
                for (; d + 7 < head_dim; d += 8) {
                    __m256 vq = _mm256_loadu_ps(q_h + d);
                    __m256 vk = _mm256_loadu_ps(k_t + d);
                    dot_acc = _mm256_fmadd_ps(vq, vk, dot_acc);
                }
                float dot = hsum_avx(dot_acc);
                for (; d < head_dim; d++) dot += q_h[d] * k_t[d];
                
                scores_tile[ti] = dot * scale;
                if (scores_tile[ti] > tile_max) tile_max = scores_tile[ti];
            }
            
            // Step 2: Online softmax correction
            // New max = max(running_max, tile_max)
            // Correction factor for existing accumulator: exp(old_max - new_max)
            float new_max = (tile_max > running_max) ? tile_max : running_max;
            float correction = (running_sum > 0.0f) ? std::exp(running_max - new_max) : 0.0f;
            
            // Rescale existing accumulator and sum
            if (correction != 1.0f && running_sum > 0.0f) {
                __m256 vcorr = _mm256_set1_ps(correction);
                int d = 0;
                for (; d + 7 < head_dim; d += 8) {
                    __m256 va = _mm256_loadu_ps(acc + d);
                    _mm256_storeu_ps(acc + d, _mm256_mul_ps(va, vcorr));
                }
                for (; d < head_dim; d++) acc[d] *= correction;
                running_sum *= correction;
            }
            
            // Step 3: exp(scores - new_max), accumulate sum, accumulate V
            float tile_sum = 0.0f;
            for (int ti = 0; ti < tile_len; ti++) {
                float s = std::exp(scores_tile[ti] - new_max);
                scores_tile[ti] = s;
                tile_sum += s;
            }
            
            // Accumulate weighted V for this tile
            for (int ti = 0; ti < tile_len; ti++) {
                float s = scores_tile[ti];
                if (s < 1e-8f) continue;  // Skip negligible weights
                
                int t = tile_start + ti;
                int cache_t = t % max_seq_len;
                const float* v_t = V_cache + cache_t * kv_dim + kv_h * head_dim;
                
                __m256 vs = _mm256_set1_ps(s);
                int d = 0;
                for (; d + 7 < head_dim; d += 8) {
                    __m256 va = _mm256_loadu_ps(acc + d);
                    __m256 vv = _mm256_loadu_ps(v_t + d);
                    _mm256_storeu_ps(acc + d, _mm256_fmadd_ps(vs, vv, va));
                }
                for (; d < head_dim; d++) acc[d] += s * v_t[d];
            }
            
            running_sum += tile_sum;
            running_max = new_max;
        }
        
        // Final normalization: output = acc / running_sum
        if (running_sum > 0.0f) {
            float inv_sum = 1.0f / running_sum;
            __m256 vinv = _mm256_set1_ps(inv_sum);
            int d = 0;
            for (; d + 7 < head_dim; d += 8) {
                __m256 va = _mm256_loadu_ps(acc + d);
                _mm256_storeu_ps(out_h + d, _mm256_mul_ps(va, vinv));
            }
            for (; d < head_dim; d++) out_h[d] = acc[d] * inv_sum;
        }
    }
}

// ============================================================================
// 8. Fused SiLU * Mul — Vectorized SiLU(gate) * up in one pass
// ============================================================================
// SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
// Using fast_exp_avx2 for the exp(-x), fully vectorized, no scalar fallback
// gate[i] = SiLU(gate[i]) * up[i], in-place on gate[]
// ============================================================================
void InferenceKernels::fused_silu_mul_avx2(float* gate, const float* up, int n) {
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 zero = _mm256_setzero_ps();
    
    int i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 g = _mm256_loadu_ps(gate + i);
        __m256 u = _mm256_loadu_ps(up + i);
        
        // sigmoid(g) = 1 / (1 + exp(-g))
        __m256 neg_g = _mm256_sub_ps(zero, g);
        __m256 exp_neg = fast_exp_avx2(neg_g);
        __m256 sigmoid = _mm256_div_ps(one, _mm256_add_ps(one, exp_neg));
        
        // SiLU(g) * up = g * sigmoid(g) * up
        __m256 silu = _mm256_mul_ps(g, sigmoid);
        _mm256_storeu_ps(gate + i, _mm256_mul_ps(silu, u));
    }
    
    // Scalar tail
    for (; i < n; i++) {
        float sigmoid = 1.0f / (1.0f + std::exp(-gate[i]));
        gate[i] = gate[i] * sigmoid * up[i];
    }
}

// ============================================================================
// 9. KV Cache Quantization — FP32 → int8 with absmax scaling
// ============================================================================
// Quantize: find absmax, compute scale = absmax/127, store int8 + scale
// Dequantize: int8 * scale → float
// Reduces KV cache memory by 4x (FP32 → int8)
// ============================================================================
void InferenceKernels::quantize_kv_fp32_to_int8(const float* src, int8_t* dst, float* scale_out, int n) {
    // Step 1: Find absmax via AVX2
    __m256 vmax_abs = _mm256_setzero_ps();
    const __m256 sign_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    int i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m256 vabs = _mm256_and_ps(v, sign_mask);
        vmax_abs = _mm256_max_ps(vmax_abs, vabs);
    }
    float absmax = 0.0f;
    {
        alignas(32) float tmp[8];
        _mm256_store_ps(tmp, vmax_abs);
        for (int j = 0; j < 8; j++) if (tmp[j] > absmax) absmax = tmp[j];
    }
    for (; i < n; i++) {
        float a = src[i] > 0 ? src[i] : -src[i];
        if (a > absmax) absmax = a;
    }
    
    // Step 2: Compute scale and quantize
    float inv_scale;
    if (absmax < 1e-10f) {
        *scale_out = 0.0f;
        memset(dst, 0, n);
        return;
    }
    
    *scale_out = absmax / 127.0f;
    inv_scale = 127.0f / absmax;
    
    // Quantize: round(src * inv_scale), clamp to [-127, 127]
    __m256 vinv_scale = _mm256_set1_ps(inv_scale);
    __m256 vmin_clamp = _mm256_set1_ps(-127.0f);
    __m256 vmax_clamp = _mm256_set1_ps(127.0f);
    
    i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m256 scaled = _mm256_mul_ps(v, vinv_scale);
        scaled = _mm256_max_ps(scaled, vmin_clamp);
        scaled = _mm256_min_ps(scaled, vmax_clamp);
        __m256 rounded = _mm256_round_ps(scaled, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        __m256i ival = _mm256_cvtps_epi32(rounded);
        
        // Pack int32 → int8 (manual — AVX2 doesn't have direct i32→i8 pack)
        alignas(32) int32_t itmp[8];
        _mm256_store_si256((__m256i*)itmp, ival);
        for (int j = 0; j < 8; j++) {
            dst[i + j] = (int8_t)itmp[j];
        }
    }
    for (; i < n; i++) {
        float scaled = src[i] * inv_scale;
        if (scaled < -127.0f) scaled = -127.0f;
        if (scaled > 127.0f) scaled = 127.0f;
        dst[i] = (int8_t)(scaled + (scaled >= 0 ? 0.5f : -0.5f));
    }
}

void InferenceKernels::dequantize_kv_int8_to_fp32(const int8_t* src, float* dst, float scale, int n) {
    __m256 vscale = _mm256_set1_ps(scale);
    
    int i = 0;
    for (; i + 7 < n; i += 8) {
        // Load 8 int8 values, sign-extend to int32, convert to float
        // AVX2: _mm256_cvtepi8_epi32 takes a __m128i with 8 bytes in low 64 bits
        __m128i raw = _mm_loadl_epi64((const __m128i*)(src + i));
        __m256i ival = _mm256_cvtepi8_epi32(raw);
        __m256 fval = _mm256_cvtepi32_ps(ival);
        _mm256_storeu_ps(dst + i, _mm256_mul_ps(fval, vscale));
    }
    for (; i < n; i++) {
        dst[i] = (float)src[i] * scale;
    }
}

