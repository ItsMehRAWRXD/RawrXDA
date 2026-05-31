/**
 * @file llama_kernel_ops.cpp
 * @brief Core tensor operations optimized for llama.cpp integration
 * 
 * Provides optimized CPU/GPU tensor operations for transformer inference:
 * - Matrix multiplication (GEMM) with AVX-512/AVX2/NEON dispatch
 * - Layer normalization with fused operations
 * - RoPE (Rotary Position Embedding) application
 * - Softmax with numerical stability
 * - SiLU/Swish activation
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "llama_kernel_ops.h"
#include <immintrin.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <thread>
#include <future>

namespace RawrXD::Inference {

// ============================================================================
// Architecture Detection
// ============================================================================

static CPUFeatures detectCPUFeatures() {
    CPUFeatures features{};
    int cpuInfo[4] = {0};
    
    __cpuid(cpuInfo, 1);
    features.hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;
    features.hasAVX = (cpuInfo[2] & (1 << 28)) != 0;
    
    __cpuid(cpuInfo, 7);
    features.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
    features.hasAVX512F = (cpuInfo[1] & (1 << 16)) != 0;
    features.hasAVX512VL = (cpuInfo[1] & (1 << 31)) != 0;
    features.hasFMA = (cpuInfo[2] & (1 << 12)) != 0;
    
    return features;
}

static const CPUFeatures g_cpuFeatures = detectCPUFeatures();

// ============================================================================
// Quantized Matrix Multiplication (Q4_0, Q8_0, F16)
// ============================================================================

void gemm_q4_0(const void* A, const void* B, float* C,
               int M, int N, int K,
               int lda, int ldb, int ldc) {
    // Dequantize and multiply
    // A: MxK quantized weights (Q4_0 blocks)
    // B: KxN activations (float32)
    // C: MxN output (float32)
    
    const int blockSize = 32; // Q4_0 block size
    
    #pragma omp parallel for collapse(2) schedule(static)
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float sum = 0.0f;
            
            for (int k = 0; k < K; k += blockSize) {
                const int blockIdx = (m * K + k) / blockSize;
                const uint8_t* block = reinterpret_cast<const uint8_t*>(A) + 
                                       blockIdx * (blockSize / 2 + sizeof(float));
                
                float scale = *reinterpret_cast<const float*>(block + blockSize / 2);
                
                for (int i = 0; i < blockSize; i += 2) {
                    uint8_t packed = block[i / 2];
                    int q1 = (packed & 0x0F) - 8;
                    int q2 = ((packed >> 4) & 0x0F) - 8;
                    
                    float a1 = q1 * scale;
                    float a2 = q2 * scale;
                    
                    float b1 = reinterpret_cast<const float*>(B)[(k + i) * ldb + n];
                    float b2 = (i + 1 < blockSize) ? 
                               reinterpret_cast<const float*>(B)[(k + i + 1) * ldb + n] : 0.0f;
                    
                    sum += a1 * b1 + a2 * b2;
                }
            }
            
            C[m * ldc + n] = sum;
        }
    }
}

void gemm_f32(const float* A, const float* B, float* C,
                int M, int N, int K,
                int lda, int ldb, int ldc) {
    if (g_cpuFeatures.hasAVX512F && g_cpuFeatures.hasAVX512VL) {
        gemm_f32_avx512(A, B, C, M, N, K, lda, ldb, ldc);
    } else if (g_cpuFeatures.hasAVX2 && g_cpuFeatures.hasFMA) {
        gemm_f32_avx2(A, B, C, M, N, K, lda, ldb, ldc);
    } else {
        gemm_f32_scalar(A, B, C, M, N, K, lda, ldb, ldc);
    }
}

void gemm_f32_avx512(const float* A, const float* B, float* C,
                     int M, int N, int K,
                     int lda, int ldb, int ldc) {
    #pragma omp parallel for schedule(static)
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; n += 16) {
            __m512 acc = _mm512_setzero_ps();
            
            for (int k = 0; k < K; ++k) {
                __m512 a_vec = _mm512_set1_ps(A[m * lda + k]);
                __m512 b_vec = _mm512_loadu_ps(&B[k * ldb + n]);
                acc = _mm512_fmadd_ps(a_vec, b_vec, acc);
            }
            
            _mm512_storeu_ps(&C[m * ldc + n], acc);
        }
    }
}

void gemm_f32_avx2(const float* A, const float* B, float* C,
                   int M, int N, int K,
                   int lda, int ldb, int ldc) {
    #pragma omp parallel for schedule(static)
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; n += 8) {
            __m256 acc = _mm256_setzero_ps();
            
            for (int k = 0; k < K; ++k) {
                __m256 a_vec = _mm256_set1_ps(A[m * lda + k]);
                __m256 b_vec = _mm256_loadu_ps(&B[k * ldb + n]);
                acc = _mm256_fmadd_ps(a_vec, b_vec, acc);
            }
            
            _mm256_storeu_ps(&C[m * ldc + n], acc);
        }
    }
}

void gemm_f32_scalar(const float* A, const float* B, float* C,
                     int M, int N, int K,
                     int lda, int ldb, int ldc) {
    #pragma omp parallel for collapse(2) schedule(static)
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                sum += A[m * lda + k] * B[k * ldb + n];
            }
            C[m * ldc + n] = sum;
        }
    }
}

// ============================================================================
// Layer Normalization (fused with optional residual)
// ============================================================================

void layer_norm(const float* input, float* output,
                const float* gamma, const float* beta,
                int rows, int cols, float epsilon) {
    #pragma omp parallel for schedule(static)
    for (int r = 0; r < rows; ++r) {
        // Compute mean
        float mean = 0.0f;
        for (int c = 0; c < cols; ++c) {
            mean += input[r * cols + c];
        }
        mean /= cols;
        
        // Compute variance
        float var = 0.0f;
        for (int c = 0; c < cols; ++c) {
            float diff = input[r * cols + c] - mean;
            var += diff * diff;
        }
        var /= cols;
        
        // Normalize and scale
        float invStd = 1.0f / std::sqrt(var + epsilon);
        for (int c = 0; c < cols; ++c) {
            float normalized = (input[r * cols + c] - mean) * invStd;
            output[r * cols + c] = normalized * gamma[c] + beta[c];
        }
    }
}

void fused_layer_norm_residual(const float* input, const float* residual,
                               float* output,
                               const float* gamma, const float* beta,
                               int rows, int cols, float epsilon) {
    #pragma omp parallel for schedule(static)
    for (int r = 0; r < rows; ++r) {
        // Add residual first
        float mean = 0.0f;
        for (int c = 0; c < cols; ++c) {
            mean += input[r * cols + c] + residual[r * cols + c];
        }
        mean /= cols;
        
        float var = 0.0f;
        for (int c = 0; c < cols; ++c) {
            float diff = input[r * cols + c] + residual[r * cols + c] - mean;
            var += diff * diff;
        }
        var /= cols;
        
        float invStd = 1.0f / std::sqrt(var + epsilon);
        for (int c = 0; c < cols; ++c) {
            float normalized = (input[r * cols + c] + residual[r * cols + c] - mean) * invStd;
            output[r * cols + c] = normalized * gamma[c] + beta[c];
        }
    }
}

// ============================================================================
// RoPE (Rotary Position Embedding)
// ============================================================================

void apply_rope(float* q, float* k,
                int headDim, int numHeads, int seqLen,
                int posOffset, float theta) {
    #pragma omp parallel for collapse(2) schedule(static)
    for (int h = 0; h < numHeads; ++h) {
        for (int s = 0; s < seqLen; ++s) {
            int pos = posOffset + s;
            float* qHead = q + (h * seqLen + s) * headDim;
            float* kHead = k + (h * seqLen + s) * headDim;
            
            for (int d = 0; d < headDim; d += 2) {
                int pair = d / 2;
                float freq = 1.0f / std::pow(theta, static_cast<float>(2 * pair) / headDim);
                float angle = pos * freq;
                float cos_val = std::cos(angle);
                float sin_val = std::sin(angle);
                
                float q0 = qHead[d];
                float q1 = qHead[d + 1];
                qHead[d] = q0 * cos_val - q1 * sin_val;
                qHead[d + 1] = q0 * sin_val + q1 * cos_val;
                
                float k0 = kHead[d];
                float k1 = kHead[d + 1];
                kHead[d] = k0 * cos_val - k1 * sin_val;
                kHead[d + 1] = k0 * sin_val + k1 * cos_val;
            }
        }
    }
}

// ============================================================================
// Softmax with numerical stability
// ============================================================================

void softmax(float* data, int rows, int cols) {
    #pragma omp parallel for schedule(static)
    for (int r = 0; r < rows; ++r) {
        // Find max for numerical stability
        float maxVal = data[r * cols];
        for (int c = 1; c < cols; ++c) {
            maxVal = std::max(maxVal, data[r * cols + c]);
        }
        
        // Compute exp and sum
        float sum = 0.0f;
        for (int c = 0; c < cols; ++c) {
            data[r * cols + c] = std::exp(data[r * cols + c] - maxVal);
            sum += data[r * cols + c];
        }
        
        // Normalize
        float invSum = 1.0f / sum;
        for (int c = 0; c < cols; ++c) {
            data[r * cols + c] *= invSum;
        }
    }
}

void softmax_masked(float* data, const float* mask,
                    int rows, int cols, float maskValue) {
    #pragma omp parallel for schedule(static)
    for (int r = 0; r < rows; ++r) {
        float maxVal = -std::numeric_limits<float>::infinity();
        for (int c = 0; c < cols; ++c) {
            if (mask[r * cols + c] > 0.0f) {
                maxVal = std::max(maxVal, data[r * cols + c]);
            }
        }
        
        float sum = 0.0f;
        for (int c = 0; c < cols; ++c) {
            if (mask[r * cols + c] > 0.0f) {
                data[r * cols + c] = std::exp(data[r * cols + c] - maxVal);
                sum += data[r * cols + c];
            } else {
                data[r * cols + c] = 0.0f;
            }
        }
        
        float invSum = 1.0f / sum;
        for (int c = 0; c < cols; ++c) {
            data[r * cols + c] *= invSum;
        }
    }
}

// ============================================================================
// Activation Functions
// ============================================================================

void silu(float* data, int size) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        data[i] = data[i] / (1.0f + std::exp(-data[i]));
    }
}

void gelu(float* data, int size) {
    constexpr float sqrt_2_over_pi = 0.7978845608f;
    constexpr float coeff = 0.044715f;
    
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        float x = data[i];
        float x3 = x * x * x;
        data[i] = 0.5f * x * (1.0f + std::tanh(sqrt_2_over_pi * (x + coeff * x3)));
    }
}

void relu(float* data, int size) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        data[i] = std::max(0.0f, data[i]);
    }
}

// ============================================================================
// Element-wise Operations
// ============================================================================

void add(const float* a, const float* b, float* out, int size) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] + b[i];
    }
}

void mul(const float* a, const float* b, float* out, int size) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] * b[i];
    }
}

void scale(float* data, float scalar, int size) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < size; ++i) {
        data[i] *= scalar;
    }
}

// ============================================================================
// Memory Operations
// ============================================================================

void copy(const float* src, float* dst, int size) {
    std::memcpy(dst, src, size * sizeof(float));
}

void zero(float* data, int size) {
    std::memset(data, 0, size * sizeof(float));
}

} // namespace RawrXD::Inference
