#pragma once
#include <cstdint>
#include <stdexcept>

// C interface for assembly functions
extern "C" {
    // RMSNorm: output = (input / sqrt(mean(input^2) + eps)) * weight
    // Requirements:
    //   - All pointers 64-byte aligned
    //   - dimension % 16 == 0
    //   - dimension > 0
    void Titan_RMSNorm_AVX512(
        float* input,
        float* weight,
        float* output,
        int dimension
    );
    
    // Softmax: output = exp(x - max(x)) / sum(exp(x - max(x)))
    // Requirements:
    //   - Pointer 64-byte aligned
    //   - dimension % 16 == 0
    //   - dimension > 0
    //   - Overwrites input buffer
    void Titan_Softmax_AVX512(
        float* data,
        int dimension
    );
}

// C++ wrapper with validation
class TitanMath {
public:
    static void rmsNorm(
        float* input,
        float* weight,
        float* output,
        int dim
    ) {
        // Validate alignment
        if (reinterpret_cast<uintptr_t>(input) % 64 != 0 ||
            reinterpret_cast<uintptr_t>(weight) % 64 != 0 ||
            reinterpret_cast<uintptr_t>(output) % 64 != 0) {
            throw std::invalid_argument("Titan_RMSNorm_AVX512: pointers must be 64-byte aligned");
        }
        
        // Validate dimension
        if (dim <= 0 || dim % 16 != 0) {
            throw std::invalid_argument("Titan_RMSNorm_AVX512: dimension must be positive and multiple of 16");
        }
        
        Titan_RMSNorm_AVX512(input, weight, output, dim);
    }
    
    static void softmax(
        float* data,
        int dim
    ) {
        // Validate alignment
        if (reinterpret_cast<uintptr_t>(data) % 64 != 0) {
            throw std::invalid_argument("Titan_Softmax_AVX512: pointer must be 64-byte aligned");
        }
        
        // Validate dimension
        if (dim <= 0 || dim % 16 != 0) {
            throw std::invalid_argument("Titan_Softmax_AVX512: dimension must be positive and multiple of 16");
        }
        
        Titan_Softmax_AVX512(data, dim);
    }
};
