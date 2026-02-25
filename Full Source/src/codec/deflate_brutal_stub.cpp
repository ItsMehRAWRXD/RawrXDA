// deflate_brutal_stub.cpp — Consolidated no-op stubs for all brutal deflate variants.
// Use this for targets (e.g. RawrXD-Win32IDE) that do not link zlib, the ASM deflate,
// or the NEON deflate. Callers get nullptr / empty result; compression is skipped.
// Merges the MASM-only stub with the NEON fallback from src/codec.cpp.

#include <cstddef>
#include <cstdlib>

extern "C" {

void* deflate_brutal_masm(const void* /*src*/, size_t /*len*/, size_t* out_len) {
    if (out_len) *out_len = 0;
    return nullptr;
}

// NEON variant falls through to the MASM stub (both are no-ops without real backends)
void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    return deflate_brutal_masm(src, len, out_len);
}

} // extern "C"
