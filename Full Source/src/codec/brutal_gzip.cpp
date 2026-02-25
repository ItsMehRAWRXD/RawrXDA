#include "brutal_gzip.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

// Forward-declare the zlib-based MASM fallback used by brutal_gzip_fallback.cpp
// If zlib is not available, we use a raw DEFLATE decoder.
extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

// Forward declaration — implemented in inflate_deflate_cpp.cpp
namespace codec { std::vector<uint8_t> inflate(const std::vector<uint8_t>&, bool*); }
static inline std::vector<uint8_t> codec_inflate_impl(const std::vector<uint8_t>& d, bool* ok) {
    return codec::inflate(d, ok);
}

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

    // Try gzip decompression using codec::inflate
    // The codec::inflate function handles DEFLATE/gzip streams
    bool ok = false;
    std::vector<uint8_t> result = codec_inflate_impl(data, &ok);
    if (ok && !result.empty()) {
        return result;
    }

    // If inflation fails, data may already be uncompressed — return as-is
    return data;
}

}
