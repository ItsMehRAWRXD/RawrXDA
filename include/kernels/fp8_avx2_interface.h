#pragma once
// Sovereign FP8 AVX2 Interface - Zero-Copy Pipeline Integration
// Provides batch-oriented quantization for 3-stage pipeline Stage 3 (Egress)
// Author: RawrXD Core Team

#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace Kernels {

// AVX2 kernel entry points (extern "C" linkage for MASM)
extern "C" {
    // Vectorized E4M3 quantization - processes 8 floats per iteration
    // Input:  32-byte aligned float array
    // Output: uint8_t array (E4M3 format)
    // Requirements: count >= 8 for vector path, input 32-byte aligned
    void SovereignQuantizeE4M3_AVX2(
        const float* input,
        uint8_t* output,
        size_t count,
        float scale
    );

    // Vectorized E5M2 quantization - processes 8 floats per iteration
    void SovereignQuantizeE5M2_AVX2(
        const float* input,
        uint8_t* output,
        size_t count,
        float scale
    );

    // Optimized 64-token batch quantization for pipeline egress
    // Input:  256-byte aligned float array (64 floats)
    // Output: uint8_t array (64 bytes, E4M3 format)
    // Requirements: batch_size must be exactly 64
    void SovereignQuantizeBatch64_AVX2(
        const float* input,
        uint8_t* output,
        size_t batch_size,
        float scale
    );
}

// C++ wrapper providing memory alignment guarantees and batch management
class FP8AVX2Quantizer {
public:
    // Alignment requirements for AVX2 operations
    static constexpr size_t kAlign32 = 32;    // For 8-float vectors
    static constexpr size_t kAlign256 = 256;  // For 64-float batches
    static constexpr size_t kBatchSize = 64;  // Optimal pipeline batch

    FP8AVX2Quantizer() = default;

    // Quantize a single batch of exactly 64 tokens
    // Input/output must be aligned to 256 bytes for optimal performance
    // Returns: true on success, false if alignment requirements not met
    static bool QuantizeBatch64(
        const float* __restrict input_aligned,
        uint8_t* __restrict output,
        float scale = 1.0f
    ) {
        // Verify alignment (optional in release, recommended in debug)
        #ifdef _DEBUG
        if ((reinterpret_cast<uintptr_t>(input_aligned) & (kAlign256 - 1)) != 0) {
            return false;  // Input not 256-byte aligned
        }
        #endif

        SovereignQuantizeBatch64_AVX2(
            input_aligned,
            output,
            kBatchSize,
            scale
        );
        return true;
    }

    // Quantize arbitrary count (will use vector path for multiples of 8)
    // Input should be 32-byte aligned for best performance
    static void QuantizeE4M3(
        const float* input,
        uint8_t* output,
        size_t count,
        float scale = 1.0f
    ) {
        SovereignQuantizeE4M3_AVX2(input, output, count, scale);
    }

    static void QuantizeE5M2(
        const float* input,
        uint8_t* output,
        size_t count,
        float scale = 1.0f
    ) {
        SovereignQuantizeE5M2_AVX2(input, output, count, scale);
    }

    // Check if AVX2 is available on this CPU
    static bool IsAVX2Available();

    // Get recommended batch size for pipeline integration
    static constexpr size_t GetOptimalBatchSize() { return kBatchSize; }
    static constexpr size_t GetAlignmentRequirement() { return kAlign256; }
};

// Pipeline-integrated batch processor
// Manages aligned memory and provides zero-copy interface to Stage 3 egress
class FP8BatchProcessor {
public:
    explicit FP8BatchProcessor(size_t max_batches = 256);
    ~FP8BatchProcessor();

    // Disable copy/move - manages aligned memory
    FP8BatchProcessor(const FP8BatchProcessor&) = delete;
    FP8BatchProcessor& operator=(const FP8BatchProcessor&) = delete;

    // Acquire a buffer for input (float[64])
    // Returns: aligned pointer ready for token data, or nullptr if full
    float* AcquireInputBuffer();

    // Release buffer and trigger quantization
    // Input buffer is consumed, output is written to provided destination
    bool CommitAndQuantize(uint8_t* output_destination, float scale = 1.0f);

    // Get current fill level
    size_t GetPendingBatches() const { return pending_count_; }
    bool IsEmpty() const { return pending_count_ == 0; }
    bool IsFull() const { return pending_count_ >= max_batches_; }

    // Process all pending batches (flush)
    size_t FlushAll(uint8_t* output_base, float scale = 1.0f);

private:
    static constexpr size_t kFloatsPerBatch = 64;
    static constexpr size_t kBytesPerBatch = 64 * sizeof(float);  // 256 bytes

    float* aligned_buffer_;      // Contiguous aligned memory block
    uint8_t* completion_flags_;  // Which batches are ready
    size_t max_batches_;
    size_t pending_count_;
    size_t write_index_;
    size_t read_index_;
};

} // namespace Kernels
} // namespace RawrXD
