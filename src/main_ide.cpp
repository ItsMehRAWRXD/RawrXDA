// Guard: compile only when RAWRXD_IDE_MAIN is defined.
// Default production entry point is win32app/Win32IDE_Main.cpp.
#ifdef RAWRXD_IDE_MAIN

#include <windows.h>
#include "ide_window.h"
#include <commctrl.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);
    
    // Initialize IDE Window
    IDEWindow ide;
    if (!ide.Initialize(hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize IDE Window", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Run message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

#endif // RAWRXD_IDE_MAIN
