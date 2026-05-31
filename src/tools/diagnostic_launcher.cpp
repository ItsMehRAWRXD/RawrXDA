// ============================================================================
// RawrXD IDE Autonomous Diagnostic Launcher
// ============================================================================
// Launches IDE in self-diagnostic mode, monitors health, and reports results
// Features:
// - Spawns IDE with autonomous agent enabled
// - Monitors digestion pipeline execution
// - Captures beacons and diagnostics
// - Automatically triggers healing on failures
// - Generates comprehensive diagnostic reports
// ============================================================================

#include "../win32app/AutonomousAgent.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <fstream>

using namespace RawrXD::Agent;

// ============================================================================
// Configuration
// ============================================================================
constexpr DWORD DEFAULT_TIMEOUT_MS = 60000;
constexpr DWORD BEACON_CHECK_INTERVAL_MS = 500;
constexpr const wchar_t* IDE_WINDOW_CLASS = L"RawrXD_IDE_Class";

// ============================================================================
// Report Generation
// ============================================================================
void GenerateDiagnosticReport(const DiagnosticReport& report, const std::string& outputPath)
{
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        
        return;
    }
    
    file << "========================================\n";
    file << "RawrXD IDE Diagnostic Report\n";
    file << "========================================\n\n";
    
    file << "Summary: " << report.summary << "\n";
    file << "Success: " << (report.success ? "YES" : "NO") << "\n\n";
    
    file << "Health Metrics:\n";
    file << "  Total Runs: " << report.metrics.totalRuns << "\n";
    file << "  Successful: " << report.metrics.successfulRuns << "\n";
    file << "  Failed: " << report.metrics.failedRuns << "\n";
    file << "  Recovered: " << report.metrics.recoveredRuns << "\n";
    file << "  Avg Latency: " << report.metrics.averageLatencyMs << "ms\n";
    file << "  Last Run Latency: " << report.metrics.lastRunLatencyMs << "ms\n\n";
    
    file << "Checkpoints (" << report.checkpoints.size() << "):\n";
    for (const auto& checkpoint : report.checkpoints) {
        file << "  " << checkpoint << "\n";
    }
    file << "\n";
    
    file << "Errors (" << report.errors.size() << "):\n";
    for (const auto& error : report.errors) {
        file << "  ERROR: " << error << "\n";
    }
    file << "\n";
    
    file << "Recovery Actions (" << report.recoveryActions.size() << "):\n";
    for (const auto& action : report.recoveryActions) {
        file << "  ACTION: " << action << "\n";
    }
    file << "\n";
    
    file << "Detailed Trace:\n";
    file << report.detailedTrace << "\n";
    
    file.close();
    
}

// ============================================================================
// IDE Process Management
// ============================================================================
struct IDEProcessInfo {
    HANDLE hProcess;
    DWORD processId;
    HWND hwnd;
    bool running;
};

BOOL CALLBACK FindIDEWindowProc(HWND hwnd, LPARAM lParam)
{
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    
    if (wcscmp(className, IDE_WINDOW_CLASS) == 0) {
        HWND* pHwnd = reinterpret_cast<HWND*>(lParam);
        *pHwnd = hwnd;
        return FALSE; // Stop enumeration
    }
    
    return TRUE; // Continue enumeration
}

HWND FindIDEWindow(DWORD processId, DWORD timeoutMs = 10000)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    while (true) {
        HWND hwnd = nullptr;
        EnumWindows(FindIDEWindowProc, reinterpret_cast<LPARAM>(&hwnd));
        
        if (hwnd) {
            // Verify it belongs to our process
            DWORD windowPid = 0;
            GetWindowThreadProcessId(hwnd, &windowPid);
            if (windowPid == processId) {
                return hwnd;
            }
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        if (elapsed > timeout) {
            break;
        }
        
        Sleep(100);
    }
    
    return nullptr;
}

IDEProcessInfo LaunchIDE(const std::wstring& idePath, const std::wstring& testFile)
{
    IDEProcessInfo info = {};
    
    std::wcout << L"Launching IDE: " << idePath << std::endl;
    
    // Build command line
    std::wstring cmdLine = L"\"" + idePath + L"\"";
    if (!testFile.empty()) {
        cmdLine += L" \"" + testFile + L"\"";
    }
    
    // Add diagnostic flags
    cmdLine += L" --diagnostic --auto-agent --beacon-log=\"C:\\RawrXD_Beacons.log\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(nullptr, const_cast<wchar_t*>(cmdLine.c_str()), nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        
        return info;
    }
    
    info.hProcess = pi.hProcess;
    info.processId = pi.dwProcessId;
    info.running = true;
    
    CloseHandle(pi.hThread);


    // Wait for window to appear
    info.hwnd = FindIDEWindow(info.processId);
    if (!info.hwnd) {
        
    } else {
        
    }
    
    return info;
}

// ============================================================================
// Beacon Monitoring
// ============================================================================
struct BeaconMonitor {
    std::vector<std::string> beacons;
    DWORD lastBeaconTime;
    bool digestionStarted;
    bool digestionCompleted;
    bool errorDetected;
    
    BeaconMonitor() : lastBeaconTime(0), digestionStarted(false), digestionCompleted(false), errorDetected(false) {}
    
    void Update(const std::string& beaconLog)
    {
        std::ifstream file(beaconLog);
        if (!file.is_open()) {
            return;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (std::find(beacons.begin(), beacons.end(), line) == beacons.end()) {
                beacons.push_back(line);
                lastBeaconTime = GetTickCount();
                
                // Parse beacon type
                if (line.find("DIGESTION_STARTED") != std::string::npos) {
                    digestionStarted = true;
                }
                if (line.find("DIGESTION_COMPLETE") != std::string::npos) {
                    digestionCompleted = true;
                }
                if (line.find("ERROR_DETECTED") != std::string::npos) {
                    errorDetected = true;
                }


            }
        }
        
        file.close();
    }
    
    bool IsStalled(DWORD stallTimeMs = 30000) const
    {
        return (GetTickCount() - lastBeaconTime) > stallTimeMs;
    }
};

// ============================================================================
// Main Diagnostic Workflow
// ============================================================================
int RunDiagnostic(const std::wstring& idePath, const std::wstring& testFile)
{


    // Launch IDE
    IDEProcessInfo ideInfo = LaunchIDE(idePath, testFile);
    if (!ideInfo.running) {
        
        return 1;
    }
    
    // Initialize beacon monitor
    BeaconMonitor monitor;
    std::string beaconLogPath = "C:\\RawrXD_Beacons.log";
    
    // Wait for IDE to initialize
    
    Sleep(2000);
    
    // Trigger digestion via keyboard shortcut
    if (ideInfo.hwnd) {


        // Send Ctrl+Shift+D to IDE window
        SetForegroundWindow(ideInfo.hwnd);
        Sleep(100);
        
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event(VK_SHIFT, 0, 0, 0);
        keybd_event('D', 0, 0, 0);
        keybd_event('D', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
    
    // Monitor beacons
    
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(DEFAULT_TIMEOUT_MS);
    
    while (true) {
        // Check if IDE process is still running
        DWORD exitCode = 0;
        if (GetExitCodeProcess(ideInfo.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            
            break;
        }
        
        // Update beacon monitor
        monitor.Update(beaconLogPath);
        
        // Check for completion
        if (monitor.digestionCompleted) {
            
            break;
        }
        
        // Check for errors
        if (monitor.errorDetected) {
            
            break;
        }
        
        // Check for timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        if (elapsed > timeout) {
            
            break;
        }
        
        // Check for stall
        if (monitor.IsStalled(15000)) {
            
        }
        
        Sleep(BEACON_CHECK_INTERVAL_MS);
    }
    
    // Cleanup
    
    if (ideInfo.hwnd) {
        PostMessageW(ideInfo.hwnd, WM_CLOSE, 0, 0);
    }
    
    WaitForSingleObject(ideInfo.hProcess, 5000);
    CloseHandle(ideInfo.hProcess);
    
    // Generate report


    std::string reportPath = "C:\\RawrXD_Diagnostic_Report.txt";
    std::ofstream report(reportPath);
    if (report.is_open()) {
        report << "========================================\n";
        report << "RawrXD IDE Diagnostic Report\n";
        report << "========================================\n\n";
        
        report << "IDE Path: " << std::string(idePath.begin(), idePath.end()) << "\n";
        report << "Test File: " << std::string(testFile.begin(), testFile.end()) << "\n";
        report << "Process ID: " << ideInfo.processId << "\n\n";
        
        report << "Beacons Captured: " << monitor.beacons.size() << "\n";
        report << "Digestion Started: " << (monitor.digestionStarted ? "YES" : "NO") << "\n";
        report << "Digestion Completed: " << (monitor.digestionCompleted ? "YES" : "NO") << "\n";
        report << "Errors Detected: " << (monitor.errorDetected ? "YES" : "NO") << "\n\n";
        
        report << "Beacon Log:\n";
        for (const auto& beacon : monitor.beacons) {
            report << "  " << beacon << "\n";
        }
        
        report.close();
        
    }
    
    // Determine exit code
    int exitCode = 0;
    if (!monitor.digestionCompleted || monitor.errorDetected) {
        exitCode = 1;
    }


    return exitCode;
}

// ============================================================================
// Entry Point
// ============================================================================
int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        std::wcout << L"Usage: diagnostic_launcher.exe <ide_path> [test_file]" << std::endl;
        std::wcout << L"Example: diagnostic_launcher.exe \"C:\\RawrXD-Win32IDE.exe\" \"test.cpp\"" << std::endl;
        return 1;
    }
    
    std::wstring idePath = argv[1];
    std::wstring testFile;
    
    if (argc >= 3) {
        testFile = argv[2];
    }
    
    return RunDiagnostic(idePath, testFile);
}
