#include <windows.h>
#include <iostream>
#include <string>

extern "C" HANDLE MASM_CreateIsolatedProcess(const char* appName, char* cmdLine, const char* workDir, DWORD memLimitMB);

int main() {
    std::cout << "[BrokerTest] Starting Isolated Process Test..." << std::endl;
    
    // Launching notepad.exe as a safe test
    char cmd[] = "notepad.exe"; 
    HANDLE hProcess = MASM_CreateIsolatedProcess(NULL, cmd, NULL, 128);
    
    if (hProcess) {
        std::cout << "[BrokerTest] SUCCESS: Process handle: " << hProcess << std::endl;
        std::cout << "[BrokerTest] Waiting 5 seconds then terminating..." << std::endl;
        Sleep(5000);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        std::cout << "[BrokerTest] Test passed." << std::endl;
    } else {
        std::cerr << "[BrokerTest] FAILURE: Could not create isolated process. Error: " << GetLastError() << std::endl;
    }
    
    return 0;
}
