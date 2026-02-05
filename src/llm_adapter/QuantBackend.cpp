#include "QuantBackend.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Conditional ggml inclusion
#ifdef HAVE_GGML
#include <ggml.h>
#endif

QuantBackend& QuantBackend::instance() {
    static QuantBackend inst;
    return inst;
}

QuantBackend::QuantBackend() {
#ifdef HAVE_GGML
    ggmlAvailable_ = true;
#else
    ggmlAvailable_ = false;
#endif
}

QuantBackend::~QuantBackend() = default;

bool QuantBackend::setMode(QuantMode mode) {
    // Fallback always available
    if (mode == QuantMode::FALLBACK) {
        mode_ = mode;
        return true;
    }
    
    // Quantized modes require ggml
    if (!ggmlAvailable_) {
        return false;
    }
    
    mode_ = mode;
    return true;
}

void QuantBackend::matmul(
    const float* A, 
    const float* B, 
    float* C, 
    int N, int M, int K
) {
    switch (mode_) {
#ifdef HAVE_GGML
        case QuantMode::Q4_0: {
            // ggml Q4_0 matmul path
            // NOTE: ggml expects row-major layout
            // This is a simplified stub - full implementation needs:
            // 1. Pre-quantized weight buffer
            // 2. ggml context management
            // 3. Proper tensor creation
            
            // For now, fall through to fallback
            // TODO: Implement ggml_mul_mat with Q4_0 tensors
            fallbackMatmul(A, B, C, N, M, K);
            break;
        }
        
        case QuantMode::Q8_0: {
            // ggml Q8_0 matmul path
            // Similar to Q4_0 but 8-bit quantization
            fallbackMatmul(A, B, C, N, M, K);
            break;
        }
        
        case QuantMode::F32: {
            // Full precision ggml path (still faster due to optimized kernels)
            fallbackMatmul(A, B, C, N, M, K);
            break;
        }
#endif
        
        case QuantMode::FALLBACK:
        default:
            fallbackMatmul(A, B, C, N, M, K);
            break;
    }
}

bool QuantBackend::quantizeWeights(
    const float* src, 
    void* dst, 
    size_t count
) {
#ifdef HAVE_GGML
    switch (mode_) {
        case QuantMode::Q4_0: {
            // ggml_quantize_q4_0(src, dst, count, ...);
            // TODO: Implement using ggml quantization API
            return false;
        }
        
        case QuantMode::Q8_0: {
            // ggml_quantize_q8_0(src, dst, count, ...);
            return false;
        }
        
        default:
            return false;
    }
#else
    (void)src;
    (void)dst;
    (void)count;
    return false;
#endif
}

float QuantBackend::getCompressionRatio() const {
    switch (mode_) {
        case QuantMode::Q4_0:
            return 8.0f;  // 32-bit → 4-bit = 8:1
        case QuantMode::Q8_0:
            return 4.0f;  // 32-bit → 8-bit = 4:1
        case QuantMode::F32:
        case QuantMode::FALLBACK:
        default:
            return 1.0f;
    }
}

void QuantBackend::fallbackMatmul(
    const float* A, 
    const float* B, 
    float* C, 
    int N, int M, int K
) {
    // Pure C++ reference implementation
    // C[i,j] = sum_k A[i,k] * B[k,j]
    // A is N x M, B is M x K, C is N x K
    
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < K; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k) {
                sum += A[i * M + k] * B[k * K + j];
            }
            C[i * K + j] = sum;
        }
    }
}
