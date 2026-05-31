#pragma once

#include <cstdint>
#include <vector>
#include <string>

// Defines the selectable precision levels for the Braided Quantizer.
// Each level represents a cumulative "braiding" of more precise data.
enum class BraidedQuantizationLevel {
    LEVEL_0_BASE,   // A coarse but very low-bit representation (e.g., 2-bit).
    LEVEL_1_REFINED, // Braids in additional bits for medium fidelity (e.g., 4-bit total).
    LEVEL_2_DETAIL,  // Braids in more bits for high fidelity (e.g., 8-bit total).
    LEVEL_3_FULL     // The original, full-precision data (e.g., FP16).
};

// Holds the braided data for a single tensor.
// It contains separate streams for the base quantization and subsequent refinement layers.
struct BraidedTensor {
    std::string name;
    std::vector<uint8_t> base_stream;      // e.g., 2-bit data
    std::vector<uint8_t> refinement_stream_1; // e.g., +2 bits of data
    std::vector<uint8_t> refinement_stream_2; // e.g., +4 bits of data
    
    // Metadata required for dequantization
    float scale;
    float zero_point;
    uint64_t num_elements;
};

// The core Braided Quantizer class.
// It handles the quantization (braiding) and dequantization (unbraiding) processes.
class BraidedQuantizer {
public:
    // Quantizes a full-precision tensor into a braided representation.
    // This is the "braiding" process.
    static BraidedTensor quantize(const std::string& name, const float* data, uint64_t num_elements);

    // Dequantizes a braided tensor to the specified precision level.
    // This is the "unbraiding" process, which can be stopped at any level.
    static std::vector<float> dequantize(const BraidedTensor& braided_tensor, BraidedQuantizationLevel target_level);
};
