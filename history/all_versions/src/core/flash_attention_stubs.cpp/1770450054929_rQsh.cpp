// ============================================================================
// flash_attention_stubs.cpp — Stub implementations for FlashAttention ASM symbols
// ============================================================================
// These stubs resolve linker errors when the AVX-512 ASM kernel
// (FlashAttention_AVX512.asm) is not compiled/linked. The Initialize()
// path in flash_attention.cpp will correctly report AVX-512 as unavailable.
// ============================================================================
// IMPORTANT: flash_attention.h declares extern "C" symbols INSIDE
// namespace RawrXD. We must include the header and provide definitions
// inside the same namespace to get matching symbol names.
// ============================================================================

#include "flash_attention.h"

namespace RawrXD {

extern "C" {

int32_t FlashAttention_CheckAVX512() {
    return 0;  // Not available (stub)
}

int32_t FlashAttention_Init() {
    return 0;  // Not available (stub)
}

int32_t FlashAttention_Forward(FlashAttentionConfig* /*cfg*/) {
    return -1;  // Error: AVX-512 not available (stub)
}

int32_t FlashAttention_GetTileConfig(FlashAttentionTileConfig* out) {
    if (out) {
        out->tileM        = 0;
        out->tileN        = 0;
        out->headDim      = 0;
        out->scratchBytes = 0;
    }
    return 1;
}

uint64_t g_FlashAttnCalls = 0;
uint64_t g_FlashAttnTiles = 0;

} // extern "C"

} // namespace RawrXD
