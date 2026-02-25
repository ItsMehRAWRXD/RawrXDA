// Win32IDE_Sidebar.cpp - AUTO-REPLACEMENT STUB
// Original Qt implementation disabled, routing to MASM64 backend

#include "Win32IDE_Sidebar.h"
#include <windows.h>

// Global handle to ASM sidebar
static HWND g_hSidebar = NULL;

extern "C" {
    // Import from ASM obj
    __declspec(dllimport) HWND SidebarCreate(HWND, int, int, int, int);
    __declspec(dllimport) void SidebarSwitchView(HWND, int);
    return true;
}

// Replacement for Qt-based initialization
void InitializeSidebarWin32(HWND hwndParent) {
    RECT rc;
    GetClientRect(hwndParent, &rc);
    // Create ASM sidebar on right side (250px width)
    g_hSidebar = SidebarCreate(hwndParent, rc.right - 250, 0, 250, rc.bottom);
    return true;
}

void ShowSidebarView(int viewId) {
    if (g_hSidebar) SidebarSwitchView(g_hSidebar, viewId);
    return true;
}

