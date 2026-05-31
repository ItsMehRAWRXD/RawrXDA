// Sovereign FP8 Quantizer Header
// Hardware-independent FP8 quantization for RX 7800 XT

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

namespace RawrXD {
namespace Kernels {

// FP8 Format enumeration
enum class FP8Format {
    E4M3,  // 4-bit exponent, 3-bit mantissa
    E5M2   // 5-bit exponent, 2-bit mantissa
};

// Sovereign FP8 Quantizer class
class SovereignFP8Quantizer {
public:
    SovereignFP8Quantizer();
    ~SovereignFP8Quantizer();

    // Initialize with format and scale
    bool Initialize(FP8Format format, float scale);

    // Quantize float array to FP8
    void Quantize(const float* input, uint8_t* output, size_t count);

    // Quantize in batches
    void QuantizeBatched(const float* input, uint8_t* output, 
                         size_t count, size_t batch_size);

    // Get optimal batch size for RX 7800 XT
    static size_t GetOptimalBatchSize();

    // Get max representable value for format
    static float GetMaxValue(FP8Format format);

    // Get format name
    static const char* GetFormatName(FP8Format format);

    // Check if initialized
    bool IsInitialized() const { return initialized_; }

private:
    FP8Format format_;
    float scale_;
    bool initialized_;
    void (*quantize_kernel_)(const float*, uint8_t*, size_t, float);
};

// Double buffer pipeline for token streaming
class SovereignDoubleBuffer {
public:
    struct DoubleBufferPipeline {
        void* buffer_a;
        void* buffer_b;
        void* active_buffer;
        size_t buffer_size;
    };

    SovereignDoubleBuffer();
    ~SovereignDoubleBuffer();

    bool Initialize(size_t buffer_size);
    void* Swap();
    void* GetActiveBuffer() const;
    bool IsInitialized() const { return initialized_; }

private:
    std::unique_ptr<DoubleBufferPipeline> pipeline_;
    bool initialized_;
};

} // namespace Kernels

// Alias for backward compatibility
namespace SovereignFP8 {
    using Quantizer = Kernels::SovereignFP8Quantizer;
    using Format = Kernels::FP8Format;
} // namespace SovereignFP8

} // namespace RawrXD
