// ============================================================================
// flash_attention_stubs.cpp — Stub implementations for FlashAttention ASM symbols
// ============================================================================
// When the MASM FlashAttention_AVX512.asm module is not assembled/linked,
// these stubs provide safe fallback implementations that:
//   - Report no AVX-512 support
//   - Return safe defaults (no-ops)
//   - Never crash
//
// Once the ASM module is assembled and linked, the linker will
// prefer the real implementations over these stubs.
// ============================================================================

#include <cstdint>
#include <cstring>

struct FlashAttentionConfig;
struct FlashAttentionTileConfig;

extern "C" {

uint64_t g_FlashAttnCalls = 0;
uint64_t g_FlashAttnTiles = 0;

int32_t FlashAttention_CheckAVX512() {
    return 0; // AVX-512 not available (stub)
}

int32_t FlashAttention_Init() {
    return 0; // Not ready — stub (no ASM backend)
}

int32_t FlashAttention_Forward(FlashAttentionConfig* /*cfg*/) {
    return -1; // Fail — no ASM backend
}

int32_t FlashAttention_GetTileConfig(FlashAttentionTileConfig* out) {
    if (out) std::memset(out, 0, 16); // Zero-fill 16-byte struct
    return 1;
}

} // extern "C"
