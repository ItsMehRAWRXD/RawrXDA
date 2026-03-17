#include "cpu_inference_engine.h"
#include "gguf_loader.h"
#include "../plugins/MemoryPlugin.hpp"
#include "inference_kernels.h"
#include "core/execution_scheduler.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <functional>
#include <random>
#include <future> 
#include <cstring>
#include <algorithm>
// #define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <immintrin.h>

namespace RawrXD {

// ============================================================================
// AVX2-Vectorized CPUOps — No scalar fallback in hot paths
// ============================================================================

// --- Helper: Horizontal sum of __m256 ---
static inline float hsum_avx_ops(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    __m128 shuf = _mm_movehdup_ps(lo);
    __m128 sums = _mm_add_ps(lo, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

namespace CPUOps {
    // AVX2 Vector Add: c = a + b
    void VectorAdd(const float* a, const float* b, float* c, int size) {
        int i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            _mm256_storeu_ps(c + i, _mm256_add_ps(va, vb));
        }
        for (; i < size; ++i) c[i] = a[i] + b[i];
    }

    // AVX2 Vector Mul: c = a * b
    void VectorMul(const float* a, const float* b, float* c, int size) {
        int i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            _mm256_storeu_ps(c + i, _mm256_mul_ps(va, vb));
        }
        for (; i < size; ++i) c[i] = a[i] * b[i];
    }

    // AVX2 MatMul: C = A * B. A[m, k], B[k, n] -> C[m, n]
    void MatMul(const float* A, const float* B, float* C, int m, int n, int k) {
        if (n == 1) {
            // Matrix-Vector: AVX2 FMA dot product per row
            #ifdef _OPENMP
            #pragma omp parallel for schedule(static)
            #endif
            for (int i = 0; i < m; ++i) {
                const float* A_row = A + i * k;
                __m256 acc = _mm256_setzero_ps();
                int j = 0;
                for (; j + 7 < k; j += 8) {
                    __m256 va = _mm256_loadu_ps(A_row + j);
                    __m256 vb = _mm256_loadu_ps(B + j);
                    acc = _mm256_fmadd_ps(va, vb, acc);
                }
                float sum = hsum_avx_ops(acc);
                for (; j < k; ++j) sum += A_row[j] * B[j];
                C[i] = sum;
            }
        } else {
            // General Matrix-Matrix with tiling for cache
            const int TILE_K = 64;
            std::memset(C, 0, m * n * sizeof(float));
            
            for (int kt = 0; kt < k; kt += TILE_K) {
                int k_end = std::min(kt + TILE_K, k);
                for (int i = 0; i < m; ++i) {
                    for (int p = kt; p < k_end; ++p) {
                        float valA = A[i * k + p];
                        __m256 va = _mm256_set1_ps(valA);
                        int j = 0;
                        for (; j + 7 < n; j += 8) {
                            __m256 vc = _mm256_loadu_ps(C + i * n + j);
                            __m256 vb = _mm256_loadu_ps(B + p * n + j);
                            _mm256_storeu_ps(C + i * n + j, _mm256_fmadd_ps(va, vb, vc));
                        }
                        for (; j < n; ++j) {
                            C[i * n + j] += valA * B[p * n + j];
                        }
                    }
                }
            }
        }
    }

    // AVX2 Softmax with stability — fully vectorized exp via fast_exp_avx2_shared
    void Softmax(float* data, int size) {
        // Find max
        __m256 vmax = _mm256_set1_ps(-1e30f);
        int i = 0;
        for (; i + 7 < size; i += 8) {
            vmax = _mm256_max_ps(vmax, _mm256_loadu_ps(data + i));
        }
        float max_val = -1e30f;
        {
            alignas(32) float tmp[8];
            _mm256_store_ps(tmp, vmax);
            for (int j = 0; j < 8; j++) if (tmp[j] > max_val) max_val = tmp[j];
        }
        for (; i < size; ++i) if (data[i] > max_val) max_val = data[i];
        
        // Exp and sum — fully vectorized, no scalar-in-SIMD loop
        __m256 vmax_bc = _mm256_set1_ps(max_val);
        __m256 vsum = _mm256_setzero_ps();
        float sum = 0.0f;
        i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 v = _mm256_loadu_ps(data + i);
            __m256 e = fast_exp_avx2_shared(_mm256_sub_ps(v, vmax_bc));
            _mm256_storeu_ps(data + i, e);
            vsum = _mm256_add_ps(vsum, e);
        }
        sum = hsum_avx_ops(vsum);
        for (; i < size; ++i) {
            data[i] = std::exp(data[i] - max_val);
            sum += data[i];
        }
        
        // Normalize
        float inv_sum = 1.0f / sum;
        __m256 vinv = _mm256_set1_ps(inv_sum);
        i = 0;
        for (; i + 7 < size; i += 8) {
            _mm256_storeu_ps(data + i, _mm256_mul_ps(_mm256_loadu_ps(data + i), vinv));
        }
        for (; i < size; ++i) data[i] *= inv_sum;
    }

    // AVX2 RMSNorm
    void RMSNorm(float* data, int size, float epsilon) {
        __m256 vss = _mm256_setzero_ps();
        int i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 v = _mm256_loadu_ps(data + i);
            vss = _mm256_fmadd_ps(v, v, vss);
        }
        float sum_sq = hsum_avx_ops(vss);
        for (; i < size; ++i) sum_sq += data[i] * data[i];
        
        float scale = 1.0f / std::sqrt(sum_sq / size + epsilon);
        __m256 vscale = _mm256_set1_ps(scale);
        i = 0;
        for (; i + 7 < size; i += 8) {
            _mm256_storeu_ps(data + i, _mm256_mul_ps(_mm256_loadu_ps(data + i), vscale));
        }
        for (; i < size; ++i) data[i] *= scale;
    }
    
    // AVX2 LayerNorm
    void LayerNorm(float* data, int size, float epsilon) {
       // Mean
       __m256 vsum = _mm256_setzero_ps();
       int i = 0;
       for (; i + 7 < size; i += 8) {
           vsum = _mm256_add_ps(vsum, _mm256_loadu_ps(data + i));
       }
       float sum = hsum_avx_ops(vsum);
       for (; i < size; ++i) sum += data[i];
       float mean = sum / size;
       
       // Variance
       __m256 vmean = _mm256_set1_ps(mean);
       __m256 vvar = _mm256_setzero_ps();
       i = 0;
       for (; i + 7 < size; i += 8) {
           __m256 diff = _mm256_sub_ps(_mm256_loadu_ps(data + i), vmean);
           vvar = _mm256_fmadd_ps(diff, diff, vvar);
       }
       float var = hsum_avx_ops(vvar);
       for (; i < size; ++i) {
           float diff = data[i] - mean;
           var += diff * diff;
       }
       var /= size;
       float scale = 1.0f / std::sqrt(var + epsilon);
       
       // Normalize
       __m256 vscale = _mm256_set1_ps(scale);
       i = 0;
       for (; i + 7 < size; i += 8) {
           __m256 v = _mm256_sub_ps(_mm256_loadu_ps(data + i), vmean);
           _mm256_storeu_ps(data + i, _mm256_mul_ps(v, vscale));
       }
       for (; i < size; ++i) {
           data[i] = (data[i] - mean) * scale;
       }
    }

    // AVX2 GELU activation — fully vectorized via fast_exp_avx2_shared (no scalar tanh)
    void GELU(float* data, int size) {
        const float SQRT_2_OVER_PI = 0.7978845608f;
        const __m256 vsqrt = _mm256_set1_ps(SQRT_2_OVER_PI);
        const __m256 vcoeff = _mm256_set1_ps(0.044715f);
        const __m256 vhalf = _mm256_set1_ps(0.5f);
        const __m256 vone = _mm256_set1_ps(1.0f);
        const __m256 vneg2 = _mm256_set1_ps(-2.0f);
        
        int i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 x = _mm256_loadu_ps(data + i);
            __m256 x3 = _mm256_mul_ps(_mm256_mul_ps(x, x), x);
            __m256 inner = _mm256_mul_ps(vsqrt, _mm256_fmadd_ps(vcoeff, x3, x));
            // tanh via: tanh(a) = (1 - exp(-2a)) / (1 + exp(-2a))
            __m256 exp_neg2a = fast_exp_avx2_shared(_mm256_mul_ps(inner, vneg2));
            __m256 vtanh = _mm256_div_ps(_mm256_sub_ps(vone, exp_neg2a), 
                                          _mm256_add_ps(vone, exp_neg2a));
            __m256 cdf = _mm256_mul_ps(vhalf, _mm256_add_ps(vone, vtanh));
            _mm256_storeu_ps(data + i, _mm256_mul_ps(x, cdf));
        }
        for (; i < size; ++i) {
            float x = data[i];
            float cdf = 0.5f * (1.0f + std::tanh(SQRT_2_OVER_PI * (x + 0.044715f * x * x * x)));
            data[i] = x * cdf;
        }
    }
    
    // AVX2 SiLU activation: x * sigmoid(x) — fully vectorized via fast_exp_avx2_shared
    void SiLU(float* data, int size) {
        const __m256 vone = _mm256_set1_ps(1.0f);
        const __m256 vzero = _mm256_setzero_ps();
        int i = 0;
        for (; i + 7 < size; i += 8) {
            __m256 x = _mm256_loadu_ps(data + i);
            __m256 neg_x = _mm256_sub_ps(vzero, x);
            // Fully vectorized exp(-x) via polynomial approximation
            __m256 vexp = fast_exp_avx2_shared(neg_x);
            __m256 sigmoid = _mm256_div_ps(vone, _mm256_add_ps(vone, vexp));
            _mm256_storeu_ps(data + i, _mm256_mul_ps(x, sigmoid));
        }
        for (; i < size; ++i) {
            float x = data[i];
            float sigmoid = 1.0f / (1.0f + std::exp(-x));
            data[i] = x * sigmoid;
        }
    }

    void RoPE(float* data, int dim, int pos, int rotary_dim) {
        // Standard RoPE: rotate pairs
        for (int i = 0; i < rotary_dim; i += 2) {
            float theta = std::pow(10000.0f, -static_cast<float>(i) / rotary_dim);
            float m_theta = pos * theta;
            float cos_theta = std::cos(m_theta);
            float sin_theta = std::sin(m_theta);
            
            float x0 = data[i];
            float x1 = data[i+1];
            
            data[i] = x0 * cos_theta - x1 * sin_theta;
            data[i+1] = x0 * sin_theta + x1 * cos_theta;
        }
    }

    // AVX2 DequantizeQ4_0 — process 32 weights per block, vectorized inner loop + prefetch
    void DequantizeQ4_0(const uint8_t* quantized, float* output, int size) {
        const int block_size = 32;
        int num_blocks = size / block_size;
        
        const uint8_t* p_src = quantized;
        float* p_dst = output;
        
        for (int i = 0; i < num_blocks; ++i) {
            // Prefetch next block
            if (i + 1 < num_blocks) {
                _mm_prefetch((const char*)(p_src + 18), _MM_HINT_T0);
            }
            
            // FP16 scale → FP32
            uint16_t d_u16;
            std::memcpy(&d_u16, p_src, 2);
            p_src += 2;
            
            uint32_t sign = (d_u16 >> 15) & 0x1;
            uint32_t exp = (d_u16 >> 10) & 0x1F;
            uint32_t mant = d_u16 & 0x3FF;
            float d_val = 0.0f;
            if (exp != 0 && exp != 31) {
                uint32_t f_val = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
                std::memcpy(&d_val, &f_val, 4);
            } else if (exp == 31) d_val = 65504.0f;

            // AVX2: unpack 16 byte-pairs → 32 floats, subtract zp, mul scale
            __m256 vd = _mm256_set1_ps(d_val);
            __m256 vzp = _mm256_set1_ps(8.0f);
            
            // Unpack all 16 bytes into 32 floats
            alignas(32) float wf[32];
            for (int j = 0; j < 16; ++j) {
                uint8_t qpair = p_src[j];
                wf[j * 2]     = (float)(qpair & 0x0F);
                wf[j * 2 + 1] = (float)((qpair >> 4) & 0x0F);
            }
            p_src += 16;
            
            // 4 AVX2 passes of 8 floats each
            for (int g = 0; g < 4; g++) {
                __m256 w = _mm256_load_ps(wf + g * 8);
                w = _mm256_mul_ps(_mm256_sub_ps(w, vzp), vd);
                _mm256_storeu_ps(p_dst + g * 8, w);
            }
            p_dst += 32;
        }
    }

    // AVX2 DequantizeQ8_0
    void DequantizeQ8_0(const uint8_t* quantized, float* output, int size) {
         const int block_size = 32;
         int num_blocks = size / block_size;
         const uint8_t* p_src = quantized;
         float* p_dst = output;

         for(int i=0; i<num_blocks; ++i) {
             uint16_t d_u16;
             std::memcpy(&d_u16, p_src, 2);
             p_src += 2;
             float d_val = 0.0f;
             uint32_t sign = (d_u16 >> 15) & 0x1;
             uint32_t exp = (d_u16 >> 10) & 0x1F;
             uint32_t mant = d_u16 & 0x3FF;
             if (exp != 0 && exp != 31) {
                 uint32_t f_val = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
                 std::memcpy(&d_val, &f_val, 4);
             }
             
             __m256 vd = _mm256_set1_ps(d_val);
             
             // Process 32 int8 values in 4 groups of 8
             for (int g = 0; g < 4; g++) {
                 alignas(32) float qf[8];
                 for (int j = 0; j < 8; j++) {
                     qf[j] = (float)(int8_t)p_src[g * 8 + j];
                 }
                 __m256 vq = _mm256_load_ps(qf);
                 _mm256_storeu_ps(p_dst + g * 8, _mm256_mul_ps(vq, vd));
             }
             p_src += 32;
             p_dst += 32;
         }
    }
    
    void EnableAVX2(bool) {}
    void EnableMultiThreading(bool) {}
    
    // AVX2 VectorScale
    void VectorScale(float* data, float scale, int size) {
        __m256 vs = _mm256_set1_ps(scale);
        int i = 0;
        for (; i + 7 < size; i += 8) {
            _mm256_storeu_ps(data + i, _mm256_mul_ps(_mm256_loadu_ps(data + i), vs));
        }
        for (; i < size; ++i) data[i] *= scale;
    }

    // ========================================================================
    // FP16 → FP32 helper (used by all K-quant decoders)
    // ========================================================================
    static inline float fp16_to_fp32(uint16_t h) {
        uint32_t sign = (h >> 15) & 0x1;
        uint32_t exp  = (h >> 10) & 0x1F;
        uint32_t mant = h & 0x3FF;
        float val = 0.0f;
        if (exp == 0) {
            // Subnormal or zero
            if (mant != 0) {
                val = std::ldexp((float)mant, -24);
                if (sign) val = -val;
            }
        } else if (exp == 31) {
            val = (mant == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
        } else {
            uint32_t f = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
            std::memcpy(&val, &f, 4);
        }
        return val;
    }

    // ========================================================================
    // K-QUANT SCALE UNPACKING
    // ========================================================================
    // Q4_K and Q5_K use 12 bytes (K_SCALE_SIZE) to pack 8 scales + 8 mins
    // encoded as 6-bit values.  Layout:
    //   bytes 0..3   → low 4 bits of scales[0..7] (nibble pairs)
    //   bytes 4..7   → low 4 bits of mins[0..7]
    //   bytes 8..11  → high 2 bits of scales[0..7] and mins[0..7]
    // ========================================================================
    static void unpack_k_scales(const uint8_t scales_packed[12],
                                 uint8_t scales_out[8], uint8_t mins_out[8]) {
        // Low 4 bits
        for (int i = 0; i < 4; ++i) {
            scales_out[2*i+0] = scales_packed[i] & 0x3F;
            scales_out[2*i+1] = scales_packed[i] >> 4; // Note: only uses bits [7:4], but we need 6 bits total
        }
        // Actually: the canonical llama.cpp encoding is:
        //   scales[i] low 6 bits come from two sources.
        // Let's use the exact ggml reference implementation logic.
        
        // Rewrite with exact ggml logic:
        // bytes 0-3: pairs of 4-bit values for low nibbles of sc and m
        //   sc[j] & 0xF for j=0..7  packed into bytes 0..3
        //   m[j]  & 0xF for j=0..7  packed into bytes 4..7
        //   (sc[j] >> 4) & 3  and (m[j] >> 4) & 3 packed into bytes 8..11
        scales_out[0] = (scales_packed[0] & 0x3F);
        scales_out[1] = (scales_packed[0] >> 6) | ((scales_packed[8] & 0x03) << 2) | ((scales_packed[1] & 0x0F) << 4 & 0); // complex
        
        // This is getting messy. Let's use the EXACT ggml reference decode.
        // From ggml-quants.c dequantize_row_q4_K:
        //
        //   const uint8_t * scales = x[i].scales;
        //   for (int j = 0; j < QK_K/64; ++j) {  // j = 0..3
        //       // 2 sub-blocks per j, 32 elements each
        //       uint8_t sc = scales[j];       // for j < 4: scale  low nibble
        //       uint8_t m  = scales[j + 4];   // for j < 4: min    low nibble
        //       ...
        //   }
        //
        // Actually the 6-bit decode is:
        //   for j=0..3:  sc[2j+0] = scales[j]&0xF | ((scales[j+8]&3)<<4)
        //                sc[2j+1] = scales[j]>>4   | (((scales[j+8]>>2)&3)<<4)
        //                 m[2j+0] = scales[j+4]&0xF | ((scales[j+8+2]&3)<<4)   wait no...
        //
        // The canonical decode from dequantize_row_q4_K in ggml-quants.c:
        //   uint8_t sc, m;
        //   get_scale_min_k4(j, x[i].scales, &sc, &m);
        
        // Let me just inline the get_scale_min_k4 logic exactly.
        // I'll leave this function as a stub and inline the decode in each kernel.
        (void)scales_packed; (void)scales_out; (void)mins_out;
    }

    // ========================================================================
    // get_scale_min_k4 — exact port from ggml-quants.c
    // j = sub-block index (0..7 for Q4_K's 8 sub-blocks of 32)
    // Returns scale and min for that sub-block.
    // ========================================================================
    static inline void get_scale_min_k4(int j, const uint8_t* q, uint8_t* sc, uint8_t* m) {
        if (j < 4) {
            *sc = q[j] & 63;
            *m  = q[j + 4] & 63;
        } else {
            *sc = (q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4);
            *m  = (q[j + 4] >>  4) | ((q[j]     >> 6) << 4);
        }
    }

    // ========================================================================
    // Q4_K DEQUANT — 256 elements per super-block, 144 bytes each
    //   Layout: { fp16 d, fp16 dmin, uint8_t scales[12], uint8_t qs[128] }
    //   8 sub-blocks of 32 weights. Each weight is 4-bit.
    //   weight = d * sc * q - dmin * m
    // ========================================================================
    void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements) {
        const int QK = 256;                    // elements per super-block
        const int BLOCK_BYTES = 144;           // sizeof(block_q4_K)
        int num_blocks = num_elements / QK;

        for (int i = 0; i < num_blocks; ++i) {
            const uint8_t* block = quantized + (size_t)i * BLOCK_BYTES;
            float* dst = output + (size_t)i * QK;

            // Read d and dmin (fp16 at offset 0 and 2)
            uint16_t d_fp16, dmin_fp16;
            std::memcpy(&d_fp16,    block + 0, 2);
            std::memcpy(&dmin_fp16, block + 2, 2);
            float d    = fp16_to_fp32(d_fp16);
            float dmin = fp16_to_fp32(dmin_fp16);

            // Scales at offset 4, 12 bytes
            const uint8_t* scales = block + 4;

            // Quants at offset 16, 128 bytes (256 / 2)
            const uint8_t* qs = block + 16;

            // 8 sub-blocks of 32 weights each
            for (int j = 0; j < QK / 32; ++j) {
                uint8_t sc, m;
                get_scale_min_k4(j, scales, &sc, &m);

                float d_sc    = d    * (float)sc;
                float dmin_m  = dmin * (float)m;

                const uint8_t* qptr = qs + j * 16;  // 16 bytes = 32 nibbles

                // AVX2: broadcast d_sc, dmin_m
                __m256 vd   = _mm256_set1_ps(d_sc);
                __m256 vm   = _mm256_set1_ps(dmin_m);

                // Unpack 16 bytes → 32 floats (low nibble, high nibble)
                alignas(32) float wf[32];
                for (int k = 0; k < 16; ++k) {
                    wf[k]      = (float)(qptr[k] & 0x0F);
                    wf[k + 16] = (float)(qptr[k] >> 4);
                }

                // 4 passes of 8 floats: w = d_sc * q - dmin_m
                for (int g = 0; g < 4; ++g) {
                    __m256 w = _mm256_load_ps(wf + g * 8);
                    w = _mm256_sub_ps(_mm256_mul_ps(w, vd), vm);
                    _mm256_storeu_ps(dst + j * 32 + g * 8, w);
                }
            }
        }
    }

    // ========================================================================
    // Q5_K DEQUANT — 256 elements per super-block, 176 bytes each
    //   Layout: { fp16 d, fp16 dmin, uint8_t scales[12], uint8_t qh[32], uint8_t qs[128] }
    //   8 sub-blocks of 32 weights. Each weight is 5-bit (4 low + 1 high).
    //   weight = d * sc * q - dmin * m
    // ========================================================================
    void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements) {
        const int QK = 256;
        const int BLOCK_BYTES = 176;           // sizeof(block_q5_K)
        int num_blocks = num_elements / QK;

        for (int i = 0; i < num_blocks; ++i) {
            const uint8_t* block = quantized + (size_t)i * BLOCK_BYTES;
            float* dst = output + (size_t)i * QK;

            // fp16 d at offset 0, dmin at offset 2
            uint16_t d_fp16, dmin_fp16;
            std::memcpy(&d_fp16,    block + 0, 2);
            std::memcpy(&dmin_fp16, block + 2, 2);
            float d    = fp16_to_fp32(d_fp16);
            float dmin = fp16_to_fp32(dmin_fp16);

            // scales[12] at offset 4
            const uint8_t* scales = block + 4;
            // qh[32] at offset 16  (high bits)
            const uint8_t* qh = block + 16;
            // qs[128] at offset 48 (low 4 bits)
            const uint8_t* qs = block + 48;

            // 8 sub-blocks of 32 weights
            for (int j = 0; j < QK / 32; ++j) {
                uint8_t sc, m;
                get_scale_min_k4(j, scales, &sc, &m);

                float d_sc   = d    * (float)sc;
                float dmin_m = dmin * (float)m;

                const uint8_t* qptr = qs + j * 16;
                // High bits: qh byte n, bit position within byte
                // For sub-block j, elements 0..31, high bit comes from qh[]

                __m256 vd = _mm256_set1_ps(d_sc);
                __m256 vm = _mm256_set1_ps(dmin_m);

                alignas(32) float wf[32];
                for (int k = 0; k < 16; ++k) {
                    int elem_lo = j * 32 + k;
                    int elem_hi = j * 32 + k + 16;

                    uint8_t q_lo = qptr[k] & 0x0F;
                    uint8_t q_hi = qptr[k] >> 4;

                    // Extract high bit from qh[]
                    uint8_t hbit_lo = (qh[elem_lo / 8] >> (elem_lo % 8)) & 1;
                    uint8_t hbit_hi = (qh[elem_hi / 8] >> (elem_hi % 8)) & 1;

                    wf[k]      = (float)(q_lo | (hbit_lo << 4));
                    wf[k + 16] = (float)(q_hi | (hbit_hi << 4));
                }

                for (int g = 0; g < 4; ++g) {
                    __m256 w = _mm256_load_ps(wf + g * 8);
                    w = _mm256_sub_ps(_mm256_mul_ps(w, vd), vm);
                    _mm256_storeu_ps(dst + j * 32 + g * 8, w);
                }
            }
        }
    }

    // ========================================================================
    // Q6_K DEQUANT — 256 elements per super-block, 210 bytes each
    //   Layout: { uint8_t ql[128], uint8_t qh[64], int8_t scales[16], fp16 d }
    //   16 sub-blocks of 16 elements. Each weight is 6-bit.
    //   weight = d * sc * q  (no min term)
    // ========================================================================
    void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements) {
        const int QK = 256;
        const int BLOCK_BYTES = 210;           // sizeof(block_q6_K)
        int num_blocks = num_elements / QK;

        for (int i = 0; i < num_blocks; ++i) {
            const uint8_t* block = quantized + (size_t)i * BLOCK_BYTES;
            float* dst = output + (size_t)i * QK;

            // ql[128] at offset 0, qh[64] at offset 128, scales[16] at offset 192, d at offset 208
            const uint8_t* ql = block;
            const uint8_t* qh = block + 128;
            const int8_t* sc  = (const int8_t*)(block + 192);

            uint16_t d_fp16;
            std::memcpy(&d_fp16, block + 208, 2);
            float d = fp16_to_fp32(d_fp16);

            // Decode 256 weights
            // ql encodes low 4 bits, qh encodes upper 2 bits
            // 16 sub-blocks of 16 elements each, 16 scales
            for (int j = 0; j < QK / 16; ++j) {
                float scale = d * (float)sc[j];
                __m256 vs = _mm256_set1_ps(scale);

                alignas(32) float wf[16];
                for (int k = 0; k < 16; ++k) {
                    int idx = j * 16 + k;
                    // Low 4 bits from ql
                    uint8_t q4;
                    if (idx < 128) {
                        q4 = ql[idx] & 0x0F;
                    } else {
                        q4 = ql[idx - 128] >> 4;
                    }

                    // High 2 bits from qh
                    int qh_idx = idx / 4;
                    int qh_shift = (idx % 4) * 2;
                    uint8_t q2 = (qh[qh_idx] >> qh_shift) & 0x03;

                    int8_t q = (int8_t)((q4 | (q2 << 4)) - 32);
                    wf[k] = (float)q;
                }

                // 2 passes of 8 floats
                for (int g = 0; g < 2; ++g) {
                    __m256 w = _mm256_load_ps(wf + g * 8);
                    w = _mm256_mul_ps(w, vs);
                    _mm256_storeu_ps(dst + j * 16 + g * 8, w);
                }
            }
        }
    }

    // ========================================================================
    // Q2_K DEQUANT — 256 elements per super-block, 84 bytes each
    //   Layout: { uint8_t scales[16], uint8_t qs[64], fp16 d, fp16 dmin }
    //   16 sub-blocks of 16 elements. Each weight is 2-bit.
    //   weight = d * sc * q - dmin * m
    // ========================================================================
    void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements) {
        const int QK = 256;
        const int BLOCK_BYTES = 84;            // sizeof(block_q2_K)
        int num_blocks = num_elements / QK;

        for (int i = 0; i < num_blocks; ++i) {
            const uint8_t* block = quantized + (size_t)i * BLOCK_BYTES;
            float* dst = output + (size_t)i * QK;

            // scales[16] at offset 0, qs[64] at offset 16, d at offset 80, dmin at offset 82
            const uint8_t* scales = block;
            const uint8_t* qs = block + 16;

            uint16_t d_fp16, dmin_fp16;
            std::memcpy(&d_fp16,    block + 80, 2);
            std::memcpy(&dmin_fp16, block + 82, 2);
            float d    = fp16_to_fp32(d_fp16);
            float dmin = fp16_to_fp32(dmin_fp16);

            // 16 sub-blocks of 16 elements
            for (int j = 0; j < 16; ++j) {
                uint8_t sc_byte = scales[j];
                float scale = d    * (float)(sc_byte & 0x0F);
                float min   = dmin * (float)(sc_byte >> 4);

                __m256 vs = _mm256_set1_ps(scale);
                __m256 vm = _mm256_set1_ps(min);

                // 4 bytes → 16 two-bit values
                alignas(32) float wf[16];
                const uint8_t* qptr = qs + j * 4;
                for (int k = 0; k < 4; ++k) {
                    uint8_t q = qptr[k];
                    wf[k * 4 + 0] = (float)((q >> 0) & 3);
                    wf[k * 4 + 1] = (float)((q >> 2) & 3);
                    wf[k * 4 + 2] = (float)((q >> 4) & 3);
                    wf[k * 4 + 3] = (float)((q >> 6) & 3);
                }

                // 2 passes of 8 floats: w = scale * q - min
                for (int g = 0; g < 2; ++g) {
                    __m256 w = _mm256_load_ps(wf + g * 8);
                    w = _mm256_sub_ps(_mm256_mul_ps(w, vs), vm);
                    _mm256_storeu_ps(dst + j * 16 + g * 8, w);
                }
            }
        }
    }

    // ========================================================================
    // Q3_K DEQUANT — 256 elements per super-block, 110 bytes each
    //   Layout: { uint8_t hmask[32], uint8_t qs[64], uint8_t scales[12], fp16 d }
    //   16 sub-blocks of 16 elements. Each weight is 3-bit (2 low + 1 high).
    //   weight = d * sc * q  (no min term; q is signed)
    // ========================================================================
    void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements) {
        const int QK = 256;
        const int BLOCK_BYTES = 110;           // sizeof(block_q3_K)
        int num_blocks = num_elements / QK;

        for (int i = 0; i < num_blocks; ++i) {
            const uint8_t* block = quantized + (size_t)i * BLOCK_BYTES;
            float* dst = output + (size_t)i * QK;

            // hmask[32] at offset 0, qs[64] at offset 32, scales[12] at offset 96, d at offset 108
            const uint8_t* hmask = block;
            const uint8_t* qs    = block + 32;
            const uint8_t* sc_raw = block + 96;

            uint16_t d_fp16;
            std::memcpy(&d_fp16, block + 108, 2);
            float d = fp16_to_fp32(d_fp16);

            // Decode scales (6-bit packed in 12 bytes → 16 scales)
            int8_t scales[16];
            {
                // Low nibbles from first 6 bytes
                for (int s = 0; s < 6; ++s) {
                    scales[2*s+0] = (int8_t)(sc_raw[s] & 0x0F);
                    scales[2*s+1] = (int8_t)(sc_raw[s] >> 4);
                }
                // High bits from bytes 6..11
                for (int s = 0; s < 4; ++s) {
                    int idx = s + 6;
                    if (idx < 12) {
                        // Scale values: subtract 32 to make signed
                        scales[2*s]     = (int8_t)((scales[2*s]     & 0xF) | ((sc_raw[idx] & 0x03) << 4)) - 32;
                        scales[2*s + 1] = (int8_t)((scales[2*s + 1] & 0xF) | (((sc_raw[idx] >> 2) & 0x03) << 4)) - 32;
                    }
                }
                // For remaining scales
                for (int s = 8; s < 16; ++s) {
                    scales[s] -= 32;
                }
            }

            // Decode 256 weights
            for (int j = 0; j < 16; ++j) {
                float scale = d * (float)scales[j];
                __m256 vs = _mm256_set1_ps(scale);

                alignas(32) float wf[16];
                for (int k = 0; k < 16; ++k) {
                    int idx = j * 16 + k;

                    // 2 low bits from qs (2 bits per element, packed 4 per byte)
                    int qs_byte_idx = idx / 4;
                    int qs_bit_shift = (idx % 4) * 2;
                    uint8_t q2 = (qs[qs_byte_idx] >> qs_bit_shift) & 3;

                    // High bit from hmask
                    uint8_t hbit = (hmask[idx / 8] >> (idx % 8)) & 1;

                    int8_t q = (int8_t)(q2 | (hbit << 2)) - 4;
                    wf[k] = (float)q;
                }

                for (int g = 0; g < 2; ++g) {
                    __m256 w = _mm256_load_ps(wf + g * 8);
                    w = _mm256_mul_ps(w, vs);
                    _mm256_storeu_ps(dst + j * 16 + g * 8, w);
                }
            }
        }
    }

    // ========================================================================
    // F16 DEQUANT — trivial FP16 → FP32 conversion
    // ========================================================================
    void DequantizeF16(const uint8_t* quantized, float* output, int num_elements) {
        const uint16_t* src = (const uint16_t*)quantized;
        int i = 0;
        // AVX2 + F16C: convert 8 fp16 → 8 fp32 at a time
        for (; i + 7 < num_elements; i += 8) {
            __m128i h8 = _mm_loadu_si128((const __m128i*)(src + i));
            __m256 f8 = _mm256_cvtph_ps(h8);
            _mm256_storeu_ps(output + i, f8);
        }
        // Scalar tail
        for (; i < num_elements; ++i) {
            output[i] = fp16_to_fp32(src[i]);
        }
    }
}

// --- Internal Helpers ---
static void DequantizeTensorPtr(const uint8_t* raw_data, size_t raw_size, float* out, size_t count, TensorType type) {
    switch (type) {
        case TensorType::Q4_0:
            CPUOps::DequantizeQ4_0(raw_data, out, (int)count);
            break;
        case TensorType::Q8_0:
            CPUOps::DequantizeQ8_0(raw_data, out, (int)count);
            break;
        case TensorType::Q4_K:
            CPUOps::DequantizeQ4_K(raw_data, out, (int)count);
            break;
        case TensorType::Q5_K:
            CPUOps::DequantizeQ5_K(raw_data, out, (int)count);
            break;
        case TensorType::Q6_K:
            CPUOps::DequantizeQ6_K(raw_data, out, (int)count);
            break;
        case TensorType::Q2_K:
            CPUOps::DequantizeQ2_K(raw_data, out, (int)count);
            break;
        case TensorType::Q3_K:
            CPUOps::DequantizeQ3_K(raw_data, out, (int)count);
            break;
        case TensorType::F16:
            CPUOps::DequantizeF16(raw_data, out, (int)count);
            break;
        case TensorType::F32:
        default:
            // F32 and unknown types: raw copy
            std::memcpy(out, raw_data, count * sizeof(float));
            break;
    }
}

// --- Bytes-per-element for each tensor type (exact, fractional) ---
// Used to compute element count from raw byte size: elems = raw_bytes / bpe
static float BytesPerElement(TensorType type) {
    switch (type) {
        case TensorType::F32:  return 4.0f;
        case TensorType::F16:  return 2.0f;
        case TensorType::Q4_0: return 18.0f / 32.0f;   // 0.5625
        case TensorType::Q4_1: return 20.0f / 32.0f;   // 0.625
        case TensorType::Q5_0: return 22.0f / 32.0f;   // 0.6875
        case TensorType::Q5_1: return 24.0f / 32.0f;   // 0.75
        case TensorType::Q8_0: return 34.0f / 32.0f;   // 1.0625
        case TensorType::Q2_K: return 84.0f / 256.0f;  // 0.328125
        case TensorType::Q3_K: return 110.0f / 256.0f;  // 0.4296875
        case TensorType::Q4_K: return 144.0f / 256.0f;  // 0.5625
        case TensorType::Q5_K: return 176.0f / 256.0f;  // 0.6875
        case TensorType::Q6_K: return 210.0f / 256.0f;  // 0.8203125
        default: return 4.0f;
    }
}

// --- Row size in bytes for a given embedding dimension and type ---
static size_t RowSizeBytes(TensorType type, size_t dim) {
    switch (type) {
        case TensorType::F32:  return dim * 4;
        case TensorType::F16:  return dim * 2;
        case TensorType::Q4_0: return (dim / 32) * 18;
        case TensorType::Q4_1: return (dim / 32) * 20;
        case TensorType::Q5_0: return (dim / 32) * 22;
        case TensorType::Q5_1: return (dim / 32) * 24;
        case TensorType::Q8_0: return (dim / 32) * 34;
        case TensorType::Q2_K: return (dim / 256) * 84;
        case TensorType::Q3_K: return (dim / 256) * 110;
        case TensorType::Q4_K: return (dim / 256) * 144;
        case TensorType::Q5_K: return (dim / 256) * 176;
        case TensorType::Q6_K: return (dim / 256) * 210;
        default: return dim * 4;
    }
}



// --- CPUInferenceEngine Implementation ---

CPUInferenceEngine::CPUInferenceEngine() 
    : m_numLayers(0), m_numHeads(0), m_embeddingDim(0), m_vocabSize(0), 
      m_threadCount(std::thread::hardware_concurrency()), m_modelLoaded(false), m_totalMemoryAllocated(0) {
    m_loader = std::make_unique<GGUFLoader>();
    
    // Explicit Logic: Load Assembly Engine
#ifdef RAWRXD_STATIC_ASM
    std::cout << "[Titan] Linking Static Assembly Engine..." << std::endl;
    fnTitan_Initialize = &Titan_Initialize;
    fnTitan_LoadModel = &Titan_LoadModel;
    fnTitan_RunInferenceStep = &Titan_RunInferenceStep;
    
    fnTitan_Initialize(&m_pTitanContext);
    std::cout << "[Titan] Static Assembly Engine Initialized." << std::endl;
    m_hTitanDLL = nullptr;
#else
    HMODULE hDll = LoadLibraryA("RawrXD_Interconnect.dll");
    
    if (hDll) {
        m_hTitanDLL = (void*)hDll;
        fnTitan_Initialize = (FTitan_Initialize)GetProcAddress(hDll, "Titan_Initialize");
        fnTitan_LoadModel = (FTitan_LoadModel)GetProcAddress(hDll, "Titan_LoadModel");
        fnTitan_RunInferenceStep = (FTitan_RunInferenceStep)GetProcAddress(hDll, "Titan_RunInferenceStep");
        
        if (fnTitan_Initialize) {
            fnTitan_Initialize(&m_pTitanContext);
            std::cout << "[Titan] Assembly Engine Initialized." << std::endl;
        }
    } else {
        // Fallback to internal CPU ops only
        std::cout << "[Titan] RawrXD_Interconnect.dll not found. Using Pure C++ Fallback." << std::endl;
        m_hTitanDLL = nullptr;
    }
#endif

    // Register default memory plugins
    RegisterMemoryPlugin(std::make_shared<RawrXD::StandardMemoryPlugin>());
    RegisterMemoryPlugin(std::make_shared<RawrXD::LargeContextPlugin>());
}

CPUInferenceEngine::~CPUInferenceEngine() {
    ClearCache();
    if(m_hTitanDLL) {
        FreeLibrary((HMODULE)m_hTitanDLL);
    }
}

bool CPUInferenceEngine::LoadModel(const std::string& model_path) {
    // If Titan is active, try loading there first
    if (m_pTitanContext && fnTitan_LoadModel) {
        std::cout << "[Titan] Loading model via Assembly Engine: " << model_path << std::endl;
        fnTitan_LoadModel(m_pTitanContext, model_path.c_str());
    }

    std::cout << "Loading model from: " << model_path << std::endl;
    if (!m_loader->Open(model_path)) return false;
    try {
        if (!m_loader->ParseHeader()) return false;
        if (!m_loader->ParseMetadata()) return false;
    } catch (...) { return false; }
    
    auto meta = m_loader->GetMetadata();
    m_vocabSize = (meta.vocab_size > 0) ? meta.vocab_size : 32000;
    m_embeddingDim = (meta.embedding_dim > 0) ? meta.embedding_dim : 4096;
    m_numLayers = (meta.layer_count > 0) ? meta.layer_count : 32;
    m_numHeads = (meta.head_count > 0) ? meta.head_count : 32;

    m_vocab = m_loader->GetVocabulary();
    if (m_vocab.empty()) {
         for(int i=0; i<m_vocabSize; ++i) m_vocab.push_back("<t" + std::to_string(i) + ">");
    } else {
         m_vocabSize = (int)m_vocab.size();
    }
    
    m_weights.clear();
    auto infos = m_loader->GetTensorInfo();
    for (const auto& info : infos) {
        Tensor t;
        t.type = static_cast<TensorType>(info.type);
        for(auto d : info.shape) t.shape.push_back((int64_t)d);
        m_weights[info.name] = t;
    }
    m_modelLoaded = true;
    return true;
}

// loadModel removed

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>&) { return true; }

size_t CPUInferenceEngine::GetMemoryUsage() const { return m_totalMemoryAllocated; }

void CPUInferenceEngine::ClearCache() {
    m_memoryPool.clear();
    m_kv_cache.clear();
    m_totalMemoryAllocated = 0;
}

// ============================================================================
// Phase 9.2: Execution Scheduler Initialization
// ============================================================================
void CPUInferenceEngine::InitScheduler() {
    if (!m_modelLoaded || m_numLayers == 0) return;
    
    // Create scheduler if not yet created
    if (!m_scheduler) {
        m_scheduler = std::make_unique<ExecutionScheduler>();
    }
    
    // Configure
    SchedulerConfig config;
    config.prefetchAhead = 1;
    config.enableAsyncPrefetch = true;
    config.prefetchTimeoutMs = 5000;
    config.maxPinnedBytes = 4ULL * 1024 * 1024 * 1024;  // 4GB
    config.enableTelemetry = true;
    config.enablePrefetchHinting = true;
    config.computeThreads = m_threadCount;
    m_scheduler->configure(config);
    
    // Bind to this engine (no streaming registry in CPU-only mode)
    StreamingEngineRegistry* registry = nullptr;
    // If we have access to the global registry, use it:
    // registry = &getStreamingEngineRegistry();
    m_scheduler->bind(this, registry);
    
    // Build tensor slot map from m_weights
    std::unordered_map<std::string, TensorSlot> tensorMap;
    for (auto& [name, tensor] : m_weights) {
        TensorSlot slot;
        slot.name = name;
        slot.nameHash = 0; // Will be computed by scheduler
        slot.state = TensorState::Cold;
        slot.layerIndex = -1;
        slot.quantType = static_cast<uint32_t>(tensor.type);
        
        // Compute sizes
        size_t totalElements = 1;
        for (auto dim : tensor.shape) totalElements *= dim;
        slot.dequantBytes = totalElements * sizeof(float);
        slot.sizeBytes = RowSizeBytes(tensor.type, totalElements);
        slot.refCount = 0;
        slot.lastAccessTick = 0;
        slot.lastPrefetchMs = 0;
        slot.lastDequantMs = 0;
        slot.lastExecMs = 0;
        
        // Parse layer index from name (e.g., "blk.5.attn_q.weight" -> 5)
        if (name.substr(0, 4) == "blk.") {
            size_t dotPos = name.find('.', 4);
            if (dotPos != std::string::npos) {
                slot.layerIndex = std::stoi(name.substr(4, dotPos - 4));
            }
        }
        
        tensorMap[name] = slot;
    }
    
    m_scheduler->buildManifests(m_numLayers, m_embeddingDim, tensorMap);
    m_useScheduler = true;
    
    std::cout << "[CPUInferenceEngine] Execution Scheduler initialized for "
              << m_numLayers << " layers" << std::endl;
}

#include "modules/native_memory.hpp"

void CPUInferenceEngine::InitKVCache() {
    // Sync context size with limit if set
    if (m_contextLimit > 0) m_contextSize = m_contextLimit;
    if (m_contextSize == 0) m_contextSize = 2048;

    // Check if we need to resize
    // We strictly assume KVCache is vector of layers
    if (m_kv_cache.size() == m_numLayers) {
        // Check dimension consistency (if context changed?)
        if (!m_kv_cache.empty() && m_kv_cache[0].keys.size() == (size_t)m_contextSize * m_embeddingDim) {
            return; // Already sized correctly
        }
        // If not, clear and realloc
        m_kv_cache.clear();
    }
    
    m_kv_cache.resize(m_numLayers);
    
    // Use memory plugins for large contexts (1M+ tokens)
    size_t layer_elements = (size_t)m_contextSize * m_embeddingDim;
    
    std::cout << "[CPUInferenceEngine] Allocating KV Cache for " << m_contextSize << " tokens..." << std::endl;
    
    // For very large contexts, use memory plugin optimization
    if (m_contextSize > 32768) {
        std::cout << "[CPUInferenceEngine] Using memory plugin optimization for large context" << std::endl;
        
        // Find appropriate memory plugin
        for (auto& plugin : m_memoryPlugins) {
            if (plugin->GetMaxContext() >= m_contextSize) {
                if (plugin->Configure(m_contextSize)) {
                    plugin->Optimize();
                    std::cout << "[CPUInferenceEngine] Active Memory Manager: " << plugin->GetName() << std::endl;
                    break;
                }
            }
        }
    }
    
    for (auto& layer : m_kv_cache) {
        try {
            layer.keys.resize(layer_elements);
            layer.values.resize(layer_elements);
            
            // Fast zero
            memset(layer.keys.data(), 0, layer_elements * sizeof(float));
            memset(layer.values.data(), 0, layer_elements * sizeof(float));
        } catch (const std::bad_alloc& e) {
            std::cerr << "[CPUInferenceEngine] FATAL: Failed to allocate KV cache for context " << m_contextSize << std::endl;
            // Fallback to smaller context
            m_contextSize = 2048;
            layer_elements = (size_t)m_contextSize * m_embeddingDim;
            layer.keys.resize(layer_elements);
            layer.values.resize(layer_elements);
        }
    }
    std::cout << "[CPUInferenceEngine] KV Cache Allocation Complete." << std::endl;
    m_currentPos = 0;
    
    // Initialize execution scheduler if not yet done
    if (!m_scheduler && m_numLayers > 0) {
        InitScheduler();
    }
}

void CPUInferenceEngine::FeedForward(const float* input, float* output, int layer_idx) {
    std::string prefix = "blk." + std::to_string(layer_idx) + ".";
    size_t dim = m_embeddingDim;
    
    auto LoadLoad = [&](const std::string& suffix, std::vector<float>& w_out, TensorType& type_out) -> bool {
         std::string name = prefix + suffix;
         std::vector<uint8_t> raw;
         if (!m_loader->LoadTensorZone(name, raw)) return false;
         
         type_out = TensorType::Q4_0;
         if (m_weights.count(name)) type_out = m_weights[name].type;
         float bpf = BytesPerElement(type_out);
         size_t elems = (size_t)(raw.size() / bpf);
         w_out.resize(elems);
         DequantizeTensor(raw, w_out.data(), elems, type_out);
         return true;
    };
    
    std::vector<float> gate_w, up_w, down_w;
    TensorType gt, ut, dt;
    
    if (!LoadLoad("ffn_gate.weight", gate_w, gt)) return;
    size_t inter_dim = gate_w.size() / dim;
    std::vector<float> gate_out(inter_dim);
    CPUOps::MatMul(gate_w.data(), input, gate_out.data(), inter_dim, 1, dim);
    CPUOps::SiLU(gate_out.data(), inter_dim);
    
    if (LoadLoad("ffn_up.weight", up_w, ut)) {
        std::vector<float> up_out(inter_dim);
        CPUOps::MatMul(up_w.data(), input, up_out.data(), inter_dim, 1, dim);
        CPUOps::VectorMul(gate_out.data(), up_out.data(), gate_out.data(), inter_dim);
    }
    if (LoadLoad("ffn_down.weight", down_w, dt)) {
        CPUOps::MatMul(down_w.data(), gate_out.data(), output, dim, 1, inter_dim);
    }
}

void CPUInferenceEngine::MultiHeadAttention(const float* query, const float* key, const float* value,
                           float* output, int seq_len, int embed_dim, int num_heads, int layer_idx) {
    int head_dim = embed_dim / num_heads;
    std::string prefix = "blk." + std::to_string(layer_idx) + ".";
    
    std::vector<float> q(embed_dim), k(embed_dim), v(embed_dim);
    auto Project = [&](const std::string& suffix, float* out_buf) {
        std::string name = prefix + suffix;
        std::vector<uint8_t> raw;
        if (m_loader->LoadTensorZone(name, raw)) {
            std::vector<float> w(embed_dim * embed_dim); 
            TensorType t = TensorType::Q4_0;
            if (m_weights.count(name)) t = m_weights[name].type;
            DequantizeTensor(raw, w.data(), w.size(), t);
            CPUOps::MatMul(w.data(), query, out_buf, embed_dim, 1, embed_dim);
        }
    };
    
    Project("attn_q.weight", q.data());
    Project("attn_k.weight", k.data());
    Project("attn_v.weight", v.data());
    
    int rotary_dim = head_dim; 
    for(int h=0; h<num_heads; ++h) {
        CPUOps::RoPE(q.data() + h*head_dim, head_dim, m_currentPos, rotary_dim);
        CPUOps::RoPE(k.data() + h*head_dim, head_dim, m_currentPos, rotary_dim);
    }
    
    if (layer_idx < m_kv_cache.size()) {
        auto& cache = m_kv_cache[layer_idx];
        int offset = m_currentPos * embed_dim;
        if (offset + embed_dim <= cache.keys.size()) {
            std::memcpy(cache.keys.data() + offset, k.data(), embed_dim * sizeof(float));
            std::memcpy(cache.values.data() + offset, v.data(), embed_dim * sizeof(float));
        }
    }
    
    std::vector<float> attn_out(embed_dim, 0.0f);
    float scale = 1.0f / std::sqrt((float)head_dim);
    const auto& cache = m_kv_cache[layer_idx];
    int kv_len = m_currentPos + 1; 
    
    for(int h=0; h<num_heads; ++h) {
        int head_off = h * head_dim;
        std::vector<float> scores(kv_len);
        for(int t=0; t<kv_len; ++t) {
            // Prefetch next K entry
            if (t + 1 < kv_len) {
                _mm_prefetch((const char*)(cache.keys.data() + (t+1)*embed_dim + head_off), _MM_HINT_T0);
            }
            
            // AVX2 dot product for score computation
            __m256 dot_acc = _mm256_setzero_ps();
            int i = 0;
            for (; i + 7 < head_dim; i += 8) {
                __m256 vq = _mm256_loadu_ps(q.data() + head_off + i);
                __m256 vk = _mm256_loadu_ps(cache.keys.data() + t*embed_dim + head_off + i);
                dot_acc = _mm256_fmadd_ps(vq, vk, dot_acc);
            }
            float dot = hsum_avx_ops(dot_acc);
            for (; i < head_dim; ++i) {
                dot += q[head_off + i] * cache.keys[t*embed_dim + head_off + i];
            }
            scores[t] = dot * scale;
        }
        CPUOps::Softmax(scores.data(), kv_len);
        float* out_h = attn_out.data() + head_off;
        for(int t=0; t<kv_len; ++t) {
            float s = scores[t];
            if (s < 1e-8f) continue;  // Skip negligible weights
            
            // Prefetch next V entry
            if (t + 1 < kv_len) {
                _mm_prefetch((const char*)(cache.values.data() + (t+1)*embed_dim + head_off), _MM_HINT_T0);
            }
            
            // AVX2 FMA value accumulation
            __m256 vs = _mm256_set1_ps(s);
            int i = 0;
            for (; i + 7 < head_dim; i += 8) {
                __m256 vo = _mm256_loadu_ps(out_h + i);
                __m256 vv = _mm256_loadu_ps(cache.values.data() + t*embed_dim + head_off + i);
                _mm256_storeu_ps(out_h + i, _mm256_fmadd_ps(vs, vv, vo));
            }
            for(int i2=i; i2<head_dim; ++i2) {
                 out_h[i2] += s * cache.values[t*embed_dim + head_off + i2];
            }
        }
    }
    
    {
        std::string name = prefix + "attn_output.weight";
        std::vector<uint8_t> raw;
        if (m_loader->LoadTensorZone(name, raw)) {
            std::vector<float> w(embed_dim * embed_dim);
            TensorType t = m_weights[name].type;
            DequantizeTensor(raw, w.data(), w.size(), t);
            CPUOps::MatMul(w.data(), attn_out.data(), output, embed_dim, 1, embed_dim);
        }
    }
}

void CPUInferenceEngine::TransformerLayer(const float* input, float* output, int layer_idx, int seq_len) {
    std::string prefix = "blk." + std::to_string(layer_idx) + ".";
    
    std::vector<float> norm1(m_embeddingDim);
    std::memcpy(norm1.data(), input, m_embeddingDim * sizeof(float));
    
    auto ApplyNorm = [&](const std::string& suffix, float* data) {
         std::string name = prefix + suffix;
         std::vector<uint8_t> raw;
         if (m_loader->LoadTensorZone(name, raw)) {
             float* w = (float*)raw.data(); 
             CPUOps::RMSNorm(data, m_embeddingDim, 1e-6f);
             CPUOps::VectorMul(data, w, data, m_embeddingDim);
         } else {
             CPUOps::RMSNorm(data, m_embeddingDim, 1e-6f);
         }
    };
    
    ApplyNorm("attn_norm.weight", norm1.data());
    
    std::vector<float> attn_out(m_embeddingDim);
    MultiHeadAttention(norm1.data(), norm1.data(), norm1.data(), attn_out.data(), 
                      seq_len, m_embeddingDim, m_numHeads, layer_idx);
    
    std::vector<float> resid1(m_embeddingDim);
    CPUOps::VectorAdd(input, attn_out.data(), resid1.data(), m_embeddingDim);
    
    std::vector<float> norm2 = resid1;
    ApplyNorm("ffn_norm.weight", norm2.data());
    
    std::vector<float> ffn_out(m_embeddingDim);
    FeedForward(norm2.data(), ffn_out.data(), layer_idx);
    
    CPUOps::VectorAdd(resid1.data(), ffn_out.data(), output, m_embeddingDim);
}

void CPUInferenceEngine::GenerateStreaming(const std::vector<int32_t>& input_tokens,
                                         int max_tokens,
                                         std::function<void(const std::string&)> token_callback,
                                         std::function<void()> complete_callback,
                                         std::function<void(int32_t)> token_id_callback) {
    if (!m_modelLoaded) { if(complete_callback) complete_callback(); return; }
    InitKVCache();
    std::vector<float> state(m_embeddingDim);
    std::vector<float> next_state(m_embeddingDim);
    
    for (size_t i = 0; i < input_tokens.size(); ++i) {
        m_currentPos = i; 
        int32_t token = input_tokens[i];
        std::vector<uint8_t> raw_emb;
        if (m_loader->LoadTensorZone("token_embd.weight", raw_emb)) {
             TensorType type = TensorType::Q4_0;
             if (m_weights.count("token_embd.weight")) {
                 type = m_weights["token_embd.weight"].type;
             }
             size_t row_size = RowSizeBytes(type, m_embeddingDim);
             
             size_t offset = token * row_size;
             if (offset + row_size <= raw_emb.size()) {
                 std::vector<uint8_t> row_data(raw_emb.begin() + offset, raw_emb.begin() + offset + row_size);
                 DequantizeTensor(row_data, state.data(), m_embeddingDim, type);
             }
        }
        
        // --- Layer Execution: Titan ASM > Scheduler > Reference ---
        if (m_pTitanContext && fnTitan_RunInferenceStep) {
            // Fast Path: AVX-512 Titan Assembly Engine
            fnTitan_RunInferenceStep(m_pTitanContext, state.data(), next_state.data());
            state = next_state;
        } else if (m_useScheduler && m_scheduler) {
            // Scheduled Path: Prefetch + Pin + Execute + Release
            m_scheduler->runForwardPass(state.data(), next_state.data(), (int)i);
            // runForwardPass copies result into state via scratch
        } else {
            // Reference Path: Sequential C++
            for (int l = 0; l < m_numLayers; ++l) {
                TransformerLayer(state.data(), next_state.data(), l, 1);
                state = next_state;
            }
        }
    }
    
    int32_t last_token = input_tokens.back();
    for (int step = 0; step < max_tokens; ++step) {
        m_currentPos = input_tokens.size() + step;
        
        {
             std::vector<uint8_t> raw;
             if (m_loader->LoadTensorZone("output_norm.weight", raw)) {
                  CPUOps::RMSNorm(state.data(), m_embeddingDim, 1e-6f);
                  CPUOps::VectorMul(state.data(), (float*)raw.data(), state.data(), m_embeddingDim);
             }
        }
        
        int next_id = 0;
        float max_val = -1e9;
        
        std::vector<uint8_t> raw_out;
        if (m_loader->LoadTensorZone("output.weight", raw_out)) {
             // Efficient ArgMax over Quantized Weights
             // Output weight is [vocab_size, embedding_dim]
             // We need to compute dot(row, state) for each row and find max
             
             TensorType out_type = TensorType::Q4_0; // Default
             if (m_weights.count("output.weight")) out_type = m_weights["output.weight"].type;
             size_t row_size_bytes = RowSizeBytes(out_type, m_embeddingDim);
             
             // Pre-allocate dequant buffer for ONE row
             std::vector<float> row_w(m_embeddingDim);
             
             for(int v=0; v<m_vocabSize; ++v) {
                 size_t offset = v * row_size_bytes;
                 if (offset + row_size_bytes > raw_out.size()) break;
                 
                 // Dequantize specific row only
                 std::vector<uint8_t> row_data(raw_out.begin() + offset, raw_out.begin() + offset + row_size_bytes);
                 DequantizeTensor(row_data, row_w.data(), m_embeddingDim, out_type);
                 
                 // AVX2 Dot Product for output projection
                 __m256 dot_acc = _mm256_setzero_ps();
                 int ki = 0;
                 for (; ki + 7 < m_embeddingDim; ki += 8) {
                     __m256 vs = _mm256_loadu_ps(state.data() + ki);
                     __m256 vw = _mm256_loadu_ps(row_w.data() + ki);
                     dot_acc = _mm256_fmadd_ps(vs, vw, dot_acc);
                 }
                 float dot = hsum_avx_ops(dot_acc);
                 for (; ki < m_embeddingDim; ++ki) {
                     dot += state[ki] * row_w[ki];
                 }
                 
                 if (dot > max_val) {
                     max_val = dot;
                     next_id = v;
                 }
             }
        } else {
            // Fallback debugging
            // std::cerr << "Output weights not found!" << std::endl;
        }
        
        std::string s = (next_id >= 0 && next_id < m_vocab.size()) ? m_vocab[next_id] : "";
        if (token_callback) token_callback(s);
        if (token_id_callback) token_id_callback(next_id);
        if (next_id == 2) break; 
        
        last_token = next_id;
        std::vector<uint8_t> raw_emb;
        if (m_loader->LoadTensorZone("token_embd.weight", raw_emb)) {
             TensorType emb_type = TensorType::Q4_0;
             if (m_weights.count("token_embd.weight")) emb_type = m_weights["token_embd.weight"].type;
             size_t row_size = RowSizeBytes(emb_type, m_embeddingDim);
             size_t offset = last_token * row_size;
             if (offset + row_size <= raw_emb.size()) {
                 std::vector<uint8_t> row(raw_emb.begin() + offset, raw_emb.begin() + offset + row_size);
                 DequantizeTensor(row, state.data(), m_embeddingDim, emb_type);
             }
        }
        
        // --- Layer Execution: Titan ASM > Scheduler > Reference ---
        if (m_pTitanContext && fnTitan_RunInferenceStep) {
             fnTitan_RunInferenceStep(m_pTitanContext, state.data(), next_state.data());
             state = next_state;
        } else if (m_useScheduler && m_scheduler) {
             m_scheduler->runForwardPass(state.data(), next_state.data(), m_currentPos);
        } else {
            for (int l = 0; l < m_numLayers; ++l) {
                TransformerLayer(state.data(), next_state.data(), l, 1);
                state = next_state;
            }
        }
    }
    if (complete_callback) complete_callback();
}

std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text) {
    std::vector<int32_t> tokens;
    size_t i = 0;
    while (i < text.length()) {
        int bestId = -1;
        size_t bestLen = 0;
        for (int id = 0; id < m_vocab.size(); ++id) {
            const std::string& token = m_vocab[id];
            if (token.empty()) continue;
            if (text.compare(i, token.length(), token) == 0) {
                if (token.length() > bestLen) {
                    bestLen = token.length();
                    bestId = id;
                }
            }
        }
        if (bestId != -1) { tokens.push_back(bestId); i += bestLen; }
        else { i++; }
    }
    tokens.insert(tokens.begin(), 1); 
    return tokens;
}

std::vector<int32_t> CPUInferenceEngine::Generate(const std::vector<int32_t>& input_tokens, int max_tokens) {
    std::vector<int32_t> result; 
    // Capturing tokens in callback if we want (not strictly required by interface but good)
    // Note: Callback gives string, not ID. Mapping back is hard.
    // For now we just run the stream.
    GenerateStreaming(input_tokens, max_tokens, nullptr, nullptr, [&](int32_t id) {
        result.push_back(id);
    });
    return result; 
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens) {
    std::string s;
    for (auto t : tokens) if (t >= 0 && t < m_vocab.size()) s += m_vocab[t];
    return s;
}

std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens) {
    if (!m_modelLoaded) return {};
    
    // Ensure cache
    InitKVCache();
    
    std::vector<float> state(m_embeddingDim);
    std::vector<float> next_state(m_embeddingDim);
    
    // Simplified single-pass eval for the whole sequence if needed, 
    // or just the last token. For optimization, we should assume
    // KV cache allows us to only process new tokens.
    // For this explicit implementation, we process the whole sequence 
    // BUT checking m_currentPos vs input size would be optimization.
    // Let's assume we re-process or append. 
    
    // Reset pos for simplicity or handle state carefully
    // In a real engine we'd optimize. Here we ensure correctness.
    m_currentPos = 0; 

    // Run Forward Pass
    for (size_t i = 0; i < input_tokens.size(); ++i) {
        m_currentPos = i; 
        int32_t token = input_tokens[i];
        
        // Load Embedding
        std::vector<uint8_t> raw_emb;
        if (m_loader->LoadTensorZone("token_embd.weight", raw_emb)) {
             TensorType type = TensorType::Q4_0;
             if (m_weights.count("token_embd.weight")) {
                 type = m_weights["token_embd.weight"].type;
             }
             size_t row_size = RowSizeBytes(type, m_embeddingDim);
             size_t offset = token * row_size;
             if (offset + row_size <= raw_emb.size()) {
                 std::vector<uint8_t> row(raw_emb.begin() + offset, raw_emb.begin() + offset + row_size);
                 DequantizeTensor(row, state.data(), m_embeddingDim, type);
             }
        }
        
        // Forward Layers
        for (int l = 0; l < m_numLayers; ++l) {
            TransformerLayer(state.data(), next_state.data(), l, 1);
            state = next_state;
        }
    }
    
    // Calculate Logits from final state
    std::vector<float> logits(m_vocabSize);
    
    // Output Norm
    std::vector<float> norm_out = state;
    std::vector<uint8_t> raw_norm;
    if (m_loader->LoadTensorZone("output_norm.weight", raw_norm)) {
        std::vector<float> w_norm(m_embeddingDim);
        DequantizeTensor(raw_norm, w_norm.data(), w_norm.size(), TensorType::F32); // Usually F32
        CPUOps::RMSNorm(norm_out.data(), m_embeddingDim, 1e-6f);
        CPUOps::VectorMul(norm_out.data(), w_norm.data(), norm_out.data(), m_embeddingDim);
    }
    
    // CAPTURE STATE FOR TRAINING (Real Last-Layer Backprop)
    m_lastState = norm_out;

    // Output Projection
    
    // [AGENTIC] EXPLICIT LOGIC: Check if we have mutable training weights first
    if (!m_outputWeights.data.empty() && m_outputWeights.type == TensorType::F32) {
         float* weights = reinterpret_cast<float*>(m_outputWeights.data.data());
         for(int v=0; v<m_vocabSize; ++v) {
             float* w_row = weights + v * m_embeddingDim;
             float dot = 0.0f;
             // Basic dot product
             for(int k=0; k<m_embeddingDim; ++k) dot += norm_out[k] * w_row[k];
             logits[v] = dot;
         }
         return logits;
    }

    // Fallback to read-only model loader
    std::vector<uint8_t> raw_out;
    if (m_loader->LoadTensorZone("output.weight", raw_out)) {
         // This is big: [vocab, embed]
         // We do a matrix mul: [1, embed] * [embed, vocab] -> [1, vocab]
         // But weights are usually [vocab, embed] in memory
         
         // Explicit implementation:
         // Dequant/Load output weights chunks and dot product.
         // This implementation processes row-by-row to handle large vocabulary matrices 
         // without allocating the full 300MB+ weight matrix in F32.
         
         TensorType type = TensorType::Q4_0;
         if (m_weights.count("output.weight")) {
             type = m_weights["output.weight"].type;
         }

         size_t row_size = RowSizeBytes(type, m_embeddingDim);

         std::vector<float> w_row(m_embeddingDim);
         for(int v=0; v<m_vocabSize; ++v) {
             size_t offset = v * row_size;
             if (offset + row_size <= raw_out.size()) {
                 std::vector<uint8_t> row_data(raw_out.begin() + offset, raw_out.begin() + offset + row_size);
                 DequantizeTensor(row_data, w_row.data(), m_embeddingDim, type);
                 
                 float dot = 0.0f;
                 for(int k=0; k<m_embeddingDim; ++k) dot += norm_out[k] * w_row[k];
                 logits[v] = dot;
             }
         }
    }
    
    return logits;
}

void CPUInferenceEngine::UpdateWeights(const std::vector<std::vector<float>>& layer_gradients, float learning_rate) {
    if (layer_gradients.size() != m_layers.size()) return;

    for (size_t i = 0; i < m_layers.size(); ++i) {
        // Apply gradients to FeedForward1 as a demo of explicit training logic
        // In a full implementation, this would update all learnable parameters (Q/K/V/O, etc.)
        auto& layer = m_layers[i];
        
        // We only support modifying F32 weights for now to ensure correctness and stability
        if (layer.feed_forward1.type == TensorType::F32) {
            float* data = reinterpret_cast<float*>(layer.feed_forward1.data.data());
            size_t count = layer.feed_forward1.data.size() / sizeof(float);
            const auto& grads = layer_gradients[i];
            
            size_t limit = std::min(count, grads.size());
            
            // Explicit weight update Loop (SGD)
            for (size_t k = 0; k < limit; ++k) {
                data[k] -= learning_rate * grads[k];
            }
        }
    }
}

// [AGENTIC] EXPLICIT LOGIC: Implemented UpdateOutputWeights (previously missing)
void CPUInferenceEngine::UpdateOutputWeights(const std::vector<float>& gradients, float learningRate) {
    if (gradients.empty()) return;
    
    // Lazy Load/Init mutable weights for training if not already resident
    if (m_outputWeights.data.empty()) {
        if (!m_loader) return;
        
        // Load original weights from disk
        std::vector<uint8_t> raw_out;
        if (!m_loader->LoadTensorZone("output.weight", raw_out)) return;
        
        TensorType type = TensorType::Q4_0; 
        if (m_weights.count("output.weight")) type = m_weights["output.weight"].type;
        
        // Convert to F32 for training (Upscale quants)
        size_t count = (size_t)m_vocabSize * (size_t)m_embeddingDim;
        m_outputWeights.data.resize(count * sizeof(float)); 
        m_outputWeights.type = TensorType::F32;
        m_outputWeights.shape = { (int64_t)m_vocabSize, (int64_t)m_embeddingDim };
        
        float* f32_data = reinterpret_cast<float*>(m_outputWeights.data.data());
        
        // Dequantize all rows
        size_t row_size_quant = raw_out.size() / m_vocabSize;
        
        // Safety check to avoid out of bounds
        if (row_size_quant == 0) return;

        for (int v = 0; v < m_vocabSize; ++v) {
             size_t offset = v * row_size_quant;
             if (offset + row_size_quant <= raw_out.size()) {
                std::vector<uint8_t> q_row(raw_out.begin() + offset, raw_out.begin() + offset + row_size_quant);
                // Call internal helper
                DequantizeTensor(q_row, f32_data + v * m_embeddingDim, m_embeddingDim, type);
             }
        }
    }
    
    // Apply Gradients: W = W - lr * (grad * input_state^T)
    if (m_lastState.empty() || m_lastState.size() != m_embeddingDim) return;
    
    float* weights = reinterpret_cast<float*>(m_outputWeights.data.data());
    
    // Update Loop
    for (int v = 0; v < m_vocabSize; ++v) {
        if (v >= gradients.size()) break;
        
        float grad_v = gradients[v];
        if (std::abs(grad_v) < 1e-9) continue; 
        
        float* w_row = weights + v * m_embeddingDim;
        for (int k = 0; k < m_embeddingDim; ++k) {
            w_row[k] -= learningRate * grad_v * m_lastState[k];
        }
    }
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<IMemoryPlugin> plugin) {
    if (plugin) {
        m_memoryPlugins.push_back(plugin);
        std::cout << "[CPUInferenceEngine] Registered memory plugin: " << plugin->GetName() << std::endl;
    }
}

void CPUInferenceEngine::SetContextLimit(size_t limit) {
    m_contextLimit = limit;
    std::cout << "[CPUInferenceEngine] Context limit set to " << limit << " tokens." << std::endl;
    
    // Find best plugin
    for (auto& plugin : m_memoryPlugins) {
        if (plugin->GetMaxContext() >= limit) {
            if (plugin->Configure(limit)) {
                plugin->Optimize();
                std::cout << "[CPUInferenceEngine] Active Memory Manager: " << plugin->GetName() << std::endl;
                return;
            }
        }
    }
    std::cout << "[CPUInferenceEngine] Warning: No plugin supports this context size. Using default fallback." << std::endl;
}

void CPUInferenceEngine::SetMaxMode(bool enabled) { 
    m_maxMode = enabled; 
    std::cout << "[CPUInferenceEngine] Max Mode " << (enabled ? "enabled" : "disabled") << std::endl;
    
    if (enabled) {
        SetThreadCount(std::thread::hardware_concurrency());
        if (m_contextLimit < 32768) {
            SetContextLimit(32768);
        }
    }
}

void CPUInferenceEngine::SetDeepThinking(bool enabled) { 
    m_deepThinking = enabled; 
    std::cout << "[CPUInferenceEngine] Deep Thinking " << (enabled ? "enabled" : "disabled") << std::endl;
    
    if (enabled && m_contextLimit < 65536) {
        SetContextLimit(65536);
    }
}

void CPUInferenceEngine::SetDeepResearch(bool enabled) { 
    m_deepResearch = enabled; 
    std::cout << "[CPUInferenceEngine] Deep Research " << (enabled ? "enabled" : "disabled") << std::endl;
    
    if (enabled && m_contextLimit < 1048576) {
        SetContextLimit(1048576);
        for (auto& plugin : m_memoryPlugins) {
            if (plugin->GetMaxContext() >= 1048576) {
                plugin->Configure(1048576);
                plugin->Optimize();
                break;
            }
        }
    }
}

void CPUInferenceEngine::DequantizeTensor(const std::vector<uint8_t>& src, float* dst, size_t size, TensorType type) {
    if (src.empty()) return;
    DequantizeTensorPtr(src.data(), src.size(), dst, size, type);
}

// ============================================================================
// CPUInferenceEngine member methods — forward to CPUOps:: free functions
// These class members are declared in the header and forward to the
// AVX2-vectorized CPUOps implementations above.
// ============================================================================

void CPUInferenceEngine::SetContextSize(size_t size) {
    std::cout << "[CPUInferenceEngine] SetContextSize: " << m_contextLimit << " -> " << size << std::endl;
    SetContextLimit(size);  // Delegate to the primary context management method
}

void CPUInferenceEngine::MatMul(const float* A, const float* B, float* C, int m, int n, int k) {
    CPUOps::MatMul(A, B, C, m, n, k);
}

void CPUInferenceEngine::Softmax(float* data, int size) {
    CPUOps::Softmax(data, size);
}

void CPUInferenceEngine::LayerNorm(float* data, int size, float epsilon) {
    CPUOps::LayerNorm(data, size, epsilon);
}

void CPUInferenceEngine::GELU(float* data, int size) {
    CPUOps::GELU(data, size);
}

void CPUInferenceEngine::RMSNorm(float* data, int size, float epsilon) {
    CPUOps::RMSNorm(data, size, epsilon);
}

void CPUInferenceEngine::RoPE(float* data, int dim, int pos, int rotary_dim) {
    CPUOps::RoPE(data, dim, pos, rotary_dim);
}

float* CPUInferenceEngine::AllocateTensor(size_t size) {
    auto ptr = std::make_unique<float[]>(size);
    float* raw = ptr.get();
    m_totalMemoryAllocated += size * sizeof(float);
    m_memoryPool.push_back(std::move(ptr));
    return raw;
}

void CPUInferenceEngine::DeallocateTensor(float* ptr) {
    if (!ptr) return;
    for (auto it = m_memoryPool.begin(); it != m_memoryPool.end(); ++it) {
        if (it->get() == ptr) {
            m_totalMemoryAllocated -= 0; // Size unknown at dealloc; tracked via pool lifetime
            m_memoryPool.erase(it);
            return;
        }
    }
    std::cerr << "[CPUInferenceEngine] DeallocateTensor: pointer not found in memory pool" << std::endl;
}

} // namespace RawrXD
