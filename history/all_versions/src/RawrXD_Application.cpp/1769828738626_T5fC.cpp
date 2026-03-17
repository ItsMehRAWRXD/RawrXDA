#include "RawrXD_Application.h"
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {

Application* Application::instance = nullptr;

Application::Application(HINSTANCE hInst) : hInstance(hInst) {
    instance = this;
}

Application::~Application() {
    instance = nullptr;
}

int Application::exec() {
    running = true;
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    running = false;
    aboutToQuit.emit((int)msg.wParam);
    return (int)msg.wParam;
}

void Application::quit(int returnCode) {
    PostQuitMessage(returnCode);
}

void Application::processEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            // Re-post quit message so the main loop catches it?
            // Or handle it here?
            PostQuitMessage((int)msg.wParam);
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

String Application::applicationDirPath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        PathRemoveFileSpecW(path);
        return String(path);
    }
    return String();
}

String Application::applicationFilePath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        return String(path);
    }
    return String();
}

void Application::setApplicationName(const String& name) {
    // maybe set registry key or something. For now just unused
}

} // namespace RawrXD
