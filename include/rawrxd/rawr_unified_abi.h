#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace rawr::abi {

constexpr std::uint32_t kVersion = 0x0100;
constexpr std::uint32_t kAlignBytes = 256;
constexpr std::uint32_t kWeightsReadySlots = 512;

constexpr std::uint64_t kTokenRingBytes = 1ull * 1024ull * 1024ull;
constexpr std::uint64_t kKvCacheBytes = 2ull * 1024ull * 1024ull * 1024ull;

// Round up helper for fixed ABI offsets.
constexpr std::uint64_t AlignUp(std::uint64_t value, std::uint64_t align) {
    return (value + align - 1ull) & ~(align - 1ull);
}

#pragma pack(push, 1)
struct RawrUnifiedFlags {
    volatile std::uint32_t new_data;      // Producer signal (CPU)
    volatile std::uint32_t data_seq;      // Total ingress bytes seen
    volatile std::uint32_t consumed_seq;  // GPU-side consumed byte seq
    volatile std::uint32_t frame_ready;   // GPU-produced frame counter
    volatile std::uint32_t error_code;    // Sticky nonzero error code
    volatile std::uint32_t shutdown;      // Host requests graceful shutdown

    // Reserve bytes so weights_ready starts at 0x200 for cacheline-friendly scans.
    std::uint32_t reserved0[122];

    // Layer readiness protocol (0 = not ready, nonzero = ready sequence/id).
    volatile std::uint32_t weights_ready[kWeightsReadySlots];
};
#pragma pack(pop)

static_assert(std::is_standard_layout_v<RawrUnifiedFlags>, "RawrUnifiedFlags must be standard layout");
static_assert(offsetof(RawrUnifiedFlags, weights_ready) == 512, "weights_ready offset drift");
static_assert(sizeof(RawrUnifiedFlags) == 2560, "RawrUnifiedFlags size drift");
static_assert(sizeof(RawrUnifiedFlags) % kAlignBytes == 0, "RawrUnifiedFlags alignment drift");

constexpr std::uint64_t kOffsetFlags = 0;
constexpr std::uint64_t kBytesFlags = sizeof(RawrUnifiedFlags);

constexpr std::uint64_t kOffsetTokenRing = AlignUp(kOffsetFlags + kBytesFlags, kAlignBytes);
constexpr std::uint64_t kOffsetKvCache = AlignUp(kOffsetTokenRing + kTokenRingBytes, kAlignBytes);
constexpr std::uint64_t kOffsetWeights = AlignUp(kOffsetKvCache + kKvCacheBytes, kAlignBytes);

// Glyph output can be negotiated per swapchain/image setup. Keep offset fixed once chosen.
constexpr std::uint64_t kOffsetGlyphDefault = AlignUp(kOffsetWeights + kAlignBytes, kAlignBytes);

static_assert(kOffsetTokenRing == 2560, "token ring offset drift");

enum FlagIndex : std::uint32_t {
    RF_NEW_DATA = 0,
    RF_DATA_SEQ = 1,
    RF_CONSUMED_SEQ = 2,
    RF_FRAME_READY = 3,
    RF_ERROR_CODE = 4,
    RF_SHUTDOWN = 5,
    RF_WEIGHTS_BASE = 128 // uint32 index where weights_ready[] starts
};

} // namespace rawr::abi
