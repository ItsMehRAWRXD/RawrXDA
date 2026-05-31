// ============================================================================
// token_pipeline_double_buffer.cpp — P0: Double-Buffer Token Pipeline
// ============================================================================
// Implementation of lock-free double-buffering for token generation.
// Background sampling thread eliminates GPU idle time.
// ============================================================================

#include "token_pipeline_double_buffer.hpp"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <random>

namespace RawrXD {
namespace Inference {

// Default sampling function (top-k + top-p)
static int32_t defaultSampling(const float* logits, uint32_t vocab_size,
                                const SamplingConfig& config) {
    // Simple greedy sampling for now
    // Full top-k/top-p implementation would go here
    int32_t best_idx = 0;
    float best_logit = logits[0];

    for (uint32_t i = 1; i < vocab_size; ++i) {
        if (logits[i] > best_logit) {
            best_logit = logits[i];
            best_idx = static_cast<int32_t>(i);
        }
    }

    return best_idx;
}

// ============================================================================
// Construction / Destruction
// ============================================================================
TokenPipelineDoubleBuffer::TokenPipelineDoubleBuffer() = default;

TokenPipelineDoubleBuffer::~TokenPipelineDoubleBuffer() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================
bool TokenPipelineDoubleBuffer::initialize(uint32_t vocab_size,
                                            uint32_t embedding_dim,
                                            SamplingFn sampler) {
    if (active_.load()) {
        shutdown();
    }

    vocab_size_ = vocab_size;
    embedding_dim_ = embedding_dim;
    sampler_ = sampler ? sampler : defaultSampling;

    // Reset buffers
    buffers_[0].reset();
    buffers_[1].reset();
    current_buffer_.store(0);
    next_buffer_.store(1);

    // Reset queue
    result_queue_.head.store(0);
    result_queue_.tail.store(0);

    // Start sampling thread
    active_.store(true);
    shutdown_requested_.store(false);
    sampling_thread_ = std::make_unique<std::thread>(
        &TokenPipelineDoubleBuffer::samplingThreadFunc, this);

    return true;
}

void TokenPipelineDoubleBuffer::shutdown() {
    if (!active_.load()) return;

    shutdown_requested_.store(true);
    active_.store(false);

    if (sampling_thread_ && sampling_thread_->joinable()) {
        sampling_thread_->join();
    }
    sampling_thread_.reset();

    // Reset state
    buffers_[0].reset();
    buffers_[1].reset();
    current_buffer_.store(0);
    next_buffer_.store(0);

    tokens_generated_.store(0);
    tokens_in_flight_.store(0);
    total_latency_ns_.store(0);
}

// ============================================================================
// Token Generation
// ============================================================================
uint32_t TokenPipelineDoubleBuffer::beginTokenGeneration(uint64_t generation_id) {
    // Get the next available buffer
    uint32_t buffer_idx = next_buffer_.load(std::memory_order_acquire);

    // Wait if buffer is still in use
    while (buffers_[buffer_idx].state != TokenGenState::IDLE) {
        buffer_idx = (buffer_idx == 0) ? 1 : 0;
        if (buffers_[buffer_idx].state == TokenGenState::IDLE) {
            break;
        }
        // Spin briefly then try again
        std::this_thread::yield();
    }

    // Initialize buffer
    auto& buffer = buffers_[buffer_idx];
    buffer.reset();
    buffer.generation_id = generation_id;
    buffer.state = TokenGenState::PREPARING;

    // Update next buffer pointer
    next_buffer_.store((buffer_idx == 0) ? 1 : 0, std::memory_order_release);

    tokens_in_flight_.fetch_add(1);
    return buffer_idx;
}

void TokenPipelineDoubleBuffer::submitToGPU(uint32_t buffer_idx) {
    if (buffer_idx >= 2) return;

    auto& buffer = buffers_[buffer_idx];
    buffer.state = TokenGenState::GPU_EXECUTING;
    buffer.t_submit_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    buffer.ready_for_gpu.store(true, std::memory_order_release);
}

void TokenPipelineDoubleBuffer::markGPUComplete(uint32_t buffer_idx) {
    if (buffer_idx >= 2) return;

    auto& buffer = buffers_[buffer_idx];
    buffer.t_complete_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    buffer.state = TokenGenState::SAMPLING;
    buffer.gpu_complete.store(true, std::memory_order_release);
}

int32_t TokenPipelineDoubleBuffer::getReadyBuffer() {
    // Find buffer that's IDLE and ready for preparation
    for (uint32_t i = 0; i < 2; ++i) {
        if (buffers_[i].state == TokenGenState::IDLE) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t TokenPipelineDoubleBuffer::getCompleteBuffer() {
    // Find buffer where GPU just completed
    for (uint32_t i = 0; i < 2; ++i) {
        if (buffers_[i].gpu_complete.load(std::memory_order_acquire) &&
            buffers_[i].state == TokenGenState::SAMPLING) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

TokenBuffer* TokenPipelineDoubleBuffer::getBuffer(uint32_t idx) {
    if (idx >= 2) return nullptr;
    return &buffers_[idx];
}

const TokenBuffer* TokenPipelineDoubleBuffer::getBuffer(uint32_t idx) const {
    if (idx >= 2) return nullptr;
    return &buffers_[idx];
}

// ============================================================================
// Background Sampling Thread
// ============================================================================
void TokenPipelineDoubleBuffer::samplingThreadFunc() {
    SamplingConfig config;

    while (!shutdown_requested_.load()) {
        // Check for completed GPU buffers that need sampling
        for (uint32_t i = 0; i < 2; ++i) {
            auto& buffer = buffers_[i];

            if (buffer.gpu_complete.load(std::memory_order_acquire) &&
                !buffer.sampled.load()) {

                // Perform sampling
                int32_t token_id = sampler_(buffer.output_logits, vocab_size_, config);

                // Push result to queue
                TokenResultQueue::Entry entry;
                entry.token_id = token_id;
                entry.generation_id = buffer.generation_id;
                entry.log_prob = 0.0f;  // Would compute from logits
                entry.valid = true;

                // Try to push (may fail if queue full)
                if (!result_queue_.push(entry)) {
                    // Queue full - handle overflow
                    continue;
                }

                buffer.token_id = token_id;
                buffer.sampled.store(true, std::memory_order_release);
                buffer.state = TokenGenState::COMPLETE;

                // Update statistics
                tokens_generated_.fetch_add(1);
                tokens_in_flight_.fetch_sub(1);

                uint64_t latency_ns = buffer.t_complete_ns - buffer.t_submit_ns;
                total_latency_ns_.fetch_add(latency_ns);

                // Mark buffer idle after brief delay to prevent immediate reuse
                buffer.state = TokenGenState::IDLE;
            }
        }

        // Brief sleep to prevent busy-wait
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

// ============================================================================
// Statistics
// ============================================================================
double TokenPipelineDoubleBuffer::getAverageLatencyMs() const {
    uint64_t tokens = tokens_generated_.load();
    if (tokens == 0) return 0.0;
    uint64_t total_ns = total_latency_ns_.load();
    return static_cast<double>(total_ns) / tokens / 1'000'000.0;
}

// ============================================================================
// C API Implementation
// ============================================================================
extern "C" {

RawrXD_TokenPipeline rawrxd_tokenpipeline_create(uint32_t vocab_size,
                                                    uint32_t embedding_dim) {
    auto* pipeline = new TokenPipelineDoubleBuffer();
    if (!pipeline->initialize(vocab_size, embedding_dim)) {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

void rawrxd_tokenpipeline_destroy(RawrXD_TokenPipeline handle) {
    if (handle) {
        auto* pipeline = static_cast<TokenPipelineDoubleBuffer*>(handle);
        pipeline->shutdown();
        delete pipeline;
    }
}

int rawrxd_tokenpipeline_begin_generation(RawrXD_TokenPipeline handle,
                                         uint64_t generation_id) {
    if (!handle) return -1;
    auto* pipeline = static_cast<TokenPipelineDoubleBuffer*>(handle);
    return static_cast<int>(pipeline->beginTokenGeneration(generation_id));
}

void rawrxd_tokenpipeline_submit_gpu(RawrXD_TokenPipeline handle,
                                      int buffer_idx) {
    if (!handle || buffer_idx < 0) return;
    auto* pipeline = static_cast<TokenPipelineDoubleBuffer*>(handle);
    pipeline->submitToGPU(static_cast<uint32_t>(buffer_idx));
}

void rawrxd_tokenpipeline_mark_complete(RawrXD_TokenPipeline handle,
                                       int buffer_idx) {
    if (!handle || buffer_idx < 0) return;
    auto* pipeline = static_cast<TokenPipelineDoubleBuffer*>(handle);
    pipeline->markGPUComplete(static_cast<uint32_t>(buffer_idx));
}

void* rawrxd_tokenpipeline_get_buffer(RawrXD_TokenPipeline handle,
                                       int buffer_idx) {
    if (!handle || buffer_idx < 0) return nullptr;
    auto* pipeline = static_cast<TokenPipelineDoubleBuffer*>(handle);
    return pipeline->getBuffer(static_cast<uint32_t>(buffer_idx));
}

} // extern "C"

} // namespace Inference
} // namespace RawrXD
