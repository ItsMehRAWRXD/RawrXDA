#include "codec/compression.h"
#include "brutal_gzip.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace codec {

// Simple stored-block decompressor for brutal compression format
// Brutal compression uses stored blocks (no compression) with a simple header
static std::vector<uint8_t> brutal_inflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
    // Check for brutal compression header: "BRUTAL" + 8-byte size
    const char* header = "BRUTAL";
    if (data.size() < 14 || memcmp(data.data(), header, 6) != 0) {
        if (success) *success = false;
        std::cerr << "Invalid brutal compression header" << std::endl;
        return std::vector<uint8_t>();
    }
    
    // Read uncompressed size from header
    uint64_t uncompressed_size = 0;
    memcpy(&uncompressed_size, data.data() + 6, 8);
    
    // Extract the stored data
    std::vector<uint8_t> result(data.begin() + 14, data.end());
    
    // Verify size matches
    if (result.size() != uncompressed_size) {
        if (success) *success = false;
        std::cerr << "Size mismatch in brutal compression data" << std::endl;
        return std::vector<uint8_t>();
    }
    
    if (success) *success = true;
    return result;
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
        return std::vector<uint8_t>();
    }
    
    // Try brutal decompression first
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