#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <cstdint>
#include <functional>
#include "gguf_loader.h"
#include "compression_interface.h"

/**
 * @file resource_optimizer.h
 * @brief Advanced resource optimization system to reduce model requirements from 91% to <10%
 * 
 * This system implements multiple optimization strategies:
 * 1. Dynamic quantization (layer-wise precision reduction)
 * 2. Sparse model representation (store only non-zero weights)
 * 3. Tensor pruning (remove low-impact weights)
 * 4. Memory-mapped lazy loading (load on access)
 * 5. Layer caching with LRU eviction
 * 6. Adaptive batch sizing
 * 7. Weight compression (beyond standard quantization)
 * 8. Knowledge distillation preparation
 */

// Forward declarations
struct TensorInfo;
struct GGUFMetadata;

/**
 * @enum OptimizationStrategy
 * @brief Available optimization strategies
 */
enum class OptimizationStrategy : uint32_t {
    NONE = 0,
    DYNAMIC_QUANTIZATION = 1 << 0,      // Reduce precision per-layer based on sensitivity
    SPARSE_REPRESENTATION = 1 << 1,      // Store only non-zero weights
    TENSOR_PRUNING = 1 << 2,             // Remove low-magnitude weights
    MEMORY_MAPPED_LAZY = 1 << 3,         // Memory-map file, load on access
    LAYER_CACHING = 1 << 4,              // Cache frequently used layers
    ADAPTIVE_BATCHING = 1 << 5,          // Dynamic batch size based on memory
    WEIGHT_COMPRESSION = 1 << 6,         // Additional compression beyond quantization
    KNOWLEDGE_DISTILLATION = 1 << 7,     // Prepare for distillation
    LOW_RANK_FACTORIZATION = 1 << 8,     // SVD-based weight matrix compression
    ACTIVATION_QUANTIZATION = 1 << 9,    // Quantize activations during inference
    GRADIENT_CHECKPOINTING = 1 << 10,    // Recompute activations to save memory
    STRUCTURED_PRUNING = 1 << 11,        // Hardware-friendly channel/filter pruning
    ACTIVATION_COMPRESSION = 1 << 12,    // Compress stored activations
    HYBRID_PRUNING = 1 << 13,            // Combine structured + unstructured pruning
    ALL = 0xFFFFFFFF
};

inline OptimizationStrategy operator|(OptimizationStrategy a, OptimizationStrategy b) {
    return static_cast<OptimizationStrategy>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline OptimizationStrategy operator&(OptimizationStrategy a, OptimizationStrategy b) {
    return static_cast<OptimizationStrategy>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @struct OptimizationConfig
 * @brief Configuration for resource optimization
 */
struct OptimizationConfig {
    // Target memory reduction (0.0-1.0, where 0.1 = 10% of original)
    double target_memory_ratio = 0.10;  // Default: reduce to 10%
    
    // Strategy flags
    OptimizationStrategy strategies = OptimizationStrategy::ALL;
    
    // Quantization settings
    struct {
        bool enable_per_layer = true;
        uint8_t min_bits = 2;           // Minimum bits (Q2_K)
        uint8_t max_bits = 8;           // Maximum bits (Q8_0)
        double sensitivity_threshold = 0.01;  // Sensitivity for layer importance
    } quantization;
    
    // Pruning settings
    struct {
        bool enable = true;
        double sparsity_target = 0.70;  // Target 70% sparsity (keep 30% of weights)
        double magnitude_threshold = 0.01;  // Remove weights below this magnitude
        bool structured = false;        // Structured vs unstructured pruning
    } pruning;
    
    // Caching settings
    struct {
        bool enable = true;
        size_t max_cached_layers = 8;   // Max layers to keep in cache
        size_t cache_size_mb = 512;     // Max cache size in MB
    } caching;
    
    // Compression settings
    struct {
        bool enable = true;
        double compression_ratio_target = 0.50;  // Target 50% size reduction
        bool use_lossy = false;         // Allow lossy compression for non-critical layers
    } compression;
    
    // Lazy loading settings
    struct {
        bool enable = true;
        bool prefetch_next = true;      // Prefetch next likely-needed layer
        size_t prefetch_window = 2;     // Number of layers to prefetch
    } lazy_loading;
};

/**
 * @struct OptimizationStats
 * @brief Statistics about applied optimizations
 */
struct OptimizationStats {
    size_t original_size_bytes = 0;
    size_t optimized_size_bytes = 0;
    double memory_reduction_ratio = 0.0;
    double compression_ratio = 0.0;
    size_t pruned_weights = 0;
    size_t total_weights = 0;
    double sparsity_ratio = 0.0;
    std::map<std::string, double> layer_memory_usage;  // Per-layer memory
    std::map<std::string, uint8_t> layer_quantization; // Per-layer bits
    
    double GetMemorySavingsMB() const {
        return (original_size_bytes - optimized_size_bytes) / (1024.0 * 1024.0);
    }
    
    double GetCompressionRatio() const {
        if (original_size_bytes == 0) return 0.0;
        return static_cast<double>(optimized_size_bytes) / original_size_bytes;
    }
};

/**
 * @class IResourceOptimizer
 * @brief Interface for resource optimization strategies
 */
class IResourceOptimizer {
public:
    virtual ~IResourceOptimizer() = default;
    
    /**
     * @brief Analyze model and recommend optimizations
     */
    virtual OptimizationConfig AnalyzeModel(const GGUFMetadata& metadata,
                                           const std::vector<TensorInfo>& tensors,
                                           size_t available_memory_bytes) = 0;
    
    /**
     * @brief Apply optimizations to model loader
     */
    virtual bool ApplyOptimizations(IGGUFLoader* loader,
                                   const OptimizationConfig& config,
                                   OptimizationStats& stats) = 0;
    
    /**
     * @brief Estimate memory savings from config
     */
    virtual size_t EstimateOptimizedSize(const std::vector<TensorInfo>& tensors,
                                        const OptimizationConfig& config) = 0;
};

/**
 * @class ResourceOptimizer
 * @brief Main resource optimization engine
 * 
 * Implements multiple optimization strategies to reduce model resource requirements:
 * - Reduces memory footprint from 91% to <10% of original
 * - Maintains model quality through intelligent layer-wise optimization
 * - Supports dynamic adaptation based on available resources
 */
class ResourceOptimizer : public IResourceOptimizer {
public:
    ResourceOptimizer();
    ~ResourceOptimizer();
    
    // IResourceOptimizer interface
    OptimizationConfig AnalyzeModel(const GGUFMetadata& metadata,
                                   const std::vector<TensorInfo>& tensors,
                                   size_t available_memory_bytes) override;
    
    bool ApplyOptimizations(IGGUFLoader* loader,
                           const OptimizationConfig& config,
                           OptimizationStats& stats) override;
    
    size_t EstimateOptimizedSize(const std::vector<TensorInfo>& tensors,
                                const OptimizationConfig& config) override;
    
    /**
     * @brief Get recommended strategy for given constraints
     */
    static OptimizationStrategy RecommendStrategy(size_t model_size_bytes,
                                                  size_t available_memory_bytes,
                                                  double quality_threshold = 0.90);
    
    /**
     * @brief Create optimized loader configuration
     */
    static OptimizationConfig CreateOptimalConfig(size_t model_size_bytes,
                                                 size_t available_memory_bytes,
                                                 bool prioritize_quality = false);
    
    /**
     * @brief Reverse-engineer minimal viable configuration
     * Finds the absolute minimum requirements while maintaining usability
     */
    static OptimizationConfig ReverseEngineerMinimalConfig(const GGUFMetadata& metadata,
                                                          const std::vector<TensorInfo>& tensors,
                                                          size_t hard_memory_limit_bytes);

private:
    // Optimization implementations
    bool ApplyDynamicQuantization(IGGUFLoader* loader,
                                  const OptimizationConfig& config,
                                  OptimizationStats& stats);
    
    bool ApplyPruning(IGGUFLoader* loader,
                     const OptimizationConfig& config,
                     OptimizationStats& stats);
    
    bool ApplyLayerCaching(IGGUFLoader* loader,
                          const OptimizationConfig& config,
                          OptimizationStats& stats);
    
    bool ApplyMemoryMapping(IGGUFLoader* loader,
                           const OptimizationConfig& config,
                           OptimizationStats& stats);
    
    // Analysis helpers
    double CalculateLayerSensitivity(const TensorInfo& tensor,
                                    const GGUFMetadata& metadata) const;
    
    bool IsLayerCritical(const std::string& tensor_name,
                        const GGUFMetadata& metadata) const;
    
    uint8_t RecommendQuantizationBits(const TensorInfo& tensor,
                                     double sensitivity,
                                     const OptimizationConfig& config) const;
    
    // Resource estimation
    size_t EstimateQuantizedSize(const TensorInfo& tensor, uint8_t bits) const;
    size_t EstimatePrunedSize(const TensorInfo& tensor, double sparsity) const;
    size_t EstimateCompressedSize(const TensorInfo& tensor, double ratio) const;
};

/**
 * @class AdaptiveResourceManager
 * @brief Manages resources adaptively based on system capabilities
 * 
 * Monitors system resources and dynamically adjusts optimization strategies
 * to maintain performance while minimizing resource usage.
 */
class AdaptiveResourceManager {
public:
    AdaptiveResourceManager();
    ~AdaptiveResourceManager();
    
    /**
     * @brief Initialize with system capabilities
     */
    void Initialize(size_t total_memory_bytes, size_t available_memory_bytes);
    
    /**
     * @brief Get current optimization config based on system state
     */
    OptimizationConfig GetCurrentConfig() const;
    
    /**
     * @brief Adjust optimization based on memory pressure
     */
    void AdjustForMemoryPressure(double pressure_ratio);  // 0.0-1.0
    
    /**
     * @brief Get recommended batch size for current state
     */
    size_t GetOptimalBatchSize(size_t layer_size_bytes) const;
    
    /**
     * @brief Check if model can fit with optimizations
     */
    bool CanFitModel(size_t model_size_bytes) const;
    
private:
    size_t total_memory_bytes_ = 0;
    size_t available_memory_bytes_ = 0;
    double memory_pressure_ = 0.0;
    OptimizationConfig current_config_;
};

/**
 * @class MinimalModelAnalyzer
 * @brief Analyzes models to find absolute minimal viable configuration
 * 
 * Reverse engineers the minimum requirements by:
 * 1. Identifying truly critical layers
 * 2. Finding optimal quantization per layer
 * 3. Determining maximum safe pruning
 * 4. Calculating theoretical minimum size
 */
class MinimalModelAnalyzer {
public:
    struct MinimalConfig {
        std::map<std::string, uint8_t> layer_quantization;  // Per-layer bits
        std::map<std::string, double> layer_sparsity;       // Per-layer pruning ratio
        std::vector<std::string> critical_layers;           // Must-load layers
        std::vector<std::string> optional_layers;           // Can be delayed/compressed
        size_t estimated_size_bytes = 0;
        double estimated_quality = 0.0;
    };
    
    /**
     * @brief Analyze model to find minimal configuration
     */
    MinimalConfig AnalyzeMinimal(const GGUFMetadata& metadata,
                                const std::vector<TensorInfo>& tensors,
                                size_t target_size_bytes);
    
    /**
     * @brief Get theoretical minimum size
     */
    size_t GetTheoreticalMinimum(const std::vector<TensorInfo>& tensors) const;
    
private:
    bool IsEmbeddingLayer(const std::string& name) const;
    bool IsOutputLayer(const std::string& name) const;
    bool IsAttentionLayer(const std::string& name) const;
    double CalculateLayerImportance(const TensorInfo& tensor,
                                   const GGUFMetadata& metadata) const;
};

/**
 * @brief Utility functions for resource optimization
 */
namespace ResourceOptimizationUtils {
    /**
     * @brief Calculate memory footprint reduction
     */
    double CalculateReductionRatio(size_t original, size_t optimized);
    
    /**
     * @brief Estimate inference quality impact
     */
    double EstimateQualityImpact(const OptimizationConfig& config);
    
    /**
     * @brief Get human-readable strategy description
     */
    std::string StrategyToString(OptimizationStrategy strategy);
    
    /**
     * @brief Parse strategy from string
     */
    OptimizationStrategy StrategyFromString(const std::string& str);
    
    /**
     * @brief Validate optimization config
     */
    bool ValidateConfig(const OptimizationConfig& config);
}

/**
 * @brief Integration functions (defined in resource_optimization_integration.cpp)
 */

/**
 * @brief Apply resource optimization to a GGUF loader
 * @param loader The GGUF loader to optimize
 * @param available_memory_bytes Available system memory in bytes
 * @param target_memory_ratio Target memory ratio (0.0-1.0), default 0.10 for 10%
 * @return OptimizationStats with results
 */
OptimizationStats ApplyResourceOptimization(IGGUFLoader* loader,
                                           size_t available_memory_bytes,
                                           double target_memory_ratio = 0.10);

/**
 * @brief Reverse engineer minimal configuration for extremely constrained systems
 * @param loader The GGUF loader
 * @param hard_memory_limit_bytes Hard memory limit that cannot be exceeded
 * @return OptimizationStats with minimal configuration results
 */
OptimizationStats ReverseEngineerMinimalConfig(IGGUFLoader* loader,
                                              size_t hard_memory_limit_bytes);

/**
 * @brief Get optimization recommendations without applying them
 * @param loader The GGUF loader to analyze
 * @param available_memory_bytes Available system memory
 * @return OptimizationConfig with recommendations
 */
OptimizationConfig GetOptimizationRecommendations(IGGUFLoader* loader,
                                                  size_t available_memory_bytes);

/**
 * @brief Create an adaptive resource manager for dynamic optimization
 * @param total_memory_bytes Total system memory
 * @param available_memory_bytes Currently available memory
 * @return Shared pointer to AdaptiveResourceManager
 */
std::shared_ptr<AdaptiveResourceManager> CreateAdaptiveResourceManager(
    size_t total_memory_bytes,
    size_t available_memory_bytes);

/**
 * @brief Analyze model to find theoretical minimum requirements
 * @param loader The GGUF loader
 * @return MinimalModelAnalyzer::MinimalConfig with analysis results
 */
MinimalModelAnalyzer::MinimalConfig AnalyzeMinimalRequirements(IGGUFLoader* loader);

