// codec.cpp — Root-level no-zlib fallback for builds without codec/ subtree.
// The real implementations live in codec/compression.cpp (uses brutal::compress).
// The real extern "C" stubs live in codec/deflate_brutal_stub.cpp.
// This file provides the fallback codec::deflate/inflate that simply fail,
// plus the same extern "C" stubs, for targets that link this file directly
// instead of the codec/ subtree.

#include "../include/codec.h"

#include <vector>
#include <cstdlib>
#include <cstddef>

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

void* deflate_brutal_masm(const void* /*src*/, size_t /*len*/, size_t* out_len) {
    if (out_len) *out_len = 0;
    return nullptr;
}

void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    return deflate_brutal_masm(src, len, out_len);
}

} // extern "C"

