// =============================================================================
// dml_asm_fallback.cpp — Production DirectML Tensor Operation Fallbacks
// =============================================================================
// Provides CPU fallbacks for DirectML quantization operations when MASM/AVX
// implementations are unavailable. Implements GGML-compatible Q4_0/Q8_0
// dequantization with proper scale factor handling and RoPE rotary embeddings.
//
// Format Specifications:
//   Q4_0 Block (18 bytes): [fp16 scale | 32×4-bit values packed]
//   Q8_0 Block (34 bytes): [fp16 scale | 32×int8 values]
//   RoPE: Rotary Position Embedding with cos/sin lookup tables
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

// FP16 to FP32 conversion helper
static inline float fp16_to_fp32(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16u;
    uint32_t exp = (h & 0x7C00u) >> 10u;
    uint32_t mantissa = h & 0x03FFu;

    if (exp == 0u) {
        if (mantissa == 0u) {
            // Zero
            uint32_t f32_bits = sign;
            float result;
            std::memcpy(&result, &f32_bits, sizeof(float));
            return result;
        }
        // Subnormal: normalize
        while ((mantissa & 0x0400u) == 0u) {
            mantissa <<= 1u;
            exp -= 1u;
        }
        mantissa &= 0x03FFu;
        exp = 0u;
    }

    if (exp == 0x1Fu) {
        // Inf or NaN
        const uint32_t f32_bits = sign | 0x7F800000u | (mantissa << 13u);
        float result;
        std::memcpy(&result, &f32_bits, sizeof(float));
        return result;
    }

    // Normal: rebias exponent from 15 to 127
    exp = exp + (127u - 15u);
    const uint32_t f32_bits = sign | (exp << 23u) | (mantissa << 13u);
    float result;
    std::memcpy(&result, &f32_bits, sizeof(float));
    return result;
}

extern "C" {

// =============================================================================
// Q4_0 Dequantization: 4-bit quantized -> FP32
// =============================================================================
// Block format: 2-byte fp16 scale + 16 bytes of packed 4-bit values (32 values)
// Each 4-bit value is signed: [-8, 7] range
// Output: dest[i] = scale * quant_value[i]
int64_t asm_dml_dequant_q4_0_to_fp32(float* dest, const uint8_t* src, uint64_t blockCount) {
    if (!dest || !src) return -1;
    if (blockCount == 0) return 0;

    constexpr size_t kValuesPerBlock = 32;
    constexpr size_t kQ4BlockSize = 18;  // 2 (scale) + 16 (packed nibbles)

    for (uint64_t blk = 0; blk < blockCount; ++blk) {
        const uint8_t* block = src + blk * kQ4BlockSize;

        // Read FP16 scale (little-endian)
        uint16_t scale_fp16;
        std::memcpy(&scale_fp16, block, sizeof(uint16_t));
        const float scale = fp16_to_fp32(scale_fp16);

        // Dequantize 32 packed 4-bit values
        const uint8_t* nibbles = block + 2;
        float* out = dest + blk * kValuesPerBlock;

        for (size_t i = 0; i < kValuesPerBlock / 2; ++i) {
            const uint8_t byte = nibbles[i];

            // Low nibble (bits 0-3)
            int8_t q0 = static_cast<int8_t>(byte & 0x0Fu);
            if (q0 >= 8) q0 -= 16;  // Sign-extend 4-bit to 8-bit
            out[i * 2] = scale * static_cast<float>(q0);

            // High nibble (bits 4-7)
            int8_t q1 = static_cast<int8_t>((byte >> 4u) & 0x0Fu);
            if (q1 >= 8) q1 -= 16;
            out[i * 2 + 1] = scale * static_cast<float>(q1);
        }
    }

    return 0;
}

// =============================================================================
// Q8_0 Dequantization: 8-bit quantized -> FP32
// =============================================================================
// Block format: 2-byte fp16 scale + 32 bytes of int8 values
// Output: dest[i] = scale * int8_value[i]
int64_t asm_dml_dequant_q8_0_to_fp32(float* dest, const uint8_t* src, uint64_t blockCount) {
    if (!dest || !src) return -1;
    if (blockCount == 0) return 0;

    constexpr size_t kValuesPerBlock = 32;
    constexpr size_t kQ8BlockSize = 34;  // 2 (scale) + 32 (int8 values)

    for (uint64_t blk = 0; blk < blockCount; ++blk) {
        const uint8_t* block = src + blk * kQ8BlockSize;

        // Read FP16 scale
        uint16_t scale_fp16;
        std::memcpy(&scale_fp16, block, sizeof(uint16_t));
        const float scale = fp16_to_fp32(scale_fp16);

        // Dequantize 32 int8 values
        const int8_t* quants = reinterpret_cast<const int8_t*>(block + 2);
        float* out = dest + blk * kValuesPerBlock;

        for (size_t i = 0; i < kValuesPerBlock; ++i) {
            out[i] = scale * static_cast<float>(quants[i]);
        }
    }

    return 0;
}

// =============================================================================
// RoPE (Rotary Position Embedding) Rotation - FP32
// =============================================================================
// Applies complex rotation to query/key vectors for positional encoding.
// Formula: [x0, x1] -> [x0*cos - x1*sin, x0*sin + x1*cos]
// Operates on pairs across the half-dimension boundary for numerical stability.
int64_t asm_dml_rope_rotate_fp32(float* qk,
                                 const float* cosTable,
                                 const float* sinTable,
                                 uint32_t halfDim,
                                 uint32_t seqLen) {
    if (!qk || !cosTable || !sinTable || halfDim == 0 || seqLen == 0) return -1;

    const uint32_t dim = halfDim * 2U;

    // Process with cache-friendly sequential access
    for (uint32_t s = 0; s < seqLen; ++s) {
        float* row = qk + static_cast<uint64_t>(s) * dim;

        for (uint32_t i = 0; i < halfDim; ++i) {
            const float x0 = row[i];
            const float x1 = row[i + halfDim];
            const float cos_val = cosTable[i];
            const float sin_val = sinTable[i];

            // Complex rotation: (x0 + i*x1) * (cos + i*sin)
            row[i]           = std::fma(x0, cos_val, -x1 * sin_val);  // FMA for accuracy
            row[i + halfDim] = std::fma(x0, sin_val,  x1 * cos_val);
        }
    }

    return 0;
}

// =============================================================================
// Prefetch Tensor Block - Cache Warming
// =============================================================================
// Touches cache lines to reduce latency for subsequent tensor operations.
// Uses 64-byte stride (typical cache line size) for optimal coverage.
int64_t asm_dml_prefetch_tensor_block(const void* address, uint64_t byteCount) {
    if (!address || byteCount == 0) return -1;

    volatile const uint8_t* p = static_cast<volatile const uint8_t*>(address);
    volatile uint8_t sink = 0;
    constexpr uint64_t kCacheLineSize = 64;

    // Touch every cache line
    for (uint64_t i = 0; i < byteCount; i += kCacheLineSize) {
        sink ^= p[i];
    }

    // Touch final byte if not already covered
    if (byteCount % kCacheLineSize != 0) {
        sink ^= p[byteCount - 1];
    }

    (void)sink;
    return 0;
}

}  // extern "C"
