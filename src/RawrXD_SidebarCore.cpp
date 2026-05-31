// RawrXD_SidebarCore.cpp — Production sidebar core implementation

#include "RawrXD_SidebarCore.h"
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <string>
#include <stdio>

static HWND g_hwndTree = nullptr;
static char g_logBuffer[4096] = {};

extern "C" void LogWrite(const char* pszString, DWORD dwLevel) {
    if (!pszString) return;
    
    const char* prefix = "[INFO]";
    switch (dwLevel) {
        case 1: prefix = "[WARN]"; break;
        case 2: prefix = "[ERROR]"; break;
        case 3: prefix = "[CRIT]"; break;
    }
    
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s %s\n", prefix, pszString);
    
    // Output to debug
    OutputDebugStringA(buf);
    
    // Append to log buffer
    strncat_s(g_logBuffer, sizeof(g_logBuffer), buf, _TRUNCATE);
}

extern "C" void DebugEngineAttach(DWORD dwProcessId, HWND hwndTree) {
    if (!hwndTree || !IsWindow(hwndTree)) {
        LogWrite("Invalid tree view handle", 2);
        return;
    }
    
    g_hwndTree = hwndTree;
    
    // Open process for debugging
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
    if (!hProcess) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Failed to open process %lu", dwProcessId);
        LogWrite(buf, 2);
        return;
    }
    
    // Clear tree
    TreeView_DeleteAllItems(hwndTree);
    
    // Add root item
    TVINSERTSTRUCTA tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    char procName[256] = "Process";
    tvis.item.pszText = procName;
    HTREEITEM hRoot = TreeView_InsertItem(hwndTree, &tvis);
    
    // Enumerate modules
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        DWORD modCount = cbNeeded / sizeof(HMODULE);
        for (DWORD i = 0; i < modCount && i < 50; i++) {
            char modName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, hMods[i], modName, sizeof(modName))) {
                TVINSERTSTRUCTA modTvis = {};
                modTvis.hParent = hRoot;
                modTvis.hInsertAfter = TVI_LAST;
                modTvis.item.mask = TVIF_TEXT;
                modTvis.item.pszText = modName;
                TreeView_InsertItem(hwndTree, &modTvis);
            }
        }
    }
    
    CloseHandle(hProcess);
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Debug engine attached to PID %lu", dwProcessId);
    LogWrite(buf, 0);
}

extern "C" void TreeLazyLoad(HWND hwndTree, LPCSTR pszPath, BOOL bAsync) {
    if (!hwndTree || !IsWindow(hwndTree) || !pszPath) {
        LogWrite("Invalid parameters for TreeLazyLoad", 2);
        return;
    }
    
    if (bAsync) {
        // For async, we'd spawn a thread. For now, do synchronous.
        LogWrite("Async tree load requested — running synchronous fallback", 1);
    }
    
    TreeView_DeleteAllItems(hwndTree);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", pszPath);
    
    hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Failed to enumerate: %s", pszPath);
        LogWrite(buf, 2);
        return;
    }
    
    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        TVINSERTSTRUCTA tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = findData.cFileName;
        TreeView_InsertItem(hwndTree, &tvis);
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

extern "C" void ForceDarkMode(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    
    // Use DWM dark mode
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    
    // Set window theme
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}
