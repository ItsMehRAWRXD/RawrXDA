#include "codec/compression.h"
#include "brutal_gzip.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success) {
    if (data.empty()) {
        if (success) *success = true;
        return std::vector<uint8_t>();
    }
    
#ifdef BRUTAL_COMPRESSION_ONLY
    return deflate_brutal_masm(data, success);
#else
    // Fallback or ZLIB logic if enabled... 
    // But since user requested strict no deps, we redirect to brutal.
    return deflate_brutal_masm(data, success);
#endif
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success) {
    // Stub for now or implement simple inflate if needed. 
    // The user emphasized brutal *compression* for speed.
    // Decompression usually requires a standard inflate. 
    // Since we can't use zlib, we might need a mini-inflate implementation.
    // However, the link error was about compress/uncompress symbols.
    // If we remove zlib calls, we solve the link error.
    
    if (success) *success = false;
    std::cerr << "Inflate not implemented in dependency-free mode yet." << std::endl;
    return std::vector<uint8_t>();
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