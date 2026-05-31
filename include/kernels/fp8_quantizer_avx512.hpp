// ============================================================================
// fp8_quantizer_avx512.hpp - AVX-512 FP8 Quantization Interface
// ============================================================================
// Drop-in replacement for scalar/AVX2 FP8 quantization
// Automatically dispatches to AVX-512 when available
//
// Usage:
//   FP8QuantizerAVX512 quantizer;
//   if (quantizer.Initialize()) {
//       quantizer.Quantize(input, output, count, scale);
//   }
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace Kernels {

// CPU feature detection result
struct CPUFeatures {
    bool has_avx512f = false;
    bool has_avx512vl = false;
    bool has_avx512bw = false;
    bool has_avx512dq = false;
    bool has_avx2 = false;
    
    void Detect();
    bool HasAVX512() const { return has_avx512f && has_avx512vl && has_avx512bw; }
    bool HasAVX2() const { return has_avx2; }
};

// Quantization strategy
enum class QuantizeStrategy {
    Auto,       // Dispatch based on CPU features
    AVX512,     // Force AVX-512 (16-wide)
    AVX2,       // Force AVX2 (8-wide)
    Scalar      // Force scalar
};

// Performance metrics
struct QuantizeMetrics {
    uint64_t totalCalls = 0;
    uint64_t avx512Calls = 0;
    uint64_t avx2Calls = 0;
    uint64_t scalarCalls = 0;
    uint64_t totalElements = 0;
    double avgThroughput = 0.0;  // elements/second
    
    void RecordCall(QuantizeStrategy strategy, size_t elements, uint64_t nanoseconds);
    void PrintReport() const;
};

// ============================================================================
// FP8 Quantizer with automatic dispatch
// ============================================================================
class FP8QuantizerAVX512 {
public:
    FP8QuantizerAVX512();
    ~FP8QuantizerAVX512();
    
    // Initialize with strategy (default: Auto)
    bool Initialize(QuantizeStrategy strategy = QuantizeStrategy::Auto);
    void Shutdown();
    
    // Suppress diagnostic output (for TUI/dashboard use)
    void SetSilent(bool silent) { silent_ = silent; }
    
    // Quantize float array to E4M3 FP8
    // Input:  float array (32-byte aligned for AVX-512)
    // Output: uint8_t array (16-byte aligned)
    // Count:  number of elements (will be rounded up to multiple of 16 for AVX-512)
    // Scale:  quantization scale factor
    void Quantize(const float* input, uint8_t* output, size_t count, float scale);
    
    // Quantize with explicit strategy override
    void QuantizeWithStrategy(const float* input, uint8_t* output, 
                               size_t count, float scale, 
                               QuantizeStrategy strategy);
    
    // Get current strategy being used
    QuantizeStrategy GetCurrentStrategy() const { return currentStrategy_; }
    
    // Get CPU features
    const CPUFeatures& GetCPUFeatures() const { return features_; }
    
    // Get performance metrics
    QuantizeMetrics GetMetrics() const { return metrics_; }
    void ResetMetrics() { metrics_ = QuantizeMetrics{}; }
    void PrintReport() const;
    
    // Check if AVX-512 is available
    static bool IsAVX512Available();
    
    // Get optimal batch size for current strategy
    size_t GetOptimalBatchSize() const;

private:
    bool initialized_ = false;
    bool silent_ = false;
    QuantizeStrategy requestedStrategy_ = QuantizeStrategy::Auto;
    QuantizeStrategy currentStrategy_ = QuantizeStrategy::Scalar;
    CPUFeatures features_;
    QuantizeMetrics metrics_;
    
    // Function pointers for each implementation
    using QuantizeFn = void (*)(const float* input, uint8_t* output, size_t count, float scale);
    QuantizeFn quantizeFn_ = nullptr;
    
    void SelectStrategy();
    void UpdateMetrics(QuantizeStrategy used, size_t elements, uint64_t nanoseconds);
};

// ============================================================================
// C API for MASM interop
// ============================================================================

extern "C" {
    // AVX-512 kernel (MASM)
    void SovereignQuantizeE4M3_AVX512(float* input, uint8_t* output, size_t count, float scale);
    void SovereignQuantizeE4M3_AVX512_Unrolled(float* input, uint8_t* output, size_t count, float scale);
    
    // CPU detection
    int Sovereign_HasAVX512F();
    
    // Dispatch wrapper
    void SovereignQuantizeE4M3_Dispatch(float* input, uint8_t* output, size_t count, float scale);
}

// ============================================================================
// Convenience functions
// ============================================================================

// One-shot quantization with auto-dispatch
inline void QuantizeFP8_AVX512(const float* input, uint8_t* output, 
                                size_t count, float scale = 1.0f) {
    static FP8QuantizerAVX512 quantizer;
    static bool initialized = false;
    if (!initialized) {
        quantizer.Initialize();
        initialized = true;
    }
    quantizer.Quantize(input, output, count, scale);
}

// Get recommended batch size for current CPU
size_t GetRecommendedFP8BatchSize();

// Benchmark different strategies
void BenchmarkFP8Quantization(size_t elementCount = 1000000);

} // namespace Kernels
} // namespace RawrXD
