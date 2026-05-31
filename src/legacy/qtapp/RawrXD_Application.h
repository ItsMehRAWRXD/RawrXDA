#pragma once
// RawrXD_Application.h
// Pure Win32 Application replacement for QApplication

#ifndef RAWRXD_APPLICATION_H
#define RAWRXD_APPLICATION_H

#include "../RawrXD_Foundation.h"
#include <windows.h>

namespace RawrXD {

class Application {
    HINSTANCE hInstance;
    String cmdLine;
    static Application* instance;
    bool quitRequested;
    
public:
    Application(HINSTANCE hInst, LPSTR cmd);
    virtual ~Application();
    
    static Application* getInstance() { return instance; }
    
    // Core Event Loop
    int exec();
    void quit();
    void processEvents();
    
    // Properties
    HINSTANCE getHInstance() const { return hInstance; }
    String arguments() const { return cmdLine; }
    
    // Global clipboard operations
    void clipboardSetText(const String& text);
    String clipboardText() const;
    
    // Global UI helpers
    void setStyle(const String& styleName);
    static int screenWidth();
    static int screenHeight();
    static int dpi();
};

#define qApp RawrXD::Application::getInstance()

} // namespace RawrXD

#endif // RAWRXD_APPLICATION_H
