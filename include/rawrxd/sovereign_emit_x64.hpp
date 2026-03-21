// =============================================================================
// rawrxd/sovereign_emit_x64.hpp — Minimal x64 opcode helpers for PE emitters
// =============================================================================
// Used when synthesizing instruction bytes (e.g. CALL [rip+disp32] = FF 15).
// RIP is the address of the byte *after* this 6-byte instruction.
//
// REL32 displacements must fit **int32_t** (see `sovereign_rel32_validate.hpp`).
// The unchecked `emit*` functions truncate like a cast; prefer `tryEmit*`.
// =============================================================================

#pragma once

#include "rawrxd/sovereign_rel32_validate.hpp"

#include <cstdint>
#include <cstring>
#include <expected>

namespace rawrxd::sovereign::emit
{

/// Size of `call qword ptr [rip+disp32]` (FF 15 rel32).
inline constexpr std::size_t kCallRipRel32Size = 6u;

/// Emit `CALL [rip+disp32]` at \p buf.
/// \p ripAfterOpcode  Absolute VA of the first byte *after* this 6-byte encoding.
/// \p iatSlotAbs     Absolute VA of the IAT slot (memory holding the function pointer).
inline void emitCallRipRel32(uint8_t* buf, std::uint64_t ripAfterOpcode, std::uint64_t iatSlotAbs)
{
    buf[0] = 0xFF;
    buf[1] = 0x15;
    auto const disp =
        static_cast<std::int32_t>(static_cast<std::int64_t>(iatSlotAbs) - static_cast<std::int64_t>(ripAfterOpcode));
    std::memcpy(buf + 2, &disp, sizeof(disp));
}

/// Same as emitCallRipRel32, but \p currentRip is the address of the FF byte.
inline void emitCallRipRel32FromInstructionStart(uint8_t* buf, std::uint64_t currentRip, std::uint64_t iatSlotAbs)
{
    emitCallRipRel32(buf, currentRip + kCallRipRel32Size, iatSlotAbs);
}

/// Emit `CALL rel32` near call (E8). Instruction length = 5.
/// \p ripAfterOpcode  VA of byte after the 5-byte encoding.
/// \p targetAbs        Absolute VA of call target.
inline void emitCallRel32(uint8_t* buf, std::uint64_t ripAfterOpcode, std::uint64_t targetAbs)
{
    buf[0] = 0xE8;
    auto const disp =
        static_cast<std::int32_t>(static_cast<std::int64_t>(targetAbs) - static_cast<std::int64_t>(ripAfterOpcode));
    std::memcpy(buf + 1, &disp, sizeof(disp));
}

inline void emitCallRel32FromInstructionStart(uint8_t* buf, std::uint64_t currentRip, std::uint64_t targetAbs)
{
    emitCallRel32(buf, currentRip + 5u, targetAbs);
}

/// Checked emit: `CALL rel32` (E8). Fails if displacement is outside int32_t.
inline std::expected<void, Rel32OutOfRange> tryEmitCallRel32(
    uint8_t* buf, std::uint64_t ripAfterOpcode, std::uint64_t targetAbs) noexcept
{
    const std::int64_t d = ripRelativeDelta(ripAfterOpcode, targetAbs);
    const auto enc = encodeRel32Displacement(d);
    if (!enc)
    {
        return std::unexpected(enc.error());
    }
    buf[0] = 0xE8;
    std::memcpy(buf + 1, &enc.value(), sizeof(std::int32_t));
    return {};
}

inline std::expected<void, Rel32OutOfRange> tryEmitCallRel32FromInstructionStart(
    uint8_t* buf, std::uint64_t currentRip, std::uint64_t targetAbs) noexcept
{
    return tryEmitCallRel32(buf, currentRip + 5u, targetAbs);
}

/// Checked emit: `CALL [rip+disp32]` (FF 15).
inline std::expected<void, Rel32OutOfRange> tryEmitCallRipRel32(
    uint8_t* buf, std::uint64_t ripAfterOpcode, std::uint64_t iatSlotAbs) noexcept
{
    const std::int64_t d = ripRelativeDelta(ripAfterOpcode, iatSlotAbs);
    const auto enc = encodeRel32Displacement(d);
    if (!enc)
    {
        return std::unexpected(enc.error());
    }
    buf[0] = 0xFF;
    buf[1] = 0x15;
    std::memcpy(buf + 2, &enc.value(), sizeof(std::int32_t));
    return {};
}

inline std::expected<void, Rel32OutOfRange> tryEmitCallRipRel32FromInstructionStart(
    uint8_t* buf, std::uint64_t currentRip, std::uint64_t iatSlotAbs) noexcept
{
    return tryEmitCallRipRel32(buf, currentRip + kCallRipRel32Size, iatSlotAbs);
}

}  // namespace rawrxd::sovereign::emit
