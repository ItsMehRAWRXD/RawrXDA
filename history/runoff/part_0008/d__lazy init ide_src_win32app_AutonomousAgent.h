#pragma once

#include <windows.h>
#include <string>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <map>
#include <utility>

// Autonomous Agent States
enum class AgentState {
    IDLE,
    DIAGNOSTIC_MODE,
    MONITORING,
    RECOVERY_MODE,
    BEACONING,
    SELF_HEALING,
    REPORTING,
    SHUTDOWN
};

// Beacon Types for Checkpointing
enum class BeaconType {
    STARTUP,
    ENGINE_LOADED,
    MESSAGE_LOOP_READY,
    HOTKEY_REGISTERED,
    DIGESTION_INIT,
    DIGESTION_QUEUED,
    DIGESTION_STARTED,
    DIGESTION_PROGRESS,
    DIGESTION_COMPLETE,
    ERROR_DETECTED,
    RECOVERY_ATTEMPT,
    RECOVERY_SUCCESS,
    RECOVERY_FAILED,
    BEACONING,
    MONITORING,
    SHUTDOWN_INIT
};

// Diagnostic Checkpoints
struct DiagnosticCheckpoint {
    BeaconType type;
    DWORD timestamp;
    DWORD threadId;
    HRESULT hr;
    std::string message;
    std::string context;
    
    DiagnosticCheckpoint(BeaconType t, HRESULT h = S_OK, const std::string& msg = "", const std::string& ctx = "")
        : type(t), timestamp(GetTickCount()), threadId(GetCurrentThreadId()), hr(h), message(msg), context(ctx) {}
};

// Self-Healing Actions
enum class HealingAction {
    NONE,
    RESTART_MESSAGE_LOOP,
    RELOAD_ENGINE,
    REINIT_HOTKEYS,
    CLEAR_CORRUPT_STATE,
    RESET_COUNTERS,
    RECREATE_WINDOWS,
    FULL_RESTART
};

// Agent Configuration
struct AgentConfig {
    bool enableAutoDiagnostics = true;
    bool enableBeaconing = true;
    bool enableSelfHealing = true;
    bool enableReporting = true;
    DWORD beaconIntervalMs = 1000;
    DWORD maxRecoveryAttempts = 3;
    DWORD recoveryTimeoutMs = 30000;
    std::string beaconLogPath = "C:\\RawrXD_Agent_Beacons.log";
    std::string diagnosticReportPath = "C:\\RawrXD_Diagnostics.json";
    std::string recoveryLogPath = "C:\\RawrXD_Recovery.log";
};

// Forward Declarations
class BeaconManager;
class DiagnosticEngine;
class SelfHealingEngine;
class DiagnosticReporter;

// Main Autonomous Agent Class
class AutonomousAgent {
private:
    static std::unique_ptr<AutonomousAgent> s_instance;
    static std::atomic<bool> s_initialized;
    
    AgentState m_state;
    AgentConfig m_config;
    std::atomic<bool> m_running;
    std::atomic<bool> m_diagnosticMode;
    
    std::unique_ptr<BeaconManager> m_beaconManager;
    std::unique_ptr<DiagnosticEngine> m_diagnosticEngine;
    std::unique_ptr<SelfHealingEngine> m_selfHealingEngine;
    std::unique_ptr<DiagnosticReporter> m_reporter;
    
    HANDLE m_agentThread;
    HANDLE m_monitorThread;
    HANDLE m_beaconThread;
    
    HWND m_hwndIDE;
    DWORD m_ideProcessId;
    
    // State tracking
    std::atomic<DWORD> m_lastKnownGoodState;
    std::atomic<DWORD> m_recoveryAttempts;
    std::atomic<bool> m_engineHealthy;
    std::atomic<bool> m_messageLoopHealthy;
    std::atomic<bool> m_hotkeySystemHealthy;
    std::atomic<bool> m_digestionSystemHealthy;
    
    // Digestion tracking
    std::atomic<DWORD> m_activeDigestionTask;
    std::atomic<bool> m_digestionInProgress;
    std::chrono::steady_clock::time_point m_digestionStartTime;
    std::wstring m_currentDigestionFile;
    HANDLE m_digestionThreadHandle;
    CRITICAL_SECTION m_digestionLock;
    CRITICAL_SECTION m_beaconLock;
    
    // Callbacks
    std::function<void(const DiagnosticCheckpoint&)> m_onCheckpoint;
    std::function<void(HealingAction)> m_onHealingAction;
    std::function<void(const std::string&)> m_onStatusUpdate;
    
    AutonomousAgent();
    
public:
    static AutonomousAgent* Instance();
    static void Initialize(const AgentConfig& config = {});
    static void Shutdown();
    
    // Core Operations
    bool Start();
    bool Stop();
    bool EnterDiagnosticMode();
    bool ExitDiagnosticMode();
    
    // Beaconing
    void EmitBeacon(BeaconType type, HRESULT hr = S_OK, const std::string& message = "", const std::string& context = "");
    void SetBeaconInterval(DWORD intervalMs);
    
    // Diagnostics
    bool RunFullDiagnostics();
    bool ValidateEngineHealth();
    bool ValidateMessageLoopHealth();
    bool ValidateHotkeySystem();
    bool ValidateDigestionSystem();
    std::vector<DiagnosticCheckpoint> GetDiagnosticHistory() const;
    
    // Self-Healing
    bool AttemptRecovery(HealingAction action);
    bool AutoHeal();
    void ResetHealthState();
    
    // Reporting
    bool GenerateDiagnosticReport();
    bool GenerateRecoveryReport();
    std::string GetStatusString() const;
    
    // IDE Integration
    void SetIDEWindow(HWND hwnd) { m_hwndIDE = hwnd; }
    void SetIDEProcessId(DWORD pid) { m_ideProcessId = pid; }
    HWND GetIDEWindow() const { return m_hwndIDE; }
    
    // Callbacks
    void SetCheckpointCallback(std::function<void(const DiagnosticCheckpoint&)> callback) { m_onCheckpoint = callback; }
    void SetHealingActionCallback(std::function<void(HealingAction)> callback) { m_onHealingAction = callback; }
    void SetStatusUpdateCallback(std::function<void(const std::string&)> callback) { m_onStatusUpdate = callback; }
    
    // Digestion Pipeline Monitoring
    void OnDigestionHotkeyPressed();
    void OnDigestionQueued(DWORD taskId, const std::wstring& source);
    void OnDigestionThreadSpawned(DWORD taskId, HANDLE hThread);
    void OnDigestionEngineInvoked(DWORD taskId);
    void OnDigestionProgress(DWORD taskId, DWORD percent);
    void OnDigestionComplete(DWORD taskId, DWORD result);
    void OnDigestionError(DWORD taskId, HRESULT hr, const std::string& context);
    
    // Autonomous Digestion Control
    bool TriggerAutonomousDigestion(const std::wstring& filepath);
    bool ValidateDigestionPrerequisites();
    bool MonitorDigestionExecution(DWORD taskId, DWORD timeoutMs = 30000);
    
    // State Queries
    AgentState GetState() const { return m_state; }
    bool IsRunning() const { return m_running; }
    bool IsDiagnosticMode() const { return m_diagnosticMode; }
    bool IsHealthy() const;
    DWORD GetActiveDigestionTaskId() const { return m_activeDigestionTask; }
    
    // Utility
    static std::string BeaconTypeToString(BeaconType type);
    static std::string AgentStateToString(AgentState state);
    static std::string HealingActionToString(HealingAction action);
    
private:
    // Thread Procedures
    static DWORD WINAPI AgentThreadProc(LPVOID lpParam);
    static DWORD WINAPI MonitorThreadProc(LPVOID lpParam);
    static DWORD WINAPI BeaconThreadProc(LPVOID lpParam);
    
    // Internal Methods
    void RunAgentLoop();
    void RunMonitorLoop();
    void RunBeaconLoop();
    void ProcessStateTransitions();
    void CheckSystemHealth();
    void ExecuteHealingAction(HealingAction action);
    void UpdateStatus(const std::string& status);
    
    // State Management
    void TransitionToState(AgentState newState);
    void RecordLastKnownGoodState(BeaconType type);
    bool CanAttemptRecovery() const;
    
    // IDE Process Monitoring
    bool IsIDEProcessAlive() const;
    bool IsIDEWindowValid() const;
    DWORD GetIDEProcessId() const;
};

// Beacon Manager - Handles State Checkpointing
class BeaconManager {
private:
    std::vector<DiagnosticCheckpoint> m_beaconHistory;
    std::atomic<DWORD> m_beaconCount;
    mutable CRITICAL_SECTION m_beaconLock;
    std::string m_logPath;
    
public:
    BeaconManager(const std::string& logPath);
    ~BeaconManager();
    
    void EmitBeacon(const DiagnosticCheckpoint& checkpoint);
    std::vector<DiagnosticCheckpoint> GetBeaconHistory() const;
    DiagnosticCheckpoint GetLastBeacon() const;
    DiagnosticCheckpoint GetLastBeaconOfType(BeaconType type) const;
    bool HasBeaconType(BeaconType type) const;
    void ClearHistory();
    void SetLogPath(const std::string& path);
    bool ExportToFile(const std::string& path) const;
    
private:
    void WriteBeaconToLog(const DiagnosticCheckpoint& checkpoint);
    std::string FormatBeacon(const DiagnosticCheckpoint& checkpoint) const;
};

// Diagnostic Engine - Performs Health Checks
class DiagnosticEngine {
private:
    std::atomic<bool> m_running;
    std::vector<std::pair<std::string, std::function<bool()>>> m_diagnosticTests;
    std::vector<std::pair<std::string, bool>> m_testResults;
    
public:
    DiagnosticEngine();
    ~DiagnosticEngine();
    
    void AddTest(const std::string& name, std::function<bool()> test);
    bool RunAllTests();
    bool RunTest(const std::string& name);
    std::vector<std::pair<std::string, bool>> GetResults() const;
    std::string GetReport() const;
    void ClearTests();
    
    // Pre-defined tests
    static bool TestEngineLoad();
    static bool TestMessageLoop();
    static bool TestHotkeyRegistration();
    static bool TestDigestionEngine();
    static bool TestMemoryAllocation();
    static bool TestThreadCreation();
    static bool TestFileAccess();
    static bool TestWindowMessages();
};

// Self-Healing Engine - Performs Recovery Actions
class SelfHealingEngine {
private:
    std::atomic<bool> m_running;
    std::map<HealingAction, std::function<bool()>> m_healingActions;
    std::vector<std::pair<HealingAction, bool>> m_actionHistory;
    DWORD m_maxAttempts;
    
public:
    SelfHealingEngine(DWORD maxAttempts = 3);
    ~SelfHealingEngine();
    
    void RegisterAction(HealingAction action, std::function<bool()> handler);
    bool ExecuteAction(HealingAction action);
    bool ExecuteActions(const std::vector<HealingAction>& actions);
    std::vector<std::pair<HealingAction, bool>> GetActionHistory() const;
    void ClearHistory();
    void SetMaxAttempts(DWORD attempts);
    
    // Pre-defined healing actions
    static bool RestartMessageLoop();
    static bool ReloadEngine();
    static bool ReinitHotkeys();
    static bool ClearCorruptState();
    static bool ResetCounters();
    static bool RecreateWindows();
    static bool PerformFullRestart();
};

// Diagnostic Reporter - Generates Reports
class DiagnosticReporter {
private:
    std::string m_reportPath;
    std::vector<DiagnosticCheckpoint> m_diagnostics;
    std::vector<std::pair<HealingAction, bool>> m_healingHistory;
    
public:
    DiagnosticReporter(const std::string& reportPath);
    ~DiagnosticReporter();
    
    void AddDiagnostic(const DiagnosticCheckpoint& checkpoint);
    void AddHealingRecord(HealingAction action, bool success);
    bool GenerateJSONReport() const;
    bool GenerateHTMLReport() const;
    bool GenerateTextReport() const;
    void Clear();
    
private:
    std::string FormatTimestamp(DWORD timestamp) const;
    std::string EscapeJSON(const std::string& str) const;
};

// Utility Functions
namespace AgentUtils {
    std::string GetLastSystemError();
    bool WriteLogEntry(const std::string& path, const std::string& entry);
    bool EnsureDirectoryExists(const std::string& path);
    std::string GenerateSessionId();
    DWORD GetSystemUptimeMs();
    bool IsDebuggerAttached();
    void OutputAgentLog(const std::string& message);
}

// Global Helper Macros
#define AGENT_BEACON(type) AutonomousAgent::Instance()->EmitBeacon(type)
#define AGENT_BEACON_MSG(type, msg) AutonomousAgent::Instance()->EmitBeacon(type, S_OK, msg)
#define AGENT_BEACON_ERR(type, hr, msg) AutonomousAgent::Instance()->EmitBeacon(type, hr, msg)
#define AGENT_CHECKPOINT() AutonomousAgent::Instance()->RecordLastKnownGoodState(__FUNCTION__)

// Error Recovery Macros
#define AGENT_TRY(expr) do { \
    HRESULT _hr = (expr); \
    if (FAILED(_hr)) { \
        AutonomousAgent::Instance()->EmitBeacon(BeaconType::ERROR_DETECTED, _hr, #expr); \
        if (AutonomousAgent::Instance()->AutoHeal()) { \
            _hr = (expr); \
        } \
    } \
} while(0)

// Health Check Macros
#define AGENT_VALIDATE_HEALTH() AutonomousAgent::Instance()->IsHealthy()
#define AGENT_ENSURE_HEALTH() if (!AGENT_VALIDATE_HEALTH()) { \
    AutonomousAgent::Instance()->EnterDiagnosticMode(); \
}
