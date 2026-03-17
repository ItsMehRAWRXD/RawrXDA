// RawrXD_MainWindow.cpp
// Pure Win32 Main Window implementation

#include "RawrXD_MainWindow.h"
#include "../gui/RawrXD_EditorWindow.h"
#include "../gui/RawrXD_Sidebar.h"
#include "../gui/RawrXD_Panel.h"

namespace RawrXD {

MainWindow::MainWindow() 
    : hwnd(nullptr), editor(nullptr), sidebar(nullptr), panel(nullptr),
      sidebarWidth(250), panelHeight(200)
{
}

MainWindow::~MainWindow() {
    // Windows are destroyed by OS typically, but we should clean up pointers if we own them
    /* if (editor) delete editor; */
}

bool MainWindow::create(const String& title, int w, int h) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = L"RawrXD_MainWindow";
    
    RegisterClassEx(&wc);
    
    // Center on screen
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    
    hwnd = CreateWindowEx(
        0,
        L"RawrXD_MainWindow",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        x, y, w, h,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        
        // Create components
        sidebar = new Sidebar();
        if (sidebar) sidebar->create(hwnd, 0, 0, sidebarWidth, h);

        panel = new Panel();
        // Geometry will be fixed in updateLayout, pass defaults here
        if (panel) panel->create(hwnd, sidebarWidth, h - panelHeight, w - sidebarWidth, panelHeight);

        editor = new EditorWindow();
        if (editor) {
            editor->create(hwnd, sidebarWidth, 0, w - sidebarWidth, h - panelHeight);
        }
    }
    
    return hwnd != nullptr;
}

void MainWindow::show() {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void MainWindow::hide() {
    ShowWindow(hwnd, SW_HIDE);
}

void MainWindow::updateLayout() {
    if (!hwnd) return;
    
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    
    if (sidebar) {
        sidebar->onResize(sidebarWidth, h);
    }

    if (editor) {
        SetWindowPos(editor->handle(), nullptr, 
            sidebarWidth, 0, 
            w - sidebarWidth, h - panelHeight, 
            SWP_NOZORDER);
        // editor->onResize called by WM_SIZE? EditorWindow has its own WndProc but it's a child.
        // It receives WM_SIZE. But helper onResize might be needed if logic differs.
        // EditorWindow::WndProc calls onResize.
        // SetWindowPos triggers it.
    }

    if (panel) {
        SetWindowPos(panel->handle(), nullptr,
            sidebarWidth, h - panelHeight,
            w - sidebarWidth, panelHeight,
            SWP_NOZORDER);
        panel->onResize(w - sidebarWidth, panelHeight);
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (msg) {
        case WM_SIZE:
            if (self) self->updateLayout();
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace RawrXD
