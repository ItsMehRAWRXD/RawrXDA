#include "codec/compression.h"
#include "brutal_gzip.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdint>

namespace codec {

static uint16_t read_le16(const uint8_t* ptr) {
    return static_cast<uint16_t>(ptr[0] | (ptr[1] << 8));
}

static uint32_t read_le32(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
}

// Gzip stored-block decompressor for brutal compression output (deflate_nasm)
static std::vector<uint8_t> brutal_inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.size() < 18) {
        if (success) *success = false;
        return {};
    }

    const uint8_t* bytes = data.data();
    size_t size = data.size();

    // GZIP header
    if (bytes[0] != 0x1F || bytes[1] != 0x8B || bytes[2] != 0x08) {
        if (success) *success = false;
        return {};
    }

    uint8_t flg = bytes[3];
    size_t pos = 10;

    if (flg & 0x04) {
        if (pos + 2 > size) { if (success) *success = false; return {}; }
        uint16_t xlen = read_le16(bytes + pos);
        pos += 2 + xlen;
    }
    if (flg & 0x08) {
        while (pos < size && bytes[pos] != 0) pos++;
        pos++;
    }
    if (flg & 0x10) {
        while (pos < size && bytes[pos] != 0) pos++;
        pos++;
    }
    if (flg & 0x02) {
        if (pos + 2 > size) { if (success) *success = false; return {}; }
        pos += 2;
    }

    if (pos + 8 > size) {
        if (success) *success = false;
        return {};
    }

    uint32_t isize = read_le32(bytes + size - 4);
    std::vector<uint8_t> output;
    output.reserve(isize);

    if (pos >= size - 8) {
        if (success) *success = false;
        return {};
    }

    uint8_t block_hdr = bytes[pos++];
    uint8_t btype = (block_hdr >> 1) & 0x03;
    if (btype != 0) {
        if (success) *success = false;
        return {};
    }

    while (output.size() < isize && pos + 4 <= size - 8) {
        uint16_t len = read_le16(bytes + pos);
        uint16_t nlen = read_le16(bytes + pos + 2);
        pos += 4;

        if (static_cast<uint16_t>(~len) != nlen) {
            if (success) *success = false;
            return {};
        }
        if (pos + len > size - 8) {
            if (success) *success = false;
            return {};
        }

        output.insert(output.end(), bytes + pos, bytes + pos + len);
        pos += len;
    }

    if (output.size() != isize) {
        if (success) *success = false;
        return {};
    }

    if (success) *success = true;
    return output;
}

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
    // Always use brutal compression for dependency-free builds
    return deflate_brutal_masm(data, success);
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return {};
    }

    return brutal_inflate(data, success);
}

std::vector<uint8_t> deflate_brutal_masm(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
    size_t out_len = 0;
    void* ptr = ::deflate_brutal_masm(data.data(), data.size(), &out_len);
    
    if (ptr) {
        std::vector<uint8_t> result(static_cast<uint8_t*>(ptr), static_cast<uint8_t*>(ptr) + out_len);
        free(ptr);
        if (success) *success = true;
        return result;
    }
    
    if (success) *success = false;
    return std::vector<uint8_t>();
}

} // namespace codec