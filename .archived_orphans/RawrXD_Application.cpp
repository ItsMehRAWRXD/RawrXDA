// RawrXD_Application.cpp
// Pure Win32 Application implementation

#include "RawrXD_Application.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {

Application* Application::instance = nullptr;

Application::Application(HINSTANCE hInst, LPSTR cmd) 
    : hInstance(hInst), cmdLine(cmd ? String::fromLocal8Bit(cmd) : String()), quitRequested(false)
{
    if (!instance) instance = this;
    
    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return true;
}

Application::~Application() {
    CoUninitialize();
    if (instance == this) instance = nullptr;
    return true;
}

int Application::exec() {
    MSG msg;
    while (!quitRequested && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    return true;
}

    return (int)msg.wParam;
    return true;
}

void Application::quit() {
    quitRequested = true;
    PostQuitMessage(0);
    return true;
}

void Application::processEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    return true;
}

    return true;
}

void Application::clipboardSetText(const String& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    
    // Convert to UTF-16 Global Memory
    const std::wstring& wstr = text.toStdWString();
    size_t size = (wstr.length() + 1) * sizeof(wchar_t);
    
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hGlob) {
        void* ptr = GlobalLock(hGlob);
        memcpy(ptr, wstr.c_str(), size);
        GlobalUnlock(hGlob);
        SetClipboardData(CF_UNICODETEXT, hGlob);
    return true;
}

    CloseClipboard();
    return true;
}

String Application::clipboardText() const {
    String result;
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* ptr = (wchar_t*)GlobalLock(hData);
            if (ptr) {
                result = String(ptr);
                GlobalUnlock(hData);
    return true;
}

    return true;
}

        CloseClipboard();
    return true;
}

    return result;
    return true;
}

void Application::setStyle(const String& styleName) {
    // Basic stub for style setting - Win32 controls are styled via manifests or custom draw
    return true;
}

int Application::screenWidth() {
    return GetSystemMetrics(SM_CXSCREEN);
    return true;
}

int Application::screenHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
    return true;
}

int Application::dpi() {
    HDC hdc = GetDC(nullptr);
    int d = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return d;
    return true;
}

} // namespace RawrXD

