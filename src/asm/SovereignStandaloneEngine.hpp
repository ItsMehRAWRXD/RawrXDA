#pragma once
#include <cstdint>

// ============================================================================
// SovereignStandaloneEngine.hpp
// C++ bridge for SovereignStandaloneEngine.asm
// Zero CRT in MASM. Real Windows API memory mapping + KV cache.
// ============================================================================

extern "C" {

    // Initialize: memory-map GGUF weights + allocate 1GB KV cache
    // path = wide-char path to .gguf file
    // Returns true on success
    bool __stdcall Engine_Initialize(const wchar_t* path);

    // Inference: speculative decode loop
    // prompt = null-terminated UTF-8 prompt string
    // output = buffer to receive generated text (min 4096 bytes)
    // Returns number of tokens generated, or 0 on failure
    int __stdcall Engine_Infer_Speculative(const char* prompt, char* output);

    // Shutdown: release all resources
    void __stdcall Engine_Shutdown();

    // Batch inference: process multiple prompts simultaneously
    int __stdcall Engine_Infer_Batch(const char** prompts, char** outputs, int count);

    // Pre-map common memory ranges for performance
    void __stdcall PreMapCommonMemoryRanges(void);

    // Optimize memory mapping settings
    void __stdcall OptimizeMemoryMapping(void);

} // extern "C"
