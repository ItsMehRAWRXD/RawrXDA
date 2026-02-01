// main.cpp
// Entry point for RawrXD IDE (Qt-free)

#include "RawrXD_Foundation.h"
#include "RawrXD_Application.h"
#include "gui/RawrXD_EditorWindow.h"
#include <windows.h>

using namespace RawrXD;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize Application
    Application app(hInstance);
    
    // Create Main Window (EditorWindow as main for now)
    EditorWindow window;
    // Create top-level window
    if (!window.create(nullptr, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768)) {
        MessageBox(nullptr, L"Failed to create main window", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Show and run
    ShowWindow(window.handle(), nCmdShow);
    UpdateWindow(window.handle());
    
    return app.exec();
}
