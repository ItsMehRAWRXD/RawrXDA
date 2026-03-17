// RawrXD Agentic IDE
// Advanced AI-powered IDE with terminal integration and agentic capabilities


#include <iostream>
#include <windows.h>
#include "agentic_ide.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);

    std::cout << "Starting RawrXD Agentic IDE (Headless/Win32 Core)..." << std::endl;

    try {
        AgenticIDE ide;
        ide.initialize();
        ide.run();

        // Keep console alive if running interactively
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

