// token_prefetch.cpp - Implementation of token prefetch on idle
// Part of the Copilot-like inference pipeline.

#include "token_prefetch.h"
#include <algorithm>

namespace RawrXD {

TokenPrefetch::TokenPrefetch() {
    stats_ = {};
    last_activity_ = std::chrono::steady_clock::now();
}

TokenPrefetch::~TokenPrefetch() {
    Stop();
}

void TokenPrefetch::SetConfig(const Config& config) {
    config_ = config;
}

void TokenPrefetch::Start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    stop_flag_.store(false);
    
    if (config_.enable_background) {
        prefetch_thread_ = std::thread(&TokenPrefetch::PrefetchThreadLoop, this);
    }
}

void TokenPrefetch::Stop() {
    if (!running_.load()) {
        return;
    }
    
    stop_flag_.store(true);
    running_.store(false);
    
    queue_cv_.notify_all();
    
    if (prefetch_thread_.joinable()) {
        prefetch_thread_.join();
    }
}

void TokenPrefetch::QueuePrefetch(const PrefetchRequest& request) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Update activity time
    {
        std::lock_guard<std::mutex> activity_lock(activity_mutex_);
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    request_queue_.push(request);
    queue_cv_.notify_one();
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_prefetches++;
    }
}

void TokenPrefetch::QueuePrefetchBatch(const std::vector<PrefetchRequest>& requests) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Update activity time
    {
        std::lock_guard<std::mutex> activity_lock(activity_mutex_);
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    for (const auto& request : requests) {
        request_queue_.push(request);
    }
    queue_cv_.notify_one();
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_prefetches += static_cast<int>(requests.size());
    }
}

bool TokenPrefetch::TryGetPrefetchResult(
    const std::string& context,
    PrefetchResult& result
) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    uint64_t hash = HashContext(context);
    auto it = result_cache_.find(hash);
    
    if (it == result_cache_.end() || !it->second.is_valid) {
        // Cache miss
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cache_misses++;
        if (stats_.cache_hits + stats_.cache_misses > 0) {
            stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                             (stats_.cache_hits + stats_.cache_misses);
        }
        return false;
    }
    
    // Cache hit
    result = it->second;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cache_hits++;
        if (stats_.cache_hits + stats_.cache_misses > 0) {
            stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                             (stats_.cache_hits + stats_.cache_misses);
        }
        stats_.avg_cache_hit_latency = (stats_.avg_cache_hit_latency * (stats_.cache_hits - 1) + 
                                        result.latency) / stats_.cache_hits;
    }
    
    return true;
}

void TokenPrefetch::InvalidatePrefetch(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Remove all results for this file
    for (auto it = result_cache_.begin(); it != result_cache_.end(); ) {
        // Note: In practice, we'd need to track file_path in PrefetchResult
        // For now, just invalidate all
        it->second.is_valid = false;
        ++it;
    }
}

void TokenPrefetch::ClearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    result_cache_.clear();
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cache_size = 0;
    }
}

void TokenPrefetch::PrefetchThreadLoop() {
    while (!stop_flag_.load()) {
        PrefetchRequest request;
        
        // Wait for request or idle timeout
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Wait for request or idle timeout
            bool timeout = !queue_cv_.wait_for(lock, config_.idle_threshold, [this]() {
                return !request_queue_.empty() || stop_flag_.load();
            });
            
            if (stop_flag_.load()) {
                break;
            }
            
            // Check if idle
            if (timeout && !IsIdle()) {
                continue;  // Not idle, wait more
            }
            
            // Get request
            if (request_queue_.empty()) {
                continue;
            }
            
            request = request_queue_.front();
            request_queue_.pop();
        }
        
        // Process request
        ProcessPrefetch(request);
    }
}

void TokenPrefetch::ProcessPrefetch(const PrefetchRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Check if we already have this cached
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        uint64_t hash = HashContext(request.context);
        auto it = result_cache_.find(hash);
        
        if (it != result_cache_.end() && it->second.is_valid) {
            // Already cached
            return;
        }
    }
    
    // Generate completion
    // TODO: Call actual inference
    PrefetchResult result;
    result.context = request.context;
    result.completion = "";  // Placeholder
    result.confidence = 0.9f;
    result.kernel_used = 1;  // Q4_K for fast prefetch
    result.is_valid = true;
    result.created = std::chrono::steady_clock::now();
    
    auto end_time = std::chrono::steady_clock::now();
    result.latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Cache result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Evict if cache is full
        while (result_cache_.size() >= config_.max_cache_size) {
            EvictLRU();
        }
        
        uint64_t hash = HashContext(request.context);
        result_cache_[hash] = result;
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.successful_prefetches++;
            stats_.cache_size = result_cache_.size();
            stats_.avg_prefetch_latency = (stats_.avg_prefetch_latency * (stats_.successful_prefetches - 1) + 
                                           result.latency) / stats_.successful_prefetches;
        }
    }
    
    // Call completion callback
    if (completion_callback_) {
        completion_callback_(request.context, result);
    }
}

std::vector<PrefetchRequest> TokenPrefetch::PredictPrefetches(
    const std::string& context,
    int cursor_line,
    int cursor_column
) {
    std::vector<PrefetchRequest> predictions;
    
    if (!config_.enable_predictive) {
        return predictions;
    }
    
    // Predict based on patterns
    // TODO: Implement pattern-based prediction
    
    // Example patterns:
    // 1. After period: predict function completion
    // 2. After newline: predict next line
    // 3. After opening brace: predict closing brace
    
    // For now, just create a single prediction
    PrefetchRequest request;
    request.context = context;
    request.cursor_line = cursor_line;
    request.cursor_column = cursor_column;
    request.max_tokens = config_.max_tokens_per_prefetch;
    request.priority = 5;
    request.created = std::chrono::steady_clock::now();
    
    predictions.push_back(request);
    
    return predictions;
}

bool TokenPrefetch::IsIdle() const {
    std::lock_guard<std::mutex> lock(activity_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_);
    return elapsed >= config_.idle_threshold;
}

void TokenPrefetch::EvictLRU() {
    // Find least recently used result
    auto oldest = result_cache_.end();
    auto oldest_time = std::chrono::steady_clock::now();
    
    for (auto it = result_cache_.begin(); it != result_cache_.end(); ++it) {
        if (it->second.created < oldest_time) {
            oldest_time = it->second.created;
            oldest = it;
        }
    }
    
    if (oldest != result_cache_.end()) {
        result_cache_.erase(oldest);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.evictions++;
            stats_.cache_size = result_cache_.size();
        }
    }
}

} // namespace RawrXD