#include "compression/sovereign_bitstream_codec.h"
#include "compression/sovereign_block_protocol.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if !defined(RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM)
#define RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM 0
#endif

namespace RawrXD::Compression
{

namespace
{

inline bool addU32Checked(uint32_t a, uint32_t b, uint32_t& out)
{
    const uint64_t sum = static_cast<uint64_t>(a) + static_cast<uint64_t>(b);
    if (sum > 0xFFFFFFFFull)
    {
        return false;
    }
    out = static_cast<uint32_t>(sum);
    return true;
}

inline bool hasInputRange(uint32_t cursor, uint32_t need, uint32_t limit)
{
    uint32_t end = 0;
    return addU32Checked(cursor, need, end) && end <= limit;
}

inline bool hasOutputRange(uint32_t cursor, uint32_t need, uint32_t capacity)
{
    uint32_t end = 0;
    return addU32Checked(cursor, need, end) && end <= capacity;
}

inline bool cpuHasAvx2AndBmi2()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuid(cpuInfo, 0);
    if (cpuInfo[0] < 7)
    {
        return false;
    }

    __cpuidex(cpuInfo, 7, 0);
    const bool hasAvx2 = (cpuInfo[1] & (1 << 5)) != 0;
    const bool hasBmi2 = (cpuInfo[1] & (1 << 8)) != 0;
    return hasAvx2 && hasBmi2;
#else
    return false;
#endif
}

inline uint32_t deltaZeroBiasDivisor()
{
    // Lower divisor => higher threshold => stronger LiteralRun bias.
    // Example: divisor=4 requires >=25% xor-bytes to be zero before choosing DeltaChunk.
    // Default 8 means >=12.5% xor-bytes zero to choose DeltaChunk.
    constexpr uint32_t kDefault = 8;
    constexpr uint32_t kMin = 2;
    constexpr uint32_t kMax = 64;

    const char* env = std::getenv("RAWRXD_SOVEREIGN_DELTA_ZERO_BIAS_DIVISOR");
    if (env == nullptr || env[0] == '\0')
    {
        return kDefault;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(env, &end, 10);
    if (end == env || *end != '\0')
    {
        return kDefault;
    }

    if (parsed < kMin)
    {
        return kMin;
    }
    if (parsed > kMax)
    {
        return kMax;
    }
    return static_cast<uint32_t>(parsed);
}

#if RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM
extern "C" uint32_t ExpandBitstream_Masm(const uint8_t* predicted, const uint8_t* inStream, uint32_t inBytes,
                                          uint8_t* outActual, uint32_t outCapacity);
extern "C" uint32_t SqueezeBitstream_Masm(const uint8_t* predicted, const uint8_t* inActual, uint32_t inBytes,
                                           uint8_t* outStream, uint32_t outCapacity);
#endif

CodecFn g_expand = nullptr;
CodecFn g_squeeze = nullptr;

inline uint32_t fnv1a32(const uint8_t* data, uint32_t bytes)
{
    if (data == nullptr)
    {
        return 0;
    }

    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < bytes; ++i)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t predictedDigest32(const uint8_t* predicted, uint32_t bytes)
{
    if (predicted == nullptr || bytes == 0)
    {
        return 0;
    }

    const uint32_t digestLen = (bytes < 64) ? bytes : 64;
    return fnv1a32(predicted, digestLen);
}

inline bool looksLikeSovereignHeader(const uint8_t* inStream, uint32_t inBytes)
{
    if (inStream == nullptr || inBytes < sizeof(SovereignBlockHeader))
    {
        return false;
    }

    SovereignBlockHeader hdr{};
    std::memcpy(&hdr, inStream, sizeof(SovereignBlockHeader));
    return hdr.Magic == kSovereignBlockMagic && hdr.Version == kSovereignBlockVersion &&
           hdr.HeaderBytes == kSovereignBlockHeaderBytes;
}

}  // namespace

extern "C" uint32_t ExpandBitstream_Ref(const uint8_t* predicted, const uint8_t* inStream, uint32_t inBytes,
                                         uint8_t* outActual, uint32_t outCapacity)
{
    if (inStream == nullptr || outActual == nullptr)
    {
        return 0;
    }

    uint32_t payloadOffset = 0;
    uint32_t payloadBytes = inBytes;
    uint32_t expectedOutBytes = 0;
    uint32_t expectedChecksum = 0;
    bool hasHeader = false;

    if (looksLikeSovereignHeader(inStream, inBytes))
    {
        SovereignBlockHeader hdr{};
        std::memcpy(&hdr, inStream, sizeof(SovereignBlockHeader));
        const uint32_t headerBytes = static_cast<uint32_t>(sizeof(SovereignBlockHeader));

        if (hdr.HeaderBytes != headerBytes || hdr.CompressedSize > (inBytes - headerBytes) ||
            hdr.UncompressedSize > outCapacity)
        {
            return 0;
        }

        if ((hdr.Flags & kSovereignFlagRequiresPredicted) != 0)
        {
            if (predicted == nullptr)
            {
                return 0;
            }
            if (hdr.PredictedDigest != 0 && hdr.PredictedDigest != predictedDigest32(predicted, hdr.UncompressedSize))
            {
                return 0;
            }
        }

        if (DecodeDigestAlgorithmFlag(hdr.Flags) != SovereignDigestAlgorithm::FNV1A32)
        {
            // Fail closed on unknown digest algorithm to avoid silent corruption.
            return 0;
        }

        payloadOffset = headerBytes;
        payloadBytes = hdr.CompressedSize;
        expectedOutBytes = hdr.UncompressedSize;
        expectedChecksum = hdr.Checksum;
        hasHeader = true;
    }

    uint32_t pIn = payloadOffset;
    uint32_t pOut = 0;
    const uint32_t inLimit = payloadOffset + payloadBytes;

    while (pIn < inLimit)
    {
        if (!hasInputRange(pIn, 1, inLimit))
        {
            return 0;
        }

        const uint8_t opcode = inStream[pIn++];

        if (!hasInputRange(pIn, 4, inLimit))
        {
            return 0;
        }

        uint32_t arg = 0;
        std::memcpy(&arg, inStream + pIn, sizeof(uint32_t));
        pIn += 4;

        if (opcode == kOpcodeEndOfBlock)
        {
            if (arg != 0)
            {
                return 0;
            }

            if (hasHeader)
            {
                if (pOut != expectedOutBytes)
                {
                    return 0;
                }
                if (fnv1a32(outActual, pOut) != expectedChecksum)
                {
                    return 0;
                }
            }
            return pOut;
        }

        if (!hasOutputRange(pOut, arg, outCapacity))
        {
            return 0;
        }

        switch (opcode)
        {
            case kOpcodeLiteralRun:
            {
                if (!hasInputRange(pIn, arg, inLimit))
                {
                    return 0;
                }
                std::memcpy(outActual + pOut, inStream + pIn, arg);
                pIn += arg;
                break;
            }
            case kOpcodeZeroRun:
            {
                std::memset(outActual + pOut, 0, arg);
                break;
            }
            case kOpcodeDeltaChunk:
            {
                if (predicted == nullptr || !hasInputRange(pIn, arg, inLimit))
                {
                    return 0;
                }

                for (uint32_t i = 0; i < arg; ++i)
                {
                    outActual[pOut + i] = predicted[pOut + i] ^ inStream[pIn + i];
                }
                pIn += arg;
                break;
            }
            default:
                return 0;
        }

        pOut += arg;
    }

    return 0;
}

extern "C" uint32_t SqueezeBitstream_Ref(const uint8_t* predicted, const uint8_t* inActual, uint32_t inBytes,
                                          uint8_t* outStream, uint32_t outCapacity)
{
    if (inActual == nullptr || outStream == nullptr)
    {
        return 0;
    }

    if (!hasOutputRange(0, static_cast<uint32_t>(sizeof(SovereignBlockHeader)), outCapacity))
    {
        return 0;
    }

    uint32_t outCursor = static_cast<uint32_t>(sizeof(SovereignBlockHeader));
    bool emittedLiteral = false;
    bool emittedZero = false;
    bool emittedDelta = false;

    auto writeOpcodeAndArg = [&](uint8_t opcode, uint32_t arg) -> bool {
        if (!hasOutputRange(outCursor, 5, outCapacity))
        {
            return false;
        }
        outStream[outCursor++] = opcode;
        std::memcpy(outStream + outCursor, &arg, sizeof(uint32_t));
        outCursor += 4;
        return true;
    };

    auto writeLiteral = [&](uint32_t start, uint32_t len) -> bool {
        if (!writeOpcodeAndArg(kOpcodeLiteralRun, len) || !hasOutputRange(outCursor, len, outCapacity))
        {
            return false;
        }
        std::memcpy(outStream + outCursor, inActual + start, len);
        outCursor += len;
        emittedLiteral = true;
        return true;
    };

    auto writeDelta = [&](uint32_t start, uint32_t len) -> bool {
        if (!writeOpcodeAndArg(kOpcodeDeltaChunk, len) || !hasOutputRange(outCursor, len, outCapacity))
        {
            return false;
        }
        for (uint32_t i = 0; i < len; ++i)
        {
            outStream[outCursor + i] = predicted[start + i] ^ inActual[start + i];
        }
        outCursor += len;
        emittedDelta = true;
        return true;
    };

    if (inBytes == 0)
    {
        if (!writeOpcodeAndArg(kOpcodeEndOfBlock, 0))
        {
            return 0;
        }
    }
    else
    {

    // Chunked reference encoder:
    // - Emit long zero spans as ZeroRun.
    // - Emit remaining spans in bounded chunks.
    // - If prediction is available, pick LiteralRun vs DeltaChunk per chunk
    //   using a cheap sparsity score (how many XOR bytes are zero).
    //   This avoids "single giant delta" blocks and improves downstream
    //   entropy coding when the stream has localized changes.
    constexpr uint32_t kMinZeroRun = 4;
    constexpr uint32_t kMaxMixedChunk = 64 * 1024;
    const uint32_t kDeltaZeroBiasDivisor = deltaZeroBiasDivisor();
    uint32_t pos = 0;

        while (pos < inBytes)
        {
        if (inActual[pos] == 0)
        {
            uint32_t z = pos;
            while (z < inBytes && inActual[z] == 0)
            {
                ++z;
            }

            const uint32_t zeroLen = z - pos;
            if (zeroLen >= kMinZeroRun)
            {
                if (!writeOpcodeAndArg(kOpcodeZeroRun, zeroLen))
                {
                    return 0;
                }
                emittedZero = true;
                pos = z;
                continue;
            }
        }

        const uint32_t chunkStart = pos;
        while (pos < inBytes)
        {
            if (inActual[pos] != 0)
            {
                ++pos;
                continue;
            }

            uint32_t z = pos;
            while (z < inBytes && inActual[z] == 0)
            {
                ++z;
            }

            if ((z - pos) >= kMinZeroRun)
            {
                break;
            }

            pos = z;
        }

        uint32_t remaining = pos - chunkStart;
        if (remaining == 0)
        {
            return 0;
        }

        uint32_t emitPos = chunkStart;
        while (remaining > 0)
        {
            const uint32_t emitLen = (remaining > kMaxMixedChunk) ? kMaxMixedChunk : remaining;

            bool ok = false;
            if (predicted == nullptr)
            {
                ok = writeLiteral(emitPos, emitLen);
            }
            else
            {
                uint32_t deltaZeroCount = 0;
                for (uint32_t i = 0; i < emitLen; ++i)
                {
                    if ((predicted[emitPos + i] ^ inActual[emitPos + i]) == 0)
                    {
                        ++deltaZeroCount;
                    }
                }

                const bool preferDelta = deltaZeroCount >= (emitLen / kDeltaZeroBiasDivisor);
                ok = preferDelta ? writeDelta(emitPos, emitLen) : writeLiteral(emitPos, emitLen);
            }

            if (!ok)
            {
                return 0;
            }

            emitPos += emitLen;
            remaining -= emitLen;
        }
        }

        if (!writeOpcodeAndArg(kOpcodeEndOfBlock, 0))
        {
            return 0;
        }
    }

    const uint32_t totalBytes = outCursor;
    const uint32_t payloadBytes = totalBytes - static_cast<uint32_t>(sizeof(SovereignBlockHeader));

    SovereignBlockHeader hdr{};
    hdr.Magic = kSovereignBlockMagic;
    hdr.Version = kSovereignBlockVersion;
    hdr.HeaderBytes = kSovereignBlockHeaderBytes;
    hdr.GenerationID = 0;
    hdr.CompressedSize = payloadBytes;
    hdr.UncompressedSize = inBytes;
    hdr.BlockType = static_cast<uint16_t>(SovereignBlockType::Raw);
    if (emittedDelta)
    {
        hdr.BlockType = static_cast<uint16_t>(SovereignBlockType::DeltaBitplane);
    }
    else if (emittedZero && !emittedLiteral)
    {
        hdr.BlockType = static_cast<uint16_t>(SovereignBlockType::ZeroFill);
    }
    hdr.Precision = 0;
    hdr.Checksum = fnv1a32(inActual, inBytes);
    hdr.LeaseState = static_cast<uint8_t>(SovereignLeaseState::None);
    hdr.Flags = EncodeDigestAlgorithmFlag(SovereignDigestAlgorithm::FNV1A32) |
                (emittedDelta ? kSovereignFlagRequiresPredicted : 0);
    hdr.Reserved0 = 0;
    hdr.PredictedDigest = emittedDelta ? predictedDigest32(predicted, inBytes) : 0;
    std::memset(hdr.Reserved, 0, sizeof(hdr.Reserved));

    std::memcpy(outStream, &hdr, sizeof(SovereignBlockHeader));

    return totalBytes;
}

CodecFn SelectExpandDecoder()
{
    if (g_expand != nullptr)
    {
        return g_expand;
    }

#if RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM
    if (cpuHasAvx2AndBmi2())
    {
        g_expand = &ExpandBitstream_Masm;
    }
    else
#endif
    {
        g_expand = &ExpandBitstream_Ref;
    }

    return g_expand;
}

CodecFn SelectSqueezeEncoder()
{
    if (g_squeeze != nullptr)
    {
        return g_squeeze;
    }

#if RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM
    if (cpuHasAvx2AndBmi2())
    {
        g_squeeze = &SqueezeBitstream_Masm;
    }
    else
#endif
    {
        g_squeeze = &SqueezeBitstream_Ref;
    }

    return g_squeeze;
}

extern "C" uint32_t ExpandBitstream(const uint8_t* predicted, const uint8_t* inStream, uint32_t inBytes,
                                     uint8_t* outActual, uint32_t outCapacity)
{
    return SelectExpandDecoder()(predicted, inStream, inBytes, outActual, outCapacity);
}

extern "C" uint32_t SqueezeBitstream(const uint8_t* predicted, const uint8_t* inActual, uint32_t inBytes,
                                      uint8_t* outStream, uint32_t outCapacity)
{
    return SelectSqueezeEncoder()(predicted, inActual, inBytes, outStream, outCapacity);
}

}  // namespace RawrXD::Compression
