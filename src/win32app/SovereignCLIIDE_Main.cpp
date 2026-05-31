// ============================================================================
// SovereignCLIIDE_Main.cpp — Standalone entry point for CLI IDE
// ============================================================================

#include "SovereignCLIIDE.h"
#include <objbase.h>
#include <iostream>
#include <string>

// Forward declaration
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

// Console entry point for standalone mode
int main(int argc, char* argv[])
{
    // Initialize COM for potential use
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    
    SovereignCLIIDE cliIDE(SovereignCLIIDE::RunMode::Standalone);
    
    if (!cliIDE.initialize()) {
        std::cerr << "Failed to initialize Sovereign CLI IDE" << std::endl;
        CoUninitialize();
        return 1;
    }
    
    std::cout << "Sovereign CLI IDE Started" << std::endl;
    std::cout << "Type 'exit' to quit" << std::endl;
    
    // Simple REPL loop
    std::string command;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);
        
        if (command == "exit" || command == "quit") {
            break;
        }
        
        if (!command.empty()) {
            auto result = cliIDE.executeCommand(command);
            if (result.success) {
                std::cout << result.output << std::endl;
            } else {
                std::cerr << "Error: " << result.error << std::endl;
            }
        }
    }
    
    cliIDE.shutdown();
    CoUninitialize();
    
    std::cout << "Sovereign CLI IDE Stopped" << std::endl;
    return 0;
}

// Windows entry point (for GUI mode if needed)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // For GUI mode, we could create a console window
    // but for now just use the console entry point
    return main(__argc, __argv);
}
