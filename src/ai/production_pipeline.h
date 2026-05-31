// production_pipeline.h - Production-ready Copilot-like inference pipeline
// Integrates all 10 critical improvements for sub-100ms latency
//
// Components:
//   1. Model residency tiers (HOT/WARM/COLD)
//   2. Mid-generation kernel switching
//   3. Hash-based KV-cache reuse
//   4. Adaptive debounce
//   5. Cancellation fast-path
//   6. Logit-level early exit
//   7. Dual-stream speculative decoding
//   8. Prefix pinning
//   9. Token prefetch on idle
//   10. Cycle-accurate latency profiler
//
// This is the complete production pipeline that makes it feel "instant".
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "model_residency.h"
#include "kernel_switcher.h"
#include "kv_cache_manager.h"
#include "adaptive_debounce.h"
#include "cancellation_manager.h"
#include "early_exit.h"
#include "dual_stream_speculative.h"
#include "prefix_pinning.h"
#include "token_prefetch.h"
#include "latency_profiler.h"
#include "streaming_inference_engine.h"
#include "ide_completion_bridge.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace RawrXD {

// Production pipeline configuration
struct ProductionConfig {
    // Model residency
    ModelResidencyManager::Policy residency_policy;
    
    // Kernel switching
    KernelSwitcher::Config kernel_config;
    
    // KV cache
    size_t kv_cache_max_entries = 100;
    size_t kv_cache_max_memory_mb = 1024;
    
    // Adaptive debounce
    AdaptiveDebounce::Config debounce_config;
    
    // Early exit
    EarlyExitManager::Config early_exit_config;
    
    // Dual-stream speculative
    DualStreamSpeculative::Config speculative_config;
    
    // Prefix pinning
    PrefixPinning::Config pinning_config;
    
    // Token prefetch
    TokenPrefetch::Config prefetch_config;
    
    // Latency profiler
    LatencyProfiler::Config profiler_config;
};

// Production pipeline statistics
struct ProductionStats {
    // Model residency
    ResidencyStats residency_stats;
    
    // Kernel switching
    SwitcherStats switcher_stats;
    
    // KV cache
    KVCacheStats kv_cache_stats;
    
    // Debounce
    DebounceStats debounce_stats;
    
    // Cancellation
    CancellationStats cancellation_stats;
    
    // Early exit
    EarlyExitStats early_exit_stats;
    
    // Dual-stream
    DualStreamStats speculative_stats;
    
    // Prefix pinning
    PinningStats pinning_stats;
    
    // Prefetch
    PrefetchStats prefetch_stats;
    
    // Latency
    ProfilerStats latency_stats;
    
    // Overall
    std::chrono::microseconds avg_first_token_latency;
    std::chrono::microseconds avg_total_latency;
    float cache_hit_rate;
    float early_exit_rate;
    float speculative_acceptance_rate;
};

// Production-ready Copilot-like pipeline
class ProductionPipeline {
public:
    ProductionPipeline();
    ~ProductionPipeline();
    
    // Initialize pipeline
    bool Initialize(const ProductionConfig& config);
    
    // Request completion (async)
    void RequestCompletion(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Cancel current completion
    void Cancel();
    
    // Accept ghost text (TAB)
    void Accept();
    
    // Reject ghost text (ESC)
    void Reject();
    
    // Get ghost text for rendering
    GhostText GetGhostText() const;
    
    // Check if generating
    bool IsGenerating() const;
    
    // Get statistics
    ProductionStats GetStats() const;
    
    // Get optimization suggestions
    std::vector<std::string> GetOptimizationSuggestions() const;
    
    // Export latency profile
    void ExportLatencyProfile(const std::string& filename) const;
    
private:
    // Process completion request
    void ProcessRequest(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Apply all optimizations
    void ApplyOptimizations(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Members
    ProductionConfig config_;
    
    // Core components
    std::unique_ptr<ModelResidencyManager> residency_manager_;
    std::unique_ptr<KernelSwitcher> kernel_switcher_;
    std::unique_ptr<KVCacheManager> kv_cache_manager_;
    std::unique_ptr<AdaptiveDebounce> adaptive_debounce_;
    std::unique_ptr<CancellationManager> cancellation_manager_;
    std::unique_ptr<EarlyExitManager> early_exit_manager_;
    std::unique_ptr<DualStreamSpeculative> speculative_decoder_;
    std::unique_ptr<PrefixPinning> prefix_pinning_;
    std::unique_ptr<TokenPrefetch> token_prefetch_;
    std::unique_ptr<LatencyProfiler> latency_profiler_;
    
    // Streaming engine
    std::unique_ptr<StreamingInferenceEngine> streaming_engine_;
    
    // IDE bridge
    std::unique_ptr<IDECompletionBridge> ide_bridge_;
    
    // State
    std::atomic<bool> generating_{false};
    std::atomic<uint64_t> current_request_id_{0};
    mutable std::mutex stats_mutex_;
};

// Inline implementations

inline bool ProductionPipeline::IsGenerating() const {
    return generating_.load();
}

inline ProductionStats ProductionPipeline::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    ProductionStats stats;
    
    if (residency_manager_) {
        stats.residency_stats = residency_manager_->GetStats();
    }
    
    if (kernel_switcher_) {
        stats.switcher_stats = kernel_switcher_->GetStats();
    }
    
    if (kv_cache_manager_) {
        stats.kv_cache_stats = kv_cache_manager_->GetStats();
    }
    
    if (adaptive_debounce_) {
        stats.debounce_stats = adaptive_debounce_->GetStats();
    }
    
    if (cancellation_manager_) {
        stats.cancellation_stats = cancellation_manager_->GetStats();
    }
    
    if (early_exit_manager_) {
        stats.early_exit_stats = early_exit_manager_->GetStats();
    }
    
    if (speculative_decoder_) {
        stats.speculative_stats = speculative_decoder_->GetStats();
    }
    
    if (prefix_pinning_) {
        stats.pinning_stats = prefix_pinning_->GetStats();
    }
    
    if (token_prefetch_) {
        stats.prefetch_stats = token_prefetch_->GetStats();
    }
    
    if (latency_profiler_) {
        stats.latency_stats = latency_profiler_->GetStats();
    }
    
    // Calculate overall stats
    if (stats.latency_stats.total_tokens > 0) {
        stats.avg_first_token_latency = stats.latency_stats.avg_first_token;
        stats.avg_total_latency = stats.latency_stats.avg_total;
    }
    
    if (stats.kv_cache_stats.total_entries > 0) {
        stats.cache_hit_rate = stats.kv_cache_stats.hit_rate;
    }
    
    if (stats.early_exit_stats.total_tokens > 0) {
        stats.early_exit_rate = stats.early_exit_stats.exit_rate;
    }
    
    if (stats.speculative_stats.total_tokens > 0) {
        stats.speculative_acceptance_rate = stats.speculative_stats.acceptance_rate;
    }
    
    return stats;
}

inline std::vector<std::string> ProductionPipeline::GetOptimizationSuggestions() const {
    std::vector<std::string> suggestions;
    
    if (latency_profiler_) {
        auto profiler_suggestions = latency_profiler_->GetOptimizationSuggestions();
        suggestions.insert(suggestions.end(), 
                          profiler_suggestions.begin(), 
                          profiler_suggestions.end());
    }
    
    // Add more suggestions based on stats
    auto stats = GetStats();
    
    if (stats.cache_hit_rate < 0.5f) {
        suggestions.push_back("Low KV cache hit rate. Consider increasing cache size or improving hash strategy.");
    }
    
    if (stats.early_exit_rate < 0.3f) {
        suggestions.push_back("Low early exit rate. Consider adjusting confidence thresholds.");
    }
    
    if (stats.speculative_acceptance_rate < 0.7f) {
        suggestions.push_back("Low speculative acceptance rate. Consider using Q5_K instead of Q4_K for draft.");
    }
    
    return suggestions;
}

inline void ProductionPipeline::ExportLatencyProfile(const std::string& filename) const {
    if (latency_profiler_) {
        latency_profiler_->ExportCSV(filename);
    }
}

} // namespace RawrXD