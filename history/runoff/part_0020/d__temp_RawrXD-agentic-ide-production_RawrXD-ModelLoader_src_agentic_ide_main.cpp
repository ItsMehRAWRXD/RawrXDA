// RawrXD Agentic IDE
// Advanced AI-powered IDE with terminal integration and agentic capabilities

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>
#include "debug_logger.h"
#include "agentic_ide.h"

// For GUI apps, we need WinMain, not main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Write to file immediately to prove we entered
    {
        std::ofstream f("D:\\temp\\winmain_entered.txt", std::ios::app);
        f << "WinMain entered\n";
        f.flush();
    }
    
    // Convert Windows command line to argc/argv for Qt
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    char** argv = new char*[argc + 1];
    
    for (int i = 0; i < argc; ++i) {
        int size = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i] = new char[size];
        WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argv[i], size, nullptr, nullptr);
    }
    argv[argc] = nullptr;
    LocalFree(argvW);
    
    // Create Qt application FIRST
    QApplication app(argc, argv);
    
    // NOW initialize the logger - after QApplication exists
    DebugLogger::getInstance().init("D:\\temp\\ide_startup.log");
    DEBUG_LOG("=== WinMain: QApplication created ===\n");
    
    try {
        DEBUG_LOG("=== WinMain: Creating AgenticIDE ===\n");
        
        AgenticIDE *ide = new AgenticIDE();
        
        DEBUG_LOG("=== WinMain: AgenticIDE created, calling show() ===\n");
        
        ide->show();
        
        DEBUG_LOG("=== WinMain: Entering event loop ===\n");
        
        int result = app.exec();
        
        DEBUG_LOG("=== WinMain: Event loop finished ===\n");
        
        delete ide;
        
        // Cleanup argv
        for (int i = 0; i < argc; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
        
        return result;
    }
    catch (const std::exception& e) {
        DEBUG_LOG("=== EXCEPTION ===\n");
        // Cleanup argv on exception
        for (int i = 0; i < argc; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
        return 1;
    }
    catch (...) {
        DEBUG_LOG("=== UNKNOWN EXCEPTION ===\n");
        // Cleanup argv on exception
        for (int i = 0; i < argc; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
        return 1;
    }
}