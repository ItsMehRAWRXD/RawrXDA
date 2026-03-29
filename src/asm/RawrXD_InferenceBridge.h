// RawrXD_InferenceBridge.h
// Include this to replace your stub inference API

#pragma once
#include <windows.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handles
typedef struct InferenceContext* HINFERENCE;
typedef struct GGUFStreamingContext* HGGUFSTREAM;

// Error codes
#define RAW_OK              0
#define RAW_ERR_NOMEM       0x00000008   // matches Windows ERROR_NOT_ENOUGH_MEMORY (0x8)
#define RAW_ERR_NOBACKEND   0xC0000001
#define RAW_ERR_INVALID     0xC0000002

// Sampling parameters
typedef struct {
    float temperature;
    float topP;
    int   topK;
    float repeatPenalty;
    int   maxNewTokens;
} GenerationParams;

// Streaming callback
// Called by InferenceEngine_SubmitInference for each token generated while streaming.
// Parameters:
//   - token:        The generated token identifier (e.g., vocabulary index).
//   - probability:  The model's estimated probability or normalized score for this
//                   token in the range [0.0f, 1.0f], where higher values indicate
//                   more likely tokens.
//   - isLast:       true if this is the final callback for the current inference
//                   request (no further tokens will be reported for that request);
//                   false otherwise.
//   - userData:     The opaque pointer provided to InferenceEngine_SubmitInference;
//                   can be used to carry caller-specific context/state.
// Threading:
//   The callback may be invoked from internal worker threads and may be called
//   concurrently for different HINFERENCE contexts. Implementations MUST be
//   thread-safe and should avoid long-running or blocking operations to prevent
//   stalling the inference engine.
// Return value / error handling:
//   This callback returns void; it MUST NOT attempt to signal errors, cancellation,
//   or flow-control decisions via a return value. Any such signaling must be done
//   indirectly, for example by updating fields reachable through userData or by
//   using other thread-safe mechanisms understood by the caller and callee.
typedef void (*TokenCallback)(int32_t token, float probability, 
                            bool isLast, void* userData);

// Core API
__declspec(dllimport) HINFERENCE InferenceEngine_CreateContext(
    void* modelHandle, 
    int maxTokens, 
    uint32_t flags
);

__declspec(dllimport) int InferenceEngine_SubmitInference(
    HINFERENCE ctx,
    const int32_t* promptTokens,
    int promptCount,
    const GenerationParams* params,
    TokenCallback callback,
    void* userData
);

__declspec(dllimport) int InferenceEngine_GetResult(
    HINFERENCE ctx,
    int32_t* outputBuffer,
    int maxTokens,
    int* actualTokens
);

__declspec(dllimport) void InferenceEngine_DestroyContext(HINFERENCE ctx);

// Streaming GGUF loader (fixes ERROR_NOT_ENOUGH_MEMORY)
__declspec(dllimport) HGGUFSTREAM GGUF_LoadModel_Streaming(
    const wchar_t* filePath,
    uint32_t flags
);

__declspec(dllimport) void* GGUF_GetTensor_Streaming(
    HGGUFSTREAM ctx,
    int tensorIndex,
    void* outputBuffer
);

__declspec(dllimport) void GGUF_UnloadModel_Streaming(HGGUFSTREAM ctx);

// Flags
#define GGUF_FLAG_FORCE_CHUNKED   0x0001
#define INFERENCE_FLAG_STREAMING    0x0001

#ifdef __cplusplus
}
#endif 