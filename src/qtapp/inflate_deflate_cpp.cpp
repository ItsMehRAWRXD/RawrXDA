#include <cstdint>
#include <cstring>

#include "brutal_gzip.h"

namespace codec {

// Use brutal_gzip MASM deflate for compression
std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok = nullptr)
{
#if defined(HAS_BRUTAL_GZIP_MASM) || defined(HAS_BRUTAL_GZIP_NEON)
    size_t out_len = 0;
    void* compressed = nullptr;
    
#ifdef HAS_BRUTAL_GZIP_MASM
    compressed = deflate_brutal_masm(in.constData(), in.size(), &out_len);
#elif defined(HAS_BRUTAL_GZIP_NEON)
    compressed = deflate_brutal_neon(in.constData(), in.size(), &out_len);
#endif
    
    if (compressed && out_len > 0) {
        std::vector<uint8_t> result(static_cast<const char*>(compressed), out_len);
        free(compressed);  // brutal_gzip uses malloc
        if (ok) *ok = true;
        return result;
    }
#endif
    
    if (ok) *ok = false;
    return std::vector<uint8_t>();
}

// Placeholder for inflate - add your inflate implementation here
// For now, return uncompressed data for testing
std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok = nullptr)
{
    // TODO: Implement inflate using your existing inflate kernel
    // For now, assume data is not compressed and return as-is
    if (ok) *ok = true;
    return in;
}

} // namespace codec

