// Auto-generated sidebar interface header
#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Exported functions from Win32IDE_Sidebar.asm
__declspec(dllimport) HWND SidebarCreate(HWND hwndParent, int x, int y, int width, int height);
__declspec(dllimport) BOOL SidebarDebugAttach(HWND hwndSidebar, LPCSTR lpApplication, LPCSTR lpCommandLine);
__declspec(dllimport) void SidebarSwitchView(HWND hwndSidebar, int viewIndex);

// View indices
#define SIDEBAR_VIEW_EXPLORER    0
#define SIDEBAR_VIEW_SEARCH      1
#define SIDEBAR_VIEW_SCM         2
#define SIDEBAR_VIEW_DEBUG       3
#define SIDEBAR_VIEW_EXTENSIONS  4

#ifdef __cplusplus
}
#endif
