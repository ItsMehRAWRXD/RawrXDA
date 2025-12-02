// flash_attn_avx2.cc — Tiled Online-Softmax Flash-Attention
// Phase 4: O(n²) → O(n) memory for long-context inference
// Target: ≥10× speedup on seq=4096, head_dim=64

#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// External AVX2 matmul kernel
extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int M, int N, int K);

namespace {

constexpr int TILE = 64;

// Horizontal max across 8 floats in AVX2 register
#ifdef __AVX2__
inline float hmax_ps(__m256 v) {
    __m256 tmp = _mm256_permute2f128_ps(v, v, 1);
    v = _mm256_max_ps(v, tmp);
    tmp = _mm256_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    v = _mm256_max_ps(v, tmp);
    tmp = _mm256_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2));
    v = _mm256_max_ps(v, tmp);
    return _mm256_cvtss_f32(v);
}

// Horizontal sum across 8 floats
inline float hsum_ps(__m256 v) {
    __m256 tmp = _mm256_permute2f128_ps(v, v, 1);
    v = _mm256_add_ps(v, tmp);
    tmp = _mm256_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
    v = _mm256_add_ps(v, tmp);
    tmp = _mm256_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2));
    v = _mm256_add_ps(v, tmp);
    return _mm256_cvtss_f32(v);
}
#endif

// Scalar baseline: materialize full QK^T (O(n²) memory)
void flash_attn_baseline(
    const float* Q, const float* K, const float* V,
    float* O, int seqLen, int headDim
) {
    const float scale = 1.0f / std::sqrtf(static_cast<float>(headDim));
    
    // Allocate QK^T: [seqLen x seqLen]
    float* QK = new float[seqLen * seqLen];
    std::memset(QK, 0, seqLen * seqLen * sizeof(float));
    
    // QK = Q @ K^T / sqrt(d)
    for (int i = 0; i < seqLen; ++i) {
        for (int j = 0; j < seqLen; ++j) {
            float sum = 0.0f;
            for (int d = 0; d < headDim; ++d) {
                sum += Q[i * headDim + d] * K[j * headDim + d];
            }
            QK[i * seqLen + j] = sum * scale;
        }
    }
    
    // Softmax per row
    for (int i = 0; i < seqLen; ++i) {
        float maxVal = -1e30f;
        for (int j = 0; j < seqLen; ++j) {
            maxVal = std::max(maxVal, QK[i * seqLen + j]);
        }
        float sumExp = 0.0f;
        for (int j = 0; j < seqLen; ++j) {
            QK[i * seqLen + j] = std::expf(QK[i * seqLen + j] - maxVal);
            sumExp += QK[i * seqLen + j];
        }
        for (int j = 0; j < seqLen; ++j) {
            QK[i * seqLen + j] /= sumExp;
        }
    }
    
    // O = QK @ V
    std::memset(O, 0, seqLen * headDim * sizeof(float));
    for (int i = 0; i < seqLen; ++i) {
        for (int j = 0; j < seqLen; ++j) {
            float attn = QK[i * seqLen + j];
            for (int d = 0; d < headDim; ++d) {
                O[i * headDim + d] += attn * V[j * headDim + d];
            }
        }
    }
    
    delete[] QK;
}

#ifdef __AVX2__
// Flash-Attention: Tiled online-softmax with AVX2 SIMD
void flash_attn_tiled_avx2(
    const float* Q, const float* K, const float* V,
    float* O, int seqLen, int headDim
) {
    const float scale = 1.0f / std::sqrtf(static_cast<float>(headDim));
    
    alignas(32) float S_tile[TILE * TILE];
    alignas(32) float rowMax[TILE];
    alignas(32) float rowSum[TILE];
    alignas(32) float O_tile[TILE * 128]; // max head_dim=128
    
    for (int q0 = 0; q0 < seqLen; q0 += TILE) {
        int qEnd = std::min(q0 + TILE, seqLen);
        int Bq = qEnd - q0;
        
        // Initialize running stats
        for (int i = 0; i < Bq; ++i) {
            rowMax[i] = -std::numeric_limits<float>::infinity();
            rowSum[i] = 0.0f;
        }
        std::memset(O_tile, 0, Bq * headDim * sizeof(float));
        
        // Causal mask: only process k <= q
        for (int k0 = 0; k0 <= q0; k0 += TILE) {
            int kEnd = std::min(k0 + TILE, seqLen);
            int Bk = kEnd - k0;
            
            // 1. Compute S_tile = Q_tile @ K_tile^T (TILE x TILE)
            std::memset(S_tile, 0, sizeof(S_tile));
            
            // Use external matmul kernel
            alignas(32) float Q_block[TILE * 128];
            alignas(32) float K_block[TILE * 128];
            
            for (int i = 0; i < Bq; ++i) {
                std::memcpy(Q_block + i * headDim, Q + (q0 + i) * headDim, headDim * sizeof(float));
            }
            for (int i = 0; i < Bk; ++i) {
                std::memcpy(K_block + i * headDim, K + (k0 + i) * headDim, headDim * sizeof(float));
            }
            
            // S = Q @ K^T
            for (int i = 0; i < Bq; ++i) {
                for (int j = 0; j < Bk; ++j) {
                    __m256 vsum = _mm256_setzero_ps();
                    int d = 0;
                    for (; d + 8 <= headDim; d += 8) {
                        __m256 vq = _mm256_loadu_ps(Q_block + i * headDim + d);
                        __m256 vk = _mm256_loadu_ps(K_block + j * headDim + d);
                        vsum = _mm256_fmadd_ps(vq, vk, vsum);
                    }
                    float sum = hsum_ps(vsum);
                    for (; d < headDim; ++d) {
                        sum += Q_block[i * headDim + d] * K_block[j * headDim + d];
                    }
                    S_tile[i * TILE + j] = sum * scale;
                }
            }
            
            // 2. Online-softmax update per row
            for (int i = 0; i < Bq; ++i) {
                float oldMax = rowMax[i];
                float newMax = oldMax;
                
                // Find max in this tile row
                for (int j = 0; j < Bk; ++j) {
                    newMax = std::max(newMax, S_tile[i * TILE + j]);
                }
                
                // Rescale previous accumulator
                float correction = std::expf(oldMax - newMax);
                rowSum[i] *= correction;
                for (int d = 0; d < headDim; ++d) {
                    O_tile[i * headDim + d] *= correction;
                }
                rowMax[i] = newMax;
                
                // Add new contributions
                float sumAdd = 0.0f;
                for (int j = 0; j < Bk; ++j) {
                    float expS = std::expf(S_tile[i * TILE + j] - newMax);
                    sumAdd += expS;
                    
                    const float* v_row = V + (k0 + j) * headDim;
                    for (int d = 0; d < headDim; ++d) {
                        O_tile[i * headDim + d] += expS * v_row[d];
                    }
                }
                rowSum[i] += sumAdd;
            }
        }
        
        // 3. Final normalization and write output
        for (int i = 0; i < Bq; ++i) {
            float invSum = 1.0f / rowSum[i];
            for (int d = 0; d < headDim; ++d) {
                O[(q0 + i) * headDim + d] = O_tile[i * headDim + d] * invSum;
            }
        }
    }
}
#endif

} // anonymous namespace

// Public API
extern "C" void flash_attention(
    const float* Q, const float* K, const float* V,
    float* O, int seqLen, int headDim
) {
#ifdef __AVX2__
    flash_attn_tiled_avx2(Q, K, V, O, seqLen, headDim);
#else
    flash_attn_baseline(Q, K, V, O, seqLen, headDim);
#endif
}

extern "C" void attention_baseline(
    const float* Q, const float* K, const float* V,
    float* O, int seqLen, int headDim
) {
    flash_attn_baseline(Q, K, V, O, seqLen, headDim);
}
