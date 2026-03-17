// #include "../include/codec.h"

// #include <zlib.h> // Not available
#include <vector>
#include <cstdlib>
#include <cstring>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) *success = false;
    return {};
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) *success = false;
    return {};
}

}  // namespace codec

extern "C" {
void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len) *out_len = 0;
    return nullptr;
}

void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    return deflate_brutal_masm(src, len, out_len);
}
}


