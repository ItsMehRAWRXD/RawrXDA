#include "Win32IDE.h"
//#include "IDELogger.h"  // TEMPORARILY DISABLED TO DEBUG CRASH
#include <windows.h>
#include <string>
#include <fstream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // ULTRA-EARLY diagnostic - write a simple text file to prove WinMain executes
    {
        std::ofstream earlyDiag("C:\\Users\\HiH8e\\Desktop\\WinMain_EXECUTED.txt");
        earlyDiag << "WinMain started at line 8" << std::endl;
        earlyDiag << "Logger DISABLED for debugging" << std::endl;
        earlyDiag.close();
    }
    
    // SKIP logger for now - it's crashing
    // Initialize file logger with ABSOLUTE path
    /*
    try {
        IDELogger::getInstance().initialize("C:\\Users\\HiH8e\\Desktop\\RawrXD_IDE.log");
        IDELogger::getInstance().setLevel(IDELogger::Level::DEBUG);
        LOG_INFO("WinMain started - Logger working!");
    } catch (...) {
        std::ofstream errLog("C:\\Users\\HiH8e\\Desktop\\LOGGER_FAILED.txt");
        errLog << "Logger initialization threw exception" << std::endl;
        errLog.close();
    }
    */
    
    // Add fallback diagnostics
    OutputDebugStringA("=== IDE STARTING ===\n");
    
    {
        std::ofstream step2("C:\\Users\\HiH8e\\Desktop\\BEFORE_IDE_CONSTRUCTOR.txt");
        step2 << "About to create Win32IDE object..." << std::endl;
        step2.close();
    }

    Win32IDE ide(hInstance);
    
    {
        std::ofstream step3("C:\\Users\\HiH8e\\Desktop\\AFTER_IDE_CONSTRUCTOR.txt");
        step3 << "Win32IDE constructor completed" << std::endl;
        step3.close();
    }

    if (!ide.createWindow()) {
        //LOG_ERROR("createWindow() failed");
        MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    //LOG_INFO("Main window created; showing window");
    ide.showWindow();
    int rc = ide.runMessageLoop();
    //LOG_INFO(std::string("Message loop exited with code ") + std::to_string(rc));
    return rc;
}