// multi_model_arbitration.h - Multi-model arbitration for instant draft + background refinement
// Implements small model for instant draft, big model for background refinement
//
// Architecture:
//   - Small model (Q4_K): Instant draft, always running
//   - Big model (Q6_K): Background refinement, runs when idle
//   - Arbitration: Decide which model to use based on latency budget
//
// Key insight:
//   - Not: One model for everything
//   - But: Multiple models, each optimized for different latency/quality tradeoff
//
// This is where we surpass Copilot.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "model_residency.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Model tier for arbitration
enum class ModelTier : uint8_t {
    SMALL = 0,      // Small model (Q4_K, instant)
    MEDIUM = 1,     // Medium model (Q5_K, balanced)
    LARGE = 2,      // Large model (Q6_K, quality)
};

// Model information
struct ModelInfo {
    std::string name;
    ModelTier tier;
    size_t vram_required;
    size_t ram_required;
    std::chrono::milliseconds load_time;
    std::chrono::milliseconds avg_latency;
    float quality_score;
    bool is_loaded;
    bool is_active;
};

// Arbitration decision
struct ArbitrationDecision {
    ModelTier selected_tier;
    std::string model_name;
    std::string reason;
    float confidence;
    std::chrono::milliseconds expected_latency;
    float expected_quality;
};

// Arbitration statistics
struct ArbitrationStats {
    int total_requests;
    int small_model_requests;
    int medium_model_requests;
    int large_model_requests;
    float small_model_percentage;
    float medium_model_percentage;
    float large_model_percentage;
    std::chrono::microseconds avg_small_latency;
    std::chrono::microseconds avg_medium_latency;
    std::chrono::microseconds avg_large_latency;
    float avg_quality;
};

// Multi-model arbitration
class MultiModelArbitration {
public:
    MultiModelArbitration();
    ~MultiModelArbitration();
    
    // Configure arbitration
    struct Config {
        std::chrono::milliseconds latency_budget{100};  // Max latency for first token
        float quality_threshold = 0.8f;                // Min quality score
        bool enable_small_model = true;              // Enable small model
        bool enable_medium_model = true;             // Enable medium model
        bool enable_large_model = true;              // Enable large model
        bool enable_background_refinement = true;      // Refine in background
        std::chrono::milliseconds refinement_timeout{500};  // Timeout for refinement
    };
    void SetConfig(const Config& config);
    
    // Register model
    void RegisterModel(const ModelInfo& model);
    
    // Unregister model
    void UnregisterModel(const std::string& model_name);
    
    // Select model for request
    ArbitrationDecision SelectModel(
        const std::string& context,
        std::chrono::milliseconds latency_budget
    );
    
    // Start background refinement
    void StartBackgroundRefinement(
        const std::string& context,
        const std::string& draft_completion
    );
    
    // Get refined completion
    bool GetRefinedCompletion(std::string& completion);
    
    // Cancel background refinement
    void CancelBackgroundRefinement();
    
    // Get statistics
    ArbitrationStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Calculate model score
    float CalculateScore(
        const ModelInfo& model,
        std::chrono::milliseconds latency_budget
    ) const;
    
    // Find best model for latency budget
    ModelInfo* FindBestModel(std::chrono::milliseconds latency_budget);
    
    // Background refinement thread
    void RefinementThread(
        const std::string& context,
        const std::string& draft_completion
    );
    
    // Members
    Config config_;
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, ModelInfo> models_;
    
    // Background refinement
    std::thread refinement_thread_;
    std::atomic<bool> refining_{false};
    std::atomic<bool> stop_refinement_{false};
    std::string refined_completion_;
    mutable std::mutex refinement_mutex_;
    std::condition_variable refinement_cv_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ArbitrationStats stats_;
};

// Inline implementations

inline ArbitrationStats MultiModelArbitration::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void MultiModelArbitration::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

} // namespace RawrXD