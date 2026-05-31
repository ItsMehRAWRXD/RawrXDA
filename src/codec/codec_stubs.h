#pragma once
#include <vector>
#include <cstdint>
#include <zlib.h>

namespace codec {
    inline std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
        if (data.empty()) {
            if (success) *success = true;
            return data;
        }
        
        z_stream strm = {};
        strm.next_in = const_cast<uint8_t*>(data.data());
        strm.avail_in = static_cast<uInt>(data.size());
        
        if (inflateInit2(&strm, 15 + 32) != Z_OK) { // 15 + 32 = auto-detect gzip/zlib
            if (success) *success = false;
            return {};
        }
        
        std::vector<uint8_t> out;
        out.reserve(data.size() * 4); // heuristic
        
        int ret;
        uint8_t buf[4096];
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);
            ret = ::inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&strm);
                if (success) *success = false;
                return {};
            }
            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);
        } while (ret != Z_STREAM_END);
        
        inflateEnd(&strm);
        if (success) *success = true;
        return out;
    }

    inline std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
        if (data.empty()) {
            if (success) *success = true;
            return data;
        }
        
        z_stream strm = {};
        strm.next_in = const_cast<uint8_t*>(data.data());
        strm.avail_in = static_cast<uInt>(data.size());
        
        if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
            if (success) *success = false;
            return {};
        }
        
        std::vector<uint8_t> out;
        out.reserve(data.size()); // compressed may be larger
        
        int ret;
        uint8_t buf[4096];
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);
            ret = ::deflate(&strm, Z_FINISH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                deflateEnd(&strm);
                if (success) *success = false;
                return {};
            }
            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);
        } while (ret != Z_STREAM_END);
        
        deflateEnd(&strm);
        if (success) *success = true;
        return out;
    }
}
