// ============================================================================
// local_parity_bridge.cpp — Local Parity Bridge (no API key)
// ============================================================================
// Connects C++ to RawrXD_LocalParity_Kernel.asm for zero-API Cursor/Copilot parity.
// Dynamically loads RawrXD_Interconnect.dll; provides SetModelPath and ManifestGet fallback.
// See: docs/LOCAL_PARITY_NO_API_KEY_SPEC.md
// ============================================================================

#include "../../include/local_parity_kernel.h"
#include <windows.h>
#include <cstring>
#include <string>

namespace {

static HMODULE s_hInterconnect = nullptr;
static std::string s_modelPath;

#define LP_FN(name) decltype(::name)* s_##name = nullptr

LP_FN(LocalParity_Init);
LP_FN(LocalParity_Shutdown);
LP_FN(LocalParity_RegisterInferenceCallback);
LP_FN(LocalParity_NextToken);
LP_FN(LocalParity_ManifestGet);
LP_FN(LocalParity_EncodeChunk);

static bool loadInterconnect() {
    if (s_hInterconnect) return true;
    s_hInterconnect = LoadLibraryA("RawrXD_Interconnect.dll");
    if (!s_hInterconnect) return false;
    s_LocalParity_Init = (decltype(s_LocalParity_Init))GetProcAddress(s_hInterconnect, "LocalParity_Init");
    s_LocalParity_Shutdown = (decltype(s_LocalParity_Shutdown))GetProcAddress(s_hInterconnect, "LocalParity_Shutdown");
    s_LocalParity_RegisterInferenceCallback = (decltype(s_LocalParity_RegisterInferenceCallback))GetProcAddress(s_hInterconnect, "LocalParity_RegisterInferenceCallback");
    s_LocalParity_NextToken = (decltype(s_LocalParity_NextToken))GetProcAddress(s_hInterconnect, "LocalParity_NextToken");
    s_LocalParity_ManifestGet = (decltype(s_LocalParity_ManifestGet))GetProcAddress(s_hInterconnect, "LocalParity_ManifestGet");
    s_LocalParity_EncodeChunk = (decltype(s_LocalParity_EncodeChunk))GetProcAddress(s_hInterconnect, "LocalParity_EncodeChunk");
    return s_LocalParity_Init && s_LocalParity_Shutdown && s_LocalParity_RegisterInferenceCallback && s_LocalParity_NextToken;
}

} // namespace

extern "C" {

int LocalParity_Init(void) {
    if (!loadInterconnect()) return 0;
    return s_LocalParity_Init();
}

void LocalParity_Shutdown(void) {
    if (s_LocalParity_Shutdown) s_LocalParity_Shutdown();
    if (s_hInterconnect) { FreeLibrary(s_hInterconnect); s_hInterconnect = nullptr; }
    s_LocalParity_Init = nullptr;
    s_LocalParity_Shutdown = nullptr;
    s_LocalParity_RegisterInferenceCallback = nullptr;
    s_LocalParity_NextToken = nullptr;
    s_LocalParity_ManifestGet = nullptr;
    s_LocalParity_EncodeChunk = nullptr;
}

void LocalParity_SetInferenceCallback(LocalParity_InferenceCallback fn) {
    if (loadInterconnect() && s_LocalParity_RegisterInferenceCallback)
        s_LocalParity_RegisterInferenceCallback(fn);
}

void LocalParity_RegisterInferenceCallback(LocalParity_InferenceCallback fn) {
    if (loadInterconnect() && s_LocalParity_RegisterInferenceCallback)
        s_LocalParity_RegisterInferenceCallback(fn);
}

void LocalParity_SetModelPath(const char* path) {
    s_modelPath = path ? path : "";
}

const char* LocalParity_GetModelPath(void) {
    return s_modelPath.empty() ? nullptr : s_modelPath.c_str();
}

int LocalParity_NextToken(void* ctx, void* modelState, uint32_t* outTokenId) {
    if (!loadInterconnect() || !s_LocalParity_NextToken) return 0;
    return s_LocalParity_NextToken(ctx, modelState, outTokenId);
}

int LocalParity_ManifestGet(const char* url, char* outBuf, uint64_t cap, uint64_t* outLen) {
    if (loadInterconnect() && s_LocalParity_ManifestGet) {
        int r = s_LocalParity_ManifestGet(url, outBuf, cap, outLen);
        if (r) return r;
    }
#ifdef _WIN32
    // Fallback: WinHTTP GET (no auth). Requires winhttp.lib
    if (!url || !outBuf || cap == 0 || !outLen) return 0;
    *outLen = 0;
    // Minimal WinHTTP implementation would go here; for now return 0 (stub)
#endif
    return 0;
}

uint64_t LocalParity_EncodeChunk(const char* text, uint64_t len, uint32_t* outIds, uint64_t maxIds) {
    if (!loadInterconnect() || !s_LocalParity_EncodeChunk) return 0;
    return s_LocalParity_EncodeChunk(text, len, outIds, maxIds);
}

} // extern "C"
