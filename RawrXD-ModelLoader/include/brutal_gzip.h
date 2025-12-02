#pragma once
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Brutal deflate (stored blocks only) - x64 MASM
// Returns a malloc-ed buffer that must be freed by the caller.
// out_len is set to the size of the compressed data.
void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

// Brutal deflate (stored blocks only) - ARM64 NEON
// Returns a malloc-ed buffer that must be freed by the caller.
// out_len is set to the size of the compressed data.
void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);

#ifdef __cplusplus
}
#endif
