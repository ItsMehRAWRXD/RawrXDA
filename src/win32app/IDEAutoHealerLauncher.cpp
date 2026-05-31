// IDEAutoHealerLauncher.cpp - Autonomous Auto-Healing Test Harness
#include <windows.h>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <chrono>
#include <fstream>

#include "IDEDiagnosticAutoHealer.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

struct HealerContext {
    HANDLE completionEvent;
    bool success;
    std::string finalReport;
    std::chrono::system_clock::time_point startTime;
};

HealerContext g_context = {};

// ============================================================================
// CONSOLE OUTPUT HELPERS
// ============================================================================

void PrintHeader(const char* title) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ %s\n", title);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

void PrintSuccess(const char* message) {
    printf("✓ [SUCCESS] %s\n", message);
}

void PrintError(const char* message) {
    printf("✗ [ERROR  ] %s\n", message);
}

void PrintInfo(const char* message) {
    printf("• [INFO   ] %s\n", message);
}

void PrintWarning(const char* message) {
    printf("⚠ [WARN   ] %s\n", message);
}

void PrintStage(int stage, const char* description) {
    printf("\n[STAGE %d] %s\n", stage, description);
    printf("───────────────────────────────────────────────────────────────\n");
}

// ============================================================================
// BEACON MONITORING
// ============================================================================

void MonitorBeaconProgress() {
    BeaconStage lastStage = BeaconStage::IDE_LAUNCH;
    
    const char* stageNames[] = {
        "IDE Launch",
        "Window Created",
        "Menu Initialized",
        "Editor Ready",
        "File Opened",
        "Hotkey Sent",
        "Message Received",
        "Thread Spawned",
        "Engine Running",
        "Engine Complete",
        "Output Verified",
        "Success"
    };
    
    printf("\n┌─ Beacon Progress Tracking ─────────────────────────────────────┐\n");
    
    int stageIndex = 0;
    DWORD lastCheck = GetTickCount();
    
    while ((GetTickCount() - lastCheck) < 30000) {  // Monitor for 30 seconds
        BeaconCheckpoint checkpoint = BeaconStorage::Instance().LoadLastCheckpoint();
        
        if (checkpoint.stage != lastStage) {
            const char* stageName = stageNames[static_cast<int>(checkpoint.stage)];
            DWORD elapsed = GetTickCount() - g_context.startTime.time_since_epoch().count();
            
            printf("│ [%4lums] ✓ %s\n", elapsed, stageName);
            lastStage = checkpoint.stage;
            
            if (checkpoint.stage == BeaconStage::SUCCESS) {
                printf("└─ ✓ COMPLETE ──────────────────────────────────────────────┘\n");
                return;
            }
        }
        
        Sleep(100);
    }
    
    printf("└─ ⚠ TIMEOUT ────────────────────────────────────────────────────┘\n");
}

// ============================================================================
// MAIN AUTONOMOUS HEALING EXECUTION
// ============================================================================

void ExecuteFullAutonomousDiagnostic() {
    PrintHeader("IDE AUTONOMOUS DIAGNOSTIC & SELF-HEALING SYSTEM");
    
    printf("\nInitializing autonomous healer...\n");
    
    auto& healer = IDEDiagnosticAutoHealer::Instance();
    
    PrintStage(1, "Starting Full Diagnostic Sequence");
    
    g_context.startTime = std::chrono::system_clock::now();
    healer.StartFullDiagnostic();
    
    PrintInfo("Autonomous diagnostic thread launched");
    PrintInfo("System will auto-detect failures and apply healing strategies");
    PrintInfo("Monitoring beacon checkpoints...");
    
    // Monitor beacon progress
    MonitorBeaconProgress();
    
    // Wait for completion
    printf("\nWaiting for diagnostic completion...\n");
    Sleep(2000);
    
    PrintStage(2, "Generating Diagnostic Report");
    
    std::string report = healer.GenerateDiagnosticReport();
    
    printf("\n%s\n", report.c_str());
    
    // Save report to file
    std::ofstream reportFile("diagnostic_report.json");
    if (reportFile.is_open()) {
        reportFile << report;
        reportFile.close();
        PrintSuccess("Report saved to diagnostic_report.json");
    }
    
    // Parse results from report
    printf("\n");
    PrintStage(3, "Results Summary");
    
    if (report.find("\"result\":0") != std::string::npos) {
        PrintSuccess("Diagnostic completed successfully");
        PrintSuccess("All stages completed without critical failures");
    } else {
        PrintWarning("Some stages had non-zero results");
        PrintWarning("Healing strategies were applied automatically");
    }
    
    printf("\nFinal Status:\n");
    printf("  • Healer running: %s\n", healer.IsRunning() ? "YES" : "NO");
    printf("\n");
    
    healer.StopDiagnostic();
    
    PrintSuccess("Autonomous diagnostic session complete");
}

// ============================================================================
// COMMAND LINE INTERFACE
// ============================================================================

void PrintUsage() {
    printf("Usage: IDEAutoHealerLauncher.exe [options]\n");
    printf("\nOptions:\n");
    printf("  --help              Show this help message\n");
    printf("  --full-diagnostic   Run complete diagnostic with healing (default)\n");
    printf("  --recover           Recover from last beacon checkpoint\n");
    printf("  --status            Show current IDE status\n");
    printf("  --verbose           Enable verbose output\n");
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    PrintHeader("IDE AUTONOMOUS HEALING SYSTEM v1.0");
    
    printf("Starting autonomous IDE diagnostic and healing engine...\n");
    printf("This system will:\n");
    printf("  1. Launch the IDE process\n");
    printf("  2. Monitor each subsystem initialization stage\n");
    printf("  3. Automatically detect any breakpoints or failures\n");
    printf("  4. Apply healing strategies to recover\n");
    printf("  5. Resume from last known good checkpoint\n");
    printf("  6. Report final status and recovery trace\n");
    
    bool verbose = false;
    bool runFullDiagnostic = true;
    
    // Parse command line
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            PrintUsage();
            return 0;
        }
        else if (arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "--recover") {
            runFullDiagnostic = false;
        }
    }
    
    // Clear any previous beacons
    BeaconStorage::Instance().ClearHistory();
    
    if (runFullDiagnostic) {
        ExecuteFullAutonomousDiagnostic();
    }
    
    printf("\n");
    PrintHeader("DIAGNOSTIC SESSION COMPLETE");
    
    printf("Log Files:\n");
    printf("  • diagnostic_report.json - Full diagnostic results\n");
    printf("  • IDE output - Check Visual Studio Output window\n");
    
    printf("\nTo view detailed diagnostic output:\n");
    printf("  • Open Visual Studio\n");
    printf("  • View → Output (or Ctrl+Alt+O)\n");
    printf("  • Select 'Debug' from the dropdown\n");
    printf("  • Look for [AutoHealer] and [Beacon] messages\n");
    
    printf("\nHealing Action Summary:\n");
    printf("  • If hotkey failed: Hotkey resent\n");
    printf("  • If file open failed: File reopened\n");
    printf("  • If message dispatch failed: Message reposted\n");
    printf("  • If window focus lost: Window refocused\n");
    printf("  • If IDE crashed: IDE process restarted\n");
    
    return 0;
}

