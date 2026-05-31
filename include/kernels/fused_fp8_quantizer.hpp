// ============================================================================
// fused_fp8_quantizer.hpp - Fused Scale-Clamp-Quantize Kernel
// ============================================================================
// Eliminates intermediate memory roundtrips by fusing operations in registers
//
// Standard pipeline (4 memory roundtrips):
//   load → scale (mul) → store → load → clamp (min) → store → load → quantize → store
//
// Fused pipeline (1 memory roundtrip):
//   load → scale → clamp → quantize → store
//
// Expected gain: 1.3-1.6x throughput reduction from eliminated roundtrips
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace Kernels {

// Fused operation configuration
struct FusedConfig {
    float scale = 1.0f;
    float clampMax = 448.0f;  // E4M3 max
    bool useBankersRounding = true;  // Round to nearest even
    bool prefetchNext = true;        // Prefetch next cache line
};

// Performance metrics for fused kernel
struct FusedMetrics {
    uint64_t totalCalls = 0;
    uint64_t totalElements = 0;
    double avgThroughput = 0.0;
    uint64_t cacheMisses = 0;
    uint64_t prefetchHits = 0;
};

// ============================================================================
// Fused FP8 Quantizer
// Single-pass: scale → clamp → quantize
// ============================================================================
class FusedFP8Quantizer {
public:
    FusedFP8Quantizer();
    ~FusedFP8Quantizer();
    
    // Initialize with configuration
    bool Initialize(const FusedConfig& config = FusedConfig{});
    
    // Fused quantization
    // Input:  float array (32-byte aligned)
    // Output: uint8_t array (16-byte aligned)
    void Quantize(const float* input, uint8_t* output, size_t count);
    
    // Quantize with explicit scale override
    void QuantizeWithScale(const float* input, uint8_t* output, 
                           size_t count, float scale);
    
    // Get performance metrics
    FusedMetrics GetMetrics() const { return metrics_; }
    void ResetMetrics() { metrics_ = FusedMetrics{}; }

private:
    FusedConfig config_;
    FusedMetrics metrics_;
    bool initialized_ = false;
    
    // Implementation variants
    void QuantizeScalarFused(const float* input, uint8_t* output, size_t count);
    void QuantizeAVX2Fused(const float* input, uint8_t* output, size_t count);
    void QuantizeAVX512Fused(const float* input, uint8_t* output, size_t count);
};

// ============================================================================
// C API for MASM interop
// ============================================================================
extern "C" {
    // Fused kernel: scale → clamp → quantize in one pass
    void SovereignFusedQuantizeE4M3(const float* input, uint8_t* output, 
                                     size_t count, float scale, float clampMax);
    
    // Stream-optimized version with prefetching
    void SovereignFusedQuantizeStream(const float* input, uint8_t* output,
                                       size_t count, float scale, float clampMax);
}

// ============================================================================
// Convenience functions
// ============================================================================

// One-shot fused quantization
inline void QuantizeFP8Fused(const float* input, uint8_t* output, 
                              size_t count, float scale = 1.0f) {
    static FusedFP8Quantizer quantizer;
    static bool initialized = false;
    if (!initialized) {
        FusedConfig config;
        config.scale = scale;
        quantizer.Initialize(config);
        initialized = true;
    }
    quantizer.QuantizeWithScale(input, output, count, scale);
}

// Benchmark: Compare fused vs non-fused
void BenchmarkFusedVsUnfused(size_t elementCount = 1000000);

} // namespace Kernels
} // namespace RawrXD
