#pragma once
#include <windows.h>

// Pure Win64 Sidebar Engine - Replaces Qt Logging + Eliminates Stubs
// C++ Linkage for MASM64 sidebar implementation

extern "C" {
    // Logging replacement for qDebug/qInfo/qWarning/qCritical
    // pszString: Message to log
    // dwLevel: 0=Info, 1=Warn, 2=Error, 3=Critical
    void LogWrite(const char* pszString, DWORD dwLevel);
    
    // Real debug engine - Attaches to process and wires debug events to tree view
    // dwProcessId: Process ID to attach debugger to
    // hwndTree: TreeView control to populate with debug variables
    void DebugEngineAttach(DWORD dwProcessId, HWND hwndTree);
    
    // Lazy tree loader - Asynchronous directory population
    // hwndTree: TreeView control handle
    // pszPath: Root path to populate from
    // bAsync: TRUE for background thread, FALSE for synchronous
    void TreeLazyLoad(HWND hwndTree, LPCSTR pszPath, BOOL bAsync);
    
    // Dark mode enforcer - Uses DWM and SetWindowTheme
    // hwnd: Window handle to apply dark mode to
    void ForceDarkMode(HWND hwnd);
}
