// ============================================================================
// File: src/memory/unified_precision_engine.cpp
// ============================================================================

#include "unified_precision_engine.hpp"
#include <algorithm>
#include <cmath>
#include <assert>

namespace rawrxd {

// ============================================================================
// PrecisionEngine Implementation
// ============================================================================

PrecisionEngine::PrecisionEngine(const UnifiedMemoryConfig& config)
    : config_(config)
{
}

UnifiedMemoryConfig::Precision PrecisionEngine::decidePrecision(
    int layer_index,
    const LayerMetadata& metadata,
    size_t current_budget,
    size_t required_bytes)
{
    // Check for override
    auto override_it = config_.layer_precision_overrides.find(layer_index);
    if (override_it != config_.layer_precision_overrides.end()) {
        return override_it->second;
    }
    
    // Check if critical layer
    if (config_.critical_layers.count(layer_index) > 0) {
        return UnifiedMemoryConfig::Precision::FP16;
    }
    
    // Check if skipped
    if (config_.skipped_layers.count(layer_index) > 0) {
        return UnifiedMemoryConfig::Precision::INT1;
    }
    
    // Adaptive decision based on importance and budget
    float utilization = static_cast<float>(current_budget) / config_.max_memory_bytes;
    current_utilization_.store(utilization);
    
    return selectPrecisionForImportance(metadata.importance_class, utilization);
}

UnifiedMemoryConfig::Compression PrecisionEngine::decideCompression(
    int layer_index,
    const LayerMetadata& metadata,
    UnifiedMemoryConfig::Precision precision)
{
    // Check for override
    auto override_it = config_.layer_compression_overrides.find(layer_index);
    if (override_it != config_.layer_compression_overrides.end()) {
        return override_it->second;
    }
    
    return selectCompressionForPrecision(precision, metadata);
}

void PrecisionEngine::classifyLayers(const std::vector<LayerMetadata>& layers) {
    layer_importance_scores_.resize(layers.size());
    layer_classes_.resize(layers.size());
    layer_access_counts_.resize(layers.size(), 0);
    
    for (size_t i = 0; i < layers.size(); ++i) {
        layer_importance_scores_[i] = layers[i].importance_score;
        layer_access_counts_[i] = layers[i].access_count;
        
        // Classify based on thresholds
        if (layers[i].importance_score >= config_.critical_threshold) {
            layer_classes_[i] = UnifiedMemoryConfig::LayerImportance::CRITICAL;
        } else if (layers[i].importance_score >= config_.importance_threshold) {
            layer_classes_[i] = UnifiedMemoryConfig::LayerImportance::IMPORTANT;
        } else if (layers[i].importance_score <= config_.reducible_threshold) {
            layer_classes_[i] = UnifiedMemoryConfig::LayerImportance::REDUCIBLE;
        } else {
            layer_classes_[i] = UnifiedMemoryConfig::LayerImportance::STANDARD;
        }
    }
}

std::vector<PrecisionEngine::LayerDecision> PrecisionEngine::decideBatch(
    const std::vector<LayerMetadata>& layers,
    size_t total_budget)
{
    classifyLayers(layers);
    
    std::vector<LayerDecision> decisions;
    decisions.reserve(layers.size());
    
    size_t accumulated_bytes = 0;
    
    for (size_t i = 0; i < layers.size(); ++i) {
        LayerDecision decision;
        decision.layer_index = static_cast<int>(i);
        
        decision.precision = decidePrecision(
            static_cast<int>(i),
            layers[i],
            accumulated_bytes,
            total_budget / layers.size()
        );
        
        decision.compression = decideCompression(
            static_cast<int>(i),
            layers[i],
            decision.precision
        );
        
        // Estimate bytes needed
        float compression_ratio = estimateCompressionRatio(decision.compression, layers[i]);
        float precision_ratio = 1.0f;
        
        switch (decision.precision) {
            case UnifiedMemoryConfig::Precision::FP32: precision_ratio = 1.0f; break;
            case UnifiedMemoryConfig::Precision::FP16: precision_ratio = 0.5f; break;
            case UnifiedMemoryConfig::Precision::BF16: precision_ratio = 0.5f; break;
            case UnifiedMemoryConfig::Precision::INT8: precision_ratio = 0.25f; break;
            case UnifiedMemoryConfig::Precision::INT4: precision_ratio = 0.125f; break;
            case UnifiedMemoryConfig::Precision::INT2: precision_ratio = 0.0625f; break;
            case UnifiedMemoryConfig::Precision::INT1: precision_ratio = 0.03125f; break;
            case UnifiedMemoryConfig::Precision::ADAPTIVE: precision_ratio = 0.2f; break;
        }
        
        // Placeholder estimate (would use actual layer size in practice)
        decision.estimated_bytes = static_cast<size_t>(
            (total_budget / layers.size()) * precision_ratio * compression_ratio
        );
        
        decision.confidence = 1.0f - std::abs(layers[i].importance_score - 0.5f) * 2.0f;
        
        accumulated_bytes += decision.estimated_bytes;
        decisions.push_back(decision);
    }
    
    return decisions;
}

void PrecisionEngine::updateImportance(int layer_index, float delta) {
    if (layer_index >= 0 && static_cast<size_t>(layer_index) < layer_importance_scores_.size()) {
        layer_importance_scores_[layer_index] = 
            std::clamp(layer_importance_scores_[layer_index] + delta, 0.0f, 1.0f);
    }
}

void PrecisionEngine::updateAccessPattern(int layer_index, uint64_t access_count) {
    if (layer_index >= 0 && static_cast<size_t>(layer_index) < layer_access_counts_.size()) {
        layer_access_counts_[layer_index] = access_count;
    }
}

float PrecisionEngine::getLayerImportance(int layer_index) const {
    if (layer_index >= 0 && static_cast<size_t>(layer_index) < layer_importance_scores_.size()) {
        return layer_importance_scores_[layer_index];
    }
    return 0.5f;
}

UnifiedMemoryConfig::LayerImportance PrecisionEngine::getLayerClass(int layer_index) const {
    if (layer_index >= 0 && static_cast<size_t>(layer_index) < layer_classes_.size()) {
        return layer_classes_[layer_index];
    }
    return UnifiedMemoryConfig::LayerImportance::STANDARD;
}

UnifiedMemoryConfig::Precision PrecisionEngine::selectPrecisionForImportance(
    UnifiedMemoryConfig::LayerImportance importance,
    float utilization)
{
    // Base precision from importance
    UnifiedMemoryConfig::Precision base_precision;
    
    switch (importance) {
        case UnifiedMemoryConfig::LayerImportance::CRITICAL:
            base_precision = UnifiedMemoryConfig::Precision::FP16;
            break;
        case UnifiedMemoryConfig::LayerImportance::IMPORTANT:
            base_precision = UnifiedMemoryConfig::Precision::INT8;
            break;
        case UnifiedMemoryConfig::LayerImportance::STANDARD:
            base_precision = UnifiedMemoryConfig::Precision::INT4;
            break;
        case UnifiedMemoryConfig::LayerImportance::REDUCIBLE:
            base_precision = UnifiedMemoryConfig::Precision::INT2;
            break;
        default:
            base_precision = UnifiedMemoryConfig::Precision::INT4;
    }
    
    // Adjust based on utilization pressure
    if (utilization > 0.9f) {
        // High pressure: reduce precision
        switch (base_precision) {
            case UnifiedMemoryConfig::Precision::FP16:
                return UnifiedMemoryConfig::Precision::INT8;
            case UnifiedMemoryConfig::Precision::INT8:
                return UnifiedMemoryConfig::Precision::INT4;
            case UnifiedMemoryConfig::Precision::INT4:
                return UnifiedMemoryConfig::Precision::INT2;
            case UnifiedMemoryConfig::Precision::INT2:
                return UnifiedMemoryConfig::Precision::INT1;
            default:
                return base_precision;
        }
    } else if (utilization < 0.5f) {
        // Low pressure: increase precision
        switch (base_precision) {
            case UnifiedMemoryConfig::Precision::INT1:
                return UnifiedMemoryConfig::Precision::INT2;
            case UnifiedMemoryConfig::Precision::INT2:
                return UnifiedMemoryConfig::Precision::INT4;
            case UnifiedMemoryConfig::Precision::INT4:
                return UnifiedMemoryConfig::Precision::INT8;
            default:
                return base_precision;
        }
    }
    
    return base_precision;
}

UnifiedMemoryConfig::Compression PrecisionEngine::selectCompressionForPrecision(
    UnifiedMemoryConfig::Precision precision,
    const LayerMetadata& metadata)
{
    // High precision = minimal compression
    if (precision == UnifiedMemoryConfig::Precision::FP32 ||
        precision == UnifiedMemoryConfig::Precision::FP16) {
        return UnifiedMemoryConfig::Compression::NONE;
    }
    
    // Low precision = aggressive compression
    if (precision == UnifiedMemoryConfig::Precision::INT1 ||
        precision == UnifiedMemoryConfig::Precision::INT2) {
        // Use hash weights for very low precision
        if (metadata.weight_sparsity > 0.5f) {
            return UnifiedMemoryConfig::Compression::HASH_WEIGHTS;
        }
        return UnifiedMemoryConfig::Compression::DELTA;
    }
    
    // Medium precision = moderate compression
    if (metadata.importance_score > 0.7f) {
        return UnifiedMemoryConfig::Compression::SVD_IMPLICIT;
    } else if (metadata.activation_variance > 0.3f) {
        return UnifiedMemoryConfig::Compression::DELTA;
    } else {
        return UnifiedMemoryConfig::Compression::HYPERNETWORK;
    }
}

float PrecisionEngine::estimateCompressionRatio(
    UnifiedMemoryConfig::Compression compression,
    const LayerMetadata& /*metadata*/) const
{
    switch (compression) {
        case UnifiedMemoryConfig::Compression::NONE:
            return 1.0f;
        case UnifiedMemoryConfig::Compression::DELTA:
            return 0.7f;
        case UnifiedMemoryConfig::Compression::SVD_IMPLICIT:
            return 0.5f;
        case UnifiedMemoryConfig::Compression::HYPERNETWORK:
            return 0.3f;
        case UnifiedMemoryConfig::Compression::HASH_WEIGHTS:
            return 0.2f;
        case UnifiedMemoryConfig::Compression::AUTO:
            return 0.6f;
        default:
            return 1.0f;
    }
}

// ============================================================================
// LayerProfiler Implementation
// ============================================================================

LayerProfiler::LayerProfiler(const UnifiedMemoryConfig& config)
    : config_(config)
{
}

LayerMetadata LayerProfiler::profileLayer(
    int layer_index,
    const float* weights,
    size_t weight_count,
    const float* activations,
    size_t activation_count)
{
    LayerMetadata metadata;
    metadata.layer_index = layer_index;
    
    if (weight_count > 0 && weights) {
        // Compute weight statistics
        float mean = 0.0f;
        for (size_t i = 0; i < weight_count; ++i) {
            mean += weights[i];
        }
        mean /= weight_count;
        
        float variance = 0.0f;
        size_t zero_count = 0;
        for (size_t i = 0; i < weight_count; ++i) {
            variance += (weights[i] - mean) * (weights[i] - mean);
            if (std::abs(weights[i]) < 1e-6f) {
                zero_count++;
            }
        }
        variance /= weight_count;
        
        metadata.weight_sparsity = static_cast<float>(zero_count) / weight_count;
    }
    
    if (activation_count > 0 && activations) {
        // Compute activation statistics
        float mean = 0.0f;
        for (size_t i = 0; i < activation_count; ++i) {
            mean += activations[i];
        }
        mean /= activation_count;
        
        float variance = 0.0f;
        for (size_t i = 0; i < activation_count; ++i) {
            variance += (activations[i] - mean) * (activations[i] - mean);
        }
        variance /= activation_count;
        
        metadata.activation_variance = variance;
    }
    
    // Compute importance score
    metadata.importance_score = computeImportanceScore(
        metadata.gradient_norm,
        metadata.activation_variance,
        metadata.access_count,
        metadata.average_attention_entropy
    );
    
    // Classify
    if (metadata.importance_score >= config_.critical_threshold) {
        metadata.importance_class = UnifiedMemoryConfig::LayerImportance::CRITICAL;
    } else if (metadata.importance_score >= config_.importance_threshold) {
        metadata.importance_class = UnifiedMemoryConfig::LayerImportance::IMPORTANT;
    } else if (metadata.importance_score <= config_.reducible_threshold) {
        metadata.importance_class = UnifiedMemoryConfig::LayerImportance::REDUCIBLE;
    } else {
        metadata.importance_class = UnifiedMemoryConfig::LayerImportance::STANDARD;
    }
    
    return metadata;
}

std::vector<LayerMetadata> LayerProfiler::profileAllLayers(
    const std::vector<std::pair<const float*, size_t>>& weight_layers,
    const std::vector<std::pair<const float*, size_t>>& activation_layers)
{
    std::vector<LayerMetadata> profiles;
    size_t n = std::min(weight_layers.size(), activation_layers.size());
    profiles.reserve(n);
    
    for (size_t i = 0; i < n; ++i) {
        profiles.push_back(profileLayer(
            static_cast<int>(i),
            weight_layers[i].first,
            weight_layers[i].second,
            activation_layers[i].first,
            activation_layers[i].second
        ));
    }
    
    layer_metadata_ = profiles;
    return profiles;
}

void LayerProfiler::updateGradients(int layer_index, const float* gradients, size_t count) {
    if (layer_index < 0 || gradients == nullptr || count == 0) {
        return;
    }
    
    float norm = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        norm += gradients[i] * gradients[i];
    }
    norm = std::sqrt(norm);
    
    if (static_cast<size_t>(layer_index) >= running_gradient_norms_.size()) {
        running_gradient_norms_.resize(layer_index + 1, 0.0f);
    }
    
    running_gradient_norms_[layer_index] = 
        0.9f * running_gradient_norms_[layer_index] + 0.1f * norm;
}

void LayerProfiler::updateActivations(int layer_index, const float* activations, size_t count) {
    if (layer_index < 0 || activations == nullptr || count == 0) {
        return;
    }
    
    float mean = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        mean += activations[i];
    }
    mean /= count;
    
    float variance = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        variance += (activations[i] - mean) * (activations[i] - mean);
    }
    variance /= count;
    
    if (static_cast<size_t>(layer_index) >= running_activation_variances_.size()) {
        running_activation_variances_.resize(layer_index + 1, 0.0f);
    }
    
    running_activation_variances_[layer_index] = 
        0.9f * running_activation_variances_[layer_index] + 0.1f * variance;
}

void LayerProfiler::recordAccess(int layer_index) {
    if (layer_index >= 0 && static_cast<size_t>(layer_index) < layer_metadata_.size()) {
        layer_metadata_[layer_index].access_count++;
    }
}

float LayerProfiler::computeImportanceScore(
    float gradient_norm,
    float activation_variance,
    uint64_t access_count,
    float attention_entropy) const
{
    // Normalize each component to [0, 1]
    float norm_gradient = std::tanh(gradient_norm);  // Squash large values
    float norm_variance = std::tanh(activation_variance);
    float norm_access = std::tanh(static_cast<float>(access_count) / 1000.0f);
    float norm_entropy = 1.0f - attention_entropy;  // Lower entropy = more focused = more important
    
    // Weighted combination
    float score = 0.3f * norm_gradient + 
                  0.3f * norm_variance + 
                  0.2f * norm_access + 
                  0.2f * norm_entropy;
    
    return std::clamp(score, 0.0f, 1.0f);
}

} // namespace rawrxd
