// ============================================================================
// iq2m_dequant_kernel.h
// C++ Interface to IQ2_M 2.5-bit Dequantization Kernels
// ============================================================================
// Provides:
//   - CPU feature detection (AVX2 / AVX-512)
//   - Block-wise dequantization entry points
//   - Direct memory-mapped pointer support (no memcpy)
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD {
namespace Compression {

// Feature flags returned by IQ2M_Init()
enum class IQ2MFeatureFlags : uint32_t {
    None    = 0,
    AVX2    = 1 << 0,
    AVX512  = 1 << 1,
    AVX512VBMI = 1 << 2
};

inline IQ2MFeatureFlags operator|(IQ2MFeatureFlags a, IQ2MFeatureFlags b) {
    return static_cast<IQ2MFeatureFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool HasFlag(IQ2MFeatureFlags flags, IQ2MFeatureFlags test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
// Detects CPU capabilities and selects optimal kernel path.
// Must be called once before any dequantization.
// Returns feature flags available on this hardware.
// ---------------------------------------------------------------------------
extern "C" uint32_t IQ2M_Init();

// ---------------------------------------------------------------------------
// Block Dequantization
// ---------------------------------------------------------------------------
// Dequantize one IQ2_M block (256 weights, 164 bytes compressed).
//
// Parameters:
//   compressed  - Pointer to compressed block (164 bytes, memory-mapped OK)
//   outFloats   - Pointer to output float buffer (256 floats = 1024 bytes)
//
// Returns:
//   Number of floats written (256 on success, 0 on failure)
//
// NOTE: Reads directly from memory-mapped pointers. No internal buffers.
//       Safe to call from multiple threads (read-only on input).
// ---------------------------------------------------------------------------
extern "C" uint32_t IQ2M_DequantBlock_AVX2(const uint8_t* compressed, float* outFloats);
extern "C" uint32_t IQ2M_DequantBlock_AVX512(const uint8_t* compressed, float* outFloats);
extern "C" uint32_t IQ2M_DequantBlock_Scalar(const uint8_t* compressed, float* outFloats);

// ---------------------------------------------------------------------------
// Convenience wrapper (auto-dispatches to best available kernel)
// ---------------------------------------------------------------------------
inline uint32_t IQ2M_DequantBlock(const uint8_t* compressed, float* outFloats) {
    static const uint32_t features = IQ2M_Init();
    if (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX512)) {
        return IQ2M_DequantBlock_AVX512(compressed, outFloats);
    }
    if (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX2)) {
        return IQ2M_DequantBlock_AVX2(compressed, outFloats);
    }
    // Fallback: scalar dequantization (slow but safe)
    return IQ2M_DequantBlock_Scalar(compressed, outFloats);
}

// ---------------------------------------------------------------------------
// Batch Dequantization (higher-level API)
// ---------------------------------------------------------------------------
// Dequantize N contiguous blocks. Useful for layer loading.
//
// Parameters:
//   compressedBase - Pointer to first compressed block (memory-mapped OK)
//   outFloatsBase  - Pointer to output float buffer
//   blockCount     - Number of blocks to dequantize
//   blockStride    - Bytes between compressed blocks (typically 164)
//
// Returns:
//   Total floats written (blockCount * 256)
// ---------------------------------------------------------------------------
extern "C" uint32_t IQ2M_DequantBatch(
    const uint8_t* compressedBase,
    float* outFloatsBase,
    uint32_t blockCount,
    uint32_t blockStride);

} // namespace Compression
} // namespace RawrXD
