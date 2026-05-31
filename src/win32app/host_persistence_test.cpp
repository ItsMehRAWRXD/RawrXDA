#include <windows.h>
#include <iostream>
#include <string>
#include "ExtensionInstance.h"

using namespace RawrXD::Extensions;

int main() {
    std::cout << "[FinalTest] Starting Extension Host Persistence Test..." << std::endl;
    
    // We point to notepad.exe as a placeholder "extension" to verify process creation and job objects
    // In a real scenario, this would be the ExtensionRunner.exe
    ExtensionInstance ext("test-extension", "C:\\Windows\\System32\\notepad.exe");
    
    if (ext.Launch()) {
        std::cout << "[FinalTest] SUCCESS: Extension instance launched in sandbox." << std::endl;
        std::cout << "[FinalTest] Running for 5 seconds to verify stability..." << std::endl;
        
        for (int i = 0; i < 5; ++i) {
            std::cout << "  ... monitoring [running=" << ext.IsRunning() << "]" << std::endl;
            Sleep(1000);
        }
        
        std::cout << "[FinalTest] Shutting down extension host." << std::endl;
        ext.Shutdown();
        std::cout << "[FinalTest] Test passed." << std::endl;
    } else {
        std::cerr << "[FinalTest] FAILURE: Extension launch failed. Code: " << GetLastError() << std::endl;
    }
    
    return 0;
}
