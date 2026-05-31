// =============================================================================
// NativeInferenceClient.h — Sovereign In-Process Inference Client (C API)
// Pure C/C++ header — ZERO network dependencies, ZERO WinSock, ZERO HTTP
// Replaces socket-based NativeInferenceClient with direct memory-mapped inference
// =============================================================================
// Usage:
//   #include "NativeInferenceClient.h"
//   bool ok = NativeInferenceClient_Initialize(L"models\\deep_thinker_v1.gguf");
//   int64_t written = NativeInferenceClient_Infer("Hello", outBuf, sizeof(outBuf));
//   NativeInferenceClient_Shutdown();
// =============================================================================

#pragma once

#include "AgentOllamaClient.h"
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Sovereign Lifecycle
// ---------------------------------------------------------------------------

/// Load a model weight file via CreateFileMapping / MapViewOfFile.
/// @param modelPath Wide-char path to GGUF or raw weight file.
/// @return true if mapped successfully.
bool NativeInferenceClient_Initialize(const wchar_t* modelPath);

/// Release mapping handles and zero state.
void NativeInferenceClient_Shutdown(void);

// ---------------------------------------------------------------------------
// Sovereign Inference
// ---------------------------------------------------------------------------

/// Run inference directly against the memory-mapped weights.
/// @param prompt    Null-terminated prompt string.
/// @param outBuf    Output buffer (caller-allocated).
/// @param outSize   Size of outBuf in bytes.
/// @return Bytes written on success, -1 on failure.
int64_t NativeInferenceClient_Infer(const char* prompt, char* outBuf, size_t outSize);

// ---------------------------------------------------------------------------
// State Introspection (optional telemetry)
// ---------------------------------------------------------------------------

extern uint64_t g_ModelSize;      ///< Mapped file size in bytes.
extern void*    g_ModelBasePtr;   ///< Base of mapped view (read-only).

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
