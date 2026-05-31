// latency_profiler.h - Cycle-accurate latency profiler for sub-100ms inference
// Implements detailed timing breakdown for CPU + GPU + UI pipeline
//
// This shows exactly where latency is coming from and how to shave it below 100ms.
//
// Timeline breakdown:
//   1. User keystroke → debounce
//   2. Context extraction
//   3. KV cache lookup
//   4. Tokenization
//   5. GPU dispatch
//   6. Kernel execution
//   7. Sampling
//   8. Detokenization
//   9. Ghost text render
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Latency breakdown for single token generation
struct TokenLatency {
    // CPU-side latency
    std::chrono::microseconds debounce_latency;
    std::chrono::microseconds context_extraction_latency;
    std::chrono::microseconds kv_cache_lookup_latency;
    std::chrono::microseconds tokenization_latency;
    std::chrono::microseconds dispatch_preparation_latency;
    
    // GPU-side latency
    std::chrono::microseconds gpu_dispatch_latency;
    std::chrono::microseconds kernel_execution_latency;
    std::chrono::microseconds result_retrieval_latency;
    
    // Post-processing latency
    std::chrono::microseconds sampling_latency;
    std::chrono::microseconds detokenization_latency;
    std::chrono::microseconds ghost_text_latency;
    
    // Total latency
    std::chrono::microseconds total_latency;
    
    // Kernel used
    int kernel_used;
    
    // Token index
    int token_index;
    
    // Timestamp
    std::chrono::steady_clock::time_point timestamp;
};

// Generation latency (multiple tokens)
struct GenerationLatency {
    std::vector<TokenLatency> tokens;
    
    // Aggregated stats
    std::chrono::microseconds first_token_latency;
    std::chrono::microseconds avg_token_latency;
    std::chrono::microseconds total_latency;
    int tokens_generated;
    int kernel_switches;
    
    // Breakdown by stage
    std::chrono::microseconds total_cpu_time;
    std::chrono::microseconds total_gpu_time;
    std::chrono::microseconds total_post_time;
    
    // Percentage breakdown
    float cpu_percent;
    float gpu_percent;
    float post_percent;
};

// Profiler statistics
struct ProfilerStats {
    int total_generations;
    int total_tokens;
    
    // Average latencies
    std::chrono::microseconds avg_first_token;
    std::chrono::microseconds avg_per_token;
    std::chrono::microseconds avg_total;
    
    // Stage breakdown
    std::chrono::microseconds avg_debounce;
    std::chrono::microseconds avg_context_extraction;
    std::chrono::microseconds avg_kv_cache_lookup;
    std::chrono::microseconds avg_tokenization;
    std::chrono::microseconds avg_dispatch_preparation;
    std::chrono::microseconds avg_gpu_dispatch;
    std::chrono::microseconds avg_kernel_execution;
    std::chrono::microseconds avg_result_retrieval;
    std::chrono::microseconds avg_sampling;
    std::chrono::microseconds avg_detokenization;
    std::chrono::microseconds avg_ghost_text;
    
    // Percentiles
    std::chrono::microseconds p50_latency;
    std::chrono::microseconds p90_latency;
    std::chrono::microseconds p99_latency;
    
    // Kernel usage
    int q4k_tokens;
    int q5k_tokens;
    int q6k_tokens;
    
    // Cache hits
    int kv_cache_hits;
    int kv_cache_misses;
    float kv_cache_hit_rate;
};

// Latency profiler
class LatencyProfiler {
public:
    LatencyProfiler();
    ~LatencyProfiler();
    
    // Configure profiler
    struct Config {
        bool enable_profiling = true;
        bool enable_detailed_timing = true;
        bool enable_percentiles = true;
        int max_generations = 1000;  // Keep last N generations
        int max_tokens = 10000;      // Keep last N tokens
        bool enable_file_output = false;
        std::string output_file = "latency_profile.csv";
    };
    void SetConfig(const Config& config);
    
    // Start timing a generation
    void StartGeneration();
    
    // Start timing a token
    void StartToken();
    
    // Record stage timing
    void RecordDebounce(std::chrono::microseconds latency);
    void RecordContextExtraction(std::chrono::microseconds latency);
    void RecordKVCacheLookup(std::chrono::microseconds latency);
    void RecordTokenization(std::chrono::microseconds latency);
    void RecordDispatchPreparation(std::chrono::microseconds latency);
    void RecordGPUDispatch(std::chrono::microseconds latency);
    void RecordKernelExecution(std::chrono::microseconds latency);
    void RecordResultRetrieval(std::chrono::microseconds latency);
    void RecordSampling(std::chrono::microseconds latency);
    void RecordDetokenization(std::chrono::microseconds latency);
    void RecordGhostText(std::chrono::microseconds latency);
    
    // End token timing
    void EndToken(int kernel_used);
    
    // End generation timing
    void EndGeneration();
    
    // Get current generation latency
    GenerationLatency GetCurrentGeneration() const;
    
    // Get statistics
    ProfilerStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
    // Export to CSV
    void ExportCSV(const std::string& filename) const;
    
    // Get breakdown report
    std::string GetBreakdownReport() const;
    
    // Get optimization suggestions
    std::vector<std::string> GetOptimizationSuggestions() const;
    
private:
    // Calculate percentiles
    void CalculatePercentiles();
    
    // Update statistics
    void UpdateStats();
    
    // Members
    Config config_;
    
    // Current generation
    GenerationLatency current_generation_;
    TokenLatency current_token_;
    
    // History
    std::vector<GenerationLatency> generation_history_;
    std::vector<TokenLatency> token_history_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ProfilerStats stats_;
    
    // Timing state
    std::chrono::steady_clock::time_point generation_start_;
    std::chrono::steady_clock::time_point token_start_;
    std::chrono::steady_clock::time_point stage_start_;
    
    // File output
    std::ofstream output_file_;
};

// Inline implementations

inline void LatencyProfiler::StartGeneration() {
    if (!config_.enable_profiling) return;
    
    generation_start_ = std::chrono::steady_clock::now();
    current_generation_ = GenerationLatency();
}

inline void LatencyProfiler::StartToken() {
    if (!config_.enable_profiling) return;
    
    token_start_ = std::chrono::steady_clock::now();
    current_token_ = TokenLatency();
    current_token_.timestamp = token_start_;
}

inline void LatencyProfiler::RecordDebounce(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.debounce_latency = latency;
}

inline void LatencyProfiler::RecordContextExtraction(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.context_extraction_latency = latency;
}

inline void LatencyProfiler::RecordKVCacheLookup(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.kv_cache_lookup_latency = latency;
}

inline void LatencyProfiler::RecordTokenization(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.tokenization_latency = latency;
}

inline void LatencyProfiler::RecordDispatchPreparation(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.dispatch_preparation_latency = latency;
}

inline void LatencyProfiler::RecordGPUDispatch(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.gpu_dispatch_latency = latency;
}

inline void LatencyProfiler::RecordKernelExecution(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.kernel_execution_latency = latency;
}

inline void LatencyProfiler::RecordResultRetrieval(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.result_retrieval_latency = latency;
}

inline void LatencyProfiler::RecordSampling(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.sampling_latency = latency;
}

inline void LatencyProfiler::RecordDetokenization(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.detokenization_latency = latency;
}

inline void LatencyProfiler::RecordGhostText(std::chrono::microseconds latency) {
    if (!config_.enable_profiling) return;
    current_token_.ghost_text_latency = latency;
}

inline void LatencyProfiler::EndToken(int kernel_used) {
    if (!config_.enable_profiling) return;
    
    auto end_time = std::chrono::steady_clock::now();
    current_token_.total_latency = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - token_start_);
    current_token_.kernel_used = kernel_used;
    current_token_.token_index = static_cast<int>(current_generation_.tokens.size());
    
    // Add to current generation
    current_generation_.tokens.push_back(current_token_);
    
    // Update totals
    current_generation_.total_cpu_time += 
        current_token_.debounce_latency +
        current_token_.context_extraction_latency +
        current_token_.kv_cache_lookup_latency +
        current_token_.tokenization_latency +
        current_token_.dispatch_preparation_latency;
    
    current_generation_.total_gpu_time += 
        current_token_.gpu_dispatch_latency +
        current_token_.kernel_execution_latency +
        current_token_.result_retrieval_latency;
    
    current_generation_.total_post_time += 
        current_token_.sampling_latency +
        current_token_.detokenization_latency +
        current_token_.ghost_text_latency;
    
    // Update kernel usage
    if (kernel_used == 1) {
        stats_.q4k_tokens++;
    } else if (kernel_used == 2) {
        stats_.q5k_tokens++;
    } else if (kernel_used == 4) {
        stats_.q6k_tokens++;
    }
}

inline void LatencyProfiler::EndGeneration() {
    if (!config_.enable_profiling) return;
    
    auto end_time = std::chrono::steady_clock::now();
    current_generation_.total_latency = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - generation_start_);
    
    current_generation_.tokens_generated = static_cast<int>(current_generation_.tokens.size());
    
    if (!current_generation_.tokens.empty()) {
        current_generation_.first_token_latency = current_generation_.tokens[0].total_latency;
        
        std::chrono::microseconds total;
        for (const auto& token : current_generation_.tokens) {
            total += token.total_latency;
        }
        current_generation_.avg_token_latency = total / current_generation_.tokens_generated;
    }
    
    // Calculate percentages
    auto total = current_generation_.total_cpu_time + 
                 current_generation_.total_gpu_time + 
                 current_generation_.total_post_time;
    
    if (total.count() > 0) {
        current_generation_.cpu_percent = 
            static_cast<float>(current_generation_.total_cpu_time.count()) / total.count() * 100.0f;
        current_generation_.gpu_percent = 
            static_cast<float>(current_generation_.total_gpu_time.count()) / total.count() * 100.0f;
        current_generation_.post_percent = 
            static_cast<float>(current_generation_.total_post_time.count()) / total.count() * 100.0f;
    }
    
    // Add to history
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        generation_history_.push_back(current_generation_);
        
        // Trim history
        while (generation_history_.size() > static_cast<size_t>(config_.max_generations)) {
            generation_history_.erase(generation_history_.begin());
        }
        
        for (const auto& token : current_generation_.tokens) {
            token_history_.push_back(token);
        }
        
        // Trim token history
        while (token_history_.size() > static_cast<size_t>(config_.max_tokens)) {
            token_history_.erase(token_history_.begin());
        }
    }
    
    // Update statistics
    UpdateStats();
}

inline ProfilerStats LatencyProfiler::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace RawrXD