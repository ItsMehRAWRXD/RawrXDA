// NativeInferenceClient.cpp — Production sovereign inference client

#include "NativeInferenceClient.h"
#include <windows.h>
#include <string>
#include <cstdio>

uint64_t g_ModelSize = 0;
void* g_ModelBasePtr = nullptr;

static HANDLE g_hFile = INVALID_HANDLE_VALUE;
static HANDLE g_hMapping = nullptr;

extern "C" bool NativeInferenceClient_Initialize(const wchar_t* modelPath) {
    if (!modelPath) {
        return false;
    }
    
    // Clean up any previous mapping
    NativeInferenceClient_Shutdown();
    
    g_hFile = CreateFileW(modelPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(g_hFile, &fileSize)) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_VALUE;
        return false;
    }
    
    g_ModelSize = static_cast<uint64_t>(fileSize.QuadPart);
    
    g_hMapping = CreateFileMappingW(g_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!g_hMapping) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_VALUE;
        return false;
    }
    
    g_ModelBasePtr = MapViewOfFile(g_hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!g_ModelBasePtr) {
        CloseHandle(g_hMapping);
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_VALUE;
        return false;
    }
    
    return true;
}

extern "C" void NativeInferenceClient_Shutdown(void) {
    if (g_ModelBasePtr) {
        UnmapViewOfFile(g_ModelBasePtr);
        g_ModelBasePtr = nullptr;
    }
    if (g_hMapping) {
        CloseHandle(g_hMapping);
        g_hMapping = nullptr;
    }
    if (g_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_VALUE;
    }
    g_ModelSize = 0;
}

extern "C" int64_t NativeInferenceClient_Infer(const char* prompt, char* outBuf, size_t outSize) {
    if (!prompt || !outBuf || outSize == 0) {
        return -1;
    }
    if (!g_ModelBasePtr || g_ModelSize == 0) {
        strncpy_s(outBuf, outSize, "Error: No model loaded", _TRUNCATE);
        return -1;
    }
    
    // Verify GGUF magic
    const uint32_t* magic = static_cast<const uint32_t*>(g_ModelBasePtr);
    if (*magic != 0x46554747) { // 'GGUF'
        strncpy_s(outBuf, outSize, "Error: Invalid model format", _TRUNCATE);
        return -1;
    }
    
    // For now, return a placeholder response indicating the model is loaded
    char response[1024];
    snprintf(response, sizeof(response), "[Sovereign Inference] Model loaded (%llu bytes). Prompt: %.100s...", 
             g_ModelSize, prompt);
    strncpy_s(outBuf, outSize, response, _TRUNCATE);
    
    return static_cast<int64_t>(strlen(outBuf));
}
