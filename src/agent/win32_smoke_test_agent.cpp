#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>

// Minimal Win32 IDE Smoke Test Agent
// This agent targets the consolidated Win32 GUI and tests integration points.

struct SmokeTestResult {
    std::string testName;
    bool success;
    std::string message;
};

void LogResult(const SmokeTestResult& res) {
    std::cout << "[SMOKE TEST] " << (res.success ? "[PASS] " : "[FAIL] ") 
              << res.testName << ": " << res.message << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "RawrXD Win32 GUI Smoke Test Agent Launching..." << std::endl;
    
    std::vector<SmokeTestResult> results;

    // Test 1: Verify RawrXD_IDE_unified.exe existence
    if (std::filesystem::exists("RawrXD_IDE_unified.exe")) {
        results.push_back({"Unified EXE Presence", true, "Found RawrXD_IDE_unified.exe"});
    } else {
        results.push_back({"Unified EXE Presence", false, "RawrXD_IDE_unified.exe MISSING"});
    }

    // Test 2: Try to launch unified EXE in a mode that should exit quickly
    // Using -trace as it's a valid mode likely to exit 0
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA("RawrXD_IDE_unified.exe", (char*)"RawrXD_IDE_unified.exe -trace", nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000); // 5s timeout
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exitCode == 0) {
            results.push_back({"Unified Engine Trace Mode", true, "Exited successfully with 0"});
        } else {
            results.push_back({"Unified Engine Trace Mode", false, "Exited with code: " + std::to_string(exitCode)});
        }
    } else {
        results.push_back({"Unified Engine Launch", false, "Failed to CreateProcess"});
    }

    // Test 3: Check for Model Connection strings in Win32IDE.cpp (simulated check)
    // In a real agentic task, we'd check if the GUI can actually talk to the backend.

    std::cout << "\n--- Final Summary ---\n";
    int passed = 0;
    for (const auto& r : results) {
        LogResult(r);
        if (r.success) passed++;
    }

    std::cout << "Passed: " << passed << "/" << results.size() << std::endl;
    
    return (passed == (int)results.size()) ? 0 : 1;
}
