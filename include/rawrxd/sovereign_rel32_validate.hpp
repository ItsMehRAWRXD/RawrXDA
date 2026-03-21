// =============================================================================
// rawrxd/sovereign_rel32_validate.hpp — REL32 displacement range (x64)
// =============================================================================
// x86-64 RIP-relative 32-bit displacements (CALL rel32, Jcc rel32, FF /0–/7
// with modrm+rip+disp32, etc.) encode a **signed int32** delta from the
// instruction’s defined “reference RIP” (usually the byte *after* the opcode /
// immediate). That delta must lie in **[INT32_MIN, INT32_MAX]** — often
// described informally as **±2 GiB** reach (strictly: −2^31 .. +2^31−1 bytes).
//
// This header is the **validation layer**: it does not emit bytes; callers
// (emitters, linkers, fixup passes) use it before patching REL32 slots.
// =============================================================================

#pragma once

#include <climits>
#include <cstdint>
#include <expected>

namespace rawrxd::sovereign::emit
{

/// REL32 cannot represent this signed displacement (|delta| too large for int32_t).
struct Rel32OutOfRange
{
};

/// Inclusive bounds for a representable REL32 displacement (same as int32_t).
inline constexpr std::int64_t kRel32DisplacementMin = static_cast<std::int64_t>(INT32_MIN);
inline constexpr std::int64_t kRel32DisplacementMax = static_cast<std::int64_t>(INT32_MAX);

/// \return true iff \p delta is representable as a signed 32-bit displacement.
inline constexpr bool rel32DisplacementFits(std::int64_t delta) noexcept
{
    return delta >= kRel32DisplacementMin && delta <= kRel32DisplacementMax;
}

/// Signed distance from \p ripAfterOpcode to \p targetAbs (PE / COFF style).
/// \p ripAfterOpcode  Absolute VA of the byte **after** the full instruction
///                    (e.g. after the 4-byte displacement for E8/FF15 encodings).
/// \p targetAbs       Absolute VA of the branch / memory target.
inline std::int64_t ripRelativeDelta(std::uint64_t ripAfterOpcode, std::uint64_t targetAbs) noexcept
{
    return static_cast<std::int64_t>(static_cast<std::int64_t>(targetAbs) -
                                     static_cast<std::int64_t>(ripAfterOpcode));
}

/// Same check as \ref rel32DisplacementFits applied to \ref ripRelativeDelta.
inline bool rel32ReachableFromRipAfter(std::uint64_t ripAfterOpcode, std::uint64_t targetAbs) noexcept
{
    return rel32DisplacementFits(ripRelativeDelta(ripAfterOpcode, targetAbs));
}

/// Encode \p delta as **int32_t** or report \ref Rel32OutOfRange.
inline std::expected<std::int32_t, Rel32OutOfRange> encodeRel32Displacement(std::int64_t delta) noexcept
{
    if (!rel32DisplacementFits(delta))
    {
        return std::unexpected(Rel32OutOfRange{});
    }
    return static_cast<std::int32_t>(delta);
}

}  // namespace rawrxd::sovereign::emit
