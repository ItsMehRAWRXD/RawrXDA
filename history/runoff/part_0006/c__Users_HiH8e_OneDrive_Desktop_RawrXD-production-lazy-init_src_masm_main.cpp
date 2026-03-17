#include "core/engine.hpp"
#include "ui/window.hpp"
#include <iostream>
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::cout << "RawrXD Win32 MASM IDE - Starting..." << std::endl;
    
    // Initialize the engine
    Engine engine;
    if (!engine.initialize(hInstance)) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return 1;
    }
    
    // Run the main message loop
    return engine.run();
}