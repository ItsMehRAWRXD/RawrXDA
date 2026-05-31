// ============================================================================
// File: src/memory/unified_memory_config.hpp
// ============================================================================

#ifndef RAWRXD_MEMORY_UNIFIED_MEMORY_CONFIG_HPP
#define RAWRXD_MEMORY_UNIFIED_MEMORY_CONFIG_HPP

#include <cstdint>
#include <string>
#include <map>
#include <set>
#include "thinking_effort_adjuster.hpp"
#include "negative_range_model.hpp"

namespace rawrxd {

// ============================================================================
// Unified Memory Configuration
// ============================================================================

struct UnifiedMemoryConfig {
    // Precision modes
    enum class Precision : uint8_t {
        FP32 = 0,
        FP16 = 1,
        BF16 = 2,
        INT8 = 3,
        INT4 = 4,
        INT2 = 5,
        INT1 = 6,      // Sub-bit via negative range encoding
        ADAPTIVE = 7   // Layer-wise adaptive
    };
    
    // Compression modes
    enum class Compression : uint8_t {
        NONE = 0,
        DELTA = 1,
        SVD_IMPLICIT = 2,
        HYPERNETWORK = 3,
        HASH_WEIGHTS = 4,
        AUTO = 5
    };
    
    // Layer importance classification
    enum class LayerImportance : uint8_t {
        CRITICAL = 0,    // Full precision
        IMPORTANT = 1,   // INT8/INT4
        STANDARD = 2,    // INT4/INT2
        REDUCIBLE = 3    // INT2/INT1
    };
    
    // Thinking effort levels
    enum class EffortLevel : uint8_t {
        MINIMAL = 0,     // Just retrieve, no computation
        LOW = 1,         // Basic quantization
        MEDIUM = 2,      // Adaptive quantization
        HIGH = 3,        // Full compression + optimization
        MAXIMUM = 4      // Maximum compression with hypernetwork
    };
    
    // Core configuration
    Precision default_precision = Precision::ADAPTIVE;
    Compression default_compression = Compression::AUTO;
    EffortLevel thinking_effort = EffortLevel::MEDIUM;
    
    // Thresholds
    float importance_threshold = 0.7f;    // Above this = important
    float critical_threshold = 0.9f;      // Above this = critical
    float reducible_threshold = 0.3f;    // Below this = reducible
    
    // Capacity limits
    size_t max_memory_bytes = 16ULL * 1024 * 1024 * 1024;  // 16 GB
    size_t kv_cache_budget = 4ULL * 1024 * 1024 * 1024;     // 4 GB for KV
    size_t weight_budget = 8ULL * 1024 * 1024 * 1024;        // 8 GB for weights
    
    // Layer-specific overrides
    std::map<int, Precision> layer_precision_overrides;
    std::map<int, Compression> layer_compression_overrides;
    std::set<int> critical_layers;
    std::set<int> skipped_layers;
    
    // Validation
    bool validate() const {
        return importance_threshold > 0.0f &&
               importance_threshold < 1.0f &&
               critical_threshold > importance_threshold &&
               reducible_threshold < importance_threshold;
    }
    
    std::string precisionToString(Precision p) const {
        switch (p) {
            case Precision::FP32: return "FP32";
            case Precision::FP16: return "FP16";
            case Precision::BF16: return "BF16";
            case Precision::INT8: return "INT8";
            case Precision::INT4: return "INT4";
            case Precision::INT2: return "INT2";
            case Precision::INT1: return "INT1";
            case Precision::ADAPTIVE: return "ADAPTIVE";
            default: return "UNKNOWN";
        }
    }
    
    std::string compressionToString(Compression c) const {
        switch (c) {
            case Compression::NONE: return "NONE";
            case Compression::DELTA: return "DELTA";
            case Compression::SVD_IMPLICIT: return "SVD_IMPLICIT";
            case Compression::HYPERNETWORK: return "HYPERNETWORK";
            case Compression::HASH_WEIGHTS: return "HASH_WEIGHTS";
            case Compression::AUTO: return "AUTO";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// Layer Metadata for Adaptive Decisions
// ============================================================================

struct LayerMetadata {
    int layer_index = -1;
    std::string layer_type;  // "attention", "feedforward", "embedding"
    
    // Importance metrics
    float importance_score = 0.5f;
    float gradient_norm = 0.0f;
    float activation_variance = 0.0f;
    float weight_sparsity = 0.0f;
    
    // Usage statistics
    uint64_t access_count = 0;
    float average_attention_entropy = 0.0f;
    
    // Computed properties
    UnifiedMemoryConfig::LayerImportance importance_class = 
        UnifiedMemoryConfig::LayerImportance::STANDARD;
    
    bool operator<(const LayerMetadata& other) const {
        return importance_score > other.importance_score;  // Higher importance first
    }
};

// ============================================================================
// Memory Budget Tracker
// ============================================================================

class MemoryBudgetTracker {
public:
    explicit MemoryBudgetTracker(const UnifiedMemoryConfig& config)
        : config_(config) {
        reset();
    }
    
    void reset() {
        kv_bytes_used_ = 0;
        weight_bytes_used_ = 0;
        total_bytes_used_ = 0;
    }
    
    bool allocateKV(size_t bytes) {
        if (kv_bytes_used_ + bytes > config_.kv_cache_budget) {
            return false;
        }
        kv_bytes_used_ += bytes;
        total_bytes_used_ += bytes;
        return true;
    }
    
    bool allocateWeight(size_t bytes) {
        if (weight_bytes_used_ + bytes > config_.weight_budget) {
            return false;
        }
        weight_bytes_used_ += bytes;
        total_bytes_used_ += bytes;
        return true;
    }
    
    void deallocateKV(size_t bytes) {
        kv_bytes_used_ = (bytes > kv_bytes_used_) ? 0 : kv_bytes_used_ - bytes;
        total_bytes_used_ = (bytes > total_bytes_used_) ? 0 : total_bytes_used_ - bytes;
    }
    
    void deallocateWeight(size_t bytes) {
        weight_bytes_used_ = (bytes > weight_bytes_used_) ? 0 : weight_bytes_used_ - bytes;
        total_bytes_used_ = (bytes > total_bytes_used_) ? 0 : total_bytes_used_ - bytes;
    }
    
    size_t getKVBytesUsed() const { return kv_bytes_used_; }
    size_t getWeightBytesUsed() const { return weight_bytes_used_; }
    size_t getTotalBytesUsed() const { return total_bytes_used_; }
    
    float getKVUtilization() const {
        return static_cast<float>(kv_bytes_used_) / config_.kv_cache_budget;
    }
    
    float getWeightUtilization() const {
        return static_cast<float>(weight_bytes_used_) / config_.weight_budget;
    }
    
    float getTotalUtilization() const {
        return static_cast<float>(total_bytes_used_) / config_.max_memory_bytes;
    }
    
private:
    const UnifiedMemoryConfig& config_;
    size_t kv_bytes_used_ = 0;
    size_t weight_bytes_used_ = 0;
    size_t total_bytes_used_ = 0;
};

} // namespace rawrxd

#endif // RAWRXD_MEMORY_UNIFIED_MEMORY_CONFIG_HPP
