#include <windows.h>
#include <iostream>

// Simple test program to verify compilation
int main() {
    std::cout << "Native IDE - Quick Test Build" << std::endl;
    std::cout << "Compilation successful!" << std::endl;
    
    // Test Windows API access
    DWORD version = GetVersion();
    std::cout << "Windows version: " << (version & 0xFF) << "." << ((version >> 8) & 0xFF) << std::endl;
    
    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();
    return 0;
}