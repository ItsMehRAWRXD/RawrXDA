// token_prefetch.h - Token prefetch on idle for instant perceived response
// Implements predictive prefetching when user pauses typing
//
// Strategy:
//   - When user pauses: predict_next_3_completions()
//   - When they type: suggestions are already computed
//   - This is one of the biggest "it feels psychic" effects
//
// Key insight:
//   - User pauses are predictable (after periods, newlines, etc.)
//   - Prefetch during idle time
//   - Have completions ready before user asks
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "kernel_arbiter.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Prefetch request
struct PrefetchRequest {
    std::string context;            // Context to prefetch from
    std::string file_path;          // File being edited
    int cursor_line;                // Cursor position
    int cursor_column;
    int max_tokens;                 // Max tokens to generate
    int priority;                   // Higher priority = prefetch first
    std::chrono::steady_clock::time_point created;
};

// Prefetch result
struct PrefetchResult {
    std::string completion;         // Generated completion
    std::string context;            // Context it was generated from
    float confidence;                // Model confidence
    int kernel_used;                // Which kernel was used
    std::chrono::microseconds latency;
    bool is_valid;                  // Whether result is still valid
    std::chrono::steady_clock::time_point created;
};

// Prefetch statistics
struct PrefetchStats {
    int total_prefetches;
    int successful_prefetches;
    int cache_hits;
    int cache_misses;
    int evictions;
    float hit_rate;
    std::chrono::microseconds avg_prefetch_latency;
    std::chrono::microseconds avg_cache_hit_latency;
    size_t cache_size;
};

// Token prefetch manager
class TokenPrefetch {
public:
    TokenPrefetch();
    ~TokenPrefetch();
    
    // Configure prefetch
    struct Config {
        int max_prefetch_count = 3;          // Prefetch N completions
        int max_tokens_per_prefetch = 50;    // Max tokens per completion
        std::chrono::milliseconds idle_threshold{500};  // Wait before prefetch
        std::chrono::milliseconds prefetch_timeout{2000};  // Timeout for prefetch
        bool enable_predictive = true;       // Predict based on patterns
        bool enable_background = true;        // Run in background thread
        size_t max_cache_size = 100;         // Max cached completions
        int priority_threshold = 5;          // Min priority to prefetch
    };
    void SetConfig(const Config& config);
    
    // Start prefetch thread
    void Start();
    
    // Stop prefetch thread
    void Stop();
    
    // Queue prefetch request
    void QueuePrefetch(const PrefetchRequest& request);
    
    // Queue multiple prefetch requests
    void QueuePrefetchBatch(const std::vector<PrefetchRequest>& requests);
    
    // Get prefetch result (if available)
    bool TryGetPrefetchResult(
        const std::string& context,
        PrefetchResult& result
    );
    
    // Check if prefetch is available
    bool HasPrefetchResult(const std::string& context) const;
    
    // Invalidate prefetch results (when context changes)
    void InvalidatePrefetch(const std::string& file_path);
    
    // Clear all prefetch results
    void ClearCache();
    
    // Get statistics
    PrefetchStats GetStats() const;
    
    // Set completion callback (for streaming)
    void SetCompletionCallback(
        std::function<void(const std::string& context, const PrefetchResult& result)> callback
    );
    
private:
    // Background thread loop
    void PrefetchThreadLoop();
    
    // Process prefetch request
    void ProcessPrefetch(const PrefetchRequest& request);
    
    // Predict what to prefetch based on context
    std::vector<PrefetchRequest> PredictPrefetches(
        const std::string& context,
        int cursor_line,
        int cursor_column
    );
    
    // Check if context is idle (no recent changes)
    bool IsIdle() const;
    
    // Evict least recently used from cache
    void EvictLRU();
    
    // Hash context for cache key
    uint64_t HashContext(const std::string& context) const;
    
    // Members
    Config config_;
    KernelArbiter arbiter_;
    
    // Thread management
    std::thread prefetch_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    
    // Request queue
    std::queue<PrefetchRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Result cache
    std::unordered_map<uint64_t, PrefetchResult> result_cache_;
    mutable std::mutex cache_mutex_;
    
    // Idle tracking
    std::chrono::steady_clock::time_point last_activity_;
    mutable std::mutex activity_mutex_;
    
    // Completion callback
    std::function<void(const std::string& context, const PrefetchResult& result)> completion_callback_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PrefetchStats stats_;
};

// Inline implementations

inline bool TokenPrefetch::HasPrefetchResult(const std::string& context) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    uint64_t hash = HashContext(context);
    auto it = result_cache_.find(hash);
    return it != result_cache_.end() && it->second.is_valid;
}

inline PrefetchStats TokenPrefetch::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void TokenPrefetch::SetCompletionCallback(
    std::function<void(const std::string& context, const PrefetchResult& result)> callback
) {
    completion_callback_ = callback;
}

inline uint64_t TokenPrefetch::HashContext(const std::string& context) const {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : context) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace RawrXD