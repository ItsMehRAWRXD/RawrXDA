// ============================================================================
// SovereignSpeculativeEngine_abi.h — MASM/C++ ABI contract for struct layouts
// Include in both C++ and ASM (via EXTRN) to guarantee layout parity.
// ============================================================================
#pragma once
#include <cstdint>
#include <cstddef>

// GenerationResult layout (must match MASM assumptions at +0 text, +32 tokens_generated, etc.)
struct alignas(8) SpecEngine_GenerationResult {
    // std::string text — MASM must treat as { ptr, size, capacity } or inline SSO
    // We use a fixed buffer layout for ABI stability:
    char        text_data[32];      // +0  SSO buffer or ptr (first 8 bytes overlap)
    uint64_t    text_size;          // +32
    uint64_t    text_capacity;      // +40
    int32_t     tokens_generated;   // +48
    int32_t     prompt_tokens;      // +52
    double      t_prompt_ms;        // +56
    double      t_gen_ms;           // +64
    uint8_t     success;            // +72
    char        error_msg[256];     // +73  Fixed buffer for error
};

static_assert(offsetof(SpecEngine_GenerationResult, text_size) == 32,
              "MASM text_size offset mismatch");
static_assert(offsetof(SpecEngine_GenerationResult, tokens_generated) == 48,
              "MASM tokens_generated offset mismatch");
static_assert(offsetof(SpecEngine_GenerationResult, success) == 72,
              "MASM success offset mismatch");
static_assert(sizeof(SpecEngine_GenerationResult) == 336,
              "MASM GenerationResult size mismatch");

// Bridge state layout (MASM .data section)
struct alignas(8) SpecEngine_State {
    uint64_t    hBridge;            // +0
    uint8_t     isLoaded;           // +8
    uint8_t     _pad[7];            // +9
    double      tokensPerSec;       // +16
    uint64_t    lastErrorPtr;       // +24
};

static_assert(offsetof(SpecEngine_State, isLoaded) == 8,
              "MASM isLoaded offset mismatch");
static_assert(offsetof(SpecEngine_State, tokensPerSec) == 16,
              "MASM tokensPerSec offset mismatch");
static_assert(sizeof(SpecEngine_State) == 32,
              "MASM State size mismatch");

// Function pointer types for MASM binding
extern "C" {
    typedef int32_t (*pfn_SpecEngine_Initialize)(const wchar_t* modelPath);
    typedef void    (*pfn_SpecEngine_Unload)(void);
    typedef int32_t (*pfn_SpecEngine_IsLoaded)(void);
    typedef void    (*pfn_SpecEngine_ClearKVCache)(void);
    typedef int32_t (*pfn_SpecEngine_Infer_Speculative)(
        const char* prompt,
        char* output,
        size_t max_out,
        int32_t* tok_count
    );
    typedef void    (*pfn_SpecEngine_SetKVQuant)(int32_t level);
    typedef double  (*pfn_SpecEngine_GetTokensPerSec)(void);
}
