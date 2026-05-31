// ============================================================================
// File: src/memory/unified_memory_system.hpp
// ============================================================================

#ifndef RAWRXD_MEMORY_UNIFIED_MEMORY_SYSTEM_HPP
#define RAWRXD_MEMORY_UNIFIED_MEMORY_SYSTEM_HPP

#include "unified_memory_config.hpp"
#include "unified_precision_engine.hpp"
#include "thinking_effort_adjuster.hpp"
#include "negative_range_model.hpp"
#include <memory>
#include <mutex>
#include <atomic>

namespace rawrxd {

// ============================================================================
// Unified Memory System - Main Facade
// ============================================================================

class UnifiedMemorySystem {
public:
    explicit UnifiedMemorySystem(const UnifiedMemoryConfig& config);
    ~UnifiedMemorySystem();
    
    // Prevent copying
    UnifiedMemorySystem(const UnifiedMemorySystem&) = delete;
    UnifiedMemorySystem& operator=(const UnifiedMemorySystem&) = delete;
    
    // ========================================================================
    // Initialization and Configuration
    // ========================================================================
    
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    void updateConfig(const UnifiedMemoryConfig& new_config);
    const UnifiedMemoryConfig& getConfig() const { return config_; }
    
    // ========================================================================
    // Layer Management
    // ========================================================================
    
    struct LayerInfo {
        int index;
        std::string type;
        size_t original_size;
        size_t compressed_size;
        UnifiedMemoryConfig::Precision precision;
        UnifiedMemoryConfig::Compression compression;
        float importance_score;
    };
    
    int registerLayer(
        const std::string& type,
        const float* weights,
        size_t weight_count,
        const float* activations,
        size_t activation_count
    );
    
    void unregisterLayer(int layer_id);
    LayerInfo getLayerInfo(int layer_id) const;
    std::vector<LayerInfo> getAllLayerInfo() const;
    
    // ========================================================================
    // Memory Optimization
    // ========================================================================
    
    struct OptimizationResult {
        bool success;
        size_t bytes_saved;
        size_t bytes_used;
        float compression_ratio;
        std::string message;
    };
    
    OptimizationResult optimizeLayer(int layer_id);
    OptimizationResult optimizeAllLayers();
    
    // Reoptimize based on updated importance/access patterns
    void rebalance();
    
    // ========================================================================
    // KV Cache Management (via ThinkingEffortAdjuster)
    // ========================================================================
    
    struct KVLayerPlan {
        int layer_index;
        UnifiedMemoryConfig::Precision kv_precision;
        size_t kv_budget_bytes;
        float importance_weight;
    };
    
    std::vector<KVLayerPlan> planKVCaches(
        size_t total_kv_budget,
        int sequence_length,
        int batch_size
    );
    
    void updateKVAccessPattern(int layer_index, uint64_t access_count);
    
    // ========================================================================
    // Weight Compression (via NegativeRangeModel)
    // ========================================================================
    
    struct WeightCompressionPlan {
        int layer_index;
        UnifiedMemoryConfig::Compression method;
        size_t original_bytes;
        size_t compressed_bytes;
        std::vector<uint8_t> compressed_data;
    };
    
    WeightCompressionPlan compressWeights(
        int layer_index,
        const float* weights,
        size_t weight_count
    );
    
    std::vector<float> decompressWeights(
        int layer_index,
        const std::vector<uint8_t>& compressed_data
    );
    
    // ========================================================================
    // Adaptive Decision Making
    // ========================================================================
    
    // Update layer importance based on runtime feedback
    void feedbackLayerImportance(int layer_index, float importance_delta);
    void feedbackLayerAccess(int layer_index, uint64_t access_count);
    
    // Get current memory status
    struct MemoryStatus {
        size_t total_bytes_used;
        size_t kv_bytes_used;
        size_t weight_bytes_used;
        float total_utilization;
        float kv_utilization;
        float weight_utilization;
        int num_layers_registered;
        int num_layers_optimized;
    };
    
    MemoryStatus getMemoryStatus() const;
    
    // ========================================================================
    // Thinking Effort Control
    // ========================================================================
    
    void setThinkingEffort(UnifiedMemoryConfig::EffortLevel level);
    UnifiedMemoryConfig::EffortLevel getThinkingEffort() const;
    
    // Advanced: direct control
    void setLayerEffortMultiplier(int layer_index, float multiplier);
    float getLayerEffortMultiplier(int layer_index) const;
    
private:
    // Internal helpers
    void applyPrecisionDecision(int layer_index, UnifiedMemoryConfig::Precision precision);
    void applyCompressionDecision(int layer_index, UnifiedMemoryConfig::Compression compression);
    
    bool validateLayerIndex(int layer_index) const;
    
    // Configuration
    UnifiedMemoryConfig config_;
    std::atomic<bool> initialized_{false};
    
    // Components
    std::unique_ptr<PrecisionEngine> precision_engine_;
    std::unique_ptr<LayerProfiler> layer_profiler_;
    std::unique_ptr<MemoryBudgetTracker> budget_tracker_;
    std::unique_ptr<ThinkingEffortAdjuster> thinking_effort_adjuster_;
    std::unique_ptr<NegativeRangeModel> negative_range_model_;
    
    // Layer state
    mutable std::mutex layers_mutex_;
    std::vector<LayerInfo> registered_layers_;
    std::vector<LayerMetadata> layer_metadata_;
    
    // Statistics
    std::atomic<size_t> total_bytes_saved_{0};
    std::atomic<int> optimization_count_{0};
};

} // namespace rawrxd

#endif // RAWRXD_MEMORY_UNIFIED_MEMORY_SYSTEM_HPP
