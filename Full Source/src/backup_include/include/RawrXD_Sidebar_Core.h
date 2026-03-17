// RawrXD Sidebar Core API - Auto-generated
#pragma once
#include <windows.h>

extern "C" {
    __declspec(dllimport) void RawrXD_LogMessage(const char* msg, const char* file, int line, int level);
    __declspec(dllimport) bool RawrXD_InitLogger(void);
    __declspec(dllimport) void RawrXD_CloseLogger(void);
    __declspec(dllimport) bool RawrXD_DebugEngineCreate(const char* exePath, const char* args);
    __declspec(dllimport) bool RawrXD_DebugEngineLoop(void);
    __declspec(dllimport) void RawrXD_DebugEngineStop(void);
    __declspec(dllimport) HWND RawrXD_InitVirtualTree(HWND hParent, int x, int y, int w, int h);
    __declspec(dllimport) void RawrXD_TreeQueuePopulate(const char* path);
    __declspec(dllimport) void RawrXD_InitSidebarResize(HWND hSidebar);
    __declspec(dllimport) bool RawrXD_SetDarkMode(HWND hWnd, bool enable);
    __declspec(dllimport) int RawrXD_ExecGitCommand(const char* args, char* output, int outputSize);
}

// Macros to replace Qt logging
#define RAWRXD_LOG(msg) RawrXD_LogMessage(msg, __FILE__, __LINE__, 0)
#define RAWRXD_DEBUG(msg) RawrXD_LogMessage(msg, __FILE__, __LINE__, 1)