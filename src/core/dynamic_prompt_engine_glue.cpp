// ============================================================================
// dynamic_prompt_engine_glue.cpp — Engine-Friendly C Glue for ASM Kernel
// ============================================================================
// Implements the additional C-ABI wrappers declared in dynamic_prompt_engine.hpp
// that Unity P/Invoke and Unreal Engine FFI callers use instead of raw ASM
// register-pair returns.
//
// These are thin wrappers — the heavy lifting is in the MASM64 kernel.
//
// Build: Include in RawrXD_DynamicPromptEngine SHARED library target.
// ============================================================================

// We ARE the DLL — export symbols
#define RAWRXD_PROMPT_ENGINE_EXPORTS 1

#include "dynamic_prompt_engine.hpp"

// ============================================================================
// Version Constants
// ============================================================================

#define RAWRXD_PROMPT_VERSION_MAJOR  1
#define RAWRXD_PROMPT_VERSION_MINOR  0
#define RAWRXD_PROMPT_VERSION_PATCH  0

// ============================================================================
// Static Mode Name Table
// ============================================================================

static const char* const s_ModeNames[RAWRXD_CTX_MODE_COUNT] = {
    "GENERIC",      // 0
    "CASUAL",       // 1
    "CODE",         // 2
    "SECURITY",     // 3
    "SHELL",        // 4
    "ENTERPRISE"    // 5
};

// ============================================================================
// C ABI Implementations
// ============================================================================

extern "C" {

RAWRXD_PROMPT_API RawrXD_ClassifyResult RAWRXD_PROMPT_CALL
PromptGen_ClassifyToStruct(const char* textPtr, size_t textLen)
{
    RawrXD_ClassifyResult result;

    // Call the raw ASM function which returns mode in EAX, score in EDX.
    // On Win64, the compiler will retrieve EAX as the return value.
    // EDX is a secondary return — we need to extract both.
    //
    // The ASM kernel sets EAX = mode, EDX = score before RET.
    // When declared as returning int64_t, MSVC packs EAX:EDX into RAX
    // (mode in lower 32, but EDX is clobbered into upper 32 by convention).
    //
    // Safest approach: call AnalyzeContext and unpack.
    int64_t raw = PromptGen_AnalyzeContext(textPtr, textLen);

    // Low 32 bits = mode (from EAX), high 32 bits = score (from EDX)
    result.mode  = static_cast<int32_t>(raw & 0xFFFFFFFF);
    result.score = static_cast<int32_t>((raw >> 32) & 0xFFFFFFFF);

    // Clamp mode to valid range
    if (result.mode < 0 || result.mode >= RAWRXD_CTX_MODE_COUNT) {
        result.mode = RAWRXD_CTX_MODE_GENERIC;
    }

    return result;
}

RAWRXD_PROMPT_API uint32_t RAWRXD_PROMPT_CALL
PromptGen_GetVersion(void)
{
    return (RAWRXD_PROMPT_VERSION_MAJOR << 16)
         | (RAWRXD_PROMPT_VERSION_MINOR << 8)
         | (RAWRXD_PROMPT_VERSION_PATCH);
}

RAWRXD_PROMPT_API const char* RAWRXD_PROMPT_CALL
PromptGen_GetModeName(int32_t mode)
{
    if (mode < 0 || mode >= RAWRXD_CTX_MODE_COUNT) {
        return nullptr;
    }
    return s_ModeNames[mode];
}

} // extern "C"
