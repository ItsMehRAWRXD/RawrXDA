// zero_copy_index.hpp — Zero-copy context search bridge for memory-mapped files.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace RawrXD::Search {

struct MappedFileView {
    const std::uint8_t* data = nullptr;
    std::size_t length = 0;
    void* fileHandle = nullptr;
    void* mappingHandle = nullptr;
    void* viewHandle = nullptr;
};

struct ContextSearchStats {
    std::size_t chunksScanned = 0;
    std::size_t matches = 0;
    bool kernelLinked = false;
};

// Pack up to 8 bytes from symbol text into a little-endian qword token.
std::uint64_t packSymbolQword(const std::string& symbol);

bool mapFileReadOnly(const std::string& path, MappedFileView& outView);
void unmapFileView(MappedFileView& view);

// Initializes the AVX-512 symbol table for the kernel path.
// Uses first 64 entries from symbolQwords and zero-fills the remaining slots.
bool initializeContextSearchSymbols(std::span<const std::uint64_t> symbolQwords);

// Scans a memory buffer for any of the active symbols and writes byte offsets for chunks with hits.
std::size_t findSymbolOffsets(const void* buffer, std::size_t bufferLength, std::uint64_t* outOffsets,
                              std::size_t maxOffsets, ContextSearchStats* outStats = nullptr);

// Convenience wrapper: memory-map file and scan in one call.
std::size_t executeContextSearch(const std::string& path, std::span<const std::uint64_t> symbolQwords,
                                 std::uint64_t* outOffsets, std::size_t maxOffsets,
                                 ContextSearchStats* outStats = nullptr);

}  // namespace RawrXD::Search
