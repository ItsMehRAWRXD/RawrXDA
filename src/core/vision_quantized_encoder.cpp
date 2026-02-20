// ============================================================================
// vision_quantized_encoder.cpp — INT8 Quantized Vision Encoder Implementation
// ============================================================================
// Full implementation of INT8 quantized inference for vision models.
//
// Quantization scheme:
//   Symmetric:   q = clamp(round(x / scale), -127, 127)
//                x ≈ q * scale
//                scale = max(|min|, |max|) / 127
//
//   Asymmetric:  q = clamp(round(x / scale) + zeroPoint, -128, 127)
//                x ≈ (q - zeroPoint) * scale
//                scale = (max - min) / 255
//                zeroPoint = round(-min / scale) - 128
//
// Linear layer (INT8):
//   For each output row r:
//     acc[r] = 0
//     for c in cols:
//       acc[r] += W_q[r,c] * x_q[c]    (INT8 × INT8 → INT32)
//     output[r] = acc[r] * scale_W[r] * scale_x + bias[r]
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "vision_quantized_encoder.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <numeric>

#ifdef _MSC_VER
#include <intrin.h>  // __cpuid for feature detection
#endif

namespace RawrXD {
namespace Vision {

// ============================================================================
// Singleton
// ============================================================================

VisionQuantizedEncoder& VisionQuantizedEncoder::instance() {
    static VisionQuantizedEncoder inst;
    return inst;
}

VisionQuantizedEncoder::VisionQuantizedEncoder()
    : config_()
{
}

// ============================================================================
// Configuration
// ============================================================================

void VisionQuantizedEncoder::configure(const QuantConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const QuantConfig& VisionQuantizedEncoder::getConfig() const {
    return config_;
}

// ============================================================================
// Find Min/Max
// ============================================================================

void VisionQuantizedEncoder::findMinMax(const float* data, uint32_t length,
                                          float& outMin, float& outMax) const {
    if (length == 0) {
        outMin = outMax = 0.0f;
        return;
    }

    float mn = data[0], mx = data[0];
    for (uint32_t i = 1; i < length; ++i) {
        if (data[i] < mn) mn = data[i];
        if (data[i] > mx) mx = data[i];
    }

    // Apply percentile clamping to handle outliers
    if (config_.clampPercentile < 100.0f && length > 10) {
        // Simple histogram-based percentile estimation
        // Divide range into 1024 bins
        constexpr int BINS = 1024;
        float range = mx - mn;
        if (range < 1e-10f) {
            outMin = mn;
            outMax = mx;
            return;
        }

        int histogram[BINS] = {};
        for (uint32_t i = 0; i < length; ++i) {
            int bin = static_cast<int>((data[i] - mn) / range * (BINS - 1));
            bin = std::max(0, std::min(bin, BINS - 1));
            histogram[bin]++;
        }

        // Find percentile boundaries
        float lowP = (100.0f - config_.clampPercentile) / 200.0f;
        float highP = 1.0f - lowP;
        int lowTarget = static_cast<int>(lowP * length);
        int highTarget = static_cast<int>(highP * length);

        int cumSum = 0;
        int lowBin = 0, highBin = BINS - 1;
        for (int b = 0; b < BINS; ++b) {
            cumSum += histogram[b];
            if (cumSum >= lowTarget && lowBin == 0) lowBin = b;
            if (cumSum >= highTarget) { highBin = b; break; }
        }

        outMin = mn + (static_cast<float>(lowBin) / BINS) * range;
        outMax = mn + (static_cast<float>(highBin + 1) / BINS) * range;
    } else {
        outMin = mn;
        outMax = mx;
    }
}

// ============================================================================
// Compute Scale and Zero-Point
// ============================================================================

void VisionQuantizedEncoder::computeScaleZeroPoint(
    float min, float max, bool symmetric,
    float& scale, int32_t& zeroPoint) const
{
    if (symmetric) {
        // Symmetric: max absolute value determines range
        float absMax = std::max(std::abs(min), std::abs(max));
        if (absMax < 1e-10f) absMax = 1e-10f;
        scale = absMax / 127.0f;
        zeroPoint = 0;
    } else {
        // Asymmetric: full range
        float range = max - min;
        if (range < 1e-10f) range = 1e-10f;
        scale = range / 255.0f;
        zeroPoint = static_cast<int32_t>(std::round(-min / scale)) - 128;
        zeroPoint = std::max(-128, std::min(127, zeroPoint));
    }
}

// ============================================================================
// Quantize Weights — FP32 → INT8
// ============================================================================

VisionResult VisionQuantizedEncoder::quantizeWeights(
    const float* weights,
    uint32_t rows, uint32_t cols,
    QuantizedTensor& output)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!weights || rows == 0 || cols == 0) {
        return VisionResult::error("Invalid weight parameters", 1);
    }

    output.rows = rows;
    output.cols = cols;
    output.originalSizeBytes = static_cast<uint64_t>(rows) * cols * sizeof(float);
    output.data.resize(static_cast<size_t>(rows) * cols);

    if (config_.perChannel) {
        // Per-channel quantization: one scale per output row
        output.scales.resize(rows);
        output.zeroPoints.resize(rows, 0);

        for (uint32_t r = 0; r < rows; ++r) {
            const float* rowPtr = weights + static_cast<size_t>(r) * cols;

            // Find min/max for this row
            float rMin, rMax;
            findMinMax(rowPtr, cols, rMin, rMax);

            // Compute scale
            float scale;
            int32_t zp;
            computeScaleZeroPoint(rMin, rMax, config_.symmetric, scale, zp);

            output.scales[r] = scale;
            output.zeroPoints[r] = zp;

            // Quantize row
            int8_t* qRow = output.data.data() + static_cast<size_t>(r) * cols;
            float invScale = (scale > 1e-10f) ? 1.0f / scale : 0.0f;

            double rowError = 0.0;

            for (uint32_t c = 0; c < cols; ++c) {
                float val = rowPtr[c];
                int32_t q;
                if (config_.symmetric) {
                    q = static_cast<int32_t>(std::round(val * invScale));
                    q = std::max(-127, std::min(127, q));
                } else {
                    q = static_cast<int32_t>(std::round(val * invScale)) + zp;
                    q = std::max(-128, std::min(127, q));
                }
                qRow[c] = static_cast<int8_t>(q);

                // Track quantization error
                float reconstructed;
                if (config_.symmetric) {
                    reconstructed = static_cast<float>(qRow[c]) * scale;
                } else {
                    reconstructed = (static_cast<float>(qRow[c]) - zp) * scale;
                }
                rowError += std::abs(val - reconstructed);
            }

            if (cols > 0) {
                quantErrorAccum_ += rowError / cols;
                quantErrorCount_++;
            }
        }
    } else {
        // Per-tensor quantization: single scale
        output.scales.resize(1);
        output.zeroPoints.resize(1, 0);

        float tMin, tMax;
        findMinMax(weights, rows * cols, tMin, tMax);

        float scale;
        int32_t zp;
        computeScaleZeroPoint(tMin, tMax, config_.symmetric, scale, zp);

        output.scales[0] = scale;
        output.zeroPoints[0] = zp;

        float invScale = (scale > 1e-10f) ? 1.0f / scale : 0.0f;

        for (uint32_t i = 0; i < rows * cols; ++i) {
            int32_t q;
            if (config_.symmetric) {
                q = static_cast<int32_t>(std::round(weights[i] * invScale));
                q = std::max(-127, std::min(127, q));
            } else {
                q = static_cast<int32_t>(std::round(weights[i] * invScale)) + zp;
                q = std::max(-128, std::min(127, q));
            }
            output.data[i] = static_cast<int8_t>(q);
        }
    }

    output.quantizedSizeBytes = output.data.size() * sizeof(int8_t) +
                                output.scales.size() * sizeof(float) +
                                output.zeroPoints.size() * sizeof(int32_t);

    uint64_t saved = (output.originalSizeBytes > output.quantizedSizeBytes)
        ? output.originalSizeBytes - output.quantizedSizeBytes
        : 0;
    memorySavedBytes_.fetch_add(saved, std::memory_order_relaxed);
    totalQuantizeOps_.fetch_add(1, std::memory_order_relaxed);

    return VisionResult::ok("Weights quantized to INT8");
}

// ============================================================================
// Quantize Patch Embedding Weights
// ============================================================================

VisionResult VisionQuantizedEncoder::quantizePatchEmbedWeights(
    const float* weights,
    uint32_t patchDim,
    uint32_t embDim,
    QuantizedTensor& output)
{
    // Patch embed weights: [embDim × patchDim]
    // Each row projects one output embedding dimension
    return quantizeWeights(weights, embDim, patchDim, output);
}

// ============================================================================
// Dequantize — INT8 → FP32
// ============================================================================

VisionResult VisionQuantizedEncoder::dequantize(
    const QuantizedTensor& quantized,
    std::vector<float>& output)
{
    if (!quantized.isValid()) {
        return VisionResult::error("Invalid quantized tensor", 1);
    }

    uint32_t rows = quantized.rows;
    uint32_t cols = quantized.cols;
    output.resize(static_cast<size_t>(rows) * cols);

    bool perChannel = (quantized.scales.size() == rows);

    for (uint32_t r = 0; r < rows; ++r) {
        float scale = perChannel ? quantized.scales[r] : quantized.scales[0];
        int32_t zp = (quantized.zeroPoints.size() > r)
                     ? quantized.zeroPoints[r]
                     : (quantized.zeroPoints.empty() ? 0 : quantized.zeroPoints[0]);

        const int8_t* qRow = quantized.data.data() + static_cast<size_t>(r) * cols;
        float* outRow = output.data() + static_cast<size_t>(r) * cols;

        for (uint32_t c = 0; c < cols; ++c) {
            if (config_.symmetric) {
                outRow[c] = static_cast<float>(qRow[c]) * scale;
            } else {
                outRow[c] = (static_cast<float>(qRow[c]) - static_cast<float>(zp)) * scale;
            }
        }
    }

    totalDequantizeOps_.fetch_add(1, std::memory_order_relaxed);

    return VisionResult::ok("Dequantized");
}

// ============================================================================
// Online Activation Quantization
// ============================================================================

VisionResult VisionQuantizedEncoder::quantizeActivation(
    const float* input,
    uint32_t length,
    std::vector<int8_t>& output,
    float& scale,
    int32_t& zeroPoint)
{
    if (!input || length == 0) {
        return VisionResult::error("Invalid activation input", 1);
    }

    float mn, mx;
    findMinMax(input, length, mn, mx);
    computeScaleZeroPoint(mn, mx, config_.symmetric, scale, zeroPoint);

    output.resize(length);
    float invScale = (scale > 1e-10f) ? 1.0f / scale : 0.0f;

    for (uint32_t i = 0; i < length; ++i) {
        int32_t q;
        if (config_.symmetric) {
            q = static_cast<int32_t>(std::round(input[i] * invScale));
            q = std::max(-127, std::min(127, q));
        } else {
            q = static_cast<int32_t>(std::round(input[i] * invScale)) + zeroPoint;
            q = std::max(-128, std::min(127, q));
        }
        output[i] = static_cast<int8_t>(q);
    }

    return VisionResult::ok("Activation quantized");
}

// ============================================================================
// INT8 Dot Product
// ============================================================================
// Computes: result = scaleA * scaleB * sum(a[i] * b[i])
// Uses INT32 accumulation to avoid overflow.

float VisionQuantizedEncoder::dotProductQ8(
    const int8_t* a, const int8_t* b,
    uint32_t length,
    float scaleA, float scaleB) const
{
    if (!a || !b || length == 0) return 0.0f;

    int32_t acc = 0;

    // Unrolled scalar loop (4x unroll)
    uint32_t i = 0;
    for (; i + 4 <= length; i += 4) {
        acc += static_cast<int32_t>(a[i])     * static_cast<int32_t>(b[i]);
        acc += static_cast<int32_t>(a[i + 1]) * static_cast<int32_t>(b[i + 1]);
        acc += static_cast<int32_t>(a[i + 2]) * static_cast<int32_t>(b[i + 2]);
        acc += static_cast<int32_t>(a[i + 3]) * static_cast<int32_t>(b[i + 3]);
    }
    for (; i < length; ++i) {
        acc += static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
    }

    return static_cast<float>(acc) * scaleA * scaleB;
}

// ============================================================================
// Quantized Linear Layer
// ============================================================================
// y[r] = sum_c(W_q[r,c] * x_q[c]) * scale_W[r] * scale_x + bias[r]

VisionResult VisionQuantizedEncoder::linearQ8(
    const QuantizedTensor& weight,
    const float* input,
    const float* bias,
    float* output,
    uint32_t inDim,
    uint32_t outDim)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!weight.isValid()) {
        return VisionResult::error("Invalid quantized weight", 1);
    }
    if (!input || !output) {
        return VisionResult::error("Null input/output pointers", 2);
    }
    if (weight.rows != outDim || weight.cols != inDim) {
        return VisionResult::error("Weight dimension mismatch", 3);
    }

    // Step 1: Quantize input activation on-the-fly
    std::vector<int8_t> xq(inDim);
    float xScale;
    int32_t xZeroPoint;

    float xMin, xMax;
    findMinMax(input, inDim, xMin, xMax);
    computeScaleZeroPoint(xMin, xMax, config_.symmetric, xScale, xZeroPoint);

    float invXScale = (xScale > 1e-10f) ? 1.0f / xScale : 0.0f;
    for (uint32_t c = 0; c < inDim; ++c) {
        int32_t q;
        if (config_.symmetric) {
            q = static_cast<int32_t>(std::round(input[c] * invXScale));
            q = std::max(-127, std::min(127, q));
        } else {
            q = static_cast<int32_t>(std::round(input[c] * invXScale)) + xZeroPoint;
            q = std::max(-128, std::min(127, q));
        }
        xq[c] = static_cast<int8_t>(q);
    }

    bool perChannel = (weight.scales.size() == outDim);

    // Step 2: INT8 matrix-vector multiplication
    for (uint32_t r = 0; r < outDim; ++r) {
        const int8_t* wRow = weight.data.data() + static_cast<size_t>(r) * inDim;
        float wScale = perChannel ? weight.scales[r] : weight.scales[0];

        // INT8 dot product with INT32 accumulation
        int32_t acc = 0;
        uint32_t c = 0;

        // 4x unrolled inner loop
        for (; c + 4 <= inDim; c += 4) {
            acc += static_cast<int32_t>(wRow[c])     * static_cast<int32_t>(xq[c]);
            acc += static_cast<int32_t>(wRow[c + 1]) * static_cast<int32_t>(xq[c + 1]);
            acc += static_cast<int32_t>(wRow[c + 2]) * static_cast<int32_t>(xq[c + 2]);
            acc += static_cast<int32_t>(wRow[c + 3]) * static_cast<int32_t>(xq[c + 3]);
        }
        for (; c < inDim; ++c) {
            acc += static_cast<int32_t>(wRow[c]) * static_cast<int32_t>(xq[c]);
        }

        // Dequantize: result = acc * wScale * xScale
        output[r] = static_cast<float>(acc) * wScale * xScale;

        // Zero-point correction for asymmetric quantization
        if (!config_.symmetric) {
            // Correction term: -zp_x * sum(W_q[r,:]) * wScale * xScale
            // Simplified: absorbed into the accumulator bias
            int32_t wRowSum = 0;
            for (uint32_t cc = 0; cc < inDim; ++cc) {
                wRowSum += static_cast<int32_t>(wRow[cc]);
            }
            output[r] -= static_cast<float>(xZeroPoint) *
                         static_cast<float>(wRowSum) * wScale * xScale;
        }

        // Add bias
        if (bias) {
            output[r] += bias[r];
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    totalLinearOps_.fetch_add(1, std::memory_order_relaxed);
    totalDotProductOps_.fetch_add(outDim, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        linearLatencyAccum_ += elapsedMs;
    }

    return VisionResult::ok("Quantized linear computed");
}

// ============================================================================
// Batch Quantized Linear
// ============================================================================

VisionResult VisionQuantizedEncoder::linearQ8Batch(
    const QuantizedTensor& weight,
    const float* inputs,
    const float* bias,
    float* outputs,
    uint32_t inDim,
    uint32_t outDim,
    uint32_t batchSize)
{
    for (uint32_t b = 0; b < batchSize; ++b) {
        const float* in = inputs + static_cast<size_t>(b) * inDim;
        float* out = outputs + static_cast<size_t>(b) * outDim;

        VisionResult r = linearQ8(weight, in, bias, out, inDim, outDim);
        if (!r.success) return r;
    }

    return VisionResult::ok("Batch quantized linear computed");
}

// ============================================================================
// Full Quantized Inference Pipeline
// ============================================================================
// Replaces VisionEncoder::inferVisionModel with INT8 path:
//   1. For each patch: project through quantized patch_embed [embDim × patchDim]
//   2. Optionally project through quantized mm_projector
//   3. L2 normalize
//   4. Compute global embedding as mean of patches

VisionResult VisionQuantizedEncoder::inferQuantized(
    const std::vector<float>& patches,
    uint32_t numPatches,
    uint32_t patchDim,
    const QuantizedTensor& patchEmbedWeight,
    const QuantizedTensor* projectorWeight,
    VisionEmbedding& output)
{
    if (patches.empty() || numPatches == 0 || patchDim == 0) {
        return VisionResult::error("Invalid patch data", 1);
    }

    uint32_t embDim = patchEmbedWeight.rows;

    if (patchEmbedWeight.cols != patchDim) {
        return VisionResult::error("Patch embed weight dimension mismatch", 2);
    }

    output.patchEmbeddings.clear();
    output.patchEmbeddings.reserve(numPatches);

    // Step 1: Project each patch through quantized patch_embed
    std::vector<float> patchEmb(embDim);

    for (uint32_t p = 0; p < numPatches; ++p) {
        const float* patchData = patches.data() + static_cast<size_t>(p) * patchDim;

        VisionResult r = linearQ8(patchEmbedWeight, patchData, nullptr,
                                   patchEmb.data(), patchDim, embDim);
        if (!r.success) return r;

        // Step 2: Optional mm_projector
        if (projectorWeight && projectorWeight->isValid()) {
            uint32_t projDim = projectorWeight->rows;
            std::vector<float> projected(projDim);

            r = linearQ8(*projectorWeight, patchEmb.data(), nullptr,
                          projected.data(), embDim, projDim);
            if (!r.success) return r;

            output.patchEmbeddings.push_back(std::move(projected));
        } else {
            output.patchEmbeddings.push_back(patchEmb);
        }
    }

    // Step 3: Global embedding = mean of patch embeddings
    uint32_t finalDim = static_cast<uint32_t>(output.patchEmbeddings[0].size());
    output.embedding.assign(finalDim, 0.0f);

    for (const auto& patch : output.patchEmbeddings) {
        for (uint32_t d = 0; d < std::min(finalDim, static_cast<uint32_t>(patch.size())); ++d) {
            output.embedding[d] += patch[d];
        }
    }

    float invN = 1.0f / static_cast<float>(numPatches);
    for (uint32_t d = 0; d < finalDim; ++d) {
        output.embedding[d] *= invN;
    }

    // Step 4: L2 normalize global embedding
    float norm = 0.0f;
    for (uint32_t d = 0; d < finalDim; ++d) {
        norm += output.embedding[d] * output.embedding[d];
    }
    norm = sqrtf(norm);
    if (norm > 1e-10f) {
        float invNorm = 1.0f / norm;
        for (uint32_t d = 0; d < finalDim; ++d) {
            output.embedding[d] *= invNorm;
        }
    }

    output.numPatches = numPatches;
    output.confidence = 0.85f; // Slightly lower confidence for quantized inference

    return VisionResult::ok("Quantized inference complete");
}

// ============================================================================
// Calibration
// ============================================================================

void VisionQuantizedEncoder::beginCalibration(uint32_t channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    calibration_.channelMins.assign(channels, 1e30f);
    calibration_.channelMaxs.assign(channels, -1e30f);
    calibration_.samplesCollected = 0;
    calibration_.ready = false;
}

void VisionQuantizedEncoder::observeCalibration(const float* activations,
                                                  uint32_t length) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t channels = static_cast<uint32_t>(calibration_.channelMins.size());
    if (channels == 0) return;

    // Update per-channel min/max
    for (uint32_t i = 0; i < length; ++i) {
        uint32_t ch = i % channels;
        calibration_.channelMins[ch] = std::min(calibration_.channelMins[ch], activations[i]);
        calibration_.channelMaxs[ch] = std::max(calibration_.channelMaxs[ch], activations[i]);
    }

    calibration_.samplesCollected++;
}

void VisionQuantizedEncoder::endCalibration() {
    std::lock_guard<std::mutex> lock(mutex_);
    calibration_.ready = (calibration_.samplesCollected >= config_.calibrationSamples);
}

bool VisionQuantizedEncoder::isCalibrated() const {
    return calibration_.ready;
}

// ============================================================================
// Statistics
// ============================================================================

QuantStats VisionQuantizedEncoder::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    QuantStats stats = {};
    stats.totalQuantizeOps = totalQuantizeOps_.load();
    stats.totalDequantizeOps = totalDequantizeOps_.load();
    stats.totalLinearOps = totalLinearOps_.load();
    stats.totalDotProductOps = totalDotProductOps_.load();
    stats.memorySavedBytes = memorySavedBytes_.load();

    if (quantErrorCount_ > 0) {
        stats.avgQuantizationError = quantErrorAccum_ / static_cast<double>(quantErrorCount_);
    }

    uint64_t linOps = stats.totalLinearOps;
    if (linOps > 0) {
        stats.avgLinearLatencyMs = linearLatencyAccum_ / static_cast<double>(linOps);
    }

    return stats;
}

void VisionQuantizedEncoder::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalQuantizeOps_.store(0);
    totalDequantizeOps_.store(0);
    totalLinearOps_.store(0);
    totalDotProductOps_.store(0);
    memorySavedBytes_.store(0);
    quantErrorAccum_ = 0.0;
    quantErrorCount_ = 0;
    linearLatencyAccum_ = 0.0;
}

} // namespace Vision
} // namespace RawrXD
