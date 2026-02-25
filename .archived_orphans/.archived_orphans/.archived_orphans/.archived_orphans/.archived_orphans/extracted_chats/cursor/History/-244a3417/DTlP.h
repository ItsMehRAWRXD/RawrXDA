#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <cstdint>
#include <functional>
#include <cstring>

/**
 * @file universal_quantization.h
 * @brief Universal Quantization System - Works with ALL model formats
 * 
 * Features:
 * - Forward quantization (F32 → compressed)
 * - Backward quantization (max compression → optimal)
 * - Reverse engineering (detect existing quantization)
 * - Full compression/decompression pipeline
 * - Universal compatibility (GGUF, PyTorch, TensorFlow, ONNX, etc.)
 * - Model-wide quantization (entire pipeline)
 */

// Forward declarations
struct TensorInfo;

/**
 * @enum QuantizationMode
 * @brief Quantization modes
 */
enum class QuantizationMode : uint8_t {
    FORWARD,        // Traditional: F32 → compressed
    BACKWARD,       // Reverse: max compression → optimal
    REVERSE_ENGINEER, // Detect existing quantization
    ADAPTIVE        // Automatically choose best mode
};

/**
 * @enum QuantizationFormat
 * @brief Supported quantization formats (universal)
 */
enum class QuantizationFormat : uint8_t {
    F32 = 0,        // 32-bit float (original)
    F16 = 1,        // 16-bit half precision
    BF16 = 2,       // 16-bit brain float
    Q8_0 = 8,       // 8-bit signed integer
    Q8_1 = 9,       // 8-bit with scale
    Q6_K = 6,       // 6-bit K-quantized
    Q5_0 = 10,      // 5-bit block quantized
    Q5_1 = 11,      // 5-bit with scale
    Q5_K = 5,       // 5-bit K-quantized
    Q4_0 = 12,      // 4-bit block quantized
    Q4_1 = 13,      // 4-bit with scale
    Q4_K = 4,       // 4-bit K-quantized
    Q3_K = 3,       // 3-bit K-quantized
    Q2_K = 2,       // 2-bit K-quantized (maximum compression)
    INT8 = 14,      // Generic 8-bit integer
    INT4 = 15,      // Generic 4-bit integer
    INT2 = 16,      // Generic 2-bit integer
    AUTO = 255      // Auto-detect
};

/**
 * @struct QuantizationConfig
 * @brief Configuration for universal quantization
 */
struct QuantizationConfig {
    QuantizationMode mode = QuantizationMode::ADAPTIVE;
    
    // Target settings
    double target_compression_ratio = 0.10;  // Target 10% of original size
    double target_quality = 0.90;            // Maintain 90% quality
    
    // Starting point (for backward mode)
    QuantizationFormat start_format = QuantizationFormat::Q2_K;  // Start from max compression
    int backwards_steps = 2;  // Steps to go back from max compression
    
    // Layer-wise settings
    bool per_layer_optimization = true;
    std::map<std::string, QuantizationFormat> layer_overrides;
    std::vector<std::string> critical_layers;  // Never quantize below Q4_K
    QuantizationFormat critical_min_format = QuantizationFormat::Q4_K;
    
    // Model format compatibility
    bool gguf_compatible = true;
    bool pytorch_compatible = true;
    bool tensorflow_compatible = true;
    bool onnx_compatible = true;
    bool custom_format = true;
    
    // Compression settings
    bool enable_compression = true;
    bool enable_decompression = true;
    bool preserve_zero = true;  // Preserve exact zeros
    
    // Reverse engineering
    bool detect_existing = true;
    bool analyze_metadata = true;
    bool infer_from_data = true;
};

/**
 * @struct QuantizedTensor
 * @brief Represents a quantized tensor with metadata
 */
struct QuantizedTensor {
    std::vector<uint8_t> data;           // Quantized data
    QuantizationFormat format;
    std::vector<size_t> shape;           // Original shape
    std::vector<float> scales;           // Scale factors (per-group)
    std::vector<int32_t> zero_points;    // Zero points (if applicable)
    size_t group_size = 0;               // Group size for group quantization
    size_t original_size_bytes = 0;      // Original size before quantization
    size_t quantized_size_bytes = 0;     // Size after quantization
    
    // Metadata
    std::string name;
    std::string dtype_original;          // Original data type (e.g., "float32")
    bool is_compressed = false;          // Whether data is additionally compressed
    
    QuantizedTensor() : format(QuantizationFormat::F32), original_size_bytes(0), 
                       quantized_size_bytes(0), is_compressed(false) {}
};

/**
 * @struct QuantizationResult
 * @brief Results from quantization operation
 */
struct QuantizationResult {
    std::map<std::string, QuantizedTensor> quantized_tensors;
    std::map<std::string, QuantizationFormat> layer_formats;
    
    // Statistics
    size_t original_size_bytes = 0;
    size_t quantized_size_bytes = 0;
    size_t compressed_size_bytes = 0;
    double compression_ratio = 0.0;
    double estimated_quality = 0.0;
    
    // Reverse engineering results
    bool quantization_detected = false;
    std::map<std::string, QuantizationFormat> detected_formats;
    
    // Success/failure
    bool success = false;
    std::string error_message;
};

/**
 * @class UniversalQuantizer
 * @brief Universal quantization system for ALL model formats
 */
class UniversalQuantizer {
public:
    UniversalQuantizer();
    ~UniversalQuantizer();
    
    // ===================================================================
    // FORWARD QUANTIZATION (Traditional: F32 → Compressed)
    // ===================================================================
    
    /**
     * @brief Quantize tensor in forward mode (F32 → compressed)
     */
    QuantizedTensor QuantizeForward(const float* data, size_t count,
                                   QuantizationFormat target_format,
                                   const QuantizationConfig& config = QuantizationConfig());
    
    /**
     * @brief Quantize entire model (all tensors) in forward mode
     */
    QuantizationResult QuantizeModelForward(
        const std::map<std::string, std::vector<float>>& model_tensors,
        const QuantizationConfig& config);
    
    // ===================================================================
    // BACKWARD QUANTIZATION (Reverse: Max Compression → Optimal)
    // ===================================================================
    
    /**
     * @brief Quantize tensor in backward mode (max compression → optimal)
     * Starts from maximum compression, works backwards to find optimal level
     */
    QuantizedTensor QuantizeBackward(const float* data, size_t count,
                                    const QuantizationConfig& config);
    
    /**
     * @brief Quantize entire model in backward mode
     */
    QuantizationResult QuantizeModelBackward(
        const std::map<std::string, std::vector<float>>& model_tensors,
        const QuantizationConfig& config);
    
    // ===================================================================
    // REVERSE ENGINEERING (Detect Existing Quantization)
    // ===================================================================
    
    /**
     * @brief Reverse-engineer quantization from existing quantized data
     * Analyzes data to detect what quantization format was used
     */
    QuantizationFormat ReverseEngineerFormat(const uint8_t* data, size_t size_bytes,
                                            const std::vector<size_t>& shape,
                                            const QuantizationConfig& config);
    
    /**
     * @brief Reverse-engineer entire model's quantization
     */
    QuantizationResult ReverseEngineerModel(
        const std::map<std::string, std::vector<uint8_t>>& quantized_tensors,
        const std::map<std::string, std::vector<size_t>>& tensor_shapes,
        const QuantizationConfig& config);
    
    // ===================================================================
    // COMPRESSION / DECOMPRESSION
    // ===================================================================
    
    /**
     * @brief Compress quantized tensor (additional compression)
     */
    bool CompressTensor(QuantizedTensor& tensor, double compression_ratio = 0.5);
    
    /**
     * @brief Decompress quantized tensor
     */
    bool DecompressTensor(QuantizedTensor& tensor);
    
    /**
     * @brief Dequantize tensor back to F32
     */
    std::vector<float> Dequantize(const QuantizedTensor& quantized);
    
    // ===================================================================
    // MODEL-WIDE OPERATIONS (Entire Pipeline)
    // ===================================================================
    
    /**
     * @brief Quantize entire model through full pipeline
     * Handles all tensors, applies compression, preserves model structure
     */
    QuantizationResult QuantizeModelComplete(
        const std::map<std::string, std::vector<float>>& model_tensors,
        const QuantizationConfig& config);
    
    /**
     * @brief Apply quantization to model during inference
     * Quantizes on-the-fly during model execution
     */
    bool ApplyQuantizationToInference(
        void* model_data,
        size_t model_size,
        QuantizationFormat format,
        const QuantizationConfig& config);
    
    /**
     * @brief Restore model from quantized state
     * Dequantizes entire model back to F32
     */
    std::map<std::string, std::vector<float>> RestoreModelFromQuantized(
        const QuantizationResult& quantized_result);
    
    // ===================================================================
    // FORMAT COMPATIBILITY (Universal Model Support)
    // ===================================================================
    
    /**
     * @brief Detect model format
     */
    enum ModelFormat {
        FORMAT_GGUF,
        FORMAT_PYTORCH,
        FORMAT_TENSORFLOW,
        FORMAT_ONNX,
        FORMAT_SAFETENSORS,
        FORMAT_UNKNOWN
    };
    
    ModelFormat DetectModelFormat(const void* data, size_t size);
    
    /**
     * @brief Quantize model regardless of format
     */
    QuantizationResult QuantizeModelUniversal(
        const void* model_data,
        size_t model_size,
        const QuantizationConfig& config);
    
    /**
     * @brief Convert quantized model to specific format
     */
    bool ConvertToFormat(const QuantizationResult& result,
                        ModelFormat target_format,
                        std::vector<uint8_t>& output);
    
    // ===================================================================
    // UTILITY METHODS
    // ===================================================================
    
    /**
     * @brief Get compression ratio for format
     */
    static double GetCompressionRatio(QuantizationFormat format);
    
    /**
     * @brief Get estimated quality loss for format
     */
    static double GetQualityLoss(QuantizationFormat format);
    
    /**
     * @brief Get bits per value for format
     */
    static uint8_t GetBits(QuantizationFormat format);
    
    /**
     * @brief Get format name as string
     */
    static std::string FormatToString(QuantizationFormat format);
    
    /**
     * @brief Parse format from string
     */
    static QuantizationFormat FormatFromString(const std::string& str);
    
    /**
     * @brief Find optimal format working backwards from max compression
     * "2 steps before start" = Q2_K → Q4_K (2 steps back)
     */
    static QuantizationFormat GetFormatStepsBefore(QuantizationFormat start, int steps);
    
    /**
     * @brief Find format working forwards from F32
     */
    static QuantizationFormat GetFormatStepsForward(QuantizationFormat start, int steps);

private:
    // Internal quantization implementations
    QuantizedTensor QuantizeF32ToQ8(const float* data, size_t count);
    QuantizedTensor QuantizeF32ToQ4_K(const float* data, size_t count, size_t group_size = 64);
    QuantizedTensor QuantizeF32ToQ3_K(const float* data, size_t count, size_t group_size = 64);
    QuantizedTensor QuantizeF32ToQ2_K(const float* data, size_t count, size_t group_size = 64);
    QuantizedTensor QuantizeF32ToF16(const float* data, size_t count);
    QuantizedTensor QuantizeF32ToINT8(const float* data, size_t count);
    QuantizedTensor QuantizeF32ToINT4(const float* data, size_t count);
    QuantizedTensor QuantizeF32ToINT2(const float* data, size_t count);
    
    // Internal decompression implementations
    std::vector<float> DequantizeQ8(const QuantizedTensor& quantized);
    std::vector<float> DequantizeQ4_K(const QuantizedTensor& quantized);
    std::vector<float> DequantizeQ3_K(const QuantizedTensor& quantized);
    std::vector<float> DequantizeQ2_K(const QuantizedTensor& quantized);
    std::vector<float> DequantizeF16(const QuantizedTensor& quantized);
    std::vector<float> DequantizeINT8(const QuantizedTensor& quantized);
    std::vector<float> DequantizeINT4(const QuantizedTensor& quantized);
    std::vector<float> DequantizeINT2(const QuantizedTensor& quantized);
    
    // Compression helpers
    bool CompressWithZlib(QuantizedTensor& tensor, double ratio);
    bool DecompressWithZlib(QuantizedTensor& tensor);
    
    // Analysis helpers
    QuantizationFormat AnalyzeDataPattern(const uint8_t* data, size_t size);
    bool DetectScaleFactors(const uint8_t* data, size_t size, std::vector<float>& scales);
    bool DetectGroupSize(const uint8_t* data, size_t size, size_t& group_size);
    
    // Model format detection
    bool IsGGUFFormat(const void* data, size_t size);
    bool IsPyTorchFormat(const void* data, size_t size);
    bool IsTensorFlowFormat(const void* data, size_t size);
    bool IsONNXFormat(const void* data, size_t size);
    bool IsSafetensorsFormat(const void* data, size_t size);
};

/**
 * @brief Global utility functions
 */
namespace UniversalQuantizationUtils {
    /**
     * @brief Create quantization config for backward mode
     */
    QuantizationConfig CreateBackwardConfig(int steps = 2, double target_quality = 0.90);
    
    /**
     * @brief Create quantization config for forward mode
     */
    QuantizationConfig CreateForwardConfig(QuantizationFormat target = QuantizationFormat::Q4_K);
    
    /**
     * @brief Create quantization config for reverse engineering
     */
    QuantizationConfig CreateReverseEngineerConfig();
    
    /**
     * @brief Estimate model size after quantization
     */
    size_t EstimateQuantizedSize(size_t original_size, QuantizationFormat format);
    
    /**
     * @brief Calculate compression ratio achieved
     */
    double CalculateCompressionRatio(size_t original, size_t quantized);
    
    /**
     * @brief Validate quantization result
     */
    bool ValidateQuantizationResult(const QuantizationResult& result);
}

