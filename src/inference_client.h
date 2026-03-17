/**
 * inference_client.h - Minimal WinSock HTTP inference client
 * 
 * Direct connection to llama-server at 127.0.0.1:8081
 * Zero deps beyond ws2_32.dll (WinSock2)
 * 
 * Designed for MASM x64 interop - all functions use __stdcall (WINAPI)
 * Convention: RCX=param1, RDX=param2, R8=param3, R9=param4 (Win64 ABI)
 */
#ifndef INFERENCE_CLIENT_H
#define INFERENCE_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INFERENCE_CLIENT_EXPORTS
#define INFER_API __declspec(dllexport)
#else
#define INFER_API __declspec(dllimport)
#endif

/* ── Callback type for streaming tokens ───────────────────────────── */
/* Called once per decoded token. Return 0 to continue, non-zero to abort. */
typedef int (__stdcall *TokenCallback)(const char* token, int token_len, void* user_data);

/* ── Status codes ─────────────────────────────────────────────────── */
#define INFER_OK             0
#define INFER_ERR_WSASTARTUP 1
#define INFER_ERR_CONNECT    2
#define INFER_ERR_SEND       3
#define INFER_ERR_RECV       4
#define INFER_ERR_PARSE      5
#define INFER_ERR_ABORTED    6
#define INFER_ERR_TIMEOUT    7

/* ── Result structure (fixed layout for ASM access) ───────────────── */
#pragma pack(push, 8)
typedef struct InferResult {
    int32_t  status;           /* offset  0: INFER_OK or error code          */
    int32_t  prompt_tokens;    /* offset  4: tokens in prompt                */
    int32_t  completion_tokens;/* offset  8: tokens generated                */
    int32_t  _pad;             /* offset 12: alignment padding               */
    int64_t  elapsed_us;       /* offset 16: wall-clock microseconds         */
    char     text[4096];       /* offset 24: null-terminated response text   */
    char     error[256];       /* offset 4120: error message if status != 0  */
} InferResult;
#pragma pack(pop)

/* ── Configuration ────────────────────────────────────────────────── */
#pragma pack(push, 8)
typedef struct InferConfig {
    const char* host;          /* default: "127.0.0.1"                       */
    uint16_t    port;          /* default: 8081                              */
    uint16_t    _pad;
    int32_t     max_tokens;    /* default: 256                               */
    float       temperature;   /* default: 0.0                               */
    int32_t     timeout_ms;    /* recv timeout in ms, 0 = no timeout         */
} InferConfig;
#pragma pack(pop)

/* ── API Functions ────────────────────────────────────────────────── */

/**
 * Initialize WinSock. Call once at startup.
 * Returns INFER_OK on success.
 */
INFER_API int __stdcall Infer_Init(void);

/**
 * Cleanup WinSock. Call once at shutdown.
 */
INFER_API void __stdcall Infer_Shutdown(void);

/**
 * Blocking inference request.
 * Connects to llama-server, sends chat completion, collects full response.
 * 
 * @param prompt    UTF-8 user message
 * @param config    Configuration (NULL for defaults)
 * @param result    Output result structure
 * @return          INFER_OK on success
 */
INFER_API int __stdcall Infer_Complete(
    const char*        prompt,
    const InferConfig* config,
    InferResult*       result
);

/**
 * Streaming inference request.
 * Connects to llama-server, sends chat completion, calls callback per token.
 * 
 * @param prompt    UTF-8 user message
 * @param config    Configuration (NULL for defaults)
 * @param callback  Called for each decoded token
 * @param user_data Passed through to callback (e.g., HWND for RichEdit)
 * @param result    Output result structure (text accumulated, may be NULL)
 * @return          INFER_OK on success
 */
INFER_API int __stdcall Infer_Stream(
    const char*        prompt,
    const InferConfig* config,
    TokenCallback      callback,
    void*              user_data,
    InferResult*       result
);

/**
 * Get default configuration.
 * host=127.0.0.1, port=8081, max_tokens=256, temperature=0, timeout=30000
 */
INFER_API void __stdcall Infer_DefaultConfig(InferConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* INFERENCE_CLIENT_H */
