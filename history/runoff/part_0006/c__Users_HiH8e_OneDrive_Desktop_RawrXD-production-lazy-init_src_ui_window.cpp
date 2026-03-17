#include "window.hpp"
#include <tchar.h>

MainWindow::MainWindow(HINSTANCE hInst) : hInst_(hInst), hwnd_(nullptr) {}

MainWindow::~MainWindow() {}

bool MainWindow::create(const wchar_t* title, int width, int height) {
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("MainWindowClass");

    if (!RegisterClassEx(&wc)) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        L"MainWindowClass",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, hInst_, nullptr
    );

    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}