/*
 * gguf_dll_wrapper.c - DLL wrapper for GGUF loader
 * Exports GGUF functions for use in applications and tests
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward declare assembly functions */
extern void* GGUF_LoadModel(const char* path);
extern void GGUF_CloseModel(void* model);
extern int GGUF_ValidateModel(void* model);
extern int GGUF_GetTensorCount(void* model);
extern int GGUF_GetErrorCount(void);
extern int GGUF_GetLoadCount(void);
extern int GGUF_SetLogLevel(int level);
extern void GGUF_LogSystemInfo(void);
extern int GGUF_RunInference(void* model, const char* prompt, int maxTokens);
extern int GGUF_StreamTokens(void* model, const char* prompt, void* callback, void* userData);

/* DLL Entry Point */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            /* Initialize on first load */
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            /* Cleanup on unload */
            break;
    }
    return TRUE;
}

/* Export wrapper functions */
__declspec(dllexport) void* __cdecl GGUF_LoadModel_Export(const char* path)
{
    return GGUF_LoadModel(path);
}

__declspec(dllexport) void __cdecl GGUF_CloseModel_Export(void* model)
{
    GGUF_CloseModel(model);
}

__declspec(dllexport) int __cdecl GGUF_ValidateModel_Export(void* model)
{
    return GGUF_ValidateModel(model);
}

__declspec(dllexport) int __cdecl GGUF_GetTensorCount_Export(void* model)
{
    return GGUF_GetTensorCount(model);
}

__declspec(dllexport) int __cdecl GGUF_GetErrorCount_Export(void)
{
    return GGUF_GetErrorCount();
}

__declspec(dllexport) int __cdecl GGUF_GetLoadCount_Export(void)
{
    return GGUF_GetLoadCount();
}

__declspec(dllexport) int __cdecl GGUF_SetLogLevel_Export(int level)
{
    return GGUF_SetLogLevel(level);
}

__declspec(dllexport) void __cdecl GGUF_LogSystemInfo_Export(void)
{
    GGUF_LogSystemInfo();
}

__declspec(dllexport) int __cdecl GGUF_RunInference_Export(void* model, const char* prompt, int maxTokens)
{
    return GGUF_RunInference(model, prompt, maxTokens);
}

__declspec(dllexport) int __cdecl GGUF_StreamTokens_Export(void* model, const char* prompt, void* callback, void* userData)
{
    return GGUF_StreamTokens(model, prompt, callback, userData);
}
