#pragma once
#include <windows.h>
extern "C" {
    __declspec(dllimport) void* Genesis_VulkanCompute_Init(void* vkInstance);
    __declspec(dllimport) bool Genesis_ExtensionHost_Create(HWND parent);
    __declspec(dllimport) void Genesis_WhiteScreenGuard_StartMonitoring(HWND hwnd);
    __declspec(dllimport) void Genesis_WhiteScreenGuard_Ping();
    __declspec(dllimport) void Genesis_WhiteScreenGuard_ForceRepaint(HWND hwnd);
    __declspec(dllimport) int Genesis_SelfHosting_CompileASM(const char* sourcePath, const char* objPath);
    __declspec(dllimport) int Genesis_SelfHosting_LinkEXE(const char* objPath, const char* exePath);
    __declspec(dllimport) int Genesis_SelfHosting_Verify(const char* exePath);
    __declspec(dllimport) int Genesis_AiBackendBridge_Init();
    __declspec(dllimport) int Genesis_AiBackendBridge_SendPrompt(const char* prompt, char* responseBuffer, size_t bufferSize);
    __declspec(dllimport) int Genesis_AiBackendBridge_StreamResponse(void* callbackProc);
    __declspec(dllimport) int Genesis_BuildOrchestrator_Init(size_t maxConcurrent);
    __declspec(dllimport) int Genesis_BuildOrchestrator_AddJob(void* jobProc, void* param);
    __declspec(dllimport) void Genesis_BuildOrchestrator_WaitAll();
}