// ============================================================================
// token_pipeline_double_buffer.hpp — P0: Double-Buffer Token Pipeline
// ============================================================================
// Eliminates GPU idle between tokens by double-buffering command lists.
// CPU prepares token N+1 while GPU executes token N.
// Sampling runs on background thread, results fed via lock-free SPSC queue.
//
// Expected gain: +15-20% TPS
// LOC: ~120 lines
// ============================================================================

#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>

namespace RawrXD {
namespace Inference {

// Forward declarations
struct Token;
struct SamplingResult;

// Token generation state
enum class TokenGenState : uint8_t {
    IDLE = 0,
    PREPARING = 1,      // CPU: Preparing inputs
    GPU_EXECUTING = 2,  // GPU: Running inference
    SAMPLING = 3,       // CPU: Sampling logits
    COMPLETE = 4        // Token ready
};

// Double-buffer slot
struct TokenBuffer {
    static constexpr uint32_t MAX_SEQ_LEN = 8192;
    static constexpr uint32_t VOCAB_SIZE = 32000;  // Configurable

    // Input/output buffers
    float input_embeddings[MAX_SEQ_LEN * 4096];   // token embeddings
    float output_logits[VOCAB_SIZE];
    int32_t token_ids[MAX_SEQ_LEN];

    // Metadata
    uint32_t seq_len = 0;
    uint32_t token_id = 0;
    TokenGenState state = TokenGenState::IDLE;
    uint64_t generation_id = 0;

    // Timing
    uint64_t t_submit_ns = 0;
    uint64_t t_complete_ns = 0;

    // State flags
    std::atomic<bool> ready_for_gpu{false};
    std::atomic<bool> gpu_complete{false};
    std::atomic<bool> sampled{false};

    void reset() {
        seq_len = 0;
        token_id = 0;
        state = TokenGenState::IDLE;
        generation_id = 0;
        t_submit_ns = 0;
        t_complete_ns = 0;
        ready_for_gpu.store(false);
        gpu_complete.store(false);
        sampled.store(false);
    }
};

// Lock-free SPSC queue for token sampling results
struct TokenResultQueue {
    static constexpr uint32_t SIZE = 16;  // Must be power of 2

    struct Entry {
        uint32_t token_id = 0;
        uint64_t generation_id = 0;
        float log_prob = 0.0f;
        bool valid = false;
    };

    Entry buffer[SIZE];
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    bool push(const Entry& entry) {
        const uint32_t current_tail = tail.load(std::memory_order_relaxed);
        const uint32_t next_tail = (current_tail + 1) & (SIZE - 1);

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        buffer[current_tail] = entry;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(Entry& entry) {
        const uint32_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        entry = buffer[current_head];
        head.store((current_head + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head.load(std::memory_order_acquire) ==
               tail.load(std::memory_order_acquire);
    }
};

// Sampling configuration
struct SamplingConfig {
    float temperature = 0.8f;
    float top_p = 0.95f;
    uint32_t top_k = 40;
    uint32_t seed = 0;
};

// Sampling function type
using SamplingFn = std::function<int32_t(const float* logits, uint32_t vocab_size,
                                         const SamplingConfig& config)>;

// ============================================================================
// Double-Buffer Token Pipeline
// ============================================================================
class TokenPipelineDoubleBuffer {
public:
    TokenPipelineDoubleBuffer();
    ~TokenPipelineDoubleBuffer();

    // No copy/move
    TokenPipelineDoubleBuffer(const TokenPipelineDoubleBuffer&) = delete;
    TokenPipelineDoubleBuffer& operator=(const TokenPipelineDoubleBuffer&) = delete;

    // Initialize with model dimensions
    bool initialize(uint32_t vocab_size, uint32_t embedding_dim,
                    SamplingFn sampler = nullptr);

    // Shutdown
    void shutdown();

    // Start generation of a new token
    // Returns buffer index to use (0 or 1)
    uint32_t beginTokenGeneration(uint64_t generation_id);

    // Submit buffer to GPU for execution
    void submitToGPU(uint32_t buffer_idx);

    // Mark GPU execution complete
    void markGPUComplete(uint32_t buffer_idx);

    // Get next buffer ready for CPU preparation
    // Returns buffer index or -1 if none available
    int32_t getReadyBuffer();

    // Get buffer that GPU just completed
    // Returns buffer index or -1 if none complete
    int32_t getCompleteBuffer();

    // Access buffer data
    TokenBuffer* getBuffer(uint32_t idx);
    const TokenBuffer* getBuffer(uint32_t idx) const;

    // Check if pipeline is active
    bool isActive() const { return active_.load(); }

    // Get statistics
    uint64_t getTokensGenerated() const { return tokens_generated_.load(); }
    uint64_t getTokensInFlight() const { return tokens_in_flight_.load(); }
    double getAverageLatencyMs() const;

    // Result queue access (for sampling thread)
    TokenResultQueue& getResultQueue() { return result_queue_; }

private:
    // Background sampling thread
    void samplingThreadFunc();

    // Configuration
    uint32_t vocab_size_ = 0;
    uint32_t embedding_dim_ = 0;
    SamplingFn sampler_;

    // Double buffers
    TokenBuffer buffers_[2];
    std::atomic<uint32_t> current_buffer_{0};
    std::atomic<uint32_t> next_buffer_{0};

    // Result queue
    TokenResultQueue result_queue_;

    // Background thread
    std::unique_ptr<std::thread> sampling_thread_;
    std::atomic<bool> active_{false};
    std::atomic<bool> shutdown_requested_{false};

    // Statistics
    std::atomic<uint64_t> tokens_generated_{0};
    std::atomic<uint64_t> tokens_in_flight_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
};

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    typedef void* RawrXD_TokenPipeline;

    RawrXD_TokenPipeline rawrxd_tokenpipeline_create(uint32_t vocab_size,
                                                     uint32_t embedding_dim);
    void rawrxd_tokenpipeline_destroy(RawrXD_TokenPipeline handle);

    int rawrxd_tokenpipeline_begin_generation(RawrXD_TokenPipeline handle,
                                               uint64_t generation_id);
    void rawrxd_tokenpipeline_submit_gpu(RawrXD_TokenPipeline handle,
                                         int buffer_idx);
    void rawrxd_tokenpipeline_mark_complete(RawrXD_TokenPipeline handle,
                                            int buffer_idx);

    void* rawrxd_tokenpipeline_get_buffer(RawrXD_TokenPipeline handle,
                                          int buffer_idx);
}

} // namespace Inference
} // namespace RawrXD
