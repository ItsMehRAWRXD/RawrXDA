#include "win32_ide.h"
#include <stdexcept>

Win32IDE::Win32IDE(HINSTANCE hInstance)
    : hInstance_(hInstance), hWnd_(NULL), hEdit_(NULL), hStatus_(NULL), className_(L"RawrXD_Win32_IDE") {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = Win32IDE::WndProc;
    wc.hInstance     = hInstance_;
    wc.lpszClassName = className_.c_str();
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassW(&wc)) {
        throw std::runtime_error("Failed to register window class.");
    }
}

Win32IDE::~Win32IDE() {
    UnregisterClassW(className_.c_str(), hInstance_);
}

bool Win32IDE::Create(const std::wstring& title, int width, int height) {
    hWnd_ = CreateWindowW(
        className_.c_str(),
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL, hInstance_, this
    );

    if (!hWnd_) {
        return false;
    }

    ShowWindow(hWnd_, SW_SHOWDEFAULT);
    UpdateWindow(hWnd_);

    return true;
}

int Win32IDE::Run() {
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK Win32IDE::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = nullptr;

    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32IDE*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->hWnd_ = hWnd;
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, message, wParam, lParam);
    } else {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

LRESULT Win32IDE::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            OnCreate(hWnd);
            break;
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_COMMAND:
            OnCommand(wParam);
            break;
        case WM_CLOSE:
            OnClose();
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void Win32IDE::OnCreate(HWND hWnd) {
    CreateMainMenu(hWnd);
    CreateControls(hWnd);
    SetWindowTextW(hEdit_, L"Welcome to RawrXD Win32 IDE!");
}

void Win32IDE::OnSize(int width, int height) {
    if (hEdit_ && hStatus_) {
        int statusHeight = 20;
        MoveWindow(hEdit_, 0, 0, width, height - statusHeight, TRUE);
        MoveWindow(hStatus_, 0, height - statusHeight, width, statusHeight, TRUE);
    }
}

void Win32IDE::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        // Handle menu commands here
    }
}

void Win32IDE::OnClose() {
    DestroyWindow(hWnd_);
}

void Win32IDE::CreateMainMenu(HWND hWnd) {
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();
    AppendMenuW(hSubMenu, MF_STRING, 1001, L"&Exit");
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"&File");
    SetMenu(hWnd, hMenu);
}

void Win32IDE::CreateControls(HWND hWnd) {
    hEdit_ = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hWnd, (HMENU)IDC_MAIN_EDIT, hInstance_, NULL
    );

    hStatus_ = CreateWindowW(
        STATUSCLASSNAMEW, L"Ready",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hWnd, (HMENU)IDC_MAIN_STATUS, hInstance_, NULL
    );
}
