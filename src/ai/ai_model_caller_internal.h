#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(RAWRXD_TITAN_BRIDGE_EXPORTS)
#define RAWRXD_TITAN_BRIDGE_API __declspec(dllexport)
#else
#define RAWRXD_TITAN_BRIDGE_API
#endif

typedef struct RAWRXD_GGML_TITAN_BRIDGE {
    void* ggml_ctx;
    void* model_tensors;
    int n_layers;
    int n_embd;
    int n_head;
    int n_vocab;
    int n_ctx;
    char model_name[256];
    char architecture[64];
} RAWRXD_GGML_TITAN_BRIDGE;

#ifdef __cplusplus
extern "C" {
#endif

extern RAWRXD_TITAN_BRIDGE_API void* g_ggml_ctx;
extern RAWRXD_TITAN_BRIDGE_API void* g_model_tensors;
extern RAWRXD_TITAN_BRIDGE_API int g_n_layers;
extern RAWRXD_TITAN_BRIDGE_API int g_n_embd;
extern RAWRXD_TITAN_BRIDGE_API int g_n_head;
extern RAWRXD_TITAN_BRIDGE_API int g_n_vocab;
extern RAWRXD_TITAN_BRIDGE_API int g_n_ctx;

RAWRXD_TITAN_BRIDGE_API int RawrXD_GetGGMLTitanBridge(RAWRXD_GGML_TITAN_BRIDGE* out_bridge);

#ifdef __cplusplus
}
#endif