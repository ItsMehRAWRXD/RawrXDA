// deflate_brutal_stub.cpp — fallback implementation when MASM/zlib deflate is unavailable.
// This keeps callers functional by returning an owned buffer containing the input bytes.

#include <cstddef>
#include <cstdlib>
#include <cstring>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!src || len == 0) {
        return nullptr;
    }

    void* out = std::malloc(len);
    if (!out) {
        return nullptr;
    }
    std::memcpy(out, src, len);
    if (out_len) {
        *out_len = len;
    }
    return out;
}

extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    return deflate_brutal_masm(src, len, out_len);
}
