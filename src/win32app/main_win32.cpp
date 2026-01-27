#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>
#include <fstream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize logger early
    try {
        IDELogger::getInstance().initialize("C:\\RawrXD_IDE.log");
        IDELogger::getInstance().setLevel(IDELogger::Level::DEBUG);
        LOG_INFO("WinMain started - RawrXD Win32 IDE initializing");
    } catch (...) {
        // Fallback to file diagnostic if logger fails
        std::ofstream errLog("C:\\LOGGER_INIT_FAILED.txt");
        errLog << "Logger initialization threw exception" << std::endl;
        errLog.close();
    }
    
    LOG_DEBUG("Creating Win32IDE instance");
    Win32IDE ide(hInstance);
    LOG_DEBUG("Win32IDE constructor completed");

    if (!ide.createWindow()) {
        LOG_ERROR("createWindow() failed");
        MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    LOG_INFO("Main window created successfully");
    ide.showWindow();
    LOG_INFO("Entering message loop");
    int rc = ide.runMessageLoop();
    LOG_INFO("Message loop exited with code " + std::to_string(rc));
    return rc;
}
