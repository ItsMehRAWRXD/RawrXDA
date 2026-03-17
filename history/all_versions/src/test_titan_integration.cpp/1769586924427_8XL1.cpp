// Simple test to verify Titan Streaming Orchestrator integration
#include <iostream>
#include <windows.h>
#include "Titan_API.h"

int main() {
    std::cout << "Testing Titan Streaming Orchestrator Integration...\n\n";
    
    // Initialize Titan
    void* titanHandle = nullptr;
    int32_t result = Titan_Initialize(&titanHandle, 0);
    
    if (result != 0) {
        std::cerr << "ERROR: Titan_Initialize failed with code: " << result << "\n";
        return 1;
    }
    
    std::cout << "✓ Titan_Initialize succeeded\n";
    std::cout << "  Handle: " << titanHandle << "\n\n";
    
    // Shutdown Titan
    result = Titan_Shutdown(titanHandle);
    
    if (result != 0) {
        std::cerr << "ERROR: Titan_Shutdown failed with code: " << result << "\n";
        return 1;
    }
    
    std::cout << "✓ Titan_Shutdown succeeded\n\n";
    std::cout << "All tests passed! Titan library is properly integrated.\n";
    
    return 0;
}
