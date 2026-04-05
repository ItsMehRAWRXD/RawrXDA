#pragma once

#include <stdint.h>

// Phase 7A: hot-path dispatcher helper for aperture seal verification.
// This keeps capability probing outside the hot path and allows A/B toggling
// through RAWRXD_INLINE_DISPATCH without altering safety semantics.
static __forceinline int RawrXD_DispatchSealVerifyInline(
    long mode,
    unsigned char avx512_supported,
    uint64_t mapped_base,
    uint64_t size_bytes,
    void* mapped_chunk_ptr) {
    if (mode == 2 && avx512_supported) {
        return Aperture_SealVerifyAvx512(mapped_base, size_bytes, mapped_chunk_ptr);
    }
    return Aperture_SealVerifyScalar(mapped_base, size_bytes, mapped_chunk_ptr);
}
