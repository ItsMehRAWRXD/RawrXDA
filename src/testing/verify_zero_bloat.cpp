#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "Starting RawrXD v14.6 Zero-Bloat Validation...\n";

    // Launch IDE
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    // Adjust path if necessary
    const wchar_t* cmd = L"D:\\rawrxd\\build\\bin\\RawrXD-Win32IDE.exe";

    if (!CreateProcessW(
        nullptr, (LPWSTR)cmd, nullptr, nullptr, 
        FALSE, 0, nullptr, nullptr, &si, &pi
    )) {
        std::cerr << "Failed to launch RawrXD-Win32IDE.exe. Error: " << GetLastError() << "\n";
        return 1;
    }
    
    std::cout << "Process launched (PID: " << pi.dwProcessId << "). Waiting for stabilization...\n";
    Sleep(5000); // 5s for WebView2/Swarm init
    
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
        size_t mb = pmc.WorkingSetSize / (1024 * 1024);
        std::cout << "-------------------------------------------\n";
        std::cout << "RawrXD v14.6 Real-time Memory Usage: " << mb << " MB\n";
        std::cout << "-------------------------------------------\n";
        
        // v14.6 target: <500MB
        if (mb < 500) {
            std::cout << "✅ ZERO BLOAT VALIDATED: " << mb << "MB < 500MB target\n";
            TerminateProcess(pi.hProcess, 0);
            return 0;
        } else {
            std::cout << "❌ MEMORY TARGET MISSED: " << mb << "MB exceeds 500MB threshold\n";
            std::cout << "Likely cause: Large model weights in resident set or leaking Qt handles.\n";
            TerminateProcess(pi.hProcess, 0);
            return 1;
        }
    } else {
        std::cerr << "Failed to retrieve memory info.\n";
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
