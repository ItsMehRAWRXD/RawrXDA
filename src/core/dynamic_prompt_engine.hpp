// ============================================================================
// dynamic_prompt_engine.hpp — C++ Bridge for RawrXD_DynamicPromptEngine.asm
// ============================================================================
// Provides type-safe wrappers around the MASM64 prompt generation kernel.
//
// Link with: RawrXD_DynamicPromptEngine.obj
// ============================================================================

#ifndef RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP
#define RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP

#include <cstdint>
#include <cstddef>

// ============================================================================
// Context Classification Modes (must match ASM constants)
// ============================================================================

enum class PromptMode : int32_t {
    Generic    = 0,   // Balanced analytical
    Casual     = 1,   // Slang-friendly, conversational
    Code       = 2,   // Technical code review
    Security   = 3,   // Vulnerability assessment
    Shell      = 4,   // Command analysis
    Enterprise = 5,   // Compliance-heavy corporate
    Count      = 6,

    Auto       = -1   // Used with ForceMode to re-enable auto-detection
};

// Template type for GetTemplate
enum class TemplateType : int32_t {
    Critic  = 0,
    Auditor = 1
};

// ============================================================================
// Classification result (unpacked from RAX:RDX return pair)
// ============================================================================

struct ClassifyResult {
    PromptMode mode;
    int32_t    score;       // Weighted match score for winning mode
};

// ============================================================================
// ASM Exports (extern "C" — Win64 calling convention)
// ============================================================================

extern "C" {

// Classify input text into a PromptMode.
// Returns mode in EAX, winning score in EDX.
//
// textPtr:  Pointer to input text (null-terminated)
// textLen:  Length in bytes (0 = auto-detect via lstrlenA)
int64_t PromptGen_AnalyzeContext(const char* textPtr, size_t textLen);

// Generate a complete Critic prompt from input context.
//
// contextPtr: Input context text (null-terminated)
// contextLen: Length (0 = auto-detect)
// outBuf:     Output buffer
// outSize:    Output buffer capacity in bytes
// Returns:    Bytes written (0 on overflow/error)
size_t PromptGen_BuildCritic(
    const char* contextPtr,
    size_t      contextLen,
    char*       outBuf,
    size_t      outSize
);

// Generate a complete Auditor prompt from input context.
// Same signature as BuildCritic.
size_t PromptGen_BuildAuditor(
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
size_t PromptGen_Interpolate(
    const char* templatePtr,
    const char* contextPtr,
    char*       outBuf,
    size_t      outSize
);

// Retrieve a raw template pointer by mode + type.
// Returns nullptr if mode or type is out of range.
const char* PromptGen_GetTemplate(int32_t mode, int32_t type);

// Set or clear the ForceMode override.
//   mode = 0..5  → force that classification
//   mode = -1    → return to auto-detection
// Returns: previous ForceMode value
int32_t PromptGen_ForceMode(int32_t mode);

} // extern "C"

// ============================================================================
// Inline C++ Wrappers (convenience layer)
// ============================================================================

namespace RawrXD {
namespace Prompt {

// Classify input and return structured result
inline ClassifyResult Classify(const char* text, size_t len = 0) {
    int64_t raw = PromptGen_AnalyzeContext(text, len);
    ClassifyResult r;
    r.mode  = static_cast<PromptMode>(static_cast<int32_t>(raw & 0xFFFFFFFF));
    r.score = static_cast<int32_t>((raw >> 32) & 0xFFFFFFFF);
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

} // namespace Prompt
} // namespace RawrXD

#endif // RAWRXD_DYNAMIC_PROMPT_ENGINE_HPP
