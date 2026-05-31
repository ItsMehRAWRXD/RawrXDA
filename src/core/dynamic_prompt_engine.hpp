// ============================================================================
// dynamic_prompt_engine.hpp — C++ Bridge for RawrXD_DynamicPromptEngine.asm
// ============================================================================
// Provides type-safe wrappers around the MASM64 prompt generation kernel.
//
// Compatibility:
//   - Native C++ (MSVC / Clang)
//   - Unity Engine (via C# P/Invoke — see dynamic_prompt_engine_unity.cs)
//   - Unreal Engine 5 (via UObject wrapper — see DynamicPromptEngineUE.h)
//   - DLL export/import via RAWRXD_PROMPT_API macro
//
// Link with: RawrXD_DynamicPromptEngine.obj / RawrXD_DynamicPromptEngine.dll
// DEF file:  RawrXD_DynamicPromptEngine.def
// ============================================================================

#ifndef RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP
#define RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP

#include <cstdint>
#include <cstddef>

// ============================================================================
// DLL Export/Import Macro — Unity/Unreal/Native DLL Boundary
// ============================================================================
//
// Usage:
//   - Building the DLL:     define RAWRXD_PROMPT_ENGINE_EXPORTS=1
//   - Static linking:        define RAWRXD_PROMPT_ENGINE_STATIC=1
//   - Consuming the DLL:     define neither (auto-imports)
//
// This follows the same pattern as the MultiWindow Kernel DLL.
// ============================================================================

#if defined(RAWRXD_PROMPT_ENGINE_STATIC)
    // Static library — no decoration
    #define RAWRXD_PROMPT_API
#elif defined(RAWRXD_PROMPT_ENGINE_EXPORTS)
    // Building the DLL — export symbols
    #if defined(_MSC_VER)
        #define RAWRXD_PROMPT_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define RAWRXD_PROMPT_API __attribute__((visibility("default")))
    #else
        #define RAWRXD_PROMPT_API
    #endif
#else
    // Consuming the DLL — import symbols
    #if defined(_MSC_VER)
        #define RAWRXD_PROMPT_API __declspec(dllimport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define RAWRXD_PROMPT_API __attribute__((visibility("default")))
    #else
        #define RAWRXD_PROMPT_API
    #endif
#endif

// ============================================================================
// Calling Convention — Matches Win64 ABI (single convention on x64)
// Ensures Unity P/Invoke and Unreal extern "C" find the right symbols.
// ============================================================================

#if defined(_WIN64) || defined(_WIN32)
    #define RAWRXD_PROMPT_CALL __cdecl
#else
    #define RAWRXD_PROMPT_CALL
#endif

// ============================================================================
// Context Classification Modes (must match ASM constants)
// ============================================================================

// Plain C enum for maximum engine compatibility (no enum class for C interop)
// Use PromptMode enum class below for C++ callers.

// C-compatible constants (used by Unity P/Invoke and C FFI consumers)
#define RAWRXD_CTX_MODE_GENERIC     0
#define RAWRXD_CTX_MODE_CASUAL      1
#define RAWRXD_CTX_MODE_CODE        2
#define RAWRXD_CTX_MODE_SECURITY    3
#define RAWRXD_CTX_MODE_SHELL       4
#define RAWRXD_CTX_MODE_ENTERPRISE  5
#define RAWRXD_CTX_MODE_COUNT       6
#define RAWRXD_CTX_MODE_AUTO       (-1)

#define RAWRXD_TEMPLATE_CRITIC      0
#define RAWRXD_TEMPLATE_AUDITOR     1

#ifdef __cplusplus

enum class PromptMode : int32_t {
    Generic    = RAWRXD_CTX_MODE_GENERIC,
    Casual     = RAWRXD_CTX_MODE_CASUAL,
    Code       = RAWRXD_CTX_MODE_CODE,
    Security   = RAWRXD_CTX_MODE_SECURITY,
    Shell      = RAWRXD_CTX_MODE_SHELL,
    Enterprise = RAWRXD_CTX_MODE_ENTERPRISE,
    Count      = RAWRXD_CTX_MODE_COUNT,

    Auto       = RAWRXD_CTX_MODE_AUTO
};

enum class TemplateType : int32_t {
    Critic  = RAWRXD_TEMPLATE_CRITIC,
    Auditor = RAWRXD_TEMPLATE_AUDITOR
};

// ============================================================================
// Classification result (unpacked from RAX:RDX return pair)
// ============================================================================

struct ClassifyResult {
    PromptMode mode;
    int32_t    score;       // Weighted match score for winning mode
};

#endif // __cplusplus

// ============================================================================
// C-compatible result struct for engine FFI (Unity/Unreal/Mono)
// ============================================================================

#pragma pack(push, 8)
typedef struct RawrXD_ClassifyResult {
    int32_t mode;           // One of RAWRXD_CTX_MODE_* constants
    int32_t score;          // Weighted match score
} RawrXD_ClassifyResult;
#pragma pack(pop)

// ============================================================================
// ASM Exports (extern "C" — Win64 calling convention)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Classify input text into a context mode.
// Returns mode in EAX, winning score in EDX.
//
// textPtr:  Pointer to input text (null-terminated)
// textLen:  Length in bytes (0 = auto-detect via lstrlenA)
RAWRXD_PROMPT_API int64_t RAWRXD_PROMPT_CALL
    PromptGen_AnalyzeContext(const char* textPtr, size_t textLen);

// Generate a complete Critic prompt from input context.
//
// contextPtr: Input context text (null-terminated)
// contextLen: Length (0 = auto-detect)
// outBuf:     Output buffer
// outSize:    Output buffer capacity in bytes
// Returns:    Bytes written (0 on overflow/error)
RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
    PromptGen_BuildCritic(
        const char* contextPtr,
        size_t      contextLen,
        char*       outBuf,
        size_t      outSize
    );

// Generate a complete Auditor prompt from input context.
// Same signature as BuildCritic.
RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
    PromptGen_BuildAuditor(
        const char* contextPtr,
        size_t      contextLen,
        char*       outBuf,
        size_t      outSize
    );

// Interpolate {{CONTEXT}} markers in an arbitrary template.
//
// templatePtr: Template with {{CONTEXT}} markers
// contextPtr:  Replacement text
// outBuf:      Output buffer
// outSize:     Output buffer capacity
// Returns:     Bytes written (0 on overflow)
RAWRXD_PROMPT_API size_t RAWRXD_PROMPT_CALL
    PromptGen_Interpolate(
        const char* templatePtr,
        const char* contextPtr,
        char*       outBuf,
        size_t      outSize
    );

// Retrieve a raw template pointer by mode + type.
// Returns NULL if mode or type is out of range.
RAWRXD_PROMPT_API const char* RAWRXD_PROMPT_CALL
    PromptGen_GetTemplate(int32_t mode, int32_t type);

// Set or clear the ForceMode override.
//   mode = 0..5  → force that classification
//   mode = -1    → return to auto-detection
// Returns: previous ForceMode value
RAWRXD_PROMPT_API int32_t RAWRXD_PROMPT_CALL
    PromptGen_ForceMode(int32_t mode);

// ============================================================================
// Engine-Friendly Wrappers (C ABI — safe for Unity P/Invoke + Unreal FFI)
// ============================================================================

// Classify and return result in a struct (avoids register-pair unpacking)
// Unity/Unreal callers should prefer this over PromptGen_AnalyzeContext.
RAWRXD_PROMPT_API RawrXD_ClassifyResult RAWRXD_PROMPT_CALL
    PromptGen_ClassifyToStruct(const char* textPtr, size_t textLen);

// Version query for runtime compatibility checks
// Returns packed version: (major << 16) | (minor << 8) | patch
RAWRXD_PROMPT_API uint32_t RAWRXD_PROMPT_CALL
    PromptGen_GetVersion(void);

// Get human-readable mode name string (e.g., "CODE", "SHELL")
// Returns static string pointer — do NOT free. NULL on invalid mode.
RAWRXD_PROMPT_API const char* RAWRXD_PROMPT_CALL
    PromptGen_GetModeName(int32_t mode);

#ifdef __cplusplus
} // extern "C"
#endif

// ============================================================================
// C++ Wrappers (convenience layer — excluded from C/Unity/Unreal C-API)
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace Prompt {

// Classify input and return structured result
inline ClassifyResult Classify(const char* text, size_t len = 0) {
    RawrXD_ClassifyResult raw = PromptGen_ClassifyToStruct(text, len);
    ClassifyResult r;
    r.mode  = static_cast<PromptMode>(raw.mode);
    r.score = raw.score;
    return r;
}

// Generate critic prompt
inline size_t BuildCritic(const char* ctx, size_t len, char* out, size_t cap) {
    return PromptGen_BuildCritic(ctx, len, out, cap);
}

// Generate auditor prompt
inline size_t BuildAuditor(const char* ctx, size_t len, char* out, size_t cap) {
    return PromptGen_BuildAuditor(ctx, len, out, cap);
}

// Interpolate template
inline size_t Interpolate(const char* tmpl, const char* ctx, char* out, size_t cap) {
    return PromptGen_Interpolate(tmpl, ctx, out, cap);
}

// Get raw template pointer (null on invalid args)
inline const char* GetTemplate(PromptMode mode, TemplateType type) {
    return PromptGen_GetTemplate(
        static_cast<int32_t>(mode),
        static_cast<int32_t>(type)
    );
}

// Force classification mode (-1 for auto)
inline PromptMode ForceMode(PromptMode mode) {
    int32_t prev = PromptGen_ForceMode(static_cast<int32_t>(mode));
    return static_cast<PromptMode>(prev);
}

// Get mode name as string
inline const char* ModeName(PromptMode mode) {
    return PromptGen_GetModeName(static_cast<int32_t>(mode));
}

// Runtime version
inline uint32_t Version() { return PromptGen_GetVersion(); }

} // namespace Prompt
} // namespace RawrXD

#endif // __cplusplus

#endif // RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP
