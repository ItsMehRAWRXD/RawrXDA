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


    // Check available memory
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);


    std::cout.flush();
    
    // Initialize Titan
    void* titanHandle = nullptr;
    
    __try {
        int32_t result = Titan_Initialize(&titanHandle, 0);


        if (result != 0) {


            return 1;
        }


        // Shutdown Titan
        result = Titan_Shutdown(titanHandle);


        if (result != 0) {
            
            return 1;
        }


    }
    __except(EXCEPTION_EXECUTE_HANDLER) {


        return 1;
    }
    
    return 0;
}
