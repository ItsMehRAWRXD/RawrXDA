// ============================================================================
// rxa_packer.cpp — GGUF → RXA writer using Brutal GZIP (MASM) compression
// ============================================================================
#include "rxa_packer.h"
#include "codec/brutal_gzip.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace RawrXD
{

namespace
{
constexpr uint32_t kRxaMagic = 0x21584152u;     // "RXA!"
constexpr uint32_t kRxaVersion1 = 0x00010000u;  // v1.0
constexpr uint8_t kRxaAlgBrutalGzip = 3;

#pragma pack(push, 1)
struct RxaHeaderV1
{
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t blockSize;
    uint64_t uncompressedSize;
    uint32_t blockCount;
    uint32_t reserved;
};

struct RxaBlockEntryV1
{
    uint64_t offset;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t crc32c;
    uint8_t algorithm;
    uint8_t reserved[3];
};
#pragma pack(pop)

// Simple CRC32C (Castagnoli) — software fallback, no hardware required
static uint32_t crc32c_table[256];
static bool crc32c_table_initialized = false;

static void init_crc32c_table()
{
    if (crc32c_table_initialized)
        return;
    constexpr uint32_t poly = 0x82F63B78u;
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j)
        {
            crc = (crc & 1u) ? (crc >> 1) ^ poly : (crc >> 1);
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_initialized = true;
}

static uint32_t crc32c_update(uint32_t crc, const uint8_t* data, size_t len)
{
    init_crc32c_table();
    crc = ~crc;
    for (size_t i = 0; i < len; ++i)
    {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFFu];
    }
    return ~crc;
}

static uint32_t crc32c(const uint8_t* data, size_t len)
{
    return crc32c_update(0, data, len);
}

} // anonymous namespace

bool PackGgufToRxa(const std::string& ggufPath,
                   const std::string& rxaPath,
                   const RxaPackOptions& options,
                   std::string* outError)
{
    if (outError)
        outError->clear();

    // -------------------------------------------------------------------------
    // 1. Read source GGUF
    // -------------------------------------------------------------------------
    std::ifstream in(ggufPath, std::ios::binary | std::ios::ate);
    if (!in.is_open())
    {
        if (outError)
            *outError = "unable to open source GGUF: " + ggufPath;
        return false;
    }

    const uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    if (fileSize == 0)
    {
        if (outError)
            *outError = "source GGUF is empty";
        return false;
    }

    std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
    in.read(reinterpret_cast<char*>(fileData.data()), static_cast<std::streamsize>(fileSize));
    if (!in.good())
    {
        if (outError)
            *outError = "failed to read source GGUF";
        return false;
    }
    in.close();

    // -------------------------------------------------------------------------
    // 2. Determine block layout
    // -------------------------------------------------------------------------
    const uint32_t blockSize = options.blockSize ? options.blockSize : (4u * 1024u * 1024u);
    const uint64_t blockCount64 = (fileSize + blockSize - 1) / blockSize;
    if (blockCount64 > (8u * 1024u * 1024u))
    {
        if (outError)
            *outError = "GGUF too large for RXA format (> 8M blocks)";
        return false;
    }
    const uint32_t blockCount = static_cast<uint32_t>(blockCount64);

    // -------------------------------------------------------------------------
    // 3. Compress blocks
    // -------------------------------------------------------------------------
    std::vector<std::vector<uint8_t>> compressedBlocks;
    compressedBlocks.reserve(blockCount);

    uint64_t totalCompressed = 0;
    for (uint32_t i = 0; i < blockCount; ++i)
    {
        const uint64_t offset = static_cast<uint64_t>(i) * blockSize;
        const uint64_t remaining = fileSize - offset;
        const uint32_t chunkSize = static_cast<uint32_t>(std::min<uint64_t>(remaining, blockSize));

        const uint8_t* chunkPtr = fileData.data() + offset;
        std::vector<uint8_t> chunk(chunkPtr, chunkPtr + chunkSize);

        // Compress via Brutal GZIP (MASM path)
        std::vector<uint8_t> compressed = brutal::compress(chunk);

        // If compression inflated the block, store raw (but still mark as brutal)
        // The brutal compressor already handles passthrough with a marker prefix.
        compressedBlocks.push_back(std::move(compressed));
        totalCompressed += compressedBlocks.back().size();

        if (options.verbose)
        {
            const size_t cs = compressedBlocks.back().size();
            const float ratio = (chunkSize > 0) ? (100.0f * cs / chunkSize) : 0.0f;
            std::cerr << "[RXA] Block " << i << ": " << chunkSize << " → " << cs
                      << " bytes (" << ratio << "% of original)\n";
        }
    }

    // -------------------------------------------------------------------------
    // 4. Write RXA archive
    // -------------------------------------------------------------------------
    std::ofstream out(rxaPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        if (outError)
            *outError = "unable to create RXA output: " + rxaPath;
        return false;
    }

    // Header
    RxaHeaderV1 header{};
    header.magic = kRxaMagic;
    header.version = kRxaVersion1;
    header.flags = 0;
    header.blockSize = blockSize;
    header.uncompressedSize = fileSize;
    header.blockCount = blockCount;
    header.reserved = 0;

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out.good())
    {
        if (outError)
            *outError = "failed to write RXA header";
        return false;
    }

    // Block index (entries written first, then payloads)
    const uint64_t indexOffset = sizeof(RxaHeaderV1);
    const uint64_t payloadOffset = indexOffset + blockCount * sizeof(RxaBlockEntryV1);

    std::vector<RxaBlockEntryV1> entries(blockCount);
    uint64_t currentOffset = payloadOffset;

    for (uint32_t i = 0; i < blockCount; ++i)
    {
        const uint64_t offset = static_cast<uint64_t>(i) * blockSize;
        const uint64_t remaining = fileSize - offset;
        const uint32_t chunkSize = static_cast<uint32_t>(std::min<uint64_t>(remaining, blockSize));

        RxaBlockEntryV1 entry{};
        entry.offset = currentOffset;
        entry.compressedSize = static_cast<uint32_t>(compressedBlocks[i].size());
        entry.uncompressedSize = chunkSize;
        entry.crc32c = crc32c(compressedBlocks[i].data(), compressedBlocks[i].size());
        entry.algorithm = kRxaAlgBrutalGzip;
        entry.reserved[0] = entry.reserved[1] = entry.reserved[2] = 0;

        entries[i] = entry;
        currentOffset += entry.compressedSize;
    }

    out.write(reinterpret_cast<const char*>(entries.data()),
              static_cast<std::streamsize>(entries.size() * sizeof(RxaBlockEntryV1)));
    if (!out.good())
    {
        if (outError)
            *outError = "failed to write RXA block index";
        return false;
    }

    // Payloads
    for (uint32_t i = 0; i < blockCount; ++i)
    {
        out.write(reinterpret_cast<const char*>(compressedBlocks[i].data()),
                  static_cast<std::streamsize>(compressedBlocks[i].size()));
        if (!out.good())
        {
            if (outError)
                *outError = "failed to write RXA block payload";
            return false;
        }
    }

    out.flush();
    if (!out.good())
    {
        if (outError)
            *outError = "failed to flush RXA archive";
        return false;
    }

    if (options.verbose)
    {
        const float overallRatio = (fileSize > 0)
            ? (100.0f * static_cast<float>(currentOffset - payloadOffset + sizeof(RxaHeaderV1)) / static_cast<float>(fileSize))
            : 0.0f;
        std::cerr << "[RXA] Archive complete: " << fileSize << " → " << currentOffset
                  << " bytes (" << overallRatio << "% of original)\n";
    }

    return true;
}

} // namespace RawrXD
