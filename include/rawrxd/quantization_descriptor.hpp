#pragma once

// ============================================================================
// quantization_descriptor.hpp — Stable UI / telemetry strings for quant formats
// ============================================================================
// Avoid ambiguous "-4" labels (minus sign vs hyphen). Prefer explicit
// bit-width + signedness + block geometry, e.g. "bit+4/sign·blk32".
// ============================================================================

#include <cstdint>
#include <string>

namespace RawrXD::Quant
{

/// Human-readable label for block-quantized weights (e.g. Q4_0, Q8_0).
/// \param storageBits Effective storage bits per weight after packing (4, 5, 8, …).
/// \param weightsSigned True if dequantized values use signed int8-style residuals.
/// \param blockWidth Weights per block (GGML often 32).
inline std::string describeBlockQuant(int storageBits, bool weightsSigned, std::uint32_t blockWidth = 32)
{
    std::string s = "bit+";
    s += std::to_string(storageBits);
    s += weightsSigned ? "/sign" : "/unsign";
    s += "·blk";
    s += std::to_string(blockWidth);
    return s;
}

/// Map common GGML type ids (subset) to descriptor; unknown → generic.
inline std::string describeFromGgmlTypeId(std::uint32_t ggmlType, std::uint32_t blockWidth = 32)
{
    // Mirrors subset of llama.cpp ggml_type for labeling only — not for tensor math.
    switch (ggmlType)
    {
        case 2:  // Q4_0
            return describeBlockQuant(4, false, blockWidth);
        case 3:  // Q4_1
            return describeBlockQuant(4, false, blockWidth) + "+minmax";
        case 6:  // Q5_0
            return describeBlockQuant(5, false, blockWidth);
        case 7:  // Q5_1
            return describeBlockQuant(5, false, blockWidth) + "+minmax";
        case 8:  // Q8_0
            return describeBlockQuant(8, true, blockWidth);
        case 9:  // Q8_1
            return describeBlockQuant(8, true, blockWidth) + "+minmax";
        default:
            return "dtype:" + std::to_string(ggmlType);
    }
}

}  // namespace RawrXD::Quant
