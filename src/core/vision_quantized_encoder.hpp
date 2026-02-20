// ============================================================================
// vision_quantized_encoder.hpp — INT8 Quantized Vision Encoder Layer
// ============================================================================
// Provides INT8 (and optionally INT4) quantized linear layers for vision
// model inference. Reduces memory footprint and increases throughput for
// patch embedding projection and mm_projector transforms.
//
// Features:
//   - Per-channel symmetric INT8 quantization with scale factors
//   - Asymmetric quantization with zero-point correction
//   - INT8×INT8 → INT32 dot product kernel (with FP32 accumulation)
//   - Quantized linear layer: y = dequant(Wq · xq + bias)
//   - Weight quantization (offline) and activation quantization (online)
//   - Calibration support (min/max range collection)
//   - Memory savings tracking
//   - AVX2-accelerated INT8 dot product (SIMD path with C++ fallback)
//
// Integration: Called by VisionEncoder::inferVisionModel as alternative to
//              FP32 patch embedding projection when quantization is enabled.
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vision_encoder.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Quantization Configuration
// ============================================================================
struct QuantConfig {
    enum class Precision : uint8_t {
        INT8  = 8,
        INT4  = 4,
        FP16  = 16   // Half-precision (not integer, but included for completeness)
    };

    Precision precision;
    bool      symmetric;           // Symmetric (zero-centered) vs asymmetric
    bool      perChannel;          // Per-channel vs per-tensor quantization
    uint32_t  calibrationSamples;  // Number of samples for calibration
    float     clampPercentile;     // Percentile for outlier clamping (99.9)

    QuantConfig()
        : precision(Precision::INT8)
        , symmetric(true)
        , perChannel(true)
        , calibrationSamples(64)
        , clampPercentile(99.9f)
    {}
};

// ============================================================================
// Quantized Tensor — INT8 data + per-channel scale/zero-point
// ============================================================================
struct QuantizedTensor {
    std::vector<int8_t>   data;          // Quantized weights (row-major)
    std::vector<float>    scales;        // Scale per channel (or per-tensor if single)
    std::vector<int32_t>  zeroPoints;    // Zero point per channel (asymmetric only)

    uint32_t rows;                       // Output dimension
    uint32_t cols;                       // Input dimension
    uint64_t originalSizeBytes;          // FP32 size for comparison
    uint64_t quantizedSizeBytes;         // INT8 size

    QuantizedTensor()
        : rows(0), cols(0), originalSizeBytes(0), quantizedSizeBytes(0)
    {}

    bool isValid() const {
        return !data.empty() && rows > 0 && cols > 0 && !scales.empty();
    }

    float compressionRatio() const {
        if (quantizedSizeBytes == 0) return 0.0f;
        return static_cast<float>(originalSizeBytes) /
               static_cast<float>(quantizedSizeBytes);
    }
};

// ============================================================================
// Calibration Data — Min/max ranges collected during calibration pass
// ============================================================================
struct CalibrationData {
    std::vector<float> channelMins;    // Per-channel minimum observed values
    std::vector<float> channelMaxs;    // Per-channel maximum observed values
    uint32_t           samplesCollected;
    bool               ready;

    CalibrationData() : samplesCollected(0), ready(false) {}
};

// ============================================================================
// Quantization Statistics
// ============================================================================
struct QuantStats {
    uint64_t totalQuantizeOps;         // Weight quantizations performed
    uint64_t totalDequantizeOps;       // Dequantizations performed
    uint64_t totalLinearOps;           // Quantized linear layer executions
    uint64_t totalDotProductOps;       // INT8 dot products computed
    double   avgQuantizationError;     // Mean absolute error from quantization
    uint64_t memorySavedBytes;         // Total bytes saved vs FP32
    double   avgLinearLatencyMs;       // Average quantized linear layer time
};

// ============================================================================
// VisionQuantizedEncoder — Quantized inference engine for vision models
// ============================================================================
class VisionQuantizedEncoder {
public:
    static VisionQuantizedEncoder& instance();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    void configure(const QuantConfig& config);
    const QuantConfig& getConfig() const;

    // -----------------------------------------------------------------------
    // Weight Quantization (Offline — done once per model load)
    // -----------------------------------------------------------------------

    // Quantize FP32 weight matrix to INT8.
    // weights: row-major [rows × cols] FP32 matrix
    VisionResult quantizeWeights(const float* weights,
                                  uint32_t rows, uint32_t cols,
                                  QuantizedTensor& output);

    // Quantize patch embedding weights specifically
    // (patchDim = patch_size² × channels, embDim = embedding dimension)
    VisionResult quantizePatchEmbedWeights(const float* weights,
                                            uint32_t patchDim,
                                            uint32_t embDim,
                                            QuantizedTensor& output);

    // Dequantize back to FP32 (for validation/debugging)
    VisionResult dequantize(const QuantizedTensor& quantized,
                             std::vector<float>& output);

    // -----------------------------------------------------------------------
    // Activation Quantization (Online — done per inference)
    // -----------------------------------------------------------------------

    // Quantize FP32 activation vector to INT8 on-the-fly.
    // Returns scale and zero-point for the quantized activation.
    VisionResult quantizeActivation(const float* input,
                                     uint32_t length,
                                     std::vector<int8_t>& output,
                                     float& scale,
                                     int32_t& zeroPoint);

    // -----------------------------------------------------------------------
    // Quantized Linear Layer
    // -----------------------------------------------------------------------

    // Compute y = dequant(Wq · xq) + bias
    // input:  FP32 [inDim] (will be quantized on-the-fly)
    // weight: pre-quantized [outDim × inDim]
    // bias:   FP32 [outDim] (optional, can be nullptr)
    // output: FP32 [outDim]
    VisionResult linearQ8(const QuantizedTensor& weight,
                           const float* input,
                           const float* bias,
                           float* output,
                           uint32_t inDim,
                           uint32_t outDim);

    // Batch linear: process multiple input vectors
    VisionResult linearQ8Batch(const QuantizedTensor& weight,
                                const float* inputs,
                                const float* bias,
                                float* outputs,
                                uint32_t inDim,
                                uint32_t outDim,
                                uint32_t batchSize);

    // -----------------------------------------------------------------------
    // INT8 Dot Product
    // -----------------------------------------------------------------------

    // Compute dot product of two INT8 vectors with scale correction.
    // result = scaleA * scaleB * sum(a[i] * b[i])
    float dotProductQ8(const int8_t* a, const int8_t* b,
                       uint32_t length,
                       float scaleA, float scaleB) const;

    // -----------------------------------------------------------------------
    // Full Quantized Inference Pipeline
    // -----------------------------------------------------------------------

    // Replace VisionEncoder::inferVisionModel with quantized path.
    // patches: FP32 [numPatches × patchDim] (normalized patches)
    // weight:  pre-quantized patch_embed [embDim × patchDim]
    // output:  VisionEmbedding with quantized inference results
    VisionResult inferQuantized(const std::vector<float>& patches,
                                 uint32_t numPatches,
                                 uint32_t patchDim,
                                 const QuantizedTensor& patchEmbedWeight,
                                 const QuantizedTensor* projectorWeight,
                                 VisionEmbedding& output);

    // -----------------------------------------------------------------------
    // Calibration
    // -----------------------------------------------------------------------

    // Begin calibration: reset min/max tracking
    void beginCalibration(uint32_t channels);

    // Observe a calibration sample
    void observeCalibration(const float* activations, uint32_t length);

    // End calibration: compute optimal scales/zero-points
    void endCalibration();

    // Check if calibration is ready
    bool isCalibrated() const;

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    QuantStats getStats() const;
    void resetStats();

private:
    VisionQuantizedEncoder();
    ~VisionQuantizedEncoder() = default;
    VisionQuantizedEncoder(const VisionQuantizedEncoder&) = delete;
    VisionQuantizedEncoder& operator=(const VisionQuantizedEncoder&) = delete;

    // Internal: find min/max of a float array
    void findMinMax(const float* data, uint32_t length,
                     float& outMin, float& outMax) const;

    // Internal: compute scale and zero-point for a given range
    void computeScaleZeroPoint(float min, float max, bool symmetric,
                                float& scale, int32_t& zeroPoint) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mutex_;
    QuantConfig config_;
    CalibrationData calibration_;

    // Statistics
    std::atomic<uint64_t> totalQuantizeOps_{0};
    std::atomic<uint64_t> totalDequantizeOps_{0};
    std::atomic<uint64_t> totalLinearOps_{0};
    std::atomic<uint64_t> totalDotProductOps_{0};
    std::atomic<uint64_t> memorySavedBytes_{0};
    double quantErrorAccum_ = 0.0;
    uint64_t quantErrorCount_ = 0;
    double linearLatencyAccum_ = 0.0;
};

} // namespace Vision
} // namespace RawrXD
