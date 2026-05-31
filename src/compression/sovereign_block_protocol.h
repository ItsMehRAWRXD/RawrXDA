#pragma once

#include <cstddef>
#include <cstdint>

namespace RawrXD::Compression
{

constexpr uint32_t kSovereignBlockMagic = 0x42565253u;  // "SVRB"
constexpr uint16_t kSovereignBlockVersion = 1;
constexpr uint16_t kSovereignBlockHeaderBytes = 64;
constexpr uint8_t kSovereignFlagRequiresPredicted = 0x01;
constexpr uint8_t kSovereignFlagDigestAlgorithmMask = 0x0E;
constexpr uint8_t kSovereignFlagDigestAlgorithmShift = 1;

enum class SovereignDigestAlgorithm : uint8_t
{
    FNV1A32 = 0,
    XXH3_64 = 1,  // Reserved for future migration.
};

constexpr uint8_t EncodeDigestAlgorithmFlag(SovereignDigestAlgorithm algorithm)
{
    return (static_cast<uint8_t>(algorithm) << kSovereignFlagDigestAlgorithmShift) &
           kSovereignFlagDigestAlgorithmMask;
}

constexpr SovereignDigestAlgorithm DecodeDigestAlgorithmFlag(uint8_t flags)
{
    return static_cast<SovereignDigestAlgorithm>((flags & kSovereignFlagDigestAlgorithmMask) >>
                                                 kSovereignFlagDigestAlgorithmShift);
}

enum class SovereignBlockType : uint16_t
{
    Raw = 0,
    DeltaBitplane = 1,
    ZeroFill = 2,
};

enum class SovereignLeaseState : uint8_t
{
    None = 0,
    Speculative = 1,
    Protected = 2,
};

struct alignas(64) SovereignBlockHeader
{
    uint32_t Magic;
    uint16_t Version;
    uint16_t HeaderBytes;
    uint64_t GenerationID;
    uint32_t CompressedSize;
    uint32_t UncompressedSize;
    uint16_t BlockType;
    uint16_t Precision;
    uint32_t Checksum;
    uint8_t LeaseState;
    uint8_t Flags;
    uint16_t Reserved0;
    uint32_t PredictedDigest;
    uint8_t Reserved[24];
};

static_assert(sizeof(SovereignBlockHeader) == 64, "SovereignBlockHeader must be 64 bytes");
static_assert(alignof(SovereignBlockHeader) == 64, "SovereignBlockHeader must be 64-byte aligned");

}  // namespace RawrXD::Compression
