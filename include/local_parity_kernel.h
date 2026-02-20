// ============================================================================
// local_parity_kernel.h — Local Parity Kernel ABI (no API key)
// ============================================================================
// x64 MASM kernel for same Cursor/GitHub parity without any API key.
// Use this path for local, agentic, and autonomous behavior (like Cursor/Copilot)
// with zero cloud or keys: inference = local GGUF + callback; manifest = WinHTTP GET, no auth.
// See: docs/LOCAL_PARITY_NO_API_KEY_SPEC.md
// ============================================================================

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------------
// One-time init (optional WinHTTP session for manifest). Returns 1 ok, 0 fail.
int LocalParity_Init(void);

// Release resources.
void LocalParity_Shutdown(void);

// ----------------------------------------------------------------------------
// Inference hot path (no API key). C++ registers callback; MASM calls it.
// ----------------------------------------------------------------------------
// Callback type: run one forward pass (GGUF), write next token to *outTokenId.
// Returns 1 if token produced, 0 if done/error.
typedef int (*LocalParity_InferenceCallback)(void* ctx, void* modelState, uint32_t* outTokenId);

// Register the C++ inference callback (GGUF forward pass). MASM kernel stores it.
void LocalParity_SetInferenceCallback(LocalParity_InferenceCallback fn);

// Bridge helpers (implemented in src/core/local_parity_bridge.cpp): register callback and set GGUF path.
void LocalParity_RegisterInferenceCallback(LocalParity_InferenceCallback fn);
void LocalParity_SetModelPath(const char* path);

// Single next-token step. ctx = opaque context, modelState = GGUF state, outTokenId = output.
// Returns 1 ok, 0 done/error.
int LocalParity_NextToken(void* ctx, void* modelState, uint32_t* outTokenId);

// ----------------------------------------------------------------------------
// Manifest fetch (no auth, no API key)
// ----------------------------------------------------------------------------
// WinHTTP GET to url; no Authorization header. Body -> outBuf, length -> *outLen.
// Returns 1 ok, 0 fail.
int LocalParity_ManifestGet(const char* url, char* outBuf, uint64_t cap, uint64_t* outLen);

// ----------------------------------------------------------------------------
// Optional: tokenizer hot path (in-process BPE, no network)
// ----------------------------------------------------------------------------
// Encode text into token IDs. Returns number of IDs written.
uint64_t LocalParity_EncodeChunk(const char* text, uint64_t len, uint32_t* outIds, uint64_t maxIds);

#ifdef __cplusplus
}
#endif
