#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>

// Autonomous Agent for Win32IDE Digestion Self-Healing
// Implements:
// - Checkpoint/Beacon system for fault recovery
// - Continuous self-diagnosis
// - Automatic remediation
// - State persistence and resumption

enum class AgentState {
    INITIALIZING,
    IDLE,
    DIAGNOSING,
    TESTING_ENGINE,
    TESTING_MESSAGE_FLOW,
    TESTING_HOTKEY,
    TESTING_THREADING,
    TESTING_CALLBACKS,
    HEALING,
    RECOVERED,
    FAILED,
    REPORTING
};

enum class BreakPoint {
    NONE,
    WINDOW_CREATION,
    MESSAGE_DISPATCH,
    HOTKEY_RECEPTION,
    FILE_OPEN,
    CONTEXT_ALLOCATION,
    MESSAGE_SEND,
    THREAD_SPAWN,
    ENGINE_CALL,
    CALLBACK_ROUTING,
    COMPLETION_POST
};

struct BeaconCheckpoint {
    BreakPoint lastGoodPoint;
    DWORD timestamp;
    DWORD processId;
    std::string stateData;
    HANDLE resumeEvent;
    bool isValid;
};

struct DiagnosticResult {
    bool passed;
    BreakPoint failurePoint;
    std::string errorMessage;
    DWORD errorCode;
    std::string remediation;
    DWORD executionTimeMs;
};

struct AgentMetrics {
    DWORD totalTests;
    DWORD passedTests;
    DWORD failedTests;
    DWORD recoveryAttempts;
    DWORD successfulRecoveries;
    std::vector<DiagnosticResult> results;
};

class Win32IDEAutonomousAgent {
public:
    Win32IDEAutonomousAgent();
    ~Win32IDEAutonomousAgent();

    // Initialize agent
    bool initialize();

    // Run autonomous diagnostics and healing loop
    bool runAutonomousLoop();

    // Core operations
    bool diagnoseDigestionFlow();
    bool testEngineDirectly();
    bool testMessageDispatching();
    bool testHotkeyReception();
    bool testThreading();
    bool testCallbackRouting();

    // Beacon system
    bool createBeacon(BreakPoint point);
    bool loadBeacon(BeaconCheckpoint& checkpoint);
    bool resumeFromBeacon(const BeaconCheckpoint& checkpoint);
    bool clearBeacons();

    // Self-healing
    bool attemptHealing(BreakPoint failurePoint);
    bool remediateWindowIssue();
    bool remediateMessageDispatch();
    bool remediateHotkeyHandler();
    bool remediateThreading();
    bool remediateCallbacks();

    // Reporting
    bool generateReport(const std::string& outputPath);
    void logBeacon(const std::string& message);
    void logDiagnostic(const std::string& message);

    // State management
    AgentState getCurrentState() const { return m_currentState; }
    const AgentMetrics& getMetrics() const { return m_metrics; }

private:
    // Core state
    AgentState m_currentState;
    AgentMetrics m_metrics;
    BeaconCheckpoint m_currentBeacon;
    DWORD m_agentProcessId;

    // Process management
    HANDLE m_ideProcess;
    DWORD m_ideProcessId;
    HWND m_ideMainWindow;

    // Autonomous loop control
    std::atomic<bool> m_running;
    std::atomic<int> m_loopCount;
    HANDLE m_diagnosticsThread;
    HANDLE m_healingThread;

    // Message queue for inter-thread communication
    std::queue<std::pair<BreakPoint, std::string>> m_diagnosticQueue;
    CRITICAL_SECTION m_queueLock;

    // Beacon storage
    std::map<BreakPoint, BeaconCheckpoint> m_beaconStore;
    HANDLE m_beaconFile;

    // Test utilities
    bool launchIDEProcess();
    bool terminateIDEProcess();
    bool findIDEWindow();
    bool waitForWindowReady(DWORD timeoutMs);
    
    // Diagnostic utilities
    bool executeTestHarness(const std::string& outputPath);
    bool monitorProcessOutput(HANDLE hProcess);
    bool captureDebugOutput(std::vector<std::string>& output);
    
    // Healing utilities
    bool injectHeartbeat();
    bool reloadModules();
    bool resetState();

    // Static thread functions
    static DWORD WINAPI DiagnosticsThreadProc(LPVOID lpParam);
    static DWORD WINAPI HealingThreadProc(LPVOID lpParam);
};
