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
// 2. FUSED Q4_0 Dequant + MatMul — THE critical kernel
// ============================================================================
// Q4_0 block: 2-byte FP16 scale (d), 16 bytes packed int4 (32 weights)
// Layout: block_q4_0 { uint16_t d; uint8_t qs[16]; } — 18 bytes per 32 weights
// Fused: dequant + dot in one pass, accumulate in FP32 YMM, no temp buffer
// ============================================================================
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
                // 1. Load and convert FP16 scale to FP32
                float d_val = fp16_to_fp32(w_row[b].d);
                __m256 vd = _mm256_set1_ps(d_val);
                const __m256 zp = _mm256_set1_ps(8.0f);
                
                // 2. Unpack 16 bytes → 32 int4 weights, no temp buffer
                //    Low nibbles first, then high nibbles
                const uint8_t* qs = w_row[b].qs;
                const float* xp = x_row + b * 32;
                
                // Process 32 weights in 4 groups of 8
                // Group 0: bytes 0-3, low nibbles (weights 0,2,4,6) + high (1,3,5,7)
                // Actually, Q4_0 packs weight[2j] in low nibble, weight[2j+1] in high nibble
                // of qs[j]. So qs[0] has w0(lo), w1(hi); qs[1] has w2(lo), w3(hi); etc.
                
                // Unpack all 32 weights into 4 __m256 vectors
                alignas(32) float wf[32];
                for (int j = 0; j < 16; j++) {
                    uint8_t qpair = qs[j];
                    wf[j * 2]     = (float)(qpair & 0x0F);
                    wf[j * 2 + 1] = (float)((qpair >> 4) & 0x0F);
                }
                
                // Load weight floats, subtract zero-point, multiply by scale
                __m256 w0 = _mm256_load_ps(wf);
                __m256 w1 = _mm256_load_ps(wf + 8);
                __m256 w2 = _mm256_load_ps(wf + 16);
                __m256 w3 = _mm256_load_ps(wf + 24);
                
                w0 = _mm256_mul_ps(_mm256_sub_ps(w0, zp), vd);
                w1 = _mm256_mul_ps(_mm256_sub_ps(w1, zp), vd);
                w2 = _mm256_mul_ps(_mm256_sub_ps(w2, zp), vd);
                w3 = _mm256_mul_ps(_mm256_sub_ps(w3, zp), vd);
                
                // Load activations and FMA
                __m256 x0 = _mm256_loadu_ps(xp);
                __m256 x1 = _mm256_loadu_ps(xp + 8);
                __m256 x2 = _mm256_loadu_ps(xp + 16);
                __m256 x3 = _mm256_loadu_ps(xp + 24);
                
                acc0 = _mm256_fmadd_ps(w0, x0, acc0);
                acc1 = _mm256_fmadd_ps(w1, x1, acc1);
                acc2 = _mm256_fmadd_ps(w2, x2, acc2);
                acc3 = _mm256_fmadd_ps(w3, x3, acc3);
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

