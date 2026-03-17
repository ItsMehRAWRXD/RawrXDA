#include <windows.h>
#include <string>
#include <exception>
#include "production_agentic_ide.h"

// Forward declaration from production_agentic_ide.cpp
void LogStartup(const std::string& message);

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    LogStartup("WinMain: enter");
    try {
        // Instantiate the production IDE. It creates its own native window,
        // menus, toolbars, and status bar, and sets up the application.
        ProductionAgenticIDE ide;
        LogStartup("WinMain: IDE constructed");

        // Run the standard message loop.
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        LogStartup("WinMain: message loop exit code=" + std::to_string(static_cast<int>(msg.wParam)));
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& ex) {
        LogStartup(std::string("WinMain: exception: ") + ex.what());
        MessageBoxA(nullptr, ex.what(), "Startup exception", MB_OK | MB_ICONERROR);
        return -1;
    } catch (...) {
        LogStartup("WinMain: unknown exception");
        MessageBoxA(nullptr, "Unknown exception during startup", "Startup exception", MB_OK | MB_ICONERROR);
        return -2;
    }
}
