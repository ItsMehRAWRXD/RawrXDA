#pragma once
#include "RawrXD_SignalSlot.h"
#include "RawrXD_Window.h"

namespace RawrXD {

class Application {
    HINSTANCE hInstance;
    static Application* instance;
    bool running = false;
    
public:
    Signal<int> aboutToQuit; // Exit code
    
    Application(HINSTANCE hInst);
    ~Application();
    
    static Application* getInstance() { return instance; }
    HINSTANCE getHInstance() const { return hInstance; }
    
    int exec();
    void quit(int returnCode = 0);
    void processEvents(); // Process pending messages (PeekMessage)
    
    static String applicationDirPath();
    static String applicationFilePath();
    static void setApplicationName(const String& name);
};

// Global macro for accessing the app
#define qApp RawrXD::Application::getInstance()

} // namespace RawrXD
