// Simple test to verify Titan Streaming Orchestrator integration
#include <iostream>
#include <iomanip>
#include <windows.h>
#include "Titan_API.h"

const char* GetErrorDescription(int32_t code) {
    switch (code) {
        case 0: return "Success";
        case 0x80070057: return "Invalid Parameter (ERROR_INVALID_PARAMETER)";
        case 0x8007000E: return "Out of Memory (ERROR_OUTOFMEMORY)";
        default: return "Unknown Error";
    }
}

int main() {
    std::cout << "Testing Titan Streaming Orchestrator Integration...\n\n";
    
    // Check available memory
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    std::cout << "System Memory Status:\n";
    std::cout << "  Total Physical: " << (memInfo.ullTotalPhys / (1024*1024)) << " MB\n";
    std::cout << "  Available Physical: " << (memInfo.ullAvailPhys / (1024*1024)) << " MB\n";
    std::cout << "  Memory Load: " << memInfo.dwMemoryLoad << "%\n\n";
    
    // Initialize Titan
    void* titanHandle = nullptr;
    int32_t result = Titan_Initialize(&titanHandle, 0);
    
    std::cout << "Titan_Initialize result:\n";
    std::cout << "  Return Code: 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << static_cast<uint32_t>(result) << std::dec << "\n";
    std::cout << "  Description: " << GetErrorDescription(result) << "\n";
    
    if (result != 0) {
        std::cerr << "\nERROR: Titan_Initialize failed\n";
        std::cerr << "This is expected if system memory is low or constrained.\n";
        std::cerr << "Titan requires ~68 MB for orchestrator + ring buffer.\n";
        return 1;
    }
    
    std::cout << "  Handle: " << titanHandle << "\n\n";
    std::cout << "✓ Titan_Initialize succeeded\n\n";
    
    // Shutdown Titan
    result = Titan_Shutdown(titanHandle);
    
    std::cout << "Titan_Shutdown result:\n";
    std::cout << "  Return Code: 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << static_cast<uint32_t>(result) << std::dec << "\n";
    
    if (result != 0) {
        std::cerr << "ERROR: Titan_Shutdown failed\n";
        return 1;
    }
    
    std::cout << "✓ Titan_Shutdown succeeded\n\n";
    std::cout << "══════════════════════════════════════════════════\n";
    std::cout << "All tests passed! Titan library is properly integrated.\n";
    std::cout << "══════════════════════════════════════════════════\n";
    
    return 0;
}
