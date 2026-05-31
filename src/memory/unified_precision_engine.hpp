// ============================================================================
// File: src/memory/unified_precision_engine.hpp
// ============================================================================

#ifndef RAWRXD_MEMORY_UNIFIED_PRECISION_ENGINE_HPP
#define RAWRXD_MEMORY_UNIFIED_PRECISION_ENGINE_HPP

#include "unified_memory_config.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace rawrxd {

// ============================================================================
// Precision Decision Engine
// ============================================================================

class PrecisionEngine {
public:
    explicit PrecisionEngine(const UnifiedMemoryConfig& config);
    
    // Main decision interface
    UnifiedMemoryConfig::Precision decidePrecision(
        int layer_index,
        const LayerMetadata& metadata,
        size_t current_budget,
        size_t required_bytes
    );
    
    UnifiedMemoryConfig::Compression decideCompression(
        int layer_index,
        const LayerMetadata& metadata,
        UnifiedMemoryConfig::Precision precision
    );
    
    // Layer classification
    void classifyLayers(const std::vector<LayerMetadata>& layers);
    
    // Batch decisions
    struct LayerDecision {
        int layer_index;
        UnifiedMemoryConfig::Precision precision;
        UnifiedMemoryConfig::Compression compression;
        size_t estimated_bytes;
        float confidence;
    };
    
    std::vector<LayerDecision> decideBatch(
        const std::vector<LayerMetadata>& layers,
        size_t total_budget
    );
    
    // Adaptive adjustment based on runtime feedback
    void updateImportance(int layer_index, float delta);
    void updateAccessPattern(int layer_index, uint64_t access_count);
    
    // Getters
    float getLayerImportance(int layer_index) const;
    UnifiedMemoryConfig::LayerImportance getLayerClass(int layer_index) const;
    
private:
    // Internal decision helpers
    UnifiedMemoryConfig::Precision selectPrecisionForImportance(
        UnifiedMemoryConfig::LayerImportance importance,
        float utilization
    );
    
    UnifiedMemoryConfig::Compression selectCompressionForPrecision(
        UnifiedMemoryConfig::Precision precision,
        const LayerMetadata& metadata
    );
    
    float estimateCompressionRatio(
        UnifiedMemoryConfig::Compression compression,
        const LayerMetadata& metadata
    ) const;
    
    // Data members
    const UnifiedMemoryConfig& config_;
    std::vector<float> layer_importance_scores_;
    std::vector<UnifiedMemoryConfig::LayerImportance> layer_classes_;
    std::vector<uint64_t> layer_access_counts_;
    
    std::atomic<float> current_utilization_{0.0f};
};

// ============================================================================
// Layer Profiler
// ============================================================================

class LayerProfiler {
public:
    explicit LayerProfiler(const UnifiedMemoryConfig& config);
    
    // Profile a layer to get metadata
    LayerMetadata profileLayer(
        int layer_index,
        const float* weights,
        size_t weight_count,
        const float* activations,
        size_t activation_count
    );
    
    // Batch profiling
    std::vector<LayerMetadata> profileAllLayers(
        const std::vector<std::pair<const float*, size_t>>& weight_layers,
        const std::vector<std::pair<const float*, size_t>>& activation_layers
    );
    
    // Update statistics incrementally
    void updateGradients(int layer_index, const float* gradients, size_t count);
    void updateActivations(int layer_index, const float* activations, size_t count);
    void recordAccess(int layer_index);
    
    // Get all metadata
    const std::vector<LayerMetadata>& getAllMetadata() const {
        return layer_metadata_;
    }
    
private:
    // Compute importance score from metrics
    float computeImportanceScore(
        float gradient_norm,
        float activation_variance,
        uint64_t access_count,
        float attention_entropy
    ) const;
    
    const UnifiedMemoryConfig& config_;
    std::vector<LayerMetadata> layer_metadata_;
    std::vector<float> running_gradient_norms_;
    std::vector<float> running_activation_variances_;
};

} // namespace rawrxd

#endif // RAWRXD_MEMORY_UNIFIED_PRECISION_ENGINE_HPP
