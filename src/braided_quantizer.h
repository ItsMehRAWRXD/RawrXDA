// d:/rawrxd/src/braided_quantizer.h
#pragma once

#include <cstdint>
#include <vector>

// Defines a single "braid" or stream of quantization data.
// A tensor is composed of a base layer + multiple optional braid layers.
struct QuantizationBraid {
    uint8_t bits;       // Number of bits for this braid (e.g., 2, 4)
    uint64_t offset;    // Offset in the model file
    uint64_t size;      // Size of this braid's data in bytes
};

// Metadata for a complete braided tensor tile.
struct BraidedTileMeta {
    std::vector<QuantizationBraid> braids;
    // Level-of-detail 0 (coarsest) byte range within the mapped model buffer.
    // Used by AdaptiveTensorCodec prefetch/IO scheduling.
    uint64_t offset_l0 = 0;
    uint64_t size_l0 = 0;
    float scale;
    float offset; // Or zero-point
    // Sparsity metadata can be added here later
};

class BraidedQuantizer {
public:
    // --- From-Scratch Quantization ---
    // Quantizes a block of float data into multiple braided streams.
    // target_bit_config specifies the desired bit allocation per braid, e.g., {4, 2, 2} for 8-bit total.
    static void quantize_float_to_braids(
        const float* input_data,
        int num_elements,
        const std::vector<int>& target_bit_config,
        std::vector<std::vector<uint8_t>>& output_braids,
        float& out_scale,
        float& out_offset
    );

    // --- From-Scratch Dequantization ---
    // Dequantizes a specified number of braids into a float buffer.
    // `braids_to_use` determines the final precision and memory usage.
    static void dequantize_braids_to_float(
        const std::vector<const uint8_t*>& input_braids,
        int num_elements,
        float scale,
        float offset,
        float* output_data
    );

    // AVX-512 optimized version (placeholder for implementation)
    static void dequantize_braids_to_float_avx512(
        const std::vector<const uint8_t*>& input_braids,
        int num_elements,
        float scale,
        float offset,
        float* output_data
    );
};
