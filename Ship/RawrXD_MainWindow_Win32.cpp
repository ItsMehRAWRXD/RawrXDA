/*
 * RawrXD_MainWindow_Win32.cpp
 * Pure Win32 replacement for Qt MainWindow
 * Replaces: QMainWindow, QDockWidget, QWidget, QMenu, QToolBar
 * Uses: CreateWindowEx, Win32 GDI, message loop
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <map>
#include <functional>

// Window class names
const wchar_t CLASS_MAINWINDOW[] = L"RawrXD_MainWindow";
const wchar_t CLASS_TEXTEDITOR[] = L"RawrXD_TextEditor";
const wchar_t CLASS_PANEL[] = L"RawrXD_Panel";

struct DockWidget {
    HWND hwnd;
    wchar_t title[256];
    int width;
    int height;
    bool visible;
};

struct MenuAction {
    wchar_t label[256];
    UINT id;
    std::function<void()> callback;
};

class RawrXDMainWindow {
private:
    HWND m_hwnd;
    HWND m_editor;
    HWND m_statusBar;
    HMENU m_mainMenu;
    HINSTANCE m_hInstance;
    
    std::vector<DockWidget> m_dockWidgets;
    std::map<UINT, MenuAction> m_menuActions;
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_CREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            RawrXDMainWindow* pThis = reinterpret_cast<RawrXDMainWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            return 0;
        }
        
        RawrXDMainWindow* pThis = reinterpret_cast<RawrXDMainWindow*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        
        if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);
        
        switch (msg) {
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_SIZE: {
                int width = GET_X_LPARAM(lParam);
                int height = GET_Y_LPARAM(lParam);
                if (pThis->m_editor) {
                    SetWindowPos(pThis->m_editor, nullptr, 0, 0, width, height - 25,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                }
                return 0;
            }
            case WM_COMMAND:
                pThis->OnCommand(wParam, lParam);
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
    
    void OnCommand(WPARAM wParam, LPARAM lParam) {
        UINT id = LOWORD(wParam);
        auto it = m_menuActions.find(id);
        if (it != m_menuActions.end() && it->second.callback) {
            it->second.callback();
        }
    }
    
    void CreateMenuBar() {
        // Menu creation deferred - use simpler approach if needed
        m_mainMenu = nullptr;
    }

public:
    RawrXDMainWindow(HINSTANCE hInstance) 
        : m_hwnd(nullptr), m_editor(nullptr), m_statusBar(nullptr),
          m_mainMenu(nullptr), m_hInstance(hInstance) {}
    
    bool Create(const wchar_t* title, int x, int y, int width, int height) {
        // Register window class
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = CLASS_MAINWINDOW;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
        
        if (!RegisterClassW(&wc)) return false;
        
        // Create main window
        m_hwnd = CreateWindowExW(
            0, CLASS_MAINWINDOW, title,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            x, y, width, height,
            nullptr, nullptr, m_hInstance, this);
        
        if (!m_hwnd) return false;
        
        // Create menu
        CreateMenuBar();
        
        // Create text editor
        m_editor = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_WANTRETURN,
            0, 0, width, height - 25,
            m_hwnd, (HMENU)1, m_hInstance, nullptr);
        
        if (!m_editor) {
            SendMessageW(m_editor, WM_SETFONT,
                (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
        }
        
        // Create status bar
        m_statusBar = CreateWindowExW(
            0, L"STATIC", L"Ready",
            WS_CHILD | WS_VISIBLE,
            0, height - 25, width, 25,
            m_hwnd, (HMENU)2, m_hInstance, nullptr);
        
        return true;
    }
    
    void RegisterMenuAction(UINT id, const wchar_t* label, std::function<void()> callback) {
        MenuAction action;
        wcscpy_s(action.label, label);
        action.id = id;
        action.callback = callback;
        m_menuActions[id] = action;
    }
    
    void SetStatusText(const wchar_t* text) {
        if (m_statusBar) {
            SetWindowTextW(m_statusBar, text);
        }
    }
    
    void AddDockWidget(const wchar_t* title, int width, int height) {
        DockWidget dock;
        dock.hwnd = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"STATIC", title,
            WS_CHILD | WS_VISIBLE,
            0, 0, width, height,
            m_hwnd, nullptr, m_hInstance, nullptr);
        
        wcscpy_s(dock.title, title);
        dock.width = width;
        dock.height = height;
        dock.visible = true;
        
        m_dockWidgets.push_back(dock);
    }
    
    HWND GetHandle() const { return m_hwnd; }
    HWND GetEditor() const { return m_editor; }
    
    int MessageLoop() {
        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return msg.wParam;
    }
    
    ~RawrXDMainWindow() {
        if (m_mainMenu) DestroyMenu(m_mainMenu);
        if (m_hwnd) DestroyWindow(m_hwnd);
        UnregisterClassW(CLASS_MAINWINDOW, m_hInstance);
    }
};

// Global instance
static RawrXDMainWindow* g_mainWindow = nullptr;

// DLL exports
extern "C" {
    __declspec(dllexport) void* __stdcall CreateMainWindow(void* hInstance, const wchar_t* title) {
        if (!g_mainWindow) {
            g_mainWindow = new RawrXDMainWindow((HINSTANCE)hInstance);
            if (g_mainWindow->Create(title, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800)) {
                return g_mainWindow;
            }
            delete g_mainWindow;
            g_mainWindow = nullptr;
        }
        return g_mainWindow;
    }
    
    __declspec(dllexport) void __stdcall DestroyMainWindow(void* window) {
        if (window && window == g_mainWindow) {
            delete g_mainWindow;
            g_mainWindow = nullptr;
        }
    }
    
    __declspec(dllexport) void __stdcall MainWindow_SetStatus(void* window, const wchar_t* text) {
        RawrXDMainWindow* w = static_cast<RawrXDMainWindow*>(window);
        if (w) w->SetStatusText(text);
    }
    
    __declspec(dllexport) int __stdcall MainWindow_RunMessageLoop(void* window) {
        RawrXDMainWindow* w = static_cast<RawrXDMainWindow*>(window);
        return w ? w->MessageLoop() : -1;
    }
    
    __declspec(dllexport) void* __stdcall MainWindow_GetHandle(void* window) {
        RawrXDMainWindow* w = static_cast<RawrXDMainWindow*>(window);
        return w ? (void*)w->GetHandle() : nullptr;
    }
}

// DLL entry
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_MainWindow_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_mainWindow) {
        delete g_mainWindow;
    }
    return TRUE;
}
