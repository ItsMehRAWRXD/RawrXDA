#pragma once
// Fused Decode + FP8 Interface
// Eliminates intermediate buffers for maximum memory efficiency

#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace Kernels {

extern "C" {
    // Fused decode + FP8 quantization
    // Input: float array (decoded tokens)
    // Output: uint8_t array (FP8 quantized)
    // Processes 8 floats per iteration with prefetching
    void SovereignFusedDecodeFP8_AVX2(
        const float* decoded_tokens,
        uint8_t* output_fp8,
        size_t count,
        float scale
    );
    
    // Cache-optimized streaming pipeline
    // Processes in 64-token chunks with aggressive prefetching
    void SovereignStreamingFP8Pipeline_AVX2(
        const float* input,
        uint8_t* output,
        size_t count,
        float scale
    );
}

// C++ wrapper for fused kernel
class FusedDecodeFP8Processor {
public:
    static constexpr size_t kOptimalChunkSize = 64;  // One cache line of output
    
    // Process decoded tokens directly to FP8
    // No intermediate storage - minimal memory bandwidth
    static void Process(
        const float* __restrict decoded,
        uint8_t* __restrict output,
        size_t count,
        float scale = 1.0f
    ) {
        SovereignFusedDecodeFP8_AVX2(decoded, output, count, scale);
    }
    
    // Streaming version with cache optimization
    static void ProcessStreaming(
        const float* __restrict decoded,
        uint8_t* __restrict output,
        size_t count,
        float scale = 1.0f
    ) {
        SovereignStreamingFP8Pipeline_AVX2(decoded, output, count, scale);
    }
    
    // Get optimal batch size for cache efficiency
    static constexpr size_t GetOptimalChunkSize() { return kOptimalChunkSize; }
};

} // namespace Kernels
} // namespace RawrXD
