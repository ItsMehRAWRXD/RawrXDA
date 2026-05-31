// =============================================================================
// model_invoker.hpp — Sovereign Model Invoker (C++ header, zero deps)
// Replaces JSON-based model_invoker.hpp with raw tensor dispatch via MASM
// =============================================================================
// Usage:
//   #include "model_invoker.hpp"
//   std::vector<int32_t> tokens(512);
//   size_t n = ModelInvoker_PrepareContext("Hello world", tokens.data(), tokens.size());
//   char out[4096];
//   int64_t written = ModelInvoker_Invoke(tokens.data(), n, out, sizeof(out));
// =============================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Sovereign Tokenization + Inference (C linkage, implemented in model_invoker.asm)
// ---------------------------------------------------------------------------

/// Zero-dependency whitespace tokenizer. Produces simple hash-based token IDs.
/// @param prompt    Null-terminated input string.
/// @param tokenIds  Output array (caller-allocated, int32_t).
/// @param maxTokens Capacity of tokenIds array.
/// @return Number of tokens produced, or -1 on error.
int64_t ModelInvoker_PrepareContext(const char* prompt, int32_t* tokenIds, size_t maxTokens);

/// Dispatch inference against memory-mapped weights via MASM tensor kernels.
/// @param tokenIds   Array of token IDs from PrepareContext.
/// @param tokenCount Number of tokens.
/// @param outBuf     Output buffer (caller-allocated).
/// @param outSize    Size of outBuf in bytes.
/// @return Bytes written on success, -1 on failure.
int64_t ModelInvoker_Invoke(int32_t* tokenIds, size_t tokenCount, char* outBuf, size_t outSize);

// ---------------------------------------------------------------------------
// Telemetry (populated by Invoke)
// ---------------------------------------------------------------------------

extern uint64_t g_LastInvokeCycles;   ///< Cycle count of last Invoke call.
extern uint64_t g_LastTokenCount;     ///< Token count of last PrepareContext call.

#ifdef __cplusplus
} // extern "C"

// ---------------------------------------------------------------------------
// Optional C++ RAII wrapper (header-only, no deps)
// ---------------------------------------------------------------------------
namespace RawrXD {
namespace Agent {

class SovereignModelInvoker {
public:
    /// Tokenize prompt into token IDs.
    static size_t prepareContext(const char* prompt, int32_t* tokenIds, size_t maxTokens) {
        int64_t n = ModelInvoker_PrepareContext(prompt, tokenIds, maxTokens);
        return (n > 0) ? static_cast<size_t>(n) : 0;
    }

    /// Run inference and return bytes written.
    static int64_t invoke(int32_t* tokenIds, size_t tokenCount, char* outBuf, size_t outSize) {
        return ModelInvoker_Invoke(tokenIds, tokenCount, outBuf, outSize);
    }

    /// Convenience: tokenize + infer in one call.
    static int64_t generate(const char* prompt, char* outBuf, size_t outSize,
                            int32_t* tokenScratch = nullptr, size_t scratchSize = 0) {
        int32_t localTokens[512];
        int32_t* tokens = tokenScratch ? tokenScratch : localTokens;
        size_t maxTokens = tokenScratch ? scratchSize : 512;

        size_t n = prepareContext(prompt, tokens, maxTokens);
        if (n == 0) return -1;
        return invoke(tokens, n, outBuf, outSize);
    }

    static uint64_t lastCycles() { return g_LastInvokeCycles; }
    static uint64_t lastTokenCount() { return g_LastTokenCount; }
};

} // namespace Agent
} // namespace RawrXD

#endif // __cplusplus
