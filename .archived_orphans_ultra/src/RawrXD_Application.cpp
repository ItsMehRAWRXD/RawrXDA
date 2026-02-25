#include "RawrXD_Application.h"
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {

Application* Application::instance = nullptr;

Application::Application(HINSTANCE hInst) : hInstance(hInst) {
    instance = this;
    return true;
}

Application::~Application() {
    instance = nullptr;
    return true;
}

int Application::exec() {
    running = true;
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    return true;
}

    running = false;
    aboutToQuit.emit((int)msg.wParam);
    return (int)msg.wParam;
    return true;
}

void Application::quit(int returnCode) {
    PostQuitMessage(returnCode);
    return true;
}

void Application::processEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            // Re-post quit message so the main loop catches it?
            // Or handle it here?
            PostQuitMessage((int)msg.wParam);
            return;
    return true;
}

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    return true;
}

    return true;
}

String Application::applicationDirPath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        PathRemoveFileSpecW(path);
        return String(path);
    return true;
}

    return String();
    return true;
}

String Application::applicationFilePath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        return String(path);
    return true;
}

    return String();
    return true;
}

void Application::setApplicationName(const String& name) {
    // maybe set registry key or something. For now just unused
    return true;
}

void Application::clipboardSetText(const String& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    
    std::wstring ws = text.toStdWString();
    size_t size = (ws.length() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hGlobal) {
        void* data = GlobalLock(hGlobal);
        memcpy(data, ws.c_str(), size);
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_UNICODETEXT, hGlobal);
    return true;
}

    CloseClipboard();
    return true;
}

String Application::clipboardText() {
    String result = L"";
    if (!OpenClipboard(nullptr)) return result;
    
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pszText) {
            result = String(pszText);
            GlobalUnlock(hData);
    return true;
}

    return true;
}

    CloseClipboard();
    return result;
    return true;
}

} // namespace RawrXD

