// final_production_pipeline.h - Final production pipeline integrating all Phase 2/3 upgrades
// This is the complete system that surpasses Copilot.
//
// Architecture:
//   Layer 1: Compute (Q4_K/Q5_K/Q6_K kernels, Vulkan execution)
//   Layer 2: Scheduling (kernel arbiter, mid-generation switching, early exit, dual-stream)
//   Layer 3: Perception (adaptive debounce, cancellation, prefetch, prefix pinning, KV reuse)
//   Layer 4: Persistence (persistent GPU loop, KV paging, async overlap)
//   Layer 5: Intelligence (predictive scheduling, multi-model arbitration, context heat map)
//
// This is a latency-aware, perception-optimized inference engine.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "production_pipeline.h"
#include "persistent_gpu_loop.h"
#include "kv_paging.h"
#include "async_overlap.h"
#include "predictive_scheduler.h"
#include "multi_model_arbitration.h"
#include "context_heat_map.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace RawrXD {

// Final production pipeline configuration
struct FinalProductionConfig {
    // Phase 1 config (from production_pipeline.h)
    ProductionConfig phase1_config;
    
    // Phase 2 config
    struct {
        bool enable_persistent_gpu_loop = true;
        bool enable_kv_paging = true;
        bool enable_async_overlap = true;
    } phase2;
    
    // Phase 3 config
    struct {
        bool enable_predictive_scheduler = true;
        bool enable_multi_model_arbitration = true;
        bool enable_context_heat_map = true;
    } phase3;
};

// Final production pipeline statistics
struct FinalProductionStats {
    // Phase 1 stats
    ProductionStats phase1_stats;
    
    // Phase 2 stats
    PersistentLoopStats persistent_loop_stats;
    PagingStats kv_paging_stats;
    AsyncOverlapStats async_overlap_stats;
    
    // Phase 3 stats
    PredictiveStats predictive_stats;
    ArbitrationStats arbitration_stats;
    HeatMapStats heat_map_stats;
    
    // Overall
    std::chrono::microseconds avg_first_token_latency;
    std::chrono::microseconds avg_per_token_latency;
    float cache_hit_rate;
    float early_exit_rate;
    float speculative_acceptance_rate;
    float predictive_accuracy;
    float multi_model_efficiency;
};

// Final production pipeline
class FinalProductionPipeline {
public:
    FinalProductionPipeline();
    ~FinalProductionPipeline();
    
    // Initialize pipeline
    bool Initialize(const FinalProductionConfig& config);
    
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
    FinalProductionStats GetStats() const;
    
    // Get optimization suggestions
    std::vector<std::string> GetOptimizationSuggestions() const;
    
    // Export latency profile
    void ExportLatencyProfile(const std::string& filename) const;
    
    // Get breakdown report
    std::string GetBreakdownReport() const;
    
private:
    // Process completion request
    void ProcessRequest(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Apply Phase 2 optimizations
    void ApplyPhase2Optimizations(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Apply Phase 3 optimizations
    void ApplyPhase3Optimizations(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Members
    FinalProductionConfig config_;
    
    // Phase 1 pipeline
    std::unique_ptr<ProductionPipeline> phase1_pipeline_;
    
    // Phase 2 components
    std::unique_ptr<PersistentGPULoop> persistent_loop_;
    std::unique_ptr<KVPaging> kv_paging_;
    std::unique_ptr<AsyncOverlap> async_overlap_;
    
    // Phase 3 components
    std::unique_ptr<PredictiveScheduler> predictive_scheduler_;
    std::unique_ptr<MultiModelArbitration> multi_model_arbitration_;
    std::unique_ptr<ContextHeatMap> context_heat_map_;
    
    // State
    std::atomic<bool> generating_{false};
    mutable std::mutex stats_mutex_;
};

// Inline implementations

inline bool FinalProductionPipeline::IsGenerating() const {
    return generating_.load();
}

inline FinalProductionStats FinalProductionPipeline::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    FinalProductionStats stats;
    
    if (phase1_pipeline_) {
        stats.phase1_stats = phase1_pipeline_->GetStats();
    }
    
    if (persistent_loop_) {
        stats.persistent_loop_stats = persistent_loop_->GetStats();
    }
    
    if (kv_paging_) {
        stats.kv_paging_stats = kv_paging_->GetStats();
    }
    
    if (async_overlap_) {
        stats.async_overlap_stats = async_overlap_->GetStats();
    }
    
    if (predictive_scheduler_) {
        stats.predictive_stats = predictive_scheduler_->GetStats();
    }
    
    if (multi_model_arbitration_) {
        stats.arbitration_stats = multi_model_arbitration_->GetStats();
    }
    
    if (context_heat_map_) {
        stats.heat_map_stats = context_heat_map_->GetStats();
    }
    
    // Calculate overall stats
    if (stats.phase1_stats.latency_stats.total_tokens > 0) {
        stats.avg_first_token_latency = stats.phase1_stats.latency_stats.avg_first_token;
        stats.avg_per_token_latency = stats.phase1_stats.latency_stats.avg_per_token;
    }
    
    stats.cache_hit_rate = stats.phase1_stats.cache_hit_rate;
    stats.early_exit_rate = stats.phase1_stats.early_exit_rate;
    stats.speculative_acceptance_rate = stats.phase1_stats.speculative_acceptance_rate;
    stats.predictive_accuracy = stats.predictive_stats.accuracy;
    stats.multi_model_efficiency = stats.arbitration_stats.avg_quality;
    
    return stats;
}

} // namespace RawrXD