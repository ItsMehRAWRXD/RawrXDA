/**
 * @file kv_cache_fp8_quantization.cpp
 * @brief KV-Cache FP8/INT8 Quantization Implementation
 * 
 * Implementation of FP8 E4M3 quantization for 50% memory bandwidth reduction.
 * 
 * @author RawrXD Performance Team
 * @version 1.0.0
 */

#include "kv_cache_fp8_quantization.h"
#include <cstring>
#include <algorithm>
#include <math>

namespace RawrXD {
namespace GPU {

// ============================================================================
// FP8 E4M3 Constants
// ============================================================================

// E4M3: 1 sign bit, 4 exponent bits, 3 mantissa bits
// Exponent bias: 7
// Max finite value: 2^(15-7) * (1 + 7/8) = 256 * 1.875 = 480 (clamped to 448)
// Min normal: 2^(1-7) = 1/64 = 0.015625
// Min subnormal: 2^(1-7) * 1/8 = 1/512 = 0.001953125

static constexpr uint32_t FP8_E4M3_EXP_BIAS = 7;
static constexpr uint32_t FP8_E4M3_EXP_BITS = 4;
static constexpr uint32_t FP8_E4M3_MAN_BITS = 3;
static constexpr float FP8_E4M3_MAX = 448.0f;  // Clamped max
static constexpr float FP8_E4M3_MIN_NORMAL = 0.015625f;
static constexpr float FP8_E4M3_MIN_SUBNORMAL = 0.001953125f;

// Lookup table for FP8 E4M3 to float conversion (256 entries)
static float g_fp8E4M3ToFloatTable[256];
static bool g_fp8TableInitialized = false;

static void initFP8Table() {
    if (g_fp8TableInitialized) return;
    
    for (int i = 0; i < 256; ++i) {
        uint8_t u = static_cast<uint8_t>(i);
        
        // Extract fields
        uint32_t sign = (u >> 7) & 0x1;
        uint32_t exp = (u >> 3) & 0xF;
        uint32_t man = u & 0x7;
        
        float result;
        if (exp == 0xF) {
            // Special: NaN or Inf (treat as max finite)
            result = FP8_E4M3_MAX;
        } else if (exp == 0) {
            // Subnormal
            result = std::ldexp(static_cast<float>(man), 1 - FP8_E4M3_EXP_BIAS - FP8_E4M3_MAN_BITS);
        } else {
            // Normal
            result = std::ldexp(static_cast<float>((1 << FP8_E4M3_MAN_BITS) | man), 
                               static_cast<int>(exp) - FP8_E4M3_EXP_BIAS - FP8_E4M3_MAN_BITS);
        }
        
        if (sign) result = -result;
        g_fp8E4M3ToFloatTable[i] = result;
    }
    
    g_fp8TableInitialized = true;
}

// ============================================================================
// FP8E4M3 Implementation
// ============================================================================

FP8E4M3::FP8E4M3(float f) {
    data = floatToFP8E4M3(f);
}

float FP8E4M3::toFloat() const {
    if (!g_fp8TableInitialized) initFP8Table();
    return g_fp8E4M3ToFloatTable[data];
}

uint8_t FP8E4M3::floatToFP8E4M3(float f) {
    if (!g_fp8TableInitialized) initFP8Table();
    
    // Handle special cases
    if (std::isnan(f) || std::isinf(f)) {
        return 0x7F; // NaN
    }
    
    // Clamp to range
    if (f > FP8_E4M3_MAX) f = FP8_E4M3_MAX;
    if (f < -FP8_E4M3_MAX) f = -FP8_E4M3_MAX;
    
    if (f == 0.0f) return 0;
    
    // Extract sign
    uint32_t sign = (f < 0.0f) ? 0x80 : 0x00;
    float absF = std::abs(f);
    
    // Compute exponent and mantissa
    int exp;
    float mantissa = std::frexp(absF, &exp);
    
    // Adjust for E4M3 format
    // frexp returns mantissa in [0.5, 1.0), we need [1.0, 2.0)
    mantissa *= 2.0f;
    exp--;
    
    // Apply bias
    int biasedExp = exp + FP8_E4M3_EXP_BIAS + FP8_E4M3_MAN_BITS;
    
    // Compute mantissa bits
    float manFloat = mantissa * (1 << FP8_E4M3_MAN_BITS);
    uint32_t man = static_cast<uint32_t>(manFloat + 0.5f) & ((1 << FP8_E4M3_MAN_BITS) - 1);
    
    // Handle exponent overflow/underflow
    if (biasedExp >= (1 << FP8_E4M3_EXP_BITS) - 1) {
        // Max exponent
        biasedExp = (1 << FP8_E4M3_EXP_BITS) - 2;
        man = (1 << FP8_E4M3_MAN_BITS) - 1;
    } else if (biasedExp <= 0) {
        // Subnormal or underflow to zero
        if (biasedExp < -FP8_E4M3_MAN_BITS) {
            return sign; // Underflow to zero
        }
        // Subnormal
        man = static_cast<uint32_t>(manFloat * std::ldexp(1.0f, biasedExp - 1) + 0.5f);
        biasedExp = 0;
    }
    
    return sign | (biasedExp << FP8_E4M3_MAN_BITS) | man;
}

float FP8E4M3::fp8E4M3ToFloat(uint8_t u) {
    if (!g_fp8TableInitialized) initFP8Table();
    return g_fp8E4M3ToFloatTable[u];
}

// ============================================================================
// INT8Quantized Implementation
// ============================================================================

INT8Quantized INT8Quantized::fromFloat(float f, float s) {
    if (s == 0.0f) s = 1.0f;
    float scaled = f / s;
    // Clamp to INT8 range
    if (scaled > 127.0f) scaled = 127.0f;
    if (scaled < -128.0f) scaled = -128.0f;
    return INT8Quantized(static_cast<int8_t>(std::round(scaled)), s);
}

// ============================================================================
// KVCacheQuantizer Implementation
// ============================================================================

KVCacheQuantizer::KVCacheQuantizer(const KVCacheQuantConfig& config)
    : config_(config), stats_{} {
    initFP8Table();
}

KVCacheQuantizer::QuantizedKVBlock KVCacheQuantizer::quantizeBlock(
    const float* keys,
    const float* values,
    uint32_t numHeads,
    uint32_t seqLen,
    uint32_t headDim) {
    
    QuantizedKVBlock block;
    block.numHeads = numHeads;
    block.seqLen = seqLen;
    block.headDim = headDim;
    
    const size_t totalValues = static_cast<size_t>(numHeads) * seqLen * headDim;
    
    // Allocate quantized storage
    block.quantizedKeys.resize(totalValues);
    block.quantizedValues.resize(totalValues);
    
    if (config_.perChannelScale) {
        block.keyScales.resize(numHeads);
        block.valueScales.resize(numHeads);
    } else {
        block.keyScales.resize(1);
        block.valueScales.resize(1);
    }
    
    // Quantize keys
    for (uint32_t h = 0; h < numHeads; ++h) {
        float keyScale = 1.0f;
        
        if (config_.perChannelScale) {
            // Compute scale for this head
            const float* headKeys = keys + h * seqLen * headDim;
            keyScale = computeScale(headKeys, seqLen * headDim);
            block.keyScales[h] = keyScale;
        } else if (h == 0) {
            block.keyScales[0] = computeScale(keys, totalValues);
            keyScale = block.keyScales[0];
        }
        
        // Quantize each value
        for (uint32_t s = 0; s < seqLen; ++s) {
            for (uint32_t d = 0; d < headDim; ++d) {
                size_t idx = (h * seqLen + s) * headDim + d;
                float val = keys[idx];
                
                switch (config_.keyFormat) {
                    case KVCacheFormat::FP8_E4M3:
                        block.quantizedKeys[idx] = FP8E4M3::floatToFP8E4M3(val);
                        break;
                    case KVCacheFormat::INT8_SYM:
                        block.quantizedKeys[idx] = static_cast<uint8_t>(
                            floatToINT8(val, keyScale));
                        break;
                    default:
                        // Fallback to FP8
                        block.quantizedKeys[idx] = FP8E4M3::floatToFP8E4M3(val);
                        break;
                }
            }
        }
    }
    
    // Quantize values
    for (uint32_t h = 0; h < numHeads; ++h) {
        float valueScale = 1.0f;
        
        if (config_.perChannelScale) {
            const float* headValues = values + h * seqLen * headDim;
            valueScale = computeScale(headValues, seqLen * headDim);
            block.valueScales[h] = valueScale;
        } else if (h == 0) {
            block.valueScales[0] = computeScale(values, totalValues);
            valueScale = block.valueScales[0];
        }
        
        for (uint32_t s = 0; s < seqLen; ++s) {
            for (uint32_t d = 0; d < headDim; ++d) {
                size_t idx = (h * seqLen + s) * headDim + d;
                float val = values[idx];
                
                switch (config_.valueFormat) {
                    case KVCacheFormat::FP8_E4M3:
                        block.quantizedValues[idx] = FP8E4M3::floatToFP8E4M3(val);
                        break;
                    case KVCacheFormat::INT8_SYM:
                        block.quantizedValues[idx] = static_cast<uint8_t>(
                            floatToINT8(val, valueScale));
                        break;
                    default:
                        block.quantizedValues[idx] = FP8E4M3::floatToFP8E4M3(val);
                        break;
                }
            }
        }
    }
    
    // Update stats
    stats_.blocksQuantized++;
    stats_.bytesSaved += totalValues * (4 - config_.bytesPerValue()); // 4 bytes FP32 - quantized size
    
    return block;
}

void KVCacheQuantizer::dequantizeKeys(
    const QuantizedKVBlock& block,
    uint32_t headIdx,
    uint32_t seqStart,
    uint32_t seqCount,
    float* outKeys) {
    
    float scale = config_.perChannelScale ? block.keyScales[headIdx] : block.keyScales[0];
    
    for (uint32_t s = 0; s < seqCount; ++s) {
        for (uint32_t d = 0; d < block.headDim; ++d) {
            size_t inIdx = (headIdx * block.seqLen + seqStart + s) * block.headDim + d;
            size_t outIdx = s * block.headDim + d;
            
            switch (config_.keyFormat) {
                case KVCacheFormat::FP8_E4M3:
                    outKeys[outIdx] = FP8E4M3::fp8E4M3ToFloat(block.quantizedKeys[inIdx]);
                    break;
                case KVCacheFormat::INT8_SYM:
                    outKeys[outIdx] = int8ToFloat(
                        static_cast<int8_t>(block.quantizedKeys[inIdx]), scale);
                    break;
                default:
                    outKeys[outIdx] = FP8E4M3::fp8E4M3ToFloat(block.quantizedKeys[inIdx]);
                    break;
            }
        }
    }
}

void KVCacheQuantizer::dequantizeValues(
    const QuantizedKVBlock& block,
    uint32_t headIdx,
    uint32_t seqStart,
    uint32_t seqCount,
    float* outValues) {
    
    float scale = config_.perChannelScale ? block.valueScales[headIdx] : block.valueScales[0];
    
    for (uint32_t s = 0; s < seqCount; ++s) {
        for (uint32_t d = 0; d < block.headDim; ++d) {
            size_t inIdx = (headIdx * block.seqLen + seqStart + s) * block.headDim + d;
            size_t outIdx = s * block.headDim + d;
            
            switch (config_.valueFormat) {
                case KVCacheFormat::FP8_E4M3:
                    outValues[outIdx] = FP8E4M3::fp8E4M3ToFloat(block.quantizedValues[inIdx]);
                    break;
                case KVCacheFormat::INT8_SYM:
                    outValues[outIdx] = int8ToFloat(
                        static_cast<int8_t>(block.quantizedValues[inIdx]), scale);
                    break;
                default:
                    outValues[outIdx] = FP8E4M3::fp8E4M3ToFloat(block.quantizedValues[inIdx]);
                    break;
            }
        }
    }
}

float KVCacheQuantizer::computeAttentionDotProduct(
    const QuantizedKVBlock& block,
    uint32_t headIdx,
    const float* query,
    uint32_t kvSeqIdx) {
    
    float scale = config_.perChannelScale ? block.keyScales[headIdx] : block.keyScales[0];
    float result = 0.0f;
    
    for (uint32_t d = 0; d < block.headDim; ++d) {
        size_t keyIdx = (headIdx * block.seqLen + kvSeqIdx) * block.headDim + d;
        float keyVal;
        
        switch (config_.keyFormat) {
            case KVCacheFormat::FP8_E4M3:
                keyVal = FP8E4M3::fp8E4M3ToFloat(block.quantizedKeys[keyIdx]);
                break;
            case KVCacheFormat::INT8_SYM:
                keyVal = int8ToFloat(static_cast<int8_t>(block.quantizedKeys[keyIdx]), scale);
                break;
            default:
                keyVal = FP8E4M3::fp8E4M3ToFloat(block.quantizedKeys[keyIdx]);
                break;
        }
        
        result += query[d] * keyVal;
    }
    
    return result;
}

float KVCacheQuantizer::computeScale(const float* data, uint32_t count) {
    // Compute max absolute value for symmetric quantization
    float maxAbs = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        maxAbs = std::max(maxAbs, std::abs(data[i]));
    }
    
    if (maxAbs < 1e-6f) return 1.0f;
    
    // Scale such that max value maps to 127 (INT8 max)
    return maxAbs / 127.0f;
}

int8_t KVCacheQuantizer::floatToINT8(float f, float scale) {
    if (scale == 0.0f) return 0;
    float scaled = f / scale;
    if (scaled > 127.0f) return 127;
    if (scaled < -128.0f) return -128;
    return static_cast<int8_t>(std::round(scaled));
}

float KVCacheQuantizer::int8ToFloat(int8_t i, float scale) {
    return static_cast<float>(i) * scale;
}

// ============================================================================
// GPU Kernel Stubs (to be implemented as HLSL/CUDA)
// ============================================================================

extern "C" {

__declspec(dllexport) void QuantizeKVCache_FP8(
    const float* keysIn,
    const float* valuesIn,
    uint8_t* keysOut,
    uint8_t* valuesOut,
    float* keyScales,
    float* valueScales,
    uint32_t numHeads,
    uint32_t seqLen,
    uint32_t headDim) {
    
    // CPU reference implementation
    // GPU implementation would use compute shaders or CUDA kernels
    
    KVCacheQuantConfig config;
    config.keyFormat = KVCacheFormat::FP8_E4M3;
    config.valueFormat = KVCacheFormat::FP8_E4M3;
    config.perChannelScale = true;
    
    KVCacheQuantizer quantizer(config);
    auto block = quantizer.quantizeBlock(keysIn, valuesIn, numHeads, seqLen, headDim);
    
    // Copy results to output buffers
    std::memcpy(keysOut, block.quantizedKeys.data(), block.quantizedKeys.size());
    std::memcpy(valuesOut, block.quantizedValues.data(), block.quantizedValues.size());
    std::memcpy(keyScales, block.keyScales.data(), block.keyScales.size() * sizeof(float));
    std::memcpy(valueScales, block.valueScales.data(), block.valueScales.size() * sizeof(float));
}

__declspec(dllexport) void DequantizeKVCache_FP8(
    const uint8_t* keysIn,
    const uint8_t* valuesIn,
    const float* keyScales,
    const float* valueScales,
    float* keysOut,
    float* valuesOut,
    uint32_t numHeads,
    uint32_t seqLen,
    uint32_t headDim) {
    
    // Reconstruct QuantizedKVBlock from inputs
    KVCacheQuantizer::QuantizedKVBlock block;
    block.numHeads = numHeads;
    block.seqLen = seqLen;
    block.headDim = headDim;
    
    size_t totalValues = static_cast<size_t>(numHeads) * seqLen * headDim;
    block.quantizedKeys.assign(keysIn, keysIn + totalValues);
    block.quantizedValues.assign(valuesIn, valuesIn + totalValues);
    block.keyScales.assign(keyScales, keyScales + numHeads);
    block.valueScales.assign(valueScales, valueScales + numHeads);
    
    KVCacheQuantConfig config;
    config.keyFormat = KVCacheFormat::FP8_E4M3;
    config.valueFormat = KVCacheFormat::FP8_E4M3;
    config.perChannelScale = true;
    
    KVCacheQuantizer quantizer(config);
    
    // Dequantize all
    for (uint32_t h = 0; h < numHeads; ++h) {
        quantizer.dequantizeKeys(block, h, 0, seqLen, keysOut + h * seqLen * headDim);
        quantizer.dequantizeValues(block, h, 0, seqLen, valuesOut + h * seqLen * headDim);
    }
}

__declspec(dllexport) void Attention_FusedFP8(
    const uint8_t* keys,
    const uint8_t* values,
    const float* keyScales,
    const float* valueScales,
    const float* queries,
    float* output,
    uint32_t numHeads,
    uint32_t seqLen,
    uint32_t headDim,
    uint32_t batchSize) {
    
    // Fused attention with on-the-fly dequantization
    // This is the high-performance path that avoids materializing full FP32 KV cache
    
    KVCacheQuantizer::QuantizedKVBlock block;
    block.numHeads = numHeads;
    block.seqLen = seqLen;
    block.headDim = headDim;
    
    size_t totalValues = static_cast<size_t>(numHeads) * seqLen * headDim;
    block.quantizedKeys.assign(keys, keys + totalValues);
    block.quantizedValues.assign(values, values + totalValues);
    block.keyScales.assign(keyScales, keyScales + numHeads);
    block.valueScales.assign(valueScales, valueScales + numHeads);
    
    KVCacheQuantConfig config;
    config.keyFormat = KVCacheFormat::FP8_E4M3;
    config.valueFormat = KVCacheFormat::FP8_E4M3;
    config.perChannelScale = true;
    
    KVCacheQuantizer quantizer(config);
    
    // Process each batch and head
    for (uint32_t b = 0; b < batchSize; ++b) {
        for (uint32_t h = 0; h < numHeads; ++h) {
            const float* query = queries + ((b * numHeads + h) * headDim);
            float* out = output + ((b * numHeads + h) * headDim);
            
            // Compute attention scores (Q @ K^T)
            std::vector<float> scores(seqLen);
            for (uint32_t s = 0; s < seqLen; ++s) {
                scores[s] = quantizer.computeAttentionDotProduct(block, h, query, s);
            }
            
            // Softmax (simplified - no masking for now)
            float maxScore = *std::max_element(scores.begin(), scores.end());
            float sumExp = 0.0f;
            for (uint32_t s = 0; s < seqLen; ++s) {
                scores[s] = std::exp(scores[s] - maxScore);
                sumExp += scores[s];
            }
            for (uint32_t s = 0; s < seqLen; ++s) {
                scores[s] /= sumExp;
            }
            
            // Weighted sum of values (scores @ V)
            std::fill(out, out + headDim, 0.0f);
            for (uint32_t s = 0; s < seqLen; ++s) {
                size_t vIdx = (h * seqLen + s) * headDim;
                float vScale = config.perChannelScale ? valueScales[h] : valueScales[0];
                
                for (uint32_t d = 0; d < headDim; ++d) {
                    float vVal = FP8E4M3::fp8E4M3ToFloat(block.quantizedValues[vIdx + d]);
                    out[d] += scores[s] * vVal;
                }
            }
        }
    }
}

} // extern "C"

} // namespace GPU
} // namespace RawrXD
