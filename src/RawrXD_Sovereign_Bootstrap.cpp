#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

// External UI Functions (from RawrXD_Native_UI.dll)
extern "C" HWND CreateRawrXDTriPane(HWND hParent);
extern "C" void ResizeRawrXDLanes(HWND hParent);
extern "C" void RunNativeMessageLoop();
extern "C" void PopulateExplorer(const char* path);
extern "C" void HandleExplorerNotify(LPARAM lParam);

// External Core Functions (from RawrXD_Native_Core.dll)
extern "C" bool GitStatusNative(const char* repoPath, char* outBuffer, int bufferSize);

// Window Procedure
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->idFrom == 101) { // hExplorer ID
                HandleExplorerNotify(lParam);
            }
            break;
        }
        case WM_CREATE: {
            char status[1024];
            GitStatusNative(".", status, 1024); // Verify bridge on launch
            return 0;
        }
        case WM_SIZE:
            ResizeRawrXDLanes(hWnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Initialize ComCtl
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // 2. Register Class
    WNDCLASSEXA wc = {0} ;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RawrXDSovereignClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    // 3. Create Main Window
    HWND hWnd = CreateWindowExA(0, "RawrXDSovereignClass", "RawrXD Sovereign v16.0.0-SOVEREIGN", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 1;

    // 4. Cross-Link UI
    CreateRawrXDTriPane(hWnd);
    PopulateExplorer("."); // Populate Explorer with current directory

    // 5. Build Display
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 6. Native Message Loop
    RunNativeMessageLoop();

    return 0;
}
