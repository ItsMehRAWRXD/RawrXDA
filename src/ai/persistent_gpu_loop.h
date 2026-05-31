// persistent_gpu_loop.h - Zero-dispatch persistent GPU execution loop
// Eliminates per-token dispatch overhead by keeping GPU continuously busy
//
// Architecture:
//   - Ring-buffered command buffers (no per-token submits)
//   - Persistent dispatch loop (GPU never idles between tokens)
//   - Streaming decode inside command buffer
//   - CPU prep, GPU exec, UI render fully overlapped
//
// Key insight:
//   - Not: CPU → submit → GPU → wait → CPU → submit → GPU → wait
//   - But: CPU prep | GPU exec | CPU prep | GPU exec | (continuous)
//
// This is the jump from "fast system" → "physically continuous system"
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "vulkan_compute.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD {

// Ring buffer for command buffers
struct CommandBufferRing {
    static constexpr int RING_SIZE = 4;  // Must be power of 2
    
    struct Entry {
        void* command_buffer;           // Vulkan command buffer
        std::atomic<bool> ready{false};  // Ready for submission
        std::atomic<bool> submitted{false}; // Submitted to GPU
        std::atomic<bool> completed{false}; // GPU finished
        std::chrono::steady_clock::time_point submit_time;
        std::chrono::steady_clock::time_point complete_time;
    };
    
    std::array<Entry, RING_SIZE> entries;
    std::atomic<int> write_index{0};   // Next write position
    std::atomic<int> read_index{0};    // Next read position
    std::atomic<int> pending{0};        // Pending submissions
    
    // Get next write entry
    Entry* NextWrite() {
        int idx = write_index.fetch_add(1) % RING_SIZE;
        return &entries[idx];
    }
    
    // Get next read entry
    Entry* NextRead() {
        int idx = read_index.fetch_add(1) % RING_SIZE;
        return &entries[idx];
    }
    
    // Check if ring is full
    bool IsFull() const {
        return pending.load() >= RING_SIZE - 1;
    }
    
    // Check if ring is empty
    bool IsEmpty() const {
        return pending.load() == 0;
    }
};

// Persistent GPU execution state
struct PersistentState {
    // GPU resources
    void* pipeline_layout;
    void* descriptor_set_layout;
    void* descriptor_pool;
    std::vector<void*> descriptor_sets;
    
    // Persistent buffers
    void* input_buffer;          // Input tokens
    void* output_buffer;         // Output logits
    void* kv_cache_buffer;       // KV cache
    void* params_buffer;         // Model parameters
    
    // Buffer sizes
    size_t input_buffer_size;
    size_t output_buffer_size;
    size_t kv_cache_buffer_size;
    size_t params_buffer_size;
    
    // Current token position
    int current_token;
    int max_tokens;
    
    // Kernel mode
    int kernel_mode;
    
    // Generation state
    std::atomic<bool> generating{false};
    std::atomic<bool> stop_requested{false};
};

// Token batch for continuous execution
struct TokenBatch {
    static constexpr int MAX_BATCH_SIZE = 8;
    std::array<uint32_t, MAX_BATCH_SIZE> tokens;
    int count;
    bool is_prompt;  // True if prompt tokens, false if generated tokens
};

// Persistent GPU loop statistics
struct PersistentLoopStats {
    int total_tokens;
    int total_dispatches;
    std::chrono::microseconds avg_dispatch_latency;
    std::chrono::microseconds avg_gpu_idle_time;
    std::chrono::microseconds avg_token_latency;
    float gpu_utilization;       // Percentage of time GPU is busy
    float dispatch_overhead;     // Percentage of time spent in dispatch
    int ring_buffer_underruns;   // Times CPU couldn't keep up
    int ring_buffer_overruns;    // Times GPU couldn't keep up
};

// Persistent GPU execution loop
class PersistentGPULoop {
public:
    PersistentGPULoop(VulkanCompute* vulkan);
    ~PersistentGPULoop();
    
    // Initialize persistent loop
    bool Initialize(int max_tokens, int kernel_mode);
    
    // Start persistent loop
    void Start();
    
    // Stop persistent loop
    void Stop();
    
    // Submit token batch for processing
    // Non-blocking, returns immediately
    bool SubmitBatch(const TokenBatch& batch);
    
    // Get next completed token
    // Non-blocking, returns false if no token ready
    bool GetNextToken(uint32_t& token, float& confidence);
    
    // Wait for next token (blocking with timeout)
    bool WaitForNextToken(
        uint32_t& token,
        float& confidence,
        std::chrono::milliseconds timeout
    );
    
    // Set kernel mode (switches kernel for next dispatch)
    void SetKernelMode(int mode);
    
    // Get current kernel mode
    int GetKernelMode() const { return state_.kernel_mode; }
    
    // Check if loop is running
    bool IsRunning() const { return running_.load(); }
    
    // Get statistics
    PersistentLoopStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Main loop thread
    void LoopThread();
    
    // Prepare next command buffer
    void PrepareCommandBuffer(CommandBufferRing::Entry* entry);
    
    // Submit command buffer to GPU
    void SubmitCommandBuffer(CommandBufferRing::Entry* entry);
    
    // Wait for command buffer completion
    void WaitForCompletion(CommandBufferRing::Entry* entry);
    
    // Process completed token
    void ProcessCompletedToken(const float* logits, int vocab_size);
    
    // Sample token from logits
    uint32_t SampleToken(const float* logits, int vocab_size, float& confidence);
    
    // Members
    VulkanCompute* vulkan_;
    PersistentState state_;
    CommandBufferRing ring_;
    
    // Threading
    std::thread loop_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    
    // Token queue
    std::queue<TokenBatch> batch_queue_;
    std::mutex batch_mutex_;
    std::condition_variable batch_cv_;
    
    // Result queue
    std::queue<std::pair<uint32_t, float>> result_queue_;
    std::mutex result_mutex_;
    std::condition_variable result_cv_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PersistentLoopStats stats_;
    
    // Timing
    std::chrono::steady_clock::time_point last_dispatch_time_;
    std::chrono::steady_clock::time_point last_complete_time_;
};

// Inline implementations

inline bool PersistentGPULoop::IsRunning() const {
    return running_.load();
}

inline PersistentLoopStats PersistentGPULoop::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void PersistentGPULoop::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

} // namespace RawrXD