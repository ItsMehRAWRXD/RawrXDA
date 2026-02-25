#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <cstdint>
#include <functional>
#include "gguf_loader.h"

/**
 * @file reverse_quantization.h
 * @brief Reverse-engineered quantization system with backwards analysis
 * 
 * This system implements quantization that works BACKWARDS:
 * - Starts from maximum compression (Q2_K, 2-bit)
 * - Works backwards to find optimal quantization levels
 * - Can analyze existing quantized models and reverse-engineer quantization scheme
 * - "2 steps before start" means it can work backwards incrementally to find sweet spot
 */

/**
 * @enum QuantizationType
 * @brief Supported quantization formats
 */
enum class QuantizationType : uint8_t {
    F32 = 0,        // 32-bit float (original, no quantization)
    F16 = 1,        // 16-bit half precision
    Q8_0 = 8,       // 8-bit signed integer
    Q6_K = 6,       // 6-bit quantization (K-quantized)
    Q5_K = 5,       // 5-bit quantization (K-quantized)
    Q4_K = 4,       // 4-bit quantization (K-quantized)
    Q3_K = 3,       // 3-bit quantization (K-quantized)
    Q2_K = 2,       // 2-bit quantization (K-quantized) - MAXIMUM COMPRESSION
    Q8_F = 9,       // 8-bit float (custom)
    Q4_F = 10,      // 4-bit float (custom)
    AUTO = 255      // Automatically determine
};

/**
 * @struct QuantizationLevel
 * @brief Represents a quantization level with metadata
 */
struct QuantizationLevel {
    QuantizationType type;
    uint8_t bits;
    double compression_ratio;      // Size ratio vs F32 (0.0-1.0)
    double estimated_quality_loss; // Estimated quality degradation (0.0-1.0)
    bool is_reversible;            // Can be reversed/analyzed
    
    QuantizationLevel() : type(QuantizationType::F32), bits(32), 
                         compression_ratio(1.0), estimated_quality_loss(0.0), 
                         is_reversible(true) {}
    
    QuantizationLevel(QuantizationType t, uint8_t b, double comp, double qual)
        : type(t), bits(b), compression_ratio(comp), 
          estimated_quality_loss(qual), is_reversible(true) {}
};

/**
 * @struct ReverseQuantizationConfig
 * @brief Configuration for reverse quantization analysis
 */
struct ReverseQuantizationConfig {
    // Starting point: maximum compression
    QuantizationType start_type = QuantizationType::Q2_K;  // Start from most compressed
    
    // Working backwards: how many steps to go back
    int backwards_steps = 2;  // "2 steps before start" - go back 2 levels
    
    // Target quality threshold (0.0-1.0)
    double target_quality = 0.90;  // Maintain 90% quality
    
    // Analysis mode
    enum AnalysisMode {
        BACKWARDS_FROM_MAX,      // Start from max compression, work backwards
        FORWARDS_FROM_MIN,       // Start from F32, work forwards (traditional)
        REVERSE_ENGINEER,        // Analyze existing quantized model
        INCREMENTAL_BACKWARDS    // Work backwards incrementally
    } mode = BACKWARDS_FROM_MAX;
    
    // Layer-wise configuration
    bool per_layer_optimization = true;
    std::map<std::string, QuantizationType> layer_overrides;  // Force specific layers
    
    // Critical layers (never quantize below this)
    std::vector<std::string> critical_layers;
    QuantizationType critical_min_type = QuantizationType::Q4_K;  // Never go below Q4_K for critical
    
    // Quality analysis
    bool enable_quality_analysis = true;
    double quality_threshold_step = 0.05;  // Check quality every 5% degradation
    
    // Reverse engineering settings
    struct {
        bool detect_existing_quantization = true;  // Try to detect existing quantization
        bool analyze_scale_factors = true;         // Analyze scale factors for reverse engineering
        bool detect_quantization_type = true;      // Auto-detect quantization type
    } reverse_engineering;
};

/**
 * @struct QuantizationAnalysis
 * @brief Results from quantization analysis
 */
struct QuantizationAnalysis {
    // Layer-wise quantization recommendations
    std::map<std::string, QuantizationType> recommended_quantization;
    std::map<std::string, double> layer_quality_scores;
    std::map<std::string, size_t> layer_size_reduction;
    
    // Overall statistics
    size_t original_size_bytes = 0;
    size_t quantized_size_bytes = 0;
    double overall_compression_ratio = 0.0;
    double estimated_quality_loss = 0.0;
    
    // Reverse engineering results (if analyzing existing model)
    std::map<std::string, QuantizationType> detected_quantization;
    bool quantization_detected = false;
    
    // Backwards analysis results
    std::vector<QuantizationLevel> backwards_sequence;  // Sequence from max compression backwards
    QuantizationLevel optimal_level;                    // Optimal level found
    
    // Quality vs. size tradeoff curve
    std::vector<std::pair<double, double>> quality_size_curve;  // (quality, size_ratio) pairs
};

/**
 * @class ReverseQuantizer
 * @brief Quantization system that works backwards from maximum compression
 */
class ReverseQuantizer {
public:
    ReverseQuantizer();
    ~ReverseQuantizer();
    
    /**
     * @brief Analyze model and determine optimal quantization working BACKWARDS
     * Starts from maximum compression (Q2_K) and works backwards to find optimal level
     */
    QuantizationAnalysis AnalyzeBackwards(const std::vector<TensorInfo>& tensors,
                                         const GGUFMetadata& metadata,
                                         const ReverseQuantizationConfig& config);
    
    /**
     * @brief Reverse-engineer quantization from existing quantized model
     * Analyzes an already-quantized model to determine what quantization was used
     */
    QuantizationAnalysis ReverseEngineerQuantization(IGGUFLoader* loader,
                                                    const ReverseQuantizationConfig& config);
    
    /**
     * @brief Apply quantization working backwards
     * Starts from max compression, works backwards to optimal level
     */
    bool ApplyBackwardsQuantization(IGGUFLoader* loader,
                                   const ReverseQuantizationConfig& config,
                                   QuantizationAnalysis& analysis);
    
    /**
     * @brief Get quantization level "N steps before start"
     * If start is Q2_K, "2 steps before" would be Q4_K
     */
    QuantizationType GetLevelStepsBefore(QuantizationType start_type, int steps);
    
    /**
     * @brief Get quantization level "N steps after start" (less compressed)
     * If start is Q2_K, "2 steps after" would be Q6_K
     */
    QuantizationType GetLevelStepsAfter(QuantizationType start_type, int steps);
    
    /**
     * @brief Create backwards sequence of quantization levels
     * Returns sequence from max compression to less compressed
     */
    std::vector<QuantizationLevel> CreateBackwardsSequence(QuantizationType start_type, int steps);
    
    /**
     * @brief Analyze quality at each quantization level (working backwards)
     * Measures quality degradation as we move backwards from max compression
     */
    std::vector<std::pair<QuantizationLevel, double>> AnalyzeQualityBackwards(
        const TensorInfo& tensor,
        const ReverseQuantizationConfig& config);
    
    /**
     * @brief Detect quantization type from tensor data
     * Reverse-engineers what quantization was applied to existing data
     */
    QuantizationType DetectQuantizationType(const std::vector<uint8_t>& data,
                                           const TensorInfo& tensor);
    
    /**
     * @brief Get all available quantization levels ordered by compression
     * Most compressed first (Q2_K) to least compressed (F32)
     */
    static std::vector<QuantizationLevel> GetAllQuantizationLevels();
    
    /**
     * @brief Get compression ratio for quantization type
     */
    static double GetCompressionRatio(QuantizationType type);
    
    /**
     * @brief Get estimated quality loss for quantization type
     */
    static double GetEstimatedQualityLoss(QuantizationType type);
    
    /**
     * @brief Get bits per value for quantization type
     */
    static uint8_t GetBits(QuantizationType type);

private:
    // Internal analysis methods
    QuantizationType AnalyzeLayerBackwards(const TensorInfo& tensor,
                                          const GGUFMetadata& metadata,
                                          const ReverseQuantizationConfig& config);
    
    bool IsCriticalLayer(const std::string& layer_name,
                        const ReverseQuantizationConfig& config) const;
    
    double EstimateLayerQuality(const TensorInfo& tensor,
                               QuantizationType quant_type) const;
    
    // Reverse engineering helpers
    bool DetectScaleFactors(const std::vector<uint8_t>& data,
                           const TensorInfo& tensor,
                           std::vector<float>& scales) const;
    
    bool AnalyzeQuantizationPattern(const std::vector<uint8_t>& data,
                                   const TensorInfo& tensor) const;
    
    // Quality measurement (would need actual tensor data in real implementation)
    double MeasureQualityImpact(const TensorInfo& tensor,
                               QuantizationType from_type,
                               QuantizationType to_type) const;
};

/**
 * @class QuantizationSequenceBuilder
 * @brief Builds backwards quantization sequences
 * 
 * Creates sequences like: Q2_K → Q3_K → Q4_K → Q5_K → Q6_K → Q8_0
 * Starting from maximum compression and working backwards
 */
class QuantizationSequenceBuilder {
public:
    /**
     * @brief Build sequence from max compression backwards
     * @param start Maximum compression level (typically Q2_K)
     * @param steps How many steps to go back
     * @return Sequence of quantization levels
     */
    static std::vector<QuantizationLevel> BuildBackwardsSequence(QuantizationType start, int steps);
    
    /**
     * @brief Build sequence from min compression forwards
     * @param start Minimum compression level (F32)
     * @param steps How many steps to go forward
     * @return Sequence of quantization levels
     */
    static std::vector<QuantizationLevel> BuildForwardsSequence(QuantizationType start, int steps);
    
    /**
     * @brief Get next level in backwards direction (less compressed)
     */
    static QuantizationType NextBackwards(QuantizationType current);
    
    /**
     * @brief Get next level in forwards direction (more compressed)
     */
    static QuantizationType NextForwards(QuantizationType current);
    
    /**
     * @brief Check if level A is more compressed than level B
     */
    static bool IsMoreCompressed(QuantizationType a, QuantizationType b);
};

/**
 * @brief Utility functions for reverse quantization
 */
namespace ReverseQuantizationUtils {
    /**
     * @brief Convert quantization type to string
     */
    std::string TypeToString(QuantizationType type);
    
    /**
     * @brief Parse quantization type from string
     */
    QuantizationType TypeFromString(const std::string& str);
    
    /**
     * @brief Get quantization level from bits
     */
    QuantizationType TypeFromBits(uint8_t bits);
    
    /**
     * @brief Calculate size after quantization
     */
    size_t CalculateQuantizedSize(size_t original_size, QuantizationType type);
    
    /**
     * @brief Estimate quality score (0.0-1.0) for quantization type
     */
    double EstimateQualityScore(QuantizationType type);
    
    /**
     * @brief Find optimal quantization level working backwards
     * Starts from max compression, finds first level meeting quality target
     */
    QuantizationLevel FindOptimalBackwards(double target_quality,
                                          const std::vector<QuantizationLevel>& sequence);
}

