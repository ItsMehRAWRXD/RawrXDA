#pragma once

#include <cstdint>

namespace RawrXD::Runtime::Quant
{

/**
 * WIRE FORMATS:
 * Q4S / Q4U : 4-bit packing (nibble-based)
 * Q6S / Q6U : 6-bit packing (6-bit mask)
 *
 * Legend:
 * UI "-4bit" -> Signed Q4S (Offset Binary)
 * UI "4bit"  -> Unsigned Q4U (Straight)
 */

// --- Signed Handlers (Offset Binary) ---

[[nodiscard]] inline int decodeSignedNibbleQ4(std::uint8_t packedLowNibble)
{
    const int v = static_cast<int>(packedLowNibble & 0x0F);
    return v - 8;  // Range [-8, 7]
}

[[nodiscard]] inline int decodeSignedQ6(std::uint8_t sixBits)
{
    const int v = static_cast<int>(sixBits & 0x3F);
    return v - 32;  // Range [-32, 31]
}

[[nodiscard]] inline std::uint8_t encodeSignedNibbleQ4(int v)
{
    const int clamped = v < -8 ? -8 : (v > 7 ? 7 : v);
    return static_cast<std::uint8_t>((clamped + 8) & 0x0F);
}

[[nodiscard]] inline std::uint8_t encodeSignedQ6(int v)
{
    const int clamped = v < -32 ? -32 : (v > 31 ? 31 : v);
    return static_cast<std::uint8_t>((clamped + 32) & 0x3F);
}

// --- Unsigned Handlers (Straight) ---

[[nodiscard]] inline int decodeUnsignedNibbleQ4(std::uint8_t packedLowNibble)
{
    return static_cast<int>(packedLowNibble & 0x0F);  // Range [0, 15]
}

[[nodiscard]] inline int decodeUnsignedQ6(std::uint8_t sixBits)
{
    return static_cast<int>(sixBits & 0x3F);  // Range [0, 63]
}

[[nodiscard]] inline std::uint8_t encodeUnsignedNibbleQ4(int v)
{
    const int clamped = v < 0 ? 0 : (v > 15 ? 15 : v);
    return static_cast<std::uint8_t>(clamped & 0x0F);
}

[[nodiscard]] inline std::uint8_t encodeUnsignedQ6(int v)
{
    const int clamped = v < 0 ? 0 : (v > 63 ? 63 : v);
    return static_cast<std::uint8_t>(clamped & 0x3F);
}

/// Log once: wire signed/unsigned Q4/Q6 vs UI dash notation (thread-safe).
void LogQuantLayoutLegendOnce();

}  // namespace RawrXD::Runtime::Quant
