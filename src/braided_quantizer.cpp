// d:/rawrxd/src/braided_quantizer.cpp
#include "braided_quantizer.h"
#include "atc_gpu_dispatch.h"
#include "gpu_enforcement.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>


// --- From-Scratch Quantization ---
// This is a conceptual, non-optimized implementation.
void BraidedQuantizer::quantize_float_to_braids(const float* input_data, int num_elements,
                                                const std::vector<int>& target_bit_config,
                                                std::vector<std::vector<uint8_t>>& output_braids, float& out_scale,
                                                float& out_offset)
{
    if (input_data == nullptr || num_elements == 0 || target_bit_config.empty())
    {
        return;
    }

    // 1. Find min/max to determine scale and offset
    float min_val = input_data[0];
    float max_val = input_data[0];
    for (int i = 1; i < num_elements; ++i)
    {
        if (input_data[i] < min_val)
            min_val = input_data[i];
        if (input_data[i] > max_val)
            max_val = input_data[i];
    }

    int total_bits = 0;
    for (int bits : target_bit_config)
    {
        total_bits += bits;
    }

    // For simplicity, we'll use a symmetric range for now.
    // A more advanced version would handle asymmetric quantization.
    out_scale = (max_val - min_val) / ((1 << total_bits) - 1);
    out_offset = min_val;  // This is our zero-point

    // 2. Resize output braid containers
    output_braids.resize(target_bit_config.size());
    for (size_t i = 0; i < target_bit_config.size(); ++i)
    {
        // Calculate required bytes for each braid
        size_t bytes_needed = (num_elements * target_bit_config[i] + 7) / 8;
        output_braids[i].assign(bytes_needed, 0);
    }

    // 3. Quantize and interleave bits into braids
    for (int i = 0; i < num_elements; ++i)
    {
        float normalized_val = (input_data[i] - out_offset) / out_scale;
        uint32_t quantized_val = static_cast<uint32_t>(round(normalized_val));

        uint32_t bit_offset = 0;
        for (size_t braid_idx = 0; braid_idx < target_bit_config.size(); ++braid_idx)
        {
            int bits_for_braid = target_bit_config[braid_idx];
            uint32_t mask = (1 << bits_for_braid) - 1;

            // Extract the bits for the current braid from the quantized value
            uint32_t braid_bits = (quantized_val >> bit_offset) & mask;

            // Pack these bits into the correct output braid vector
            // This is a simple, slow bit-packing implementation.
            int bit_pos_in_braid = i * bits_for_braid;
            for (int b = 0; b < bits_for_braid; ++b)
            {
                if ((braid_bits >> b) & 1)
                {
                    int byte_idx = (bit_pos_in_braid + b) / 8;
                    int bit_idx_in_byte = (bit_pos_in_braid + b) % 8;
                    output_braids[braid_idx][byte_idx] |= (1 << bit_idx_in_byte);
                }
            }
            bit_offset += bits_for_braid;
        }
    }
}


// --- GPU-only Dequantization ---
// Both entry points fan out to rxd::atc::dequantize_braids_gpu().
// There is no CPU fallback; the GPU enforcement gate aborts the process if
// no GPU backend is available, so by the time we reach this code a GPU is
// guaranteed.
namespace
{
void dequantize_via_gpu(const std::vector<const uint8_t*>& input_braids, int num_elements, float scale, float offset,
                        float* output_data)
{
    rxd::gpu::require();
    if (input_braids.empty() || num_elements <= 0 || output_data == nullptr)
    {
        return;
    }
    // Default bit-config: 4-bit base + 2-bit refinement braids, matching the
    // ATC packer's standard {4,2,2,...} layout. This is a transitional
    // assumption; the codec-side dispatcher passes explicit bit-widths.
    std::vector<int> bits(input_braids.size(), 0);
    bits[0] = 4;
    for (size_t i = 1; i < bits.size(); ++i)
        bits[i] = 2;

    // API expects `const uint8_t**` (non-const pointer); our vector yields
    // `const uint8_t* const*`. This cast is safe because callee does not mutate
    // the pointers.
    rxd::atc::dequantize_braids_gpu(const_cast<const uint8_t**>(input_braids.data()), bits.data(),
                                    static_cast<int>(input_braids.size()), num_elements, scale, offset, output_data);
}
}  // namespace

void BraidedQuantizer::dequantize_braids_to_float(const std::vector<const uint8_t*>& input_braids, int num_elements,
                                                  float scale, float offset, float* output_data)
{
    dequantize_via_gpu(input_braids, num_elements, scale, offset, output_data);
}

void BraidedQuantizer::dequantize_braids_to_float_avx512(const std::vector<const uint8_t*>& input_braids,
                                                         int num_elements, float scale, float offset,
                                                         float* output_data)
{
    // The "AVX-512 path" name is retained for ABI compatibility, but the work
    // is performed on the GPU. See gpu_enforcement.cpp for the fail-closed
    // policy.
    dequantize_via_gpu(input_braids, num_elements, scale, offset, output_data);
}
