#include "brutal_gzip.h"
#include "codec/gzip_brutal_inflate.hpp"

#include <cstdlib>
#include <cstring>

// MASM x64: linked via deflate_brutal_masm.obj when HAS_BRUTAL_GZIP_MASM=1
extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

namespace brutal {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    // Use the real zlib-based compressor via deflate_brutal_masm
    size_t out_len = 0;
    void* compressed = deflate_brutal_masm(data.data(), data.size(), &out_len);
    if (!compressed || out_len == 0) {
        // Fallback: return uncompressed with marker prefix
        std::vector<uint8_t> result;
        result.reserve(data.size() + 4);
        // 4-byte magic indicating uncompressed passthrough: 0x00 0x00 0x00 0x00
        result.push_back(0x00); result.push_back(0x00);
        result.push_back(0x00); result.push_back(0x00);
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    std::vector<uint8_t> result((uint8_t*)compressed, (uint8_t*)compressed + out_len);
    std::free(compressed);
    return result;
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    // Check for uncompressed passthrough marker
    if (data.size() > 4 && data[0] == 0x00 && data[1] == 0x00
        && data[2] == 0x00 && data[3] == 0x00) {
        return std::vector<uint8_t>(data.begin() + 4, data.end());
    }

    // Inflate gzip from real MASM encoder (stored DEFLATE blocks) — no codec::inflate
    // (avoids recursion: codec::inflate -> brutal::decompress -> …).
    std::vector<uint8_t> inflated;
    if (RawrXD::Codec::gzipBrutalInflateStoredBlocks(reinterpret_cast<const uint8_t*>(data.data()), data.size(),
                                                     inflated)) {
        return inflated;
    }

    return data;
}

}
