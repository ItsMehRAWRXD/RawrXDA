#include "BraidedQuantizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

// --- Helper Functions for Bit Manipulation ---

// Packs two 4-bit values into a single byte.
uint8_t pack_4bit(uint8_t high, uint8_t low) {
    return (high << 4) | (low & 0x0F);
}

// Unpacks the high 4 bits from a byte.
uint8_t unpack_high_4bit(uint8_t byte) {
    return byte >> 4;
}

// Unpacks the low 4 bits from a byte.
uint8_t unpack_low_4bit(uint8_t byte) {
    return byte & 0x0F;
}

// Packs four 2-bit values into a single byte.
uint8_t pack_2bit(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    return (b1 << 6) | ((b2 & 0x03) << 4) | ((b3 & 0x03) << 2) | (b4 & 0x03);
}

// Unpacks the first 2-bit value from a byte.
uint8_t unpack_2bit_1(uint8_t byte) { return byte >> 6; }
// Unpacks the second 2-bit value.
uint8_t unpack_2bit_2(uint8_t byte) { return (byte >> 4) & 0x03; }
// Unpacks the third 2-bit value.
uint8_t unpack_2bit_3(uint8_t byte) { return (byte >> 2) & 0x03; }
// Unpacks the fourth 2-bit value.
uint8_t unpack_2bit_4(uint8_t byte) { return byte & 0x03; }


// --- BraidedQuantizer Implementation ---

BraidedTensor BraidedQuantizer::quantize(const std::string& name, const float* data, uint64_t num_elements) {
    BraidedTensor braided_tensor;
    braided_tensor.name = name;
    braided_tensor.num_elements = num_elements;

    // 1. Find min/max to determine scale and zero-point for 8-bit quantization
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    for (uint64_t i = 0; i < num_elements; ++i) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }

    braided_tensor.scale = (max_val - min_val) / 255.0f;
    braided_tensor.zero_point = -min_val / braided_tensor.scale;

    // 2. Quantize to intermediate 8-bit representation
    std::vector<uint8_t> temp_8bit(num_elements);
    for (uint64_t i = 0; i < num_elements; ++i) {
        temp_8bit[i] = static_cast<uint8_t>(std::round(data[i] / braided_tensor.scale + braided_tensor.zero_point));
    }

    // 3. "Braid" the 8-bit data into three separate streams (2-bit, 2-bit, 4-bit)
    braided_tensor.base_stream.reserve(num_elements / 4);
    braided_tensor.refinement_stream_1.reserve(num_elements / 4);
    braided_tensor.refinement_stream_2.reserve(num_elements / 2);

    for (uint64_t i = 0; i < num_elements; i += 4) {
        // Get the 2 Most Significant Bits (MSBs) for the base stream
        uint8_t b1_2bit = temp_8bit[i] >> 6;
        uint8_t b2_2bit = (i + 1 < num_elements) ? temp_8bit[i + 1] >> 6 : 0;
        uint8_t b3_2bit = (i + 2 < num_elements) ? temp_8bit[i + 2] >> 6 : 0;
        uint8_t b4_2bit = (i + 3 < num_elements) ? temp_8bit[i + 3] >> 6 : 0;
        braided_tensor.base_stream.push_back(pack_2bit(b1_2bit, b2_2bit, b3_2bit, b4_2bit));

        // Get the next 2 bits for the first refinement stream
        uint8_t r1_2bit = (temp_8bit[i] >> 4) & 0x03;
        uint8_t r2_2bit = (i + 1 < num_elements) ? (temp_8bit[i + 1] >> 4) & 0x03 : 0;
        uint8_t r3_2bit = (i + 2 < num_elements) ? (temp_8bit[i + 2] >> 4) & 0x03 : 0;
        uint8_t r4_2bit = (i + 3 < num_elements) ? (temp_8bit[i + 3] >> 4) & 0x03 : 0;
        braided_tensor.refinement_stream_1.push_back(pack_2bit(r1_2bit, r2_2bit, r3_2bit, r4_2bit));
    }
    
    for (uint64_t i = 0; i < num_elements; i += 2) {
        // Get the 4 Least Significant Bits (LSBs) for the final detail stream
        uint8_t lsb1_4bit = temp_8bit[i] & 0x0F;
        uint8_t lsb2_4bit = (i + 1 < num_elements) ? temp_8bit[i + 1] & 0x0F : 0;
        braided_tensor.refinement_stream_2.push_back(pack_4bit(lsb1_4bit, lsb2_4bit));
    }


    return braided_tensor;
}

std::vector<float> BraidedQuantizer::dequantize(const BraidedTensor& braided_tensor, BraidedQuantizationLevel target_level) {
    std::vector<uint8_t> temp_8bit(braided_tensor.num_elements, 0);

    // Level 0: Unbraid the base 2-bit stream
    if (target_level >= BraidedQuantizationLevel::LEVEL_0_BASE) {
        for (uint64_t i = 0; i < braided_tensor.base_stream.size(); ++i) {
            uint8_t packed = braided_tensor.base_stream[i];
            if (i * 4 < temp_8bit.size()) temp_8bit[i * 4] |= (unpack_2bit_1(packed) << 6);
            if (i * 4 + 1 < temp_8bit.size()) temp_8bit[i * 4 + 1] |= (unpack_2bit_2(packed) << 6);
            if (i * 4 + 2 < temp_8bit.size()) temp_8bit[i * 4 + 2] |= (unpack_2bit_3(packed) << 6);
            if (i * 4 + 3 < temp_8bit.size()) temp_8bit[i * 4 + 3] |= (unpack_2bit_4(packed) << 6);
        }
    }

    // Level 1: Braid in the next 2 bits
    if (target_level >= BraidedQuantizationLevel::LEVEL_1_REFINED) {
        for (uint64_t i = 0; i < braided_tensor.refinement_stream_1.size(); ++i) {
            uint8_t packed = braided_tensor.refinement_stream_1[i];
            if (i * 4 < temp_8bit.size()) temp_8bit[i * 4] |= (unpack_2bit_1(packed) << 4);
            if (i * 4 + 1 < temp_8bit.size()) temp_8bit[i * 4 + 1] |= (unpack_2bit_2(packed) << 4);
            if (i * 4 + 2 < temp_8bit.size()) temp_8bit[i * 4 + 2] |= (unpack_2bit_3(packed) << 4);
            if (i * 4 + 3 < temp_8bit.size()) temp_8bit[i * 4 + 3] |= (unpack_2bit_4(packed) << 4);
        }
    }

    // Level 2: Braid in the final 4 bits
    if (target_level >= BraidedQuantizationLevel::LEVEL_2_DETAIL) {
        for (uint64_t i = 0; i < braided_tensor.refinement_stream_2.size(); ++i) {
            uint8_t packed = braided_tensor.refinement_stream_2[i];
            if (i * 2 < temp_8bit.size()) temp_8bit[i * 2] |= unpack_high_4bit(packed);
            if (i * 2 + 1 < temp_8bit.size()) temp_8bit[i * 2 + 1] |= unpack_low_4bit(packed);
        }
    }
    
    // If target is full precision, we can't reconstruct it perfectly from quantized data.
    // This level is a placeholder for returning original data if it were stored.
    // For this simulation, LEVEL_2_DETAIL is the highest we can reconstruct.

    // Final dequantization from 8-bit to float
    std::vector<float> result(braided_tensor.num_elements);
    for (uint64_t i = 0; i < braided_tensor.num_elements; ++i) {
        result[i] = (static_cast<float>(temp_8bit[i]) - braided_tensor.zero_point) * braided_tensor.scale;
    }

    return result;
}
