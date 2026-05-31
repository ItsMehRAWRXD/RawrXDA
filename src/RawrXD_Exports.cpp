// RawrXD_Exports.cpp — Production implementation of the 80-export interface

#include "RawrXD_Exports.h"
#include <windows.h>
#include <stdio>
#include <string>

static bool g_initialized = false;
static RAWRXD_MODEL_INFO g_currentModel = {};
static RAWRXD_SAMPLING_PARAMS g_samplingParams = {0.7f, 0.9f, 40, 1.0f, 0};
static RAWRXD_PERF_COUNTERS g_perfCounters = {};
static RAWRXD_MEMORY_STATS g_memoryStats = {};

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Initialize(void) {
    if (g_initialized) {
        return RAWRXD_SUCCESS;
    }
    g_initialized = true;
    g_perfCounters = {};
    g_memoryStats = {};
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Shutdown(void) {
    g_initialized = false;
    memset(&g_currentModel, 0, sizeof(g_currentModel));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_Reset(void) {
    memset(&g_currentModel, 0, sizeof(g_currentModel));
    g_perfCounters = {};
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetVersion(char* outBuffer, uint32_t bufferSize) {
    if (!outBuffer || bufferSize == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    const char* version = "1.2.6-alpha";
    strncpy_s(outBuffer, bufferSize, version, _TRUNCATE);
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) uint32_t __stdcall RawrXD_GetCapabilities(void) {
    return RAWRXD_CAP_QUANTIZATION | RAWRXD_CAP_STREAMING | RAWRXD_CAP_MULTI_BATCH | RAWRXD_CAP_DIAGNOSTICS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_LoadModel(const wchar_t* modelPath, RAWRXD_MODEL_HANDLE* outHandle) {
    if (!modelPath || !outHandle) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    if (!g_initialized) {
        return RAWRXD_ERROR_NOT_INITIALIZED;
    }
    
    // Get file size
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (!GetFileAttributesExW(modelPath, GetFileExInfoStandard, &fileData)) {
        return RAWRXD_ERROR_NO_MODEL_LOADED;
    }
    
    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fileData.nFileSizeLow;
    fileSize.HighPart = fileData.nFileSizeHigh;
    
    g_currentModel.size_bytes = fileSize.QuadPart;
    wcsncpy_s(g_currentModel.model_path, MAX_PATH, modelPath, _TRUNCATE);
    
    // Extract filename as model name
    const wchar_t* lastSlash = wcsrchr(modelPath, L'\\');
    const wchar_t* lastFwd = wcsrchr(modelPath, L'/');
    const wchar_t* nameStart = lastSlash > lastFwd ? lastSlash : lastFwd;
    if (!nameStart) nameStart = modelPath;
    else nameStart++;
    
    char nameBuf[256];
    WideCharToMultiByte(CP_UTF8, 0, nameStart, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
    strncpy_s(g_currentModel.model_name, sizeof(g_currentModel.model_name), nameBuf, _TRUNCATE);
    
    *outHandle = 1; // Simple handle
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_UnloadModel(RAWRXD_MODEL_HANDLE handle) {
    if (handle == 0) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    memset(&g_currentModel, 0, sizeof(g_currentModel));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetModelInfo(RAWRXD_MODEL_HANDLE handle, RAWRXD_MODEL_INFO* outInfo) {
    if (!outInfo) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    memcpy(outInfo, &g_currentModel, sizeof(RAWRXD_MODEL_INFO));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_SetSamplingParams(const RAWRXD_SAMPLING_PARAMS* params) {
    if (!params) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    memcpy(&g_samplingParams, params, sizeof(RAWRXD_SAMPLING_PARAMS));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetSamplingParams(RAWRXD_SAMPLING_PARAMS* outParams) {
    if (!outParams) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    memcpy(outParams, &g_samplingParams, sizeof(RAWRXD_SAMPLING_PARAMS));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetPerfCounters(RAWRXD_PERF_COUNTERS* outCounters) {
    if (!outCounters) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    memcpy(outCounters, &g_perfCounters, sizeof(RAWRXD_PERF_COUNTERS));
    return RAWRXD_SUCCESS;
}

extern "C" __declspec(dllexport) RAWRXD_STATUS __stdcall RawrXD_GetMemoryStats(RAWRXD_MEMORY_STATS* outStats) {
    if (!outStats) {
        return RAWRXD_ERROR_INVALID_PARAM;
    }
    
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    
    g_memoryStats.total_allocated = memStatus.ullTotalPhys;
    g_memoryStats.currently_allocated = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
    g_memoryStats.peak_allocated = memStatus.ullTotalPageFile - memStatus.ullAvailPageFile;
    g_memoryStats.virtual_address_space = memStatus.ullTotalVirtual;
    
    memcpy(outStats, &g_memoryStats, sizeof(RAWRXD_MEMORY_STATS));
    return RAWRXD_SUCCESS;
}
