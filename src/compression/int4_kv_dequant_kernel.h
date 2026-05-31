// ============================================================================
// int4_kv_dequant_kernel.h
// INT4 KV cache dequantization hot-path for 4x memory compression
// ============================================================================
// Purpose: Unpack 4-bit quantized KV values on-the-fly during attention
//          Enables slicing KV cache from ~100GB to ~25GB for 2T models
//          Uses SIMD (AVX2/AVX-512) for vectorized nibble unpacking
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>

namespace RawrXD {
namespace Compression {

// INT4 KV block header: packed format info
struct Int4KVBlockHeader {
    float scale;           // Single per-block quantization scale
    uint8_t reserved[12];  // Padding for cache-line alignment
};

static_assert(sizeof(Int4KVBlockHeader) == 16, "Header must be 16 bytes");

// Constants for INT4 packing
constexpr size_t kInt4KVBlockSize = 256;           // 256 KV pairs per block
constexpr size_t kInt4KVBitsPerValue = 4;          // 4 bits per value
constexpr size_t kInt4KVBytesPerBlock = 
    (kInt4KVBlockSize * kInt4KVBitsPerValue) / 8;  // 128 bytes payload
constexpr size_t kInt4KVBlockTotalBytes = 
    sizeof(Int4KVBlockHeader) + kInt4KVBytesPerBlock; // 144 bytes

// Quantization range: INT4 signed = [-8, 7]
constexpr float kInt4MinValue = -8.0f;
constexpr float kInt4MaxValue = 7.0f;

// Forward declarations
class Int4KVQuantizer;
class Int4KVDequantizer;

// ============================================================================
// Int4KVQuantizer: Encode floating-point KV to INT4
// ============================================================================
class Int4KVQuantizer {
public:
    // Quantize one block of KV values
    // Input: blockFp32 = pointer to 256 float32 values (1024 bytes)
    // Output: blockInt4 = pointer to 144-byte quantized block
    // Returns: number of values successfully quantized
    static size_t QuantizeBlock(const float* blockFp32, uint8_t* blockInt4);

    // Quantize an entire KV cache tensor
    // Input: kvFp32 = full KV tensor (rows, cols) in row-major
    // Output: kvInt4 = quantized tensor
    // Returns: total bytes written to kvInt4
    static uint64_t QuantizeTensor(const float* kvFp32, size_t rows, size_t cols,
                                    uint8_t* kvInt4);

private:
    static float computeScale(const float* block, size_t count);
    static int8_t quantizeValue(float value, float scale);
};

// ============================================================================
// Int4KVDequantizer: Decode INT4 KV to floating-point (on-demand)
// ============================================================================
class Int4KVDequantizer {
public:
    // Dequantize one block in-place or to output buffer
    // Input: blockInt4 = 144-byte quantized block
    // Output: blockFp32 = pointer to 1024-byte output buffer (or nullptr for in-place)
    // Returns: pointer to dequantized values (blockFp32 or allocated buffer)
    static float* DequantizeBlock(const uint8_t* blockInt4);

    // Dequantize a range of KV pairs from a quantized tensor
    // Input: kvInt4 = quantized tensor
    // startIdx, endIdx = pair range to decompress
    // Output: outValues = preallocated output buffer
    // Returns: number of values written
    static size_t DequantizeRange(const uint8_t* kvInt4, size_t startIdx, size_t endIdx,
                                   float* outValues);

    // Prepare dequantized KV cache for inference (eager path, for small contexts)
    // Input: kvInt4 = quantized tensor (rows * cols / 2 bytes)
    // Output: kvFp32 = output buffer
    // Returns: bytes written to kvFp32
    static uint64_t DequantizeTensor(const uint8_t* kvInt4, size_t rows, size_t cols,
                                      float* kvFp32);

private:
    static float dequantizeValue(uint8_t nibble, float scale);
};

// ============================================================================
// Attention Helper: Lazy KV dequantization during Q·K^T compute
// ============================================================================
class LazyInt4KVAttention {
public:
    // Query the quantized KV cache and dequantize only the needed range
    // This is the hot-path for reducing memory pressure during attention
    // Input: kvInt4 = compressed cache, queryPos = current token index
    // Returns: pointer to dequantized KV values for this attention window
    static const float* GetDequantizedKVWindow(const uint8_t* kvInt4, 
                                                size_t queryPos,
                                                size_t windowSize);

    // Release cached dequantized values
    static void ReleaseCached();
};

// ============================================================================
// Fast path for common configs (e.g., 4K context, 32 heads, 128 dim)
// ============================================================================
namespace FastPaths {

// Dequantize and compute 4 query heads against quantized KV cache
// Vectorized for AVX2
void AttentionQKInt4Heads4(
    const float* query,           // Shape: [4, 128] (4 heads, 128 dims)
    const uint8_t* kvInt4,        // Quantized KV cache
    size_t kvSeqLen,              // KV sequence length
    float* outScores);            // Output: [4, kvSeqLen] logits

}  // namespace FastPaths

} // namespace Compression
} // namespace RawrXD
