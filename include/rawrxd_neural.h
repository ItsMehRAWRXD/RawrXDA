#ifndef RAWRXD_NEURAL_H
#define RAWRXD_NEURAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct _RawrXD_Model RawrXD_Model;
typedef void (__stdcall *RAWRXD_PROGRESS_CB)(RawrXD_Model* ctx, int stage, int percent, const char* msg);

// Model loading
__declspec(dllimport) RawrXD_Model* __stdcall RawrXD_ShowUploaderDialog(HWND hParent, DWORD flags);
__declspec(dllimport) BOOL __stdcall RawrXD_LoadModel(RawrXD_Model* ctx);
__declspec(dllimport) void __stdcall RawrXD_UnloadModel(RawrXD_Model* ctx);

// Tokenization
__declspec(dllimport) int __stdcall RawrXD_Tokenize(RawrXD_Model* ctx, const char* text, int* tokens, int maxTokens);
__declspec(dllimport) int __stdcall RawrXD_Detokenize(RawrXD_Model* ctx, const int* tokens, int nTokens, char* buffer, int bufSize);

// Inference
typedef struct {
    float temperature;
    float top_p;
    int top_k;
    float repeat_penalty;
    unsigned int seed;
    int max_tokens;
    BOOL stream_output;
} RawrXD_Params;

__declspec(dllimport) int __stdcall RawrXD_Inference(RawrXD_Model* ctx, const int* promptTokens, int nPrompt, 
    const RawrXD_Params* params, int* outputTokens, int maxOutput);

// UI Integration
__declspec(dllimport) void __stdcall RawrXD_RegisterDragDrop(HWND hWnd);
__declspec(dllimport) void __stdcall RawrXD_CancelOperation(RawrXD_Model* ctx);

// Stats
typedef struct {
    double tps;              // Tokens per second
    size_t memoryUsed;
    size_t memoryPeak;
    int nThreads;
    const char* modelArch;
} RawrXD_Stats;

__declspec(dllimport) void __stdcall RawrXD_GetStats(RawrXD_Model* ctx, RawrXD_Stats* stats);

#ifdef __cplusplus
}
#endif

#endif // RAWRXD_NEURAL_H
