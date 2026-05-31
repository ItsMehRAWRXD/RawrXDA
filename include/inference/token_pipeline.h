#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <vector>

namespace RawrXD {
namespace Inference {

// Lock-free SPSC (Single Producer Single Consumer) ring buffer
// for token pipeline double-buffering
template<typename T, size_t Capacity>
class alignas(64) DoubleBufferQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    DoubleBufferQueue() : head_(0), tail_(0) {}
    
    // Producer: Push single item
    bool Push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Capacity - 1);
        
        // Check if full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Producer: Push multiple items (batch)
    bool PushBatch(const T* items, size_t count) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);
        
        size_t available = (current_head - current_tail - 1) & (Capacity - 1);
        if (count > available) {
            return false; // Not enough space
        }
        
        for (size_t i = 0; i < count; i++) {
            buffer_[(current_tail + i) & (Capacity - 1)] = items[i];
        }
        
        tail_.store((current_tail + count) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    // Consumer: Pop single item
    bool Pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Check if empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    // Consumer: Pop multiple items (batch)
    size_t PopBatch(T* items, size_t max_count) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);
        
        size_t available = (current_tail - current_head) & (Capacity - 1);
        size_t to_pop = (max_count < available) ? max_count : available;
        
        for (size_t i = 0; i < to_pop; i++) {
            items[i] = buffer_[(current_head + i) & (Capacity - 1)];
        }
        
        head_.store((current_head + to_pop) & (Capacity - 1), std::memory_order_release);
        return to_pop;
    }
    
    // Check if empty (consumer side)
    bool Empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    // Check if full (producer side)
    bool Full() const {
        size_t next_tail = (tail_.load(std::memory_order_acquire) + 1) & (Capacity - 1);
        return next_tail == head_.load(std::memory_order_acquire);
    }
    
    // Get approximate size (may be slightly stale)
    size_t Size() const {
        return (tail_.load(std::memory_order_acquire) - 
                head_.load(std::memory_order_acquire)) & (Capacity - 1);
    }
    
private:
    alignas(64) T buffer_[Capacity];
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

// Token buffer for double-buffering
template<size_t MaxTokens = 4096>
struct TokenBuffer {
    static constexpr size_t CAPACITY = MaxTokens;
    
    uint32_t tokens[MaxTokens];
    float logits[MaxTokens];      // For speculative decoding
    float probabilities[MaxTokens];
    size_t count = 0;
    size_t processed = 0;
    uint64_t sequence_id = 0;
    bool is_final = false;
    
    void Clear() {
        count = 0;
        processed = 0;
        is_final = false;
    }
    
    bool Full() const { return count >= MaxTokens; }
    bool Empty() const { return count == 0; }
    size_t Remaining() const { return MaxTokens - count; }
};

// Double-buffer token pipeline manager
class TokenPipeline {
public:
    static constexpr size_t BUFFER_A = 0;
    static constexpr size_t BUFFER_B = 1;
    
    TokenPipeline();
    ~TokenPipeline();
    
    // Initialize with sequence parameters
    bool Initialize(size_t maxSequenceLength, size_t batchSize = 1);
    
    // Producer (GPU/Inference thread): Submit tokens
    bool SubmitTokens(const uint32_t* tokens, const float* logits, 
                      size_t count, uint64_t sequence_id);
    
    // Consumer (CPU/Verification thread): Retrieve tokens
    size_t RetrieveTokens(uint32_t* tokens, float* logits, 
                          size_t max_count, uint64_t& sequence_id);
    
    // Mark end of sequence
    void EndSequence(uint64_t sequence_id);
    
    // Get current buffer stats
    struct Stats {
        size_t pending_tokens;
        size_t processed_tokens;
        size_t dropped_tokens;
        double avg_latency_us;
    };
    Stats GetStats() const;
    
    // Wait for pipeline to drain
    void Drain();
    
    // Reset for new sequence
    void Reset();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// C-API for FFI
extern "C" {
    typedef struct RawrXD_TokenPipeline RawrXD_TokenPipeline;
    
    RawrXD_TokenPipeline* RawrXD_token_pipeline_create(size_t max_seq_len);
    void RawrXD_token_pipeline_destroy(RawrXD_TokenPipeline* pipeline);
    
    int RawrXD_token_pipeline_submit(RawrXD_TokenPipeline* pipeline,
                                      const uint32_t* tokens,
                                      const float* logits,
                                      size_t count,
                                      uint64_t sequence_id);
    
    size_t RawrXD_token_pipeline_retrieve(RawrXD_TokenPipeline* pipeline,
                                           uint32_t* tokens,
                                           float* logits,
                                           size_t max_count,
                                           uint64_t* sequence_id);
    
    void RawrXD_token_pipeline_drain(RawrXD_TokenPipeline* pipeline);
    void RawrXD_token_pipeline_reset(RawrXD_TokenPipeline* pipeline);
}

} // namespace Inference
} // namespace RawrXD
