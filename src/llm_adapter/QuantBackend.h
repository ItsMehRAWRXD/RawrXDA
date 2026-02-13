#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @brief Quantization backend switcher for GGUFRunner
 * 
 * Provides runtime switching between:
 * - Fallback (pure C++)
 * - Q4_0 (ggml 4-bit quantization)
 * - Q8_0 (ggml 8-bit quantization)
 * - F32 (full precision)
 */

enum class QuantMode {
    FALLBACK,  // Pure C++ matmul (slow, no dependencies)
    Q4_0,      // 4-bit quantized (13GB → 3.5GB RAM)
    Q8_0,      // 8-bit quantized (13GB → 7GB RAM)
    F32        // Full precision (baseline)
};

class QuantBackend {
public:
    static QuantBackend& instance();
    
    // Set quantization mode (returns false if backend unavailable)
    bool setMode(QuantMode mode);
    QuantMode currentMode() const { return mode_; }
    
    // Matrix multiply: C = A @ B (dimensions: N x M @ M x K = N x K)
    void matmul(
        const float* A, 
        const float* B, 
        float* C, 
        int N, int M, int K
    );
    
    // Quantize weights from F32 to current mode
    bool quantizeWeights(
        const float* src, 
        void* dst, 
        size_t count
    );
    
    // Get RAM reduction ratio for current mode
    float getCompressionRatio() const;
    
private:
    QuantBackend();
    ~QuantBackend();
    
    QuantMode mode_ = QuantMode::FALLBACK;
    bool ggmlAvailable_ = false;
    
    // Fallback implementation
    void fallbackMatmul(
        const float* A, 
        const float* B, 
        float* C, 
        int N, int M, int K
    );
};
