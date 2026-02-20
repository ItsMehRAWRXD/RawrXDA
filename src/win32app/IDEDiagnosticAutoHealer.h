// IDEDiagnosticAutoHealer.h - Autonomous Win32 IDE Self-Healing Diagnostic System
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <queue>
// ============================================================================
// BEACON SYSTEM - Persistent State Checkpoints
// ============================================================================

enum class BeaconStage {
    IDE_LAUNCH = 0,           // IDE process started
    WINDOW_CREATED = 1,       // Main window created
    MENU_INITIALIZED = 2,     // Menu system ready
    EDITOR_READY = 3,         // Editor window active
    FILE_OPENED = 4,          // Test file loaded in editor
    HOTKEY_SENT = 5,          // Ctrl+Shift+D sent
    MESSAGE_RECEIVED = 6,     // WM_RUN_DIGESTION received
    THREAD_SPAWNED = 7,       // Digestion thread created
    ENGINE_RUNNING = 8,       // Engine executing
    ENGINE_COMPLETE = 9,      // Engine finished
    OUTPUT_VERIFIED = 10,     // Digest file verified
    SUCCESS = 11              // Full cycle complete
};

struct BeaconCheckpoint {
    BeaconStage stage;
    DWORD timestamp;
    HRESULT result;
    std::string diagnosticData;
    std::string stackTrace;
};

// ============================================================================
// DIAGNOSTIC TESTS
// ============================================================================

enum class DiagnosticTest {
    ENGINE_LOAD,
    MESSAGE_LOOP,
    HOTKEY_SYSTEM,
    DIGESTION_PIPELINE,
    MEMORY_INTEGRITY,
    FILE_ACCESS,
    WINDOW_HIERARCHY,
    CALLBACK_ROUTING,
    CONTEXT_VALIDATION,
    ERROR_HANDLING
};

struct DiagnosticResult {
    DiagnosticTest test;
    bool passed;
    HRESULT errorCode;
    std::string failureReason;
    std::string remediation;
    DWORD executionTime;
};

// ============================================================================
// AUTO-HEALING STRATEGIES
// ============================================================================

enum class HealingStrategy {
    HOTKEY_RESEND = 0,           // Re-send Ctrl+Shift+D
    FILE_REOPEN = 1,             // Re-open test file
    MESSAGE_REPOST = 2,          // Re-post digestion message
    THREAD_RESTART = 3,          // Restart digestion thread
    ENGINE_RELOAD = 4,           // Reload engine DLL
    WINDOW_REFOCUS = 5,          // Re-focus IDE window
    PROCESS_RESTART = 6,         // Restart IDE process
    FULL_DIAGNOSTIC_RESET = 7    // Reset all subsystems
};

// ============================================================================
// IDE DIAGNOSTIC AUTO-HEALER
// ============================================================================

class IDEDiagnosticAutoHealer {
public:
    static IDEDiagnosticAutoHealer& Instance();
    
    // Lifecycle
    void StartFullDiagnostic();
    void StopDiagnostic();
    bool IsRunning() const { return m_running; }
    
    // Manual checkpoint recovery
    void RecoverFromCheckpoint(BeaconStage stage);
    void ResumeFromLastKnownGood();
    
    // Real-time monitoring
    void MonitorIDEProcess(DWORD processId);
    void DetectAndHealFailures();
    
    // Reporting
    std::string GenerateDiagnosticReport();
    std::string GenerateHealingLog();
    
private:
    IDEDiagnosticAutoHealer() = default;
    ~IDEDiagnosticAutoHealer() = default;
    
    // Stage executors
    HRESULT ExecuteIDELaunch();
    HRESULT ExecuteWindowCreation();
    HRESULT ExecuteMenuInit();
    HRESULT ExecuteEditorReady();
    HRESULT ExecuteFileOpen();
    HRESULT ExecuteHotkeySend();
    HRESULT ExecuteMessageReceived();
    HRESULT ExecuteThreadSpawn();
    HRESULT ExecuteEngineRun();
    HRESULT ExecuteEngineComplete();
    HRESULT ExecuteOutputVerify();
    HRESULT ExecuteSuccessValidation();
    
    // Diagnostic runners
    DiagnosticResult RunTest(DiagnosticTest test);
    void RunAllDiagnostics();
    
    // Healing actions
    void ApplyHealing(HealingStrategy strategy);
    void ExecuteHotkeyResend();
    void ExecuteFileReopen();
    void ExecuteMessageRepost();
    void ExecuteThreadRestart();
    void ExecuteEngineReload();
    void ExecuteWindowRefocus();
    void ExecuteProcessRestart();
    void ExecuteFullReset();
    
    // Beacon management
    void EmitBeacon(BeaconStage stage, HRESULT result, const std::string& data = "");
    BeaconCheckpoint ReadLastBeacon();
    void SaveBeacon(const BeaconCheckpoint& checkpoint);
    
    // Monitoring threads
    void MonitoringThreadProc();
    void DiagnosticThreadProc();
    void HealingThreadProc();
    
    // State management
    HANDLE m_idleEvent;
    HANDLE m_diagEvent;
    HANDLE m_healEvent;
    
    std::atomic<bool> m_running{false};
    std::atomic<BeaconStage> m_currentStage{BeaconStage::IDE_LAUNCH};
    std::atomic<bool> m_healingInProgress{false};
    std::atomic<int> m_healingAttempts{0};
    
    DWORD m_ideProcessId{0};
    HWND m_ideMainWindow{nullptr};
    HWND m_ideEditorWindow{nullptr};
    
    std::vector<DiagnosticResult> m_diagnosticResults;
    std::vector<HealingStrategy> m_appliedHealings;
    std::vector<BeaconCheckpoint> m_beaconHistory;
    
    std::unique_ptr<std::thread> m_monitorThread;
    std::unique_ptr<std::thread> m_diagnosticThread;
    std::unique_ptr<std::thread> m_healingThread;
    
    CRITICAL_SECTION m_lock;
    
    // Configuration
    struct {
        std::wstring ideExePath;
        std::wstring testFilePath;
        std::wstring diagnosticReportPath;
        DWORD maxRetries;
        DWORD timeoutMs;
        bool autoHealEnabled;
    } m_config;
};

// ============================================================================
// BEACON STORAGE
// ============================================================================

class BeaconStorage {
public:
    static BeaconStorage& Instance();
    
    void SaveCheckpoint(BeaconStage stage, HRESULT result, const std::string& data);
    BeaconCheckpoint LoadLastCheckpoint();
    void ClearHistory();
    
    std::vector<BeaconCheckpoint> GetFullHistory() const;
    
private:
    BeaconStorage() = default;
    std::wstring GetBeaconFilePath() const;
    
    std::vector<BeaconCheckpoint> m_checkpoints;
    CRITICAL_SECTION m_lock;
};

// ============================================================================
// DIAGNOSTIC ENGINE
// ============================================================================

class DiagnosticEngine {
public:
    DiagnosticEngine();
    ~DiagnosticEngine();
    
    // Test execution
    DiagnosticResult TestEngineLoad();
    DiagnosticResult TestMessageLoopHealth();
    DiagnosticResult TestHotkeySending();
    DiagnosticResult TestMessageDispatching();
    DiagnosticResult TestThreadCreation();
    DiagnosticResult TestDigestionEngineExecution();
    DiagnosticResult TestFileIOCapability();
    DiagnosticResult TestMemoryAllocation();
    DiagnosticResult TestCallbackRouting();
    DiagnosticResult TestWindowHierarchy();
    
    // Batch testing
    std::vector<DiagnosticResult> RunAllTests();
    
    // Reporting
    std::string GenerateReport(const std::vector<DiagnosticResult>& results);
    
private:
    HMODULE m_engineModule{nullptr};
};

// ============================================================================
// AUTO-HEALING ENGINE
// ============================================================================

class AutoHealingEngine {
public:
    AutoHealingEngine();
    ~AutoHealingEngine();
    
    // Healing execution
    bool HealHotkeySendFailure();
    bool HealMessageDispatchFailure();
    bool HealThreadCreationFailure();
    bool HealEngineLoadFailure();
    bool HealMemoryAllocationFailure();
    bool HealWindowFocusFailure();
    bool HealProcessFailure();
    
    // Strategy application
    bool ApplyHealingStrategy(HealingStrategy strategy);
    
    // State tracking
    int GetHealingAttemptCount() const { return m_healingAttempts; }
    bool IsMaxAttemptsReached() const { return m_healingAttempts >= m_maxAttempts; }
    
private:
    int m_healingAttempts{0};
    int m_maxAttempts{5};
    std::vector<HealingStrategy> m_appliedStrategies;
};

// ============================================================================
// REAL-TIME MONITORING
// ============================================================================

class ProcessMonitor {
public:
    ProcessMonitor(DWORD processId);
    ~ProcessMonitor();
    
    bool IsProcessAlive() const;
    bool IsMainWindowResponding() const;
    DWORD GetMainWindowThreadId() const;
    
    // Event tracking
    void MonitorDebugOutput();
    std::vector<std::string> GetRecentDebugLines() const;
    
private:
    DWORD m_processId;
    HANDLE m_processHandle;
    HWND m_mainWindow;
    std::queue<std::string> m_debugOutputBuffer;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

namespace DiagnosticUtils {
    // Process management
    DWORD LaunchIDEProcess(const std::wstring& exePath);
    bool TerminateProcessGracefully(DWORD processId);
    HWND FindIDEMainWindow(DWORD processId);
    
    // Window interaction
    bool SendHotkey(HWND hwnd, UINT vk, bool ctrl, bool shift, bool alt);
    bool OpenFileInEditor(HWND hwnd, const std::wstring& filePath);
    
    // File operations
    bool VerifyDigestFileCreated(const std::wstring& sourceFile);
    std::string ReadDigestFile(const std::wstring& sourceFile);
    
    // Debug output capture
    void CaptureDebugOutput(std::vector<std::string>& buffer);
    
    // Timing
    DWORD GetTimestamp();
    bool WaitForCondition(std::function<bool()> condition, DWORD timeoutMs);
    
    // Logging
    void LogDiagnostic(const std::string& message);
    void LogHealing(const std::string& action, bool success);
}

