#include "kernels/sovereign_fp8_quantizer.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <string>

// MASM64 kernel declarations
// Windows x64 ABI: RCX=input, RDX=output, R8=count, XMM3=scale
extern "C" {
    void SovereignQuantizeE4M3(const float* input, uint8_t* output, size_t count, float scale);
    void SovereignQuantizeE5M2(const float* input, uint8_t* output, size_t count, float scale);
    void* SovereignDoubleBufferInit(size_t buffer_size);
    void* SovereignDoubleBufferSwap(void* pipeline);
    void SovereignGPUFlush();
}

namespace RawrXD {
namespace Kernels {

// =============================================================================
// SovereignFP8Quantizer Implementation
// =============================================================================

SovereignFP8Quantizer::SovereignFP8Quantizer()
    : format_(FP8Format::E4M3)
    , scale_(1.0f)
    , initialized_(false)
    , quantize_kernel_(nullptr)
{
}

SovereignFP8Quantizer::~SovereignFP8Quantizer() {
    // Cleanup if needed
}

bool SovereignFP8Quantizer::Initialize(FP8Format format, float scale) {
    if (scale <= 0.0f) {
        std::cerr << "[SovereignFP8] Invalid scale: " << scale << std::endl;
        return false;
    }

    format_ = format;
    scale_ = scale;

    // Select appropriate MASM kernel
    switch (format) {
        case FP8Format::E4M3:
            quantize_kernel_ = SovereignQuantizeE4M3;
            break;
        case FP8Format::E5M2:
            quantize_kernel_ = SovereignQuantizeE5M2;
            break;
        default:
            std::cerr << "[SovereignFP8] Unknown format" << std::endl;
            return false;
    }

    initialized_ = true;
    std::cout << "[SovereignFP8] Initialized " << GetFormatName(format_) 
              << " with scale=" << scale_ << std::endl;
    std::cout << "[SovereignFP8] Kernel pointer: " << quantize_kernel_ << std::endl;
    return true;
}

void SovereignFP8Quantizer::Quantize(const float* input, uint8_t* output, size_t count) {
    if (!initialized_ || !quantize_kernel_) {
        std::cerr << "[SovereignFP8] Not initialized" << std::endl;
        return;
    }

    // Call MASM64 kernel directly - no vendor SDK
    // Windows x64 ABI: RCX=input, RDX=output, R8=count, XMM3=scale
    quantize_kernel_(input, output, count, scale_);

    // Ensure GPU completion without driver overhead
    SovereignGPUFlush();
}

void SovereignFP8Quantizer::QuantizeBatched(const float* input, uint8_t* output, 
                                           size_t count, size_t batch_size) {
    if (!initialized_) return;

    size_t processed = 0;
    while (processed < count) {
        size_t current_batch = std::min(batch_size, count - processed);
        Quantize(input + processed, output + processed, current_batch);
        processed += current_batch;
    }
}

size_t SovereignFP8Quantizer::GetOptimalBatchSize() {
    // RX 7800 XT (Navi 32) optimal batch size
    // Balances memory bandwidth with compute utilization
    return 8192; // 8K elements per batch
}

float SovereignFP8Quantizer::GetMaxValue(FP8Format format) {
    switch (format) {
        case FP8Format::E4M3:
            return 448.0f;
        case FP8Format::E5M2:
            return 57344.0f;
        default:
            return 0.0f;
    }
}

const char* SovereignFP8Quantizer::GetFormatName(FP8Format format) {
    switch (format) {
        case FP8Format::E4M3:
            return "E4M3";
        case FP8Format::E5M2:
            return "E5M2";
        default:
            return "Unknown";
    }
}

// =============================================================================
// SovereignDoubleBuffer Implementation
// =============================================================================

SovereignDoubleBuffer::SovereignDoubleBuffer()
    : initialized_(false)
{
}

SovereignDoubleBuffer::~SovereignDoubleBuffer() {
    // Cleanup handled by unique_ptr
}

bool SovereignDoubleBuffer::Initialize(size_t buffer_size) {
    if (buffer_size == 0) {
        return false;
    }

    // Allocate pipeline structure
    pipeline_ = std::make_unique<DoubleBufferPipeline>();
    
    // Allocate buffers using aligned memory for AVX2
    pipeline_->buffer_a = _aligned_malloc(buffer_size, 32);
    pipeline_->buffer_b = _aligned_malloc(buffer_size, 32);
    
    if (!pipeline_->buffer_a || !pipeline_->buffer_b) {
        if (pipeline_->buffer_a) _aligned_free(pipeline_->buffer_a);
        if (pipeline_->buffer_b) _aligned_free(pipeline_->buffer_b);
        return false;
    }

    // Initialize state
    pipeline_->active_buffer = 0;
    pipeline_->write_offset = 0;
    pipeline_->buffer_size = buffer_size;
    pipeline_->ready_flag = 0;
    pipeline_->lock = 0;

    initialized_ = true;
    std::cout << "[SovereignDoubleBuffer] Initialized with " << buffer_size << " bytes" << std::endl;
    return true;
}

void* SovereignDoubleBuffer::AcquireWriteBuffer() {
    if (!initialized_ || !pipeline_) return nullptr;

    // Spinlock acquire
    while (pipeline_->lock.exchange(1, std::memory_order_acquire) != 0) {
        _mm_pause(); // Hint to CPU we're in a spinloop
    }

    // Return inactive buffer for writing
    int inactive = 1 - pipeline_->active_buffer.load();
    void* buffer = (inactive == 0) ? pipeline_->buffer_a : pipeline_->buffer_b;

    pipeline_->lock.store(0, std::memory_order_release);
    return buffer;
}

void SovereignDoubleBuffer::ReleaseWriteBuffer(size_t bytes_written) {
    if (!initialized_ || !pipeline_) return;

    // Spinlock acquire
    while (pipeline_->lock.exchange(1, std::memory_order_acquire) != 0) {
        _mm_pause();
    }

    pipeline_->write_offset = bytes_written;
    pipeline_->ready_flag = 1;

    pipeline_->lock.store(0, std::memory_order_release);
}

void* SovereignDoubleBuffer::TryAcquireReadBuffer() {
    if (!initialized_ || !pipeline_) return nullptr;

    // Spinlock acquire
    while (pipeline_->lock.exchange(1, std::memory_order_acquire) != 0) {
        _mm_pause();
    }

    // Check if ready
    if (pipeline_->ready_flag.load() == 0) {
        pipeline_->lock.store(0, std::memory_order_release);
        return nullptr;
    }

    // Return active buffer for reading
    int active = pipeline_->active_buffer.load();
    void* buffer = (active == 0) ? pipeline_->buffer_a : pipeline_->buffer_b;

    pipeline_->lock.store(0, std::memory_order_release);
    return buffer;
}

void SovereignDoubleBuffer::ReleaseReadBuffer() {
    if (!initialized_ || !pipeline_) return;

    // Spinlock acquire
    while (pipeline_->lock.exchange(1, std::memory_order_acquire) != 0) {
        _mm_pause();
    }

    // Clear ready flag
    pipeline_->ready_flag = 0;

    pipeline_->lock.store(0, std::memory_order_release);
}

void SovereignDoubleBuffer::SwapBuffers() {
    if (!initialized_ || !pipeline_) return;

    // Spinlock acquire
    while (pipeline_->lock.exchange(1, std::memory_order_acquire) != 0) {
        _mm_pause();
    }

    // Only swap if next buffer is ready
    if (pipeline_->ready_flag.load() != 0) {
        int current = pipeline_->active_buffer.load();
        pipeline_->active_buffer = 1 - current;
        pipeline_->ready_flag = 0;
    }

    pipeline_->lock.store(0, std::memory_order_release);
}

bool SovereignDoubleBuffer::IsReadBufferReady() const {
    if (!initialized_ || !pipeline_) return false;
    return pipeline_->ready_flag.load() != 0;
}

int SovereignDoubleBuffer::GetActiveBuffer() const {
    if (!initialized_ || !pipeline_) return -1;
    return pipeline_->active_buffer.load();
}

// =============================================================================
// SovereignQuantizationPipeline Implementation
// =============================================================================

SovereignQuantizationPipeline::SovereignQuantizationPipeline()
    : initialized_(false)
    , current_throughput_(0.0)
{
}

SovereignQuantizationPipeline::~SovereignQuantizationPipeline() {
}

bool SovereignQuantizationPipeline::Initialize(FP8Format format, float scale, size_t buffer_size) {
    quantizer_ = std::make_unique<SovereignFP8Quantizer>();
    if (!quantizer_->Initialize(format, scale)) {
        return false;
    }

    double_buffer_ = std::make_unique<SovereignDoubleBuffer>();
    if (!double_buffer_->Initialize(buffer_size)) {
        return false;
    }

    initialized_ = true;
    std::cout << "[SovereignPipeline] Initialized with format=" 
              << SovereignFP8Quantizer::GetFormatName(format) << std::endl;
    return true;
}

void SovereignQuantizationPipeline::SubmitJob(const float* input, uint8_t* output, size_t count) {
    if (!initialized_) return;

    auto start = std::chrono::high_resolution_clock::now();

    quantizer_->Quantize(input, output, count);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    if (duration > 0) {
        current_throughput_ = (count * 1000000.0) / duration; // tokens/sec
    }
}

void SovereignQuantizationPipeline::WaitForCompletion() {
    // In async mode, this would wait for GPU completion
    // For now, just flush
    SovereignGPUFlush();
}

void SovereignQuantizationPipeline::ProcessStream(const float* input_stream, 
                                                   uint8_t* output_stream,
                                                   size_t total_elements, 
                                                   size_t batch_size) {
    if (!initialized_) return;

    size_t processed = 0;
    int ping_pong = 0;

    while (processed < total_elements) {
        size_t current_batch = std::min(batch_size, total_elements - processed);

        // Acquire write buffer
        void* write_buf = double_buffer_->AcquireWriteBuffer();
        if (!write_buf) continue;

        // Quantize into buffer
        quantizer_->Quantize(
            input_stream + processed,
            static_cast<uint8_t*>(write_buf),
            current_batch
        );

        // Release and mark ready
        double_buffer_->ReleaseWriteBuffer(current_batch);

        // Try to swap
        double_buffer_->SwapBuffers();

        // Read from active buffer
        void* read_buf = double_buffer_->TryAcquireReadBuffer();
        if (read_buf) {
            // Copy to output stream
            std::memcpy(
                output_stream + processed,
                read_buf,
                current_batch
            );
            double_buffer_->ReleaseReadBuffer();
        }

        processed += current_batch;
        ping_pong ^= 1;
    }
}

double SovereignQuantizationPipeline::GetThroughput() const {
    return current_throughput_;
}

void SovereignQuantizationPipeline::EnterHighThroughputMode() {
    // Hint to OS that we're doing high-priority work
    // This is a no-op in sovereign mode (no OS coordination)
    std::cout << "[SovereignPipeline] Entering high-throughput mode" << std::endl;
}

void SovereignQuantizationPipeline::ExitHighThroughputMode() {
    std::cout << "[SovereignPipeline] Exiting high-throughput mode" << std::endl;
}

// =============================================================================
// C API Implementation
// =============================================================================

extern "C" {

SovereignQuantizerHandle SovereignQuantizer_Create(int format, float scale) {
    auto* quantizer = new SovereignFP8Quantizer();
    if (!quantizer->Initialize(static_cast<FP8Format>(format), scale)) {
        delete quantizer;
        return nullptr;
    }
    return quantizer;
}

void SovereignQuantizer_Destroy(SovereignQuantizerHandle handle) {
    delete static_cast<SovereignFP8Quantizer*>(handle);
}

void SovereignQuantizer_Quantize(SovereignQuantizerHandle handle,
                                  const float* input, uint8_t* output, 
                                  size_t count) {
    if (!handle) return;
    static_cast<SovereignFP8Quantizer*>(handle)->Quantize(input, output, count);
}

SovereignDoubleBufferHandle SovereignDoubleBuffer_Create(size_t buffer_size) {
    auto* db = new SovereignDoubleBuffer();
    if (!db->Initialize(buffer_size)) {
        delete db;
        return nullptr;
    }
    return db;
}

void SovereignDoubleBuffer_Destroy(SovereignDoubleBufferHandle handle) {
    delete static_cast<SovereignDoubleBuffer*>(handle);
}

void* SovereignDoubleBuffer_AcquireWrite(SovereignDoubleBufferHandle handle) {
    if (!handle) return nullptr;
    return static_cast<SovereignDoubleBuffer*>(handle)->AcquireWriteBuffer();
}

void SovereignDoubleBuffer_ReleaseWrite(SovereignDoubleBufferHandle handle, size_t bytes) {
    if (!handle) return;
    static_cast<SovereignDoubleBuffer*>(handle)->ReleaseWriteBuffer(bytes);
}

void* SovereignDoubleBuffer_TryAcquireRead(SovereignDoubleBufferHandle handle) {
    if (!handle) return nullptr;
    return static_cast<SovereignDoubleBuffer*>(handle)->TryAcquireReadBuffer();
}

void SovereignDoubleBuffer_ReleaseRead(SovereignDoubleBufferHandle handle) {
    if (!handle) return;
    static_cast<SovereignDoubleBuffer*>(handle)->ReleaseReadBuffer();
}

SovereignPipelineHandle SovereignPipeline_Create(int format, float scale, size_t buffer_size) {
    auto* pipeline = new SovereignQuantizationPipeline();
    if (!pipeline->Initialize(static_cast<FP8Format>(format), scale, buffer_size)) {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

void SovereignPipeline_Destroy(SovereignPipelineHandle handle) {
    delete static_cast<SovereignQuantizationPipeline*>(handle);
}

void SovereignPipeline_Submit(SovereignPipelineHandle handle,
                               const float* input, uint8_t* output, size_t count) {
    if (!handle) return;
    static_cast<SovereignQuantizationPipeline*>(handle)->SubmitJob(input, output, count);
}

void SovereignPipeline_Wait(SovereignPipelineHandle handle) {
    if (!handle) return;
    static_cast<SovereignQuantizationPipeline*>(handle)->WaitForCompletion();
}

void SovereignPipeline_EnterHighThroughput(SovereignPipelineHandle handle) {
    if (!handle) return;
    static_cast<SovereignQuantizationPipeline*>(handle)->EnterHighThroughputMode();
}

void SovereignPipeline_ExitHighThroughput(SovereignPipelineHandle handle) {
    if (!handle) return;
    static_cast<SovereignQuantizationPipeline*>(handle)->ExitHighThroughputMode();
}

// Simple FP8 kernel getter for pipeline integration
typedef void (*fp8_quantize_fn)(const float* input, uint8_t* output, size_t n, int format);
fp8_quantize_fn GetSovereignFP8Kernel() {
    // Return E4M3 kernel as default (format 0 = E4M3, format 1 = E5M2)
    return [](const float* input, uint8_t* output, size_t n, int format) {
        if (format == 0) {
            SovereignQuantizeE4M3(input, output, n, 1.0f);
        } else {
            SovereignQuantizeE5M2(input, output, n, 1.0f);
        }
    };
}

} // extern "C"

} // namespace Kernels
} // namespace RawrXD
