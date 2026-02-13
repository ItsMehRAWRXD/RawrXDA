// deflate_brutal_stub.cpp — No-zlib stub for deflate_brutal_masm when MASM version is not linked.
// Use this for targets (e.g. RawrXD-Win32IDE) that do not link zlib or the ASM deflate.
// Callers get nullptr / empty result; compression is skipped.

#include <cstddef>
#include <cstdlib>

extern "C" void* deflate_brutal_masm(const void* /*src*/, size_t /*len*/, size_t* out_len) {
    if (out_len) *out_len = 0;
    return nullptr;
}
