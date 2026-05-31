// ============================================================================
// int4_kv_dequant_kernel.cpp
// INT4 KV cache dequantization implementation
// ============================================================================

#include "compression/int4_kv_dequant_kernel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <immintrin.h>
#endif

namespace RawrXD {
namespace Compression {

// ============================================================================
// Int4KVQuantizer Implementation
// ============================================================================

float Int4KVQuantizer::computeScale(const float* block, size_t count) {
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();

    for (size_t i = 0; i < count; ++i) {
        minVal = std::min(minVal, block[i]);
        maxVal = std::max(maxVal, block[i]);
    }

    // Compute scale to map [minVal, maxVal] to [-8, 7]
    float range = maxVal - minVal;
    if (range < 1e-6f) {
        return 1.0f;  // Degenerate case
    }

    // scale = 15.0f / range  (to map 15 discrete levels to the range)
    return 15.0f / range;
}

int8_t Int4KVQuantizer::quantizeValue(float value, float scale) {
    // Shift and scale value to [-8, 7] range
    int32_t quantized = static_cast<int32_t>(std::round(value * scale));
    // Clamp to [-8, 7]
    quantized = std::max(-8, std::min(7, quantized));
    return static_cast<int8_t>(quantized);
}

size_t Int4KVQuantizer::QuantizeBlock(const float* blockFp32, uint8_t* blockInt4) {
    if (!blockFp32 || !blockInt4) {
        return 0;
    }

    // Compute scale factor for this block
    float scale = computeScale(blockFp32, kInt4KVBlockSize);

    // Write header
    Int4KVBlockHeader* hdr = reinterpret_cast<Int4KVBlockHeader*>(blockInt4);
    hdr->scale = scale;
    std::memset(hdr->reserved, 0, sizeof(hdr->reserved));

    // Pack INT4 values (4 per byte) into payload
    uint8_t* payload = blockInt4 + sizeof(Int4KVBlockHeader);
    size_t payloadIdx = 0;

    for (size_t i = 0; i < kInt4KVBlockSize; i += 2) {
        int8_t val0 = quantizeValue(blockFp32[i], scale);
        int8_t val1 = quantizeValue(blockFp32[i + 1], scale);

        // Pack two INT4 values into one byte: [val0:4 bits][val1:4 bits]
        uint8_t byte0 = ((val0 & 0x0F) << 4) | (val1 & 0x0F);
        payload[payloadIdx++] = byte0;
    }

    return kInt4KVBlockSize;
}

uint64_t Int4KVQuantizer::QuantizeTensor(const float* kvFp32, size_t rows, size_t cols,
                                          uint8_t* kvInt4) {
    if (!kvFp32 || !kvInt4 || rows == 0 || cols == 0) {
        return 0;
    }

    uint64_t totalWritten = 0;
    const size_t floatsPerBlock = kInt4KVBlockSize;
    const size_t blocksPerCol = (cols + floatsPerBlock - 1) / floatsPerBlock;

    for (size_t row = 0; row < rows; ++row) {
        for (size_t blockCol = 0; blockCol < blocksPerCol; ++blockCol) {
            const float* blockStart = kvFp32 + (row * cols) + (blockCol * floatsPerBlock);
            uint8_t* blockOut = kvInt4 + totalWritten;

            QuantizeBlock(blockStart, blockOut);
            totalWritten += kInt4KVBlockTotalBytes;
        }
    }

    return totalWritten;
}

// ============================================================================
// Int4KVDequantizer Implementation
// ============================================================================

inline float Int4KVDequantizer::dequantizeValue(uint8_t nibble, float scale) {
    // Nibble is a 4-bit signed value [-8, 7]
    int8_t signed_val = static_cast<int8_t>((nibble << 4)) >> 4;  // Sign-extend
    return static_cast<float>(signed_val) / scale;
}

float* Int4KVDequantizer::DequantizeBlock(const uint8_t* blockInt4) {
    if (!blockInt4) {
        return nullptr;
    }

    const Int4KVBlockHeader* hdr = reinterpret_cast<const Int4KVBlockHeader*>(blockInt4);
    float scale = hdr->scale;

    // Allocate output buffer
    float* output = new float[kInt4KVBlockSize];
    if (!output) {
        return nullptr;
    }

    const uint8_t* payload = blockInt4 + sizeof(Int4KVBlockHeader);
    size_t payloadIdx = 0;

    // Unpack INT4 values
    for (size_t i = 0; i < kInt4KVBlockSize; i += 2) {
        uint8_t byte = payload[payloadIdx++];

        // Extract two INT4 values
        uint8_t nibble0 = (byte >> 4) & 0x0F;
        uint8_t nibble1 = byte & 0x0F;

        output[i] = dequantizeValue(nibble0, scale);
        output[i + 1] = dequantizeValue(nibble1, scale);
    }

    return output;
}

size_t Int4KVDequantizer::DequantizeRange(const uint8_t* kvInt4, size_t startIdx, size_t endIdx,
                                           float* outValues) {
    if (!kvInt4 || !outValues || startIdx >= endIdx) {
        return 0;
    }

    size_t totalDecompressed = 0;
    const size_t floatsPerBlock = kInt4KVBlockSize;

    for (size_t idx = startIdx; idx < endIdx; ++idx) {
        size_t blockIdx = idx / floatsPerBlock;
        size_t offsetInBlock = idx % floatsPerBlock;

        const uint8_t* blockStart = kvInt4 + (blockIdx * kInt4KVBlockTotalBytes);
        float* decompressed = DequantizeBlock(blockStart);

        if (decompressed) {
            size_t count = std::min(floatsPerBlock - offsetInBlock, endIdx - idx);
            std::memcpy(outValues + totalDecompressed,
                        decompressed + offsetInBlock,
                        count * sizeof(float));
            totalDecompressed += count;
            delete[] decompressed;
        }
    }

    return totalDecompressed;
}

uint64_t Int4KVDequantizer::DequantizeTensor(const uint8_t* kvInt4, size_t rows, size_t cols,
                                              float* kvFp32) {
    if (!kvInt4 || !kvFp32 || rows == 0 || cols == 0) {
        return 0;
    }

    uint64_t totalWritten = 0;
    const size_t floatsPerBlock = kInt4KVBlockSize;
    const size_t blocksPerRow = (cols + floatsPerBlock - 1) / floatsPerBlock;

    for (size_t row = 0; row < rows; ++row) {
        for (size_t blockCol = 0; blockCol < blocksPerRow; ++blockCol) {
            const uint8_t* blockIn = kvInt4 + (row * blocksPerRow + blockCol) * kInt4KVBlockTotalBytes;
            float* blockOut = kvFp32 + (row * cols) + (blockCol * floatsPerBlock);

            float* decompressed = DequantizeBlock(blockIn);
            if (decompressed) {
                size_t count = std::min(floatsPerBlock, cols - blockCol * floatsPerBlock);
                std::memcpy(blockOut, decompressed, count * sizeof(float));
                delete[] decompressed;
                totalWritten += count * sizeof(float);
            }
        }
    }

    return totalWritten;
}

// ============================================================================
// Lazy KV Attention
// ============================================================================

static thread_local float* t_cachedDequantized = nullptr;
static thread_local size_t t_cachedStartIdx = 0;
static thread_local size_t t_cachedEndIdx = 0;

const float* LazyInt4KVAttention::GetDequantizedKVWindow(const uint8_t* kvInt4,
                                                          size_t queryPos,
                                                          size_t windowSize) {
    if (!kvInt4) {
        return nullptr;
    }

    // Check if cached window covers the requested range
    if (t_cachedDequantized &&
        queryPos >= t_cachedStartIdx &&
        queryPos + windowSize <= t_cachedEndIdx) {
        return t_cachedDequantized + (queryPos - t_cachedStartIdx);
    }

    // Release old cache
    if (t_cachedDequantized) {
        delete[] t_cachedDequantized;
    }

    // Allocate and decompress new window
    t_cachedDequantized = new float[windowSize];
    if (!t_cachedDequantized) {
        return nullptr;
    }

    Int4KVDequantizer::DequantizeRange(kvInt4, queryPos, queryPos + windowSize,
                                        t_cachedDequantized);

    t_cachedStartIdx = queryPos;
    t_cachedEndIdx = queryPos + windowSize;

    return t_cachedDequantized;
}

void LazyInt4KVAttention::ReleaseCached() {
    if (t_cachedDequantized) {
        delete[] t_cachedDequantized;
        t_cachedDequantized = nullptr;
    }
    t_cachedStartIdx = 0;
    t_cachedEndIdx = 0;
}

// ============================================================================
// Fast Path: Multi-head attention with INT4 KV
// ============================================================================

namespace FastPaths {

void AttentionQKInt4Heads4(
    const float* query,           // [4, 128]
    const uint8_t* kvInt4,        // Quantized KV cache
    size_t kvSeqLen,              // KV sequence length
    float* outScores) {           // Output: [4, kvSeqLen]

    if (!query || !kvInt4 || !outScores || kvSeqLen == 0) {
        return;
    }

    const size_t numHeads = 4;
    const size_t dimPerHead = 128;
    const size_t headStride = dimPerHead;

    // For each head
    for (size_t headIdx = 0; headIdx < numHeads; ++headIdx) {
        const float* queryHead = query + headIdx * headStride;
        float* scoresHead = outScores + headIdx * kvSeqLen;

        // For each position in KV cache
        for (size_t kvIdx = 0; kvIdx < kvSeqLen; ++kvIdx) {
            // Dequantize KV entry (lazy, should use batched approach in prod)
            const float* kvEntry = LazyInt4KVAttention::GetDequantizedKVWindow(
                kvInt4, kvIdx, 1);

            if (kvEntry) {
                // Compute dot product: query_head · kv_entry
                float score = 0.0f;
                for (size_t d = 0; d < dimPerHead; ++d) {
                    score += queryHead[d] * kvEntry[d];
                }
                scoresHead[kvIdx] = score;
            }
        }
    }
}

}  // namespace FastPaths

} // namespace Compression
} // namespace RawrXD
