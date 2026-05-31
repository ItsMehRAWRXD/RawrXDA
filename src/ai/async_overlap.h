// async_overlap.h - Async overlap perfection for CPU/GPU/UI pipeline
// Implements fully overlapped CPU prep, GPU exec, UI render
//
// Architecture:
//   - CPU prep: Prepare next token while GPU executes current
//   - GPU exec: Never idle, always has work
//   - UI render: Stream tokens as they complete
//
// Key insight:
//   - Not: CPU → GPU → CPU → GPU → UI
//   - But: CPU prep | GPU exec | UI render (all concurrent)
//
// This is the final piece for sub-100ms perceived latency.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "persistent_gpu_loop.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace RawrXD {

// Async stage types
enum class AsyncStage : uint8_t {
    CPU_PREP = 0,       // CPU preparation
    GPU_EXEC = 1,       // GPU execution
    UI_RENDER = 2,      // UI rendering
    COMPLETE = 3,       // All stages complete
};

// Async work item
struct AsyncWorkItem {
    int token_index;
    uint32_t token_id;
    std::string token_text;
    float confidence;
    
    // Stage timing
    std::chrono::steady_clock::time_point cpu_start;
    std::chrono::steady_clock::time_point cpu_end;
    std::chrono::steady_clock::time_point gpu_start;
    std::chrono::steady_clock::time_point gpu_end;
    std::chrono::steady_clock::time_point ui_start;
    std::chrono::steady_clock::time_point ui_end;
    
    // Stage completion flags
    std::atomic<bool> cpu_done{false};
    std::atomic<bool> gpu_done{false};
    std::atomic<bool> ui_done{false};
};

// Async overlap statistics
struct AsyncOverlapStats {
    int total_tokens;
    
    // Stage latencies
    std::chrono::microseconds avg_cpu_latency;
    std::chrono::microseconds avg_gpu_latency;
    std::chrono::microseconds avg_ui_latency;
    
    // Overlap metrics
    std::chrono::microseconds avg_overlap_time;  // Time when all 3 stages are active
    float overlap_percentage;  // Percentage of time with full overlap
    float cpu_utilization;     // CPU utilization
    float gpu_utilization;     // GPU utilization
    float ui_utilization;      // UI thread utilization
    
    // Pipeline efficiency
    float pipeline_efficiency;  // Actual throughput / theoretical max
    int pipeline_stalls;        // Times pipeline had to wait
    int pipeline_bubbles;       // Times GPU was idle
};

// Async overlap manager
class AsyncOverlap {
public:
    AsyncOverlap(PersistentGPULoop* gpu_loop);
    ~AsyncOverlap();
    
    // Initialize async pipeline
    bool Initialize();
    
    // Start async pipeline
    void Start();
    
    // Stop async pipeline
    void Stop();
    
    // Submit work item
    void SubmitWork(const AsyncWorkItem& work);
    
    // Process work items (main loop)
    void ProcessLoop();
    
    // Set CPU prep callback
    void SetCPUPrepCallback(std::function<void(AsyncWorkItem&)> callback);
    
    // Set GPU exec callback
    void SetGPUExecCallback(std::function<void(AsyncWorkItem&)> callback);
    
    // Set UI render callback
    void SetUIRenderCallback(std::function<void(const AsyncWorkItem&)> callback);
    
    // Get statistics
    AsyncOverlapStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
    // Check if pipeline is running
    bool IsRunning() const { return running_.load(); }
    
private:
    // CPU prep thread
    void CPUPrepThread();
    
    // GPU exec thread
    void GPUExecThread();
    
    // UI render thread
    void UIRenderThread();
    
    // Pipeline coordinator thread
    void CoordinatorThread();
    
    // Members
    PersistentGPULoop* gpu_loop_;
    
    // Threading
    std::thread cpu_thread_;
    std::thread gpu_thread_;
    std::thread ui_thread_;
    std::thread coordinator_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    
    // Work queues
    std::queue<AsyncWorkItem> cpu_queue_;
    std::queue<AsyncWorkItem> gpu_queue_;
    std::queue<AsyncWorkItem> ui_queue_;
    std::queue<AsyncWorkItem> complete_queue_;
    
    std::mutex cpu_mutex_;
    std::mutex gpu_mutex_;
    std::mutex ui_mutex_;
    std::mutex complete_mutex_;
    
    std::condition_variable cpu_cv_;
    std::condition_variable gpu_cv_;
    std::condition_variable ui_cv_;
    std::condition_variable complete_cv_;
    
    // Callbacks
    std::function<void(AsyncWorkItem&)> cpu_prep_callback_;
    std::function<void(AsyncWorkItem&)> gpu_exec_callback_;
    std::function<void(const AsyncWorkItem&)> ui_render_callback_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    AsyncOverlapStats stats_;
    
    // Timing
    std::chrono::steady_clock::time_point pipeline_start_;
};

// Inline implementations

inline AsyncOverlapStats AsyncOverlap::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void AsyncOverlap::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

} // namespace RawrXD