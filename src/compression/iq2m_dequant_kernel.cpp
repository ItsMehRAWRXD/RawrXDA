// ============================================================================
// iq2m_dequant_kernel.cpp
// C++ Implementation: Scalar Fallback + Batch Dispatch
// ============================================================================

#include "compression/iq2m_dequant_kernel.h"

#include <cstring>  // for memset

namespace RawrXD {
namespace Compression {

// ============================================================================
// Scalar Fallback
// ============================================================================
// Slow but correct reference implementation.
// Dequantizes one IQ2_M block without SIMD.
//
// IQ2_M packing (2.5 bits per weight):
//   - Each weight uses 5 bits (2 bits mantissa + 3-bit shared exponent scale)
//   - Actually: 2-bit quantization with shared FP32 scale factor
//   - Block: 256 weights, 1 FP32 scale (4 bytes) + 64 bytes of packed 2-bit values
//   - Total: 68 bytes per block (not 164 - that was for more complex format)
//
// For this implementation, we use a simplified 2-bit format:
//   - Scale: FP32 (4 bytes)
//   - 256 weights as 2-bit values: 64 bytes
//   - Total: 68 bytes per block
//
// 2-bit values map to:
//   0 -> -1.0f * scale
//   1 -> -0.333f * scale
//   2 -> +0.333f * scale
//   3 -> +1.0f * scale
// ============================================================================

extern "C" uint32_t IQ2M_DequantBlock_Scalar(const uint8_t* compressed, float* outFloats) {
    if (!compressed || !outFloats) return 0;

    // Read scale factor (first 4 bytes as little-endian FP32)
    float scale;
    std::memcpy(&scale, compressed, sizeof(float));
    if (scale == 0.0f) scale = 1.0f;  // Guard against zero scale

    const uint8_t* packed = compressed + 4;  // Skip scale

    // Dequantize 256 weights
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t byteIdx = i / 4;
        uint32_t bitOffset = (i % 4) * 2;
        uint32_t val = (packed[byteIdx] >> bitOffset) & 0x3;

        // Map 2-bit value to float
        float dequant;
        switch (val) {
            case 0: dequant = -1.0f; break;
            case 1: dequant = -0.33333333f; break;
            case 2: dequant = +0.33333333f; break;
            case 3: dequant = +1.0f; break;
            default: dequant = 0.0f; break;  // unreachable
        }

        outFloats[i] = dequant * scale;
    }

    return 256;
}

// ============================================================================
// Batch Dequantization
// ============================================================================
// Dequantizes N contiguous blocks, dispatching to the best kernel per block.
// Uses the same kernel for all blocks (determined by first block's feature check).
// ============================================================================

extern "C" uint32_t IQ2M_DequantBatch(
    const uint8_t* compressedBase,
    float* outFloatsBase,
    uint32_t blockCount,
    uint32_t blockStride) {
    if (!compressedBase || !outFloatsBase || blockCount == 0) return 0;

    static const uint32_t features = IQ2M_Init();

    // Select kernel once for the entire batch
    using DequantFn = uint32_t (*)(const uint8_t*, float*);
    DequantFn dequantFn;

    if (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX512)) {
        dequantFn = IQ2M_DequantBlock_AVX512;
    } else if (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX2)) {
        dequantFn = IQ2M_DequantBlock_AVX2;
    } else {
        dequantFn = IQ2M_DequantBlock_Scalar;
    }

    uint32_t totalFloats = 0;
    const uint8_t* compressed = compressedBase;
    float* outFloats = outFloatsBase;

    for (uint32_t b = 0; b < blockCount; ++b) {
        uint32_t written = dequantFn(compressed, outFloats);
        if (written == 0) break;  // Error in block
        totalFloats += written;
        compressed += blockStride;
        outFloats += 256;  // 256 floats per block
    }

    return totalFloats;
}

} // namespace Compression
} // namespace RawrXD
