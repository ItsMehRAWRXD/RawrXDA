#include "brutal_gzip.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>

// Since we are moving to production, we need real ZLib
// If ZLib is not strict available, we will include a minimal implementation via miniz or similar if needed
// But assuming environment has zlib based on previous context (msys64)
#include <zlib.h>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (!src || len == 0 || !out_len) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // High performance compression using zlib defaults optimized for speed
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        *out_len = 0;
        return nullptr;
    }

    zs.next_in = (Bytef*)src;
    zs.avail_in = (uInt)len;

    // Estimate output size (usually smaller, but can be slightly larger)
    size_t max_out = deflateBound(&zs, (uInt)len);
    void* out_buf = malloc(max_out);
    if (!out_buf) {
        deflateEnd(&zs);
        *out_len = 0;
        return nullptr;
    }

    zs.next_out = (Bytef*)out_buf;
    zs.avail_out = (uInt)max_out;

    int ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        free(out_buf);
        deflateEnd(&zs);
        *out_len = 0;
        return nullptr;
    }

    deflateEnd(&zs);
    *out_len = zs.total_out;
    return out_buf;
}

extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    // ARM implementation fallback to standard generic optimized version
    return deflate_brutal_masm(src, len, out_len);
}

namespace CPUInference {
namespace brutal {

std::vector<uint8_t> compress_std(const std::vector<uint8_t>& input) {
    if (input.empty()) return {};
    
    size_t out_len = 0;
    void* out_ptr = deflate_brutal_masm(input.data(), input.size(), &out_len);
    
    if (!out_ptr || out_len == 0) return {};
    
    std::vector<uint8_t> result(static_cast<uint8_t*>(out_ptr), static_cast<uint8_t*>(out_ptr) + out_len);
    free(out_ptr);
    return result;
}

} // namespace brutal

namespace codec {

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = false;
        return {};
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    // Enable gzip/zlib detection (32 used to detect)
    if (inflateInit2(&zs, 15 + 32) != Z_OK) {
        if (success) *success = false;
        return {};
    }

    zs.next_in = (Bytef*)data.data();
    zs.avail_in = (uInt)data.size();

    int ret;
    std::vector<uint8_t> outbuffer;
    const size_t BUFSIZE = 32768; // 32KB chunks
    std::vector<uint8_t> temp_buf(BUFSIZE);

    do {
        zs.next_out = temp_buf.data();
        zs.avail_out = BUFSIZE;

        ret = ::inflate(&zs, 0);

        if (outbuffer.size() < zs.total_out) {
             size_t have = BUFSIZE - zs.avail_out;
             outbuffer.insert(outbuffer.end(), temp_buf.data(), temp_buf.data() + have);
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        if (success) *success = false;
        return {}; // Parsing error
    }

    if (success) *success = true;
    return outbuffer;
}

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
    bool ok = true;
    auto res = brutal::compress_std(data);
    if (res.empty() && !data.empty()) ok = false;
    if (success) *success = ok;
    return res;
}

} // namespace codec
} // namespace CPUInference
