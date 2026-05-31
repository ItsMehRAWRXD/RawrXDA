// ============================================================================
// RawrXD Sovereign FP8 Quantizer Implementation
// Direct bit-manipulation without vendor telemetry hooks
// ============================================================================

#include "fp8_quantizer.h"
#include <cstring>
#include <limits>
#include <immintrin.h>  // For AVX-512 intrinsics

namespace RawrXD {
namespace Quantization {

// ============================================================================
// SovereignFP8Quantizer Implementation
// ============================================================================

SovereignFP8Quantizer::SovereignFP8Quantizer(FP8Format format)
    : m_format(format)
    , m_stochastic(true)
    , m_randState(0x12345678)
{
}

const char* SovereignFP8Quantizer::getFormatName() const {
    switch (m_format) {
        case FP8Format::E4M3: return "E4M3";
        case FP8Format::E5M2: return "E5M2";
        default: return "Unknown";
    }
}

uint32_t SovereignFP8Quantizer::xorshift32() const {
    // Simple PRNG for stochastic rounding
    // Note: mutable would be better but we use const_cast for simplicity
    uint32_t& state = const_cast<uint32_t&>(m_randState);
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float SovereignFP8Quantizer::stochasticRound(float value) const {
    if (!m_stochastic) return value;
    
    // Add random noise in [-0.5, 0.5) ULP for unbiased rounding
    float noise = (static_cast<float>(xorshift32() & 0xFFFF) / 65536.0f) - 0.5f;
    return value + noise * std::numeric_limits<float>::epsilon() * std::abs(value);
}

// Extract IEEE 754 float components
struct FloatComponents {
    uint32_t sign;
    uint32_t exponent;
    uint32_t mantissa;
    
    explicit FloatComponents(float f) {
        uint32_t bits;
        static_assert(sizeof(f) == sizeof(bits), "Float size mismatch");
        std::memcpy(&bits, &f, sizeof(bits));
        
        sign = (bits >> 31) & 0x1;
        exponent = (bits >> 23) & 0xFF;
        mantissa = bits & 0x7FFFFF;
    }
};

uint8_t SovereignFP8Quantizer::floatToE4M3(float value) const {
    // Handle special cases
    if (std::isnan(value)) return 0x7F;  // NaN
    if (std::isinf(value)) return (value > 0) ? 0x7E : 0xFE;  // +/- inf (clamped)
    if (value == 0.0f) return 0;
    
    // Clamp to E4M3 range
    float clamped = std::max(-FP8_E4M3_MAX, std::min(FP8_E4M3_MAX, value));
    
    FloatComponents fc(clamped);
    
    // Extract sign
    uint8_t sign_bit = fc.sign << 7;
    
    // Adjust exponent bias from 127 (FP32) to 7 (E4M3)
    int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E4M3_BIAS;
    
    // Handle denormals and underflow
    if (exp <= 0) {
        // Denormal or underflow to zero
        if (exp < -3) return sign_bit;  // Underflow to signed zero
        
        // Denormal: shift mantissa right
        uint32_t mant = fc.mantissa | 0x800000;  // Add implicit leading 1
        mant >>= (1 - exp);  // Right shift to denormalize
        uint8_t mant_bits = (mant >> 20) & 0x7;  // Keep top 3 bits
        
        return sign_bit | mant_bits;
    }
    
    // Clamp exponent to 4-bit range
    if (exp > 15) exp = 15;  // Max exponent for E4M3
    
    // Extract top 3 bits of mantissa
    uint8_t mant_bits = (fc.mantissa >> 20) & 0x7;
    
    // Round to nearest even
    uint32_t round_bit = (fc.mantissa >> 19) & 0x1;
    uint32_t sticky_bits = fc.mantissa & 0x7FFFF;
    
    if (round_bit && (sticky_bits || (mant_bits & 0x1))) {
        mant_bits++;
        if (mant_bits > 0x7) {
            mant_bits = 0;
            exp++;
            if (exp > 15) {
                exp = 15;
                mant_bits = 0x6;  // 448.0 = 0x7E (largest finite)
            }
        }
    }
    
    uint8_t exp_bits = (exp & 0xF) << 3;
    return sign_bit | exp_bits | mant_bits;
}

uint8_t SovereignFP8Quantizer::floatToE5M2(float value) const {
    // Handle special cases
    if (std::isnan(value)) return 0x7F;  // NaN
    if (std::isinf(value)) return (value > 0) ? 0x7B : 0xFB;  // +/- inf
    if (value == 0.0f) return 0;
    
    // Clamp to E5M2 range
    float clamped = std::max(-FP8_E5M2_MAX, std::min(FP8_E5M2_MAX, value));
    
    FloatComponents fc(clamped);
    
    // Extract sign
    uint8_t sign_bit = fc.sign << 7;
    
    // Adjust exponent bias from 127 (FP32) to 15 (E5M2)
    int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E5M2_BIAS;
    
    // Handle denormals
    if (exp <= 0) {
        if (exp < -1) return sign_bit;  // Underflow
        
        // Denormal
        uint32_t mant = fc.mantissa | 0x800000;
        mant >>= (1 - exp);
        uint8_t mant_bits = (mant >> 21) & 0x3;
        return sign_bit | mant_bits;
    }
    
    // Clamp exponent to 5-bit range
    if (exp > 31) exp = 31;
    
    // Extract top 2 bits of mantissa
    uint8_t mant_bits = (fc.mantissa >> 21) & 0x3;
    
    // Round to nearest even
    uint32_t round_bit = (fc.mantissa >> 20) & 0x1;
    uint32_t sticky_bits = fc.mantissa & 0xFFFFF;
    
    if (round_bit && (sticky_bits || (mant_bits & 0x1))) {
        mant_bits++;
        if (mant_bits > 0x3) {
            mant_bits = 0;
            exp++;
            if (exp > 31) {
                exp = 31;
                mant_bits = 0x2;  // Clamp to max finite
            }
        }
    }
    
    uint8_t exp_bits = (exp & 0x1F) << 2;
    return sign_bit | exp_bits | mant_bits;
}

float SovereignFP8Quantizer::e4m3ToFloat(uint8_t fp8) const {
    if (fp8 == 0) return 0.0f;
    
    uint8_t sign_bit = (fp8 >> 7) & 0x1;
    uint8_t exp_bits = (fp8 >> 3) & 0xF;
    uint8_t mant_bits = fp8 & 0x7;
    
    // Reconstruct FP32
    uint32_t sign = sign_bit << 31;
    
    if (exp_bits == 0) {
        // Denormal: value = (-1)^sign * 2^(1-bias) * 0.mantissa
        // For E4M3: 2^(-6) * mantissa/8
        float val = static_cast<float>(mant_bits) / 8.0f * std::pow(2.0f, -6.0f);
        return sign_bit ? -val : val;
    }
    
    // Normal number
    int32_t exp = static_cast<int32_t>(exp_bits) - FP8_E4M3_BIAS + 127;
    uint32_t mantissa = static_cast<uint32_t>(mant_bits) << 20;
    
    uint32_t result = sign | (static_cast<uint32_t>(exp) << 23) | mantissa;
    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f;
}

float SovereignFP8Quantizer::e5m2ToFloat(uint8_t fp8) const {
    if (fp8 == 0) return 0.0f;
    
    uint8_t sign_bit = (fp8 >> 7) & 0x1;
    uint8_t exp_bits = (fp8 >> 2) & 0x1F;
    uint8_t mant_bits = fp8 & 0x3;
    
    uint32_t sign = sign_bit << 31;
    
    if (exp_bits == 0) {
        // Denormal
        float val = static_cast<float>(mant_bits) / 4.0f * std::pow(2.0f, -14.0f);
        return sign_bit ? -val : val;
    }
    
    int32_t exp = static_cast<int32_t>(exp_bits) - FP8_E5M2_BIAS + 127;
    uint32_t mantissa = static_cast<uint32_t>(mant_bits) << 21;
    
    uint32_t result = sign | (static_cast<uint32_t>(exp) << 23) | mantissa;
    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f;
}

uint8_t SovereignFP8Quantizer::quantize(float value) const {
    if (m_stochastic) {
        value = stochasticRound(value);
    }
    
    switch (m_format) {
        case FP8Format::E4M3: return floatToE4M3(value);
        case FP8Format::E5M2: return floatToE5M2(value);
        default: return floatToE4M3(value);
    }
}

float SovereignFP8Quantizer::dequantize(uint8_t fp8) const {
    switch (m_format) {
        case FP8Format::E4M3: return e4m3ToFloat(fp8);
        case FP8Format::E5M2: return e5m2ToFloat(fp8);
        default: return e4m3ToFloat(fp8);
    }
}

void SovereignFP8Quantizer::quantizeBatch(const float* input, uint8_t* output, size_t count, float scale) {
    // Use AVX-512 if available and count is large enough
    #ifdef __AVX512F__
    if (count >= 64) {
        if (m_format == FP8Format::E4M3) {
            quantizeBatchAVX512_E4M3(input, output, count, scale);
            return;
        } else {
            quantizeBatchAVX512_E5M2(input, output, count, scale);
            return;
        }
    }
    #endif
    
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        output[i] = quantize(input[i] * scale);
    }
}

void SovereignFP8Quantizer::dequantizeBatch(const uint8_t* input, float* output, size_t count, float scale) const {
    #ifdef __AVX512F__
    if (count >= 64) {
        if (m_format == FP8Format::E4M3) {
            dequantizeBatchAVX512_E4M3(input, output, count, scale);
            return;
        } else {
            dequantizeBatchAVX512_E5M2(input, output, count, scale);
            return;
        }
    }
    #endif
    
    for (size_t i = 0; i < count; ++i) {
        output[i] = dequantize(input[i]) / scale;
    }
}

float SovereignFP8Quantizer::computeScale(const float* data, size_t count) {
    // Find absmax for symmetric quantization
    float max_val = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        max_val = std::max(max_val, std::abs(data[i]));
    }
    
    if (max_val == 0.0f) return 1.0f;
    
    // Scale = max_representable / max_abs_value
    float max_repr = (m_format == FP8Format::E4M3) ? FP8_E4M3_MAX : FP8_E5M2_MAX;
    return max_repr / max_val;
}

// ============================================================================
// AVX-512 Accelerated Implementations
// ============================================================================

#ifdef __AVX512F__

void quantizeBatchAVX512_E4M3(const float* input, uint8_t* output, size_t count, float scale) {
    // Process 16 floats at a time (512 bits / 32 bits per float)
    const __m512 scale_vec = _mm512_set1_ps(scale);
    
    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        __m512 vec = _mm512_loadu_ps(&input[i]);
        vec = _mm512_mul_ps(vec, scale_vec);
        
        // Clamp to E4M3 range
        __m512 max_val = _mm512_set1_ps(FP8_E4M3_MAX);
        __m512 min_val = _mm512_set1_ps(-FP8_E4M3_MAX);
        vec = _mm512_max_ps(min_val, _mm512_min_ps(max_val, vec));
        
        // Extract and pack to FP8 (simplified - full impl would use lookup tables)
        alignas(64) float temp[16];
        _mm512_storeu_ps(temp, vec);
        
        for (int j = 0; j < 16; ++j) {
            // Use scalar conversion for now - can be optimized with shuffle
            FloatComponents fc(temp[j]);
            uint8_t sign_bit = fc.sign << 7;
            int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E4M3_BIAS;
            
            if (exp <= 0) {
                output[i + j] = sign_bit;  // Underflow to zero
            } else {
                exp = std::min(15, exp);
                uint8_t mant_bits = (fc.mantissa >> 20) & 0x7;
                output[i + j] = sign_bit | ((exp & 0xF) << 3) | mant_bits;
            }
        }
    }
    
    // Scalar tail
    for (; i < count; ++i) {
        float val = input[i] * scale;
        val = std::max(-FP8_E4M3_MAX, std::min(FP8_E4M3_MAX, val));
        FloatComponents fc(val);
        uint8_t sign_bit = fc.sign << 7;
        int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E4M3_BIAS;
        if (exp <= 0) {
            output[i] = sign_bit;
        } else {
            exp = std::min(15, exp);
            uint8_t mant_bits = (fc.mantissa >> 20) & 0x7;
            output[i] = sign_bit | ((exp & 0xF) << 3) | mant_bits;
        }
    }
}

void quantizeBatchAVX512_E5M2(const float* input, uint8_t* output, size_t count, float scale) {
    const __m512 scale_vec = _mm512_set1_ps(scale);
    
    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        __m512 vec = _mm512_loadu_ps(&input[i]);
        vec = _mm512_mul_ps(vec, scale_vec);
        
        __m512 max_val = _mm512_set1_ps(FP8_E5M2_MAX);
        __m512 min_val = _mm512_set1_ps(-FP8_E5M2_MAX);
        vec = _mm512_max_ps(min_val, _mm512_min_ps(max_val, vec));
        
        alignas(64) float temp[16];
        _mm512_storeu_ps(temp, vec);
        
        for (int j = 0; j < 16; ++j) {
            FloatComponents fc(temp[j]);
            uint8_t sign_bit = fc.sign << 7;
            int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E5M2_BIAS;
            
            if (exp <= 0) {
                output[i + j] = sign_bit;
            } else {
                exp = std::min(31, exp);
                uint8_t mant_bits = (fc.mantissa >> 21) & 0x3;
                output[i + j] = sign_bit | ((exp & 0x1F) << 2) | mant_bits;
            }
        }
    }
    
    for (; i < count; ++i) {
        float val = input[i] * scale;
        val = std::max(-FP8_E5M2_MAX, std::min(FP8_E5M2_MAX, val));
        FloatComponents fc(val);
        uint8_t sign_bit = fc.sign << 7;
        int32_t exp = static_cast<int32_t>(fc.exponent) - 127 + FP8_E5M2_BIAS;
        if (exp <= 0) {
            output[i] = sign_bit;
        } else {
            exp = std::min(31, exp);
            uint8_t mant_bits = (fc.mantissa >> 21) & 0x3;
            output[i] = sign_bit | ((exp & 0x1F) << 2) | mant_bits;
        }
    }
}

void dequantizeBatchAVX512_E4M3(const uint8_t* input, float* output, size_t count, float scale) {
    size_t i = 0;
    const float inv_scale = 1.0f / scale;
    const __m512 inv_scale_vec = _mm512_set1_ps(inv_scale);
    
    for (; i + 16 <= count; i += 16) {
        // Load 16 FP8 values and expand to floats
        // This is a simplified version - full impl would use gather
        for (int j = 0; j < 16; ++j) {
            uint8_t fp8 = input[i + j];
            if (fp8 == 0) {
                output[i + j] = 0.0f;
                continue;
            }
            
            uint8_t sign_bit = (fp8 >> 7) & 0x1;
            uint8_t exp_bits = (fp8 >> 3) & 0xF;
            uint8_t mant_bits = fp8 & 0x7;
            
            uint32_t sign = sign_bit << 31;
            int32_t exp = static_cast<int32_t>(exp_bits) - FP8_E4M3_BIAS + 127;
            uint32_t mantissa = static_cast<uint32_t>(mant_bits) << 20;
            
            uint32_t result = sign | (static_cast<uint32_t>(exp) << 23) | mantissa;
            float f;
            std::memcpy(&f, &result, sizeof(f));
            output[i + j] = f * inv_scale;
        }
    }
    
    for (; i < count; ++i) {
        output[i] = SovereignFP8Quantizer(FP8Format::E4M3).dequantize(input[i]) * inv_scale;
    }
}

void dequantizeBatchAVX512_E5M2(const uint8_t* input, float* output, size_t count, float scale) {
    size_t i = 0;
    const float inv_scale = 1.0f / scale;
    
    for (; i < count; ++i) {
        output[i] = SovereignFP8Quantizer(FP8Format::E5M2).dequantize(input[i]) * inv_scale;
    }
}

#endif  // __AVX512F__

// ============================================================================
// QuantizedKVBlock Implementation
// ============================================================================

void QuantizedKVBlock::allocate(size_t numElements) {
    k_data.resize(numElements);
    v_data.resize(numElements);
}

void QuantizedKVBlock::dequantizeTo(float* k_out, float* v_out, const SovereignFP8Quantizer& quantizer) {
    for (size_t i = 0; i < k_data.size(); ++i) {
        k_out[i] = quantizer.dequantize(k_data[i]) / k_scale;
        v_out[i] = quantizer.dequantize(v_data[i]) / v_scale;
    }
}

// ============================================================================
// KVCacheFP8Manager Implementation
// ============================================================================

KVCacheFP8Manager::KVCacheFP8Manager(const KVCacheFP8Config& config)
    : m_config(config)
    , m_quantizer(config.format)
{
    if (config.stochasticRounding) {
        m_quantizer.enableStochasticRounding(true);
    }
}

void KVCacheFP8Manager::quantizeAndStore(int blockId, const float* k_tensor, 
                                          const float* v_tensor, size_t numElements) {
    // Ensure block exists
    if (blockId >= static_cast<int>(m_blocks.size())) {
        m_blocks.resize(blockId + 1);
    }
    
    if (!m_blocks[blockId]) {
        m_blocks[blockId] = std::make_unique<QuantizedKVBlock>();
        m_blocks[blockId]->allocate(numElements);
    }
    
    auto& block = m_blocks[blockId];
    
    // Compute per-block scales
    block->k_scale = m_quantizer.computeScale(k_tensor, numElements);
    block->v_scale = m_quantizer.computeScale(v_tensor, numElements);
    
    // Quantize K and V
    m_quantizer.quantizeBatch(k_tensor, block->k_data.data(), numElements, block->k_scale);
    m_quantizer.quantizeBatch(v_tensor, block->v_data.data(), numElements, block->v_scale);
    
    block->token_count = static_cast<uint32_t>(numElements);
}

void KVCacheFP8Manager::retrieveAndDequantize(int blockId, float* k_out, 
                                               float* v_out, size_t numElements) {
    if (blockId < 0 || blockId >= static_cast<int>(m_blocks.size()) || !m_blocks[blockId]) {
        // Return zeros for missing blocks
        std::memset(k_out, 0, numElements * sizeof(float));
        std::memset(v_out, 0, numElements * sizeof(float));
        return;
    }
    
    auto& block = m_blocks[blockId];
    size_t toCopy = std::min(numElements, static_cast<size_t>(block->token_count));
    
    // Dequantize
    for (size_t i = 0; i < toCopy; ++i) {
        k_out[i] = m_quantizer.dequantize(block->k_data[i]) / block->k_scale;
        v_out[i] = m_quantizer.dequantize(block->v_data[i]) / block->v_scale;
    }
    
    // Zero remaining
    if (toCopy < numElements) {
        std::memset(k_out + toCopy, 0, (numElements - toCopy) * sizeof(float));
        std::memset(v_out + toCopy, 0, (numElements - toCopy) * sizeof(float));
    }
}

} // namespace Quantization
} // namespace RawrXD
