#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <atomic>

namespace RawrXD {
namespace Kernels {

// FP8 format types
enum class FP8Format {
    E4M3,   // 1 sign, 4 exponent, 3 mantissa - higher precision
    E5M2    // 1 sign, 5 exponent, 2 mantissa - higher dynamic range
};

// Double-buffer pipeline state
struct DoubleBufferPipeline {
    void* buffer_a;
    void* buffer_b;
    std::atomic<uint64_t> active_buffer;
    std::atomic<uint64_t> write_offset;
    uint64_t buffer_size;
    std::atomic<uint64_t> ready_flag;
    std::atomic<uint64_t> lock;
};

// Sovereign FP8 Quantizer - Hardware-independent quantization
class SovereignFP8Quantizer {
public:
    SovereignFP8Quantizer();
    ~SovereignFP8Quantizer();

    // Initialize quantizer with format and scale
    bool Initialize(FP8Format format, float scale);

    // Quantize float array to FP8
    // Uses MASM64 kernel for zero-telemetry execution
    void Quantize(const float* input, uint8_t* output, size_t count);

    // Batch quantize with async pre-fetch
    void QuantizeBatched(const float* input, uint8_t* output, 
                       size_t count, size_t batch_size);

    // Get optimal batch size for RX 7800 XT
    static size_t GetOptimalBatchSize();

    // Format utilities
    static float GetMaxValue(FP8Format format);
    static const char* GetFormatName(FP8Format format);

private:
    FP8Format format_;
    float scale_;
    bool initialized_;

    // Function pointers to MASM kernels
    using QuantizeFunc = void (*)(const float*, uint8_t*, size_t, float);
    QuantizeFunc quantize_kernel_;
};

// Double-buffer pipeline for async token generation
class SovereignDoubleBuffer {
public:
    SovereignDoubleBuffer();
    ~SovereignDoubleBuffer();

    // Initialize with buffer size
    bool Initialize(size_t buffer_size);

    // Get write buffer (for producer)
    void* AcquireWriteBuffer();
    void ReleaseWriteBuffer(size_t bytes_written);

    // Get read buffer (for consumer) - non-blocking
    void* TryAcquireReadBuffer();
    void ReleaseReadBuffer();

    // Force buffer swap (for synchronization points)
    void SwapBuffers();

    // Check if read buffer is ready
    bool IsReadBufferReady() const;

    // Get current active buffer index (0 or 1)
    int GetActiveBuffer() const;

private:
    std::unique_ptr<DoubleBufferPipeline> pipeline_;
    bool initialized_;
};

// High-throughput quantization pipeline
// Combines FP8 quantizer with double-buffering
class SovereignQuantizationPipeline {
public:
    SovereignQuantizationPipeline();
    ~SovereignQuantizationPipeline();

    // Initialize pipeline
    bool Initialize(FP8Format format, float scale, size_t buffer_size);

    // Submit quantization job (async)
    void SubmitJob(const float* input, uint8_t* output, size_t count);

    // Wait for completion
    void WaitForCompletion();

    // Process with double-buffering (optimal for streaming)
    void ProcessStream(const float* input_stream, uint8_t* output_stream,
                       size_t total_elements, size_t batch_size);

    // Get throughput metrics (tokens/sec)
    double GetThroughput() const;

    // Force GPU into high-throughput mode
    void EnterHighThroughputMode();
    void ExitHighThroughputMode();

private:
    std::unique_ptr<SovereignFP8Quantizer> quantizer_;
    std::unique_ptr<SovereignDoubleBuffer> double_buffer_;
    bool initialized_;
    double current_throughput_;
};

// C API for FFI integration
extern "C" {
    typedef void* SovereignQuantizerHandle;
    typedef void* SovereignDoubleBufferHandle;
    typedef void* SovereignPipelineHandle;

    // Quantizer API
    SovereignQuantizerHandle SovereignQuantizer_Create(int format, float scale);
    void SovereignQuantizer_Destroy(SovereignQuantizerHandle handle);
    void SovereignQuantizer_Quantize(SovereignQuantizerHandle handle,
                                      const float* input, uint8_t* output, 
                                      size_t count);

    // Double buffer API
    SovereignDoubleBufferHandle SovereignDoubleBuffer_Create(size_t buffer_size);
    void SovereignDoubleBuffer_Destroy(SovereignDoubleBufferHandle handle);
    void* SovereignDoubleBuffer_AcquireWrite(SovereignDoubleBufferHandle handle);
    void SovereignDoubleBuffer_ReleaseWrite(SovereignDoubleBufferHandle handle, size_t bytes);
    void* SovereignDoubleBuffer_TryAcquireRead(SovereignDoubleBufferHandle handle);
    void SovereignDoubleBuffer_ReleaseRead(SovereignDoubleBufferHandle handle);

    // Pipeline API
    SovereignPipelineHandle SovereignPipeline_Create(int format, float scale, size_t buffer_size);
    void SovereignPipeline_Destroy(SovereignPipelineHandle handle);
    void SovereignPipeline_Submit(SovereignPipelineHandle handle,
                                   const float* input, uint8_t* output, size_t count);
    void SovereignPipeline_Wait(SovereignPipelineHandle handle);
    void SovereignPipeline_EnterHighThroughput(SovereignPipelineHandle handle);
    void SovereignPipeline_ExitHighThroughput(SovereignPipelineHandle handle);
}

} // namespace Kernels
} // namespace RawrXD
