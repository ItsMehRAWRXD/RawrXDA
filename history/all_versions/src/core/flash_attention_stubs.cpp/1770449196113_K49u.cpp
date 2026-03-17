// ============================================================================
// flash_attention_stubs.cpp — Stub implementations for FlashAttention ASM symbols
// ============================================================================
// These stubs resolve linker errors when the AVX-512 ASM kernel
// (FlashAttention_AVX512.asm) is not compiled/linked. The Initialize()
// path in flash_attention.cpp will correctly report AVX-512 as unavailable.
// ============================================================================

#include <cstdint>

extern "C" {

int32_t FlashAttention_CheckAVX512() {
    return 0;  // Not available (stub)
}

int32_t FlashAttention_Init() {
    return 0;  // Not available (stub)
}

namespace RawrXD {
struct FlashAttentionConfig;
struct FlashAttentionTileConfig;
}

int32_t FlashAttention_Forward(RawrXD::FlashAttentionConfig* /*cfg*/) {
    return -1;  // Error: AVX-512 not available (stub)
}

int32_t FlashAttention_GetTileConfig(RawrXD::FlashAttentionTileConfig* out) {
    if (out) {
        auto* p = reinterpret_cast<int32_t*>(out);
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
    }
    return 1;
}

uint64_t g_FlashAttnCalls = 0;
uint64_t g_FlashAttnTiles = 0;

} // extern "C"
