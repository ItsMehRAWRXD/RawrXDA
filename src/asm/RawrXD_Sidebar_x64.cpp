// RawrXD_Sidebar_x64.cpp — Production sidebar x64 implementation

#include "RawrXD_Sidebar_x64.h"
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <string>
#include <cstdio>
#include <fstream>

static std::ofstream g_logFile;
static char g_logFilename[256] = "rawrxd_sidebar.log";

extern "C" void RawrXD_Logger_Init(const char* filename) {
    if (filename) {
        strncpy_s(g_logFilename, sizeof(g_logFilename), filename, _TRUNCATE);
    }
    if (g_logFile.is_open()) {
        g_logFile.close();
    }
    g_logFile.open(g_logFilename, std::ios::app);
}

extern "C" void RawrXD_Logger_Write(const char* level, const char* file, 
                                       unsigned int line, const char* msg) {
    char buf[1024];
    DWORD tick = GetTickCount();
    DWORD pid = GetCurrentProcessId();
    
    snprintf(buf, sizeof(buf), "[RAWRXD] %08X %lu %s %s:%u %s\r\n",
             tick, pid, level, file, line, msg);
    
    OutputDebugStringA(buf);
    
    if (g_logFile.is_open()) {
        g_logFile << buf;
        g_logFile.flush();
    }
}

extern "C" BOOL RawrXD_Debug_Attach(DWORD dwProcessId) {
    return DebugActiveProcess(dwProcessId) ? TRUE : FALSE;
}

extern "C" BOOL RawrXD_Debug_Wait(void* lpDebugEvent, DWORD dwMilliseconds) {
    if (!lpDebugEvent) return FALSE;
    return WaitForDebugEvent(static_cast<DEBUG_EVENT*>(lpDebugEvent), dwMilliseconds) ? TRUE : FALSE;
}

extern "C" void RawrXD_Debug_Step(HANDLE hThread, void* pContext) {
    if (!hThread || !pContext) return;
    
    CONTEXT* ctx = static_cast<CONTEXT*>(pContext);
    ctx->EFlags |= 0x100; // Set trap flag
    SetThreadContext(hThread, ctx);
}

extern "C" void RawrXD_Tree_LazyLoad(HWND hWndTree) {
    if (!hWndTree || !IsWindow(hWndTree)) return;
    
    DWORD style = TreeView_GetExtendedStyle(hWndTree);
    style |= TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS;
    TreeView_SetExtendedStyle(hWndTree, style, TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS);
}

extern "C" void RawrXD_DarkMode_Force(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;
    
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
}
