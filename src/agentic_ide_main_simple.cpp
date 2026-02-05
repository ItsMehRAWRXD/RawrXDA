// RawrXD Agentic IDE - Minimal Main
// Entry point for GUI application

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <fstream>
#include "debug_logger.h"

// For GUI apps, we need WinMain, not main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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
    
    // Create minimal main window
    QMainWindow window;
    window.setWindowTitle("RawrXD Agentic IDE");
    window.setMinimumSize(1200, 800);
    
    QWidget *central = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(central);
    QLabel *label = new QLabel("RawrXD Agentic IDE - Starting up...");
    layout->addWidget(label);
    window.setCentralWidget(central);
    
    DEBUG_LOG("=== WinMain: Window created and shown ===\n");
    
    window.show();
    
    DEBUG_LOG("=== WinMain: Entering event loop ===\n");
    
    int result = app.exec();
    
    DEBUG_LOG("=== WinMain: Event loop finished ===\n");
    
    // Cleanup argv
    for (int i = 0; i < argc; ++i) {
        delete[] argv[i];
    }
    delete[] argv;
    
    return result;
}
