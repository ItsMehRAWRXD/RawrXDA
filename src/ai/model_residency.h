// model_residency.h - Model residency tier management for low-latency inference
// Implements HOT/WARM/COLD model residency to prevent cold start spikes and VRAM thrash
// 
// Residency tiers:
//   HOT  - Always resident in VRAM (Q4_K quantized, instant access)
//   WARM - Partially resident (Q5_K, fast load from system RAM)
//   COLD - On-demand (Q6_K, loaded from disk when needed)
//
// This prevents:
//   - Cold start spikes (seconds of latency)
//   - VRAM thrash when switching models
//   - Unnecessary re-encoding of context
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD {

// Residency tier levels
enum class ModelTier : uint8_t {
    HOT = 0,    // Always resident in VRAM (Q4_K)
    WARM = 1,   // Partially resident (Q5_K)
    COLD = 2,   // On-demand (Q6_K)
};

// Model metadata
struct ModelInfo {
    std::string name;               // e.g., "codestral:22b"
    std::string path;                // Path to model weights
    size_t vram_required;           // Bytes needed in VRAM
    size_t ram_required;             // Bytes needed in system RAM
    int quantization_level;          // 4, 5, or 6 for Q4_K, Q5_K, Q6_K
    ModelTier default_tier;          // Preferred residency tier
    std::chrono::milliseconds load_time;  // Average load time
    int access_count;                // How often this model is used
    std::chrono::steady_clock::time_point last_access;
};

// Residency slot in VRAM
struct ResidencySlot {
    std::string model_name;
    ModelTier tier;
    void* vram_ptr;                  // GPU memory pointer
    size_t vram_size;
    bool is_loading;
    bool is_valid;
    std::chrono::steady_clock::time_point loaded_at;
};

// Residency statistics
struct ResidencyStats {
    int hot_models_loaded;
    int warm_models_loaded;
    int cold_loads;
    int cache_evictions;
    int tier_promotions;             // COLD -> WARM -> HOT
    int tier_demotions;              // HOT -> WARM -> COLD
    std::chrono::microseconds avg_hot_access_latency;
    std::chrono::microseconds avg_warm_access_latency;
    std::chrono::microseconds avg_cold_load_latency;
    size_t vram_used;
    size_t vram_total;
};

// Model residency manager
class ModelResidencyManager {
public:
    ModelResidencyManager(size_t vram_budget_gb = 24.0);
    ~ModelResidencyManager();
    
    // Register a model with its metadata
    void RegisterModel(const ModelInfo& model);
    
    // Unregister a model
    void UnregisterModel(const std::string& model_name);
    
    // Request model access (promotes to appropriate tier)
    // Returns immediately if HOT, blocks if COLD
    void* RequestModel(
        const std::string& model_name,
        ModelTier requested_tier = ModelTier::HOT,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)
    );
    
    // Release model (allows demotion)
    void ReleaseModel(const std::string& model_name);
    
    // Pin model to HOT tier (prevents eviction)
    void PinModel(const std::string& model_name);
    
    // Unpin model (allows demotion)
    void UnpinModel(const std::string& model_name);
    
    // Promote model to higher tier
    bool PromoteModel(const std::string& model_name);
    
    // Demote model to lower tier
    bool DemoteModel(const std::string& model_name);
    
    // Prefetch model into WARM tier (background load)
    void PrefetchModel(const std::string& model_name);
    
    // Get current residency status
    ModelTier GetModelTier(const std::string& model_name) const;
    bool IsModelResident(const std::string& model_name) const;
    
    // Get statistics
    ResidencyStats GetStats() const;
    
    // Set VRAM budget
    void SetVRAMBudget(size_t budget_gb);
    
    // Set residency policy
    struct Policy {
        int max_hot_models = 2;              // Max models in HOT tier
        int max_warm_models = 4;              // Max models in WARM tier
        std::chrono::seconds hot_timeout{300}; // Demote after 5 min idle
        std::chrono::seconds warm_timeout{600}; // Demote after 10 min idle
        bool auto_promote = true;             // Auto-promote frequently used
        bool auto_demote = true;              // Auto-demote idle models
        int promotion_threshold = 5;         // Accesses before promotion
    };
    void SetPolicy(const Policy& policy);
    
    // Background thread for tier management
    void StartBackgroundThread();
    void StopBackgroundThread();
    
private:
    // Internal model state
    struct ModelState {
        ModelInfo info;
        ModelTier current_tier;
        ResidencySlot slot;
        bool is_pinned;
        int pending_requests;
        std::mutex mutex;
        std::condition_variable cv;
    };
    
    // Load model into VRAM
    bool LoadModelIntoVRAM(ModelState& model);
    
    // Unload model from VRAM
    void UnloadModelFromVRAM(ModelState& model);
    
    // Evict least recently used model from tier
    bool EvictLRU(ModelTier tier);
    
    // Background tier management loop
    void BackgroundThreadLoop();
    
    // Find model state
    ModelState* FindModel(const std::string& model_name);
    const ModelState* FindModel(const std::string& model_name) const;
    
    // Members
    size_t vram_budget_;
    size_t vram_used_;
    Policy policy_;
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, std::unique_ptr<ModelState>> models_;
    
    // HOT tier slots (limited by max_hot_models)
    std::unordered_set<std::string> hot_slots_;
    
    // WARM tier slots (limited by max_warm_models)
    std::unordered_set<std::string> warm_slots_;
    
    // Background thread
    std::thread background_thread_;
    std::atomic<bool> background_running_{false};
    std::condition_variable background_cv_;
    std::mutex background_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ResidencyStats stats_;
};

// Inline implementations

inline ModelTier ModelResidencyManager::GetModelTier(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return ModelTier::COLD;
    }
    return it->second->current_tier;
}

inline bool ModelResidencyManager::IsModelResident(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    auto it = models_.find(model_name);
    if (it == models_.end()) {
        return false;
    }
    return it->second->current_tier == ModelTier::HOT || 
           it->second->current_tier == ModelTier::WARM;
}

inline ResidencyStats ModelResidencyManager::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace RawrXD