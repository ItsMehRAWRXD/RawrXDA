#include "RawrXD_DockManager.h"
#include <windowsx.h>

#define DOCK_HOST_CLASS "RawrXDDockHost"

UI_STATE g_uiState = {0};
PANEL_DESC g_panels[3] = {0};

static LRESULT CALLBACK DockHostWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UI_STATE* state = (UI_STATE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!state && uMsg != WM_CREATE) return DefWindowProcA(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
            state = (UI_STATE*)cs->lpCreateParams;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)state);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // Prevent white flicker!

        case WM_SIZE: {
            if (state) {
                GetClientRect(hwnd, &state->rcDockHost);
                // Hardcode layout for now so it works EXACTLY like before visually
                int cx = state->rcDockHost.right;
                int cy = state->rcDockHost.bottom;
                int split1 = 200;
                int split2 = cx - 300;
                
                // Reposition child panels
                if (state->pPanels) {
                    if (state->pPanels[0].hwnd)
                        MoveWindow(state->pPanels[0].hwnd, 0, 0, split1, cy, TRUE); // Explorer
                    if (state->pPanels[1].hwnd)
                        MoveWindow(state->pPanels[1].hwnd, split1 + 4, 0, split2 - split1 - 4, cy, TRUE); // Editor
                    if (state->pPanels[2].hwnd)
                        MoveWindow(state->pPanels[2].hwnd, split2 + 4, 0, cx - split2 - 4, cy, TRUE); // Chat
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBitmap);

            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hMemDC, &rc, hBrush);
            DeleteObject(hBrush);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hBitmap);
            DeleteDC(hMemDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

extern "C" void Dock_Initialize(HINSTANCE hInst) {
    WNDCLASSEXA wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = DockHostWndProc;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = DOCK_HOST_CLASS;

    RegisterClassExA(&wcex);
    g_uiState.pPanels = g_panels;
}

extern "C" HWND Dock_GetHostWindow(HWND hwndParent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
    HWND hwndDock = CreateWindowExA(
        0, DOCK_HOST_CLASS, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hwndParent, NULL, hInst, &g_uiState
    );
    g_uiState.hwndDockHost = hwndDock;
    return hwndDock;
}

extern "C" void Dock_UpdateSize(HWND hwndDockHost, int right, int bottom) {
    MoveWindow(hwndDockHost, 0, 0, right, bottom, TRUE);
}

extern "C" void Dock_RegisterPanels(HWND hwndExplorer, HWND hwndEditor, HWND hwndChat) {
    g_panels[0].hwnd = hwndExplorer;
    g_panels[1].hwnd = hwndEditor;
    g_panels[2].hwnd = hwndChat;
    
    if (g_uiState.hwndDockHost) {
        RECT rc;
        GetClientRect(g_uiState.hwndDockHost, &rc);
        SendMessageA(g_uiState.hwndDockHost, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    }
}
