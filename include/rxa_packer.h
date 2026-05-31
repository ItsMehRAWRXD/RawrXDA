#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// ============================================================================
// RXA Packer — writes GGUF → RXA using Brutal GZIP (MASM) compression
// ============================================================================

struct RxaPackOptions {
    uint32_t blockSize = 4 * 1024 * 1024;   // 4 MiB default blocks
    uint8_t algorithm = 3;                   // kRxaAlgBrutalGzip
    bool verbose = false;
};

// Pack a GGUF file into an RXA archive.
// Returns true on success, false on failure (error written to outError).
bool PackGgufToRxa(
    const std::string& ggufPath,
    const std::string& rxaPath,
    const RxaPackOptions& options = {},
    std::string* outError = nullptr
);

// Convenience: pack with default options.
inline bool PackGgufToRxaDefault(const std::string& ggufPath, const std::string& rxaPath,
                                  std::string* outError = nullptr)
{
    return PackGgufToRxa(ggufPath, rxaPath, RxaPackOptions{}, outError);
}

} // namespace RawrXD
