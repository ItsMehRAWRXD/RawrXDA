#pragma once
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "brutal_gzip.h"

namespace brutal {

/**
 * @brief Compress std::vector<uint8_t> using brutal MASM stored-block gzip
 * @param in Raw input data
 * @return Compressed gzip stream
 */
inline std::vector<uint8_t> compress_std(const std::vector<uint8_t>& in)
{
    if (in.empty()) return {};

    size_t out_len = 0;
    void* out_ptr = deflate_brutal_masm(in.data(), in.size(), &out_len);
    
    if (!out_ptr) return {};

    std::vector<uint8_t> result(out_len);
    std::memcpy(result.data(), out_ptr, out_len);
    std::free(out_ptr);
    
    return result;
}

/**
 * @brief Compress raw buffer using brutal MASM stored-block gzip
 */
inline std::vector<uint8_t> compress_buf(const void* data, size_t len)
{
    if (!data || len == 0) return {};

    size_t out_len = 0;
    void* out_ptr = deflate_brutal_masm(data, len, &out_len);
    
    if (!out_ptr) return {};

    std::vector<uint8_t> result(out_len);
    std::memcpy(result.data(), out_ptr, out_len);
    std::free(out_ptr);
    
    return result;
}

} // namespace brutal
