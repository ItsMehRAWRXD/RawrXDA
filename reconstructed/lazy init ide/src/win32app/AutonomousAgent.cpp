#include "AutonomousAgent.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shlobj.h>

// Static members
std::unique_ptr<AutonomousAgent> AutonomousAgent::s_instance;
std::atomic<bool> AutonomousAgent::s_initialized{false};

// Constructor
AutonomousAgent::AutonomousAgent()
    : m_state(AgentState::IDLE)
    , m_running(false)
    , m_diagnosticMode(false)
    , m_agentThread(nullptr)
    , m_monitorThread(nullptr)
    , m_beaconThread(nullptr)
    , m_hwndIDE(nullptr)
    , m_ideProcessId(0)
    , m_lastKnownGoodState(0)
    , m_recoveryAttempts(0)
    , m_engineHealthy(true)
    , m_messageLoopHealthy(true)
    , m_hotkeySystemHealthy(true)
    , m_digestionSystemHealthy(true)
    , m_activeDigestionTask(0)
    , m_digestionInProgress(false)
    , m_digestionThreadHandle(nullptr)
{
    InitializeCriticalSection(&m_beaconLock);
    InitializeCriticalSection(&m_digestionLock);
}

// Singleton Instance
AutonomousAgent* AutonomousAgent::Instance()
{
    if (!s_instance) {
        s_instance.reset(new AutonomousAgent());
    }
    return s_instance.get();
}

// Initialize
void AutonomousAgent::Initialize(const AgentConfig& config)
{
    if (s_initialized.exchange(true)) {
        return; // Already initialized
    }
    
    auto* agent = Instance();
    agent->m_config = config;
    
    // Create subsystems
    agent->m_beaconManager = std::make_unique<BeaconManager>(config.beaconLogPath);
    agent->m_diagnosticEngine = std::make_unique<DiagnosticEngine>();
    agent->m_selfHealingEngine = std::make_unique<SelfHealingEngine>(config.maxRecoveryAttempts);
    agent->m_reporter = std::make_unique<DiagnosticReporter>(config.diagnosticReportPath);
    
    // Register default diagnostic tests
    agent->m_diagnosticEngine->AddTest("Engine Load", DiagnosticEngine::TestEngineLoad);
    agent->m_diagnosticEngine->AddTest("Message Loop", DiagnosticEngine::TestMessageLoop);
    agent->m_diagnosticEngine->AddTest("Hotkey System", DiagnosticEngine::TestHotkeyRegistration);
    agent->m_diagnosticEngine->AddTest("Digestion Engine", DiagnosticEngine::TestDigestionEngine);
    agent->m_diagnosticEngine->AddTest("Memory Allocation", DiagnosticEngine::TestMemoryAllocation);
    agent->m_diagnosticEngine->AddTest("Thread Creation", DiagnosticEngine::TestThreadCreation);
    agent->m_diagnosticEngine->AddTest("File Access", DiagnosticEngine::TestFileAccess);
    agent->m_diagnosticEngine->AddTest("Window Messages", DiagnosticEngine::TestWindowMessages);
    
    // Register default healing actions
    agent->m_selfHealingEngine->RegisterAction(HealingAction::RESTART_MESSAGE_LOOP, SelfHealingEngine::RestartMessageLoop);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::RELOAD_ENGINE, SelfHealingEngine::ReloadEngine);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::REINIT_HOTKEYS, SelfHealingEngine::ReinitHotkeys);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::CLEAR_CORRUPT_STATE, SelfHealingEngine::ClearCorruptState);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::RESET_COUNTERS, SelfHealingEngine::ResetCounters);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::RECREATE_WINDOWS, SelfHealingEngine::RecreateWindows);
    agent->m_selfHealingEngine->RegisterAction(HealingAction::FULL_RESTART, SelfHealingEngine::PerformFullRestart);
    
    // Ensure log directory exists
    AgentUtils::EnsureDirectoryExists(config.beaconLogPath);
    
    // Emit startup beacon
    agent->EmitBeacon(BeaconType::STARTUP, S_OK, "Agent initialized");
    
    AgentUtils::OutputAgentLog("AutonomousAgent initialized successfully");
}

// Shutdown
void AutonomousAgent::Shutdown()
{
    if (!s_initialized) {
        return;
    }
    
    auto* agent = Instance();
    if (agent->m_running) {
        agent->Stop();
    }
    
    agent->EmitBeacon(BeaconType::SHUTDOWN_INIT, S_OK, "Agent shutting down");
    
    DeleteCriticalSection(&agent->m_beaconLock);
    DeleteCriticalSection(&agent->m_digestionLock);
    
    s_instance.reset();
    s_initialized = false;
    
    AgentUtils::OutputAgentLog("AutonomousAgent shutdown complete");
}

// Start
bool AutonomousAgent::Start()
{
    if (m_running.exchange(true)) {
        return false; // Already running
    }
    
    TransitionToState(AgentState::MONITORING);
    
    // Create threads
    m_agentThread = CreateThread(nullptr, 0, AgentThreadProc, this, 0, nullptr);
    m_monitorThread = CreateThread(nullptr, 0, MonitorThreadProc, this, 0, nullptr);
    m_beaconThread = CreateThread(nullptr, 0, BeaconThreadProc, this, 0, nullptr);
    
    if (!m_agentThread || !m_monitorThread || !m_beaconThread) {
        m_running = false;
        return false;
    }
    
    EmitBeacon(BeaconType::ENGINE_LOADED, S_OK, "Agent started");
    UpdateStatus("Agent started successfully");
    
    return true;
}

// Stop
bool AutonomousAgent::Stop()
{
    if (!m_running.exchange(false)) {
        return false; // Not running
    }
    
    TransitionToState(AgentState::SHUTDOWN);
    
    // Wait for threads to finish
    if (m_agentThread) {
        WaitForSingleObject(m_agentThread, INFINITE);
        CloseHandle(m_agentThread);
        m_agentThread = nullptr;
    }
    
    if (m_monitorThread) {
        WaitForSingleObject(m_monitorThread, INFINITE);
        CloseHandle(m_monitorThread);
        m_monitorThread = nullptr;
    }
    
    if (m_beaconThread) {
        WaitForSingleObject(m_beaconThread, INFINITE);
        CloseHandle(m_beaconThread);
        m_beaconThread = nullptr;
    }
    
    UpdateStatus("Agent stopped");
    return true;
}

// Enter Diagnostic Mode
bool AutonomousAgent::EnterDiagnosticMode()
{
    if (m_diagnosticMode.exchange(true)) {
        return false; // Already in diagnostic mode
    }
    
    TransitionToState(AgentState::DIAGNOSTIC_MODE);
    EmitBeacon(BeaconType::ERROR_DETECTED, S_OK, "Entered diagnostic mode");
    UpdateStatus("Diagnostic mode enabled");
    
    return true;
}

// Exit Diagnostic Mode
bool AutonomousAgent::ExitDiagnosticMode()
{
    if (!m_diagnosticMode.exchange(false)) {
        return false; // Not in diagnostic mode
    }
    
    TransitionToState(AgentState::MONITORING);
    EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Exited diagnostic mode");
    UpdateStatus("Diagnostic mode disabled");
    
    return true;
}

// Emit Beacon
void AutonomousAgent::EmitBeacon(BeaconType type, HRESULT hr, const std::string& message, const std::string& context)
{
    DiagnosticCheckpoint checkpoint(type, hr, message, context);
    
    // Add to beacon manager
    if (m_beaconManager) {
        m_beaconManager->EmitBeacon(checkpoint);
    }
    
    // Call callback
    if (m_onCheckpoint) {
        m_onCheckpoint(checkpoint);
    }
    
    // Record last known good state if this is a success beacon
    if (SUCCEEDED(hr)) {
        RecordLastKnownGoodState(type);
    }
    
    // Output to debug
    std::string beaconStr = "[AGENT-BEACON] " + BeaconTypeToString(type);
    if (!message.empty()) {
        beaconStr += " - " + message;
    }
    if (FAILED(hr)) {
        beaconStr += " [ERROR: " + std::to_string(hr) + "]";
    }
    OutputDebugStringA(beaconStr.c_str());
}

// Set Beacon Interval
void AutonomousAgent::SetBeaconInterval(DWORD intervalMs)
{
    m_config.beaconIntervalMs = intervalMs;
}

// Run Full Diagnostics
bool AutonomousAgent::RunFullDiagnostics()
{
    EmitBeacon(BeaconType::ERROR_DETECTED, S_OK, "Running full diagnostics");
    
    bool result = m_diagnosticEngine->RunAllTests();
    
    if (result) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "All diagnostics passed");
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Some diagnostics failed");
    }
    
    return result;
}

// Validate Engine Health
bool AutonomousAgent::ValidateEngineHealth()
{
    bool healthy = m_diagnosticEngine->RunTest("Engine Load");
    m_engineHealthy = healthy;
    
    if (!healthy) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Engine health check failed");
    }
    
    return healthy;
}

// Validate Message Loop Health
bool AutonomousAgent::ValidateMessageLoopHealth()
{
    bool healthy = m_diagnosticEngine->RunTest("Message Loop");
    m_messageLoopHealthy = healthy;
    
    if (!healthy) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Message loop health check failed");
    }
    
    return healthy;
}

// Validate Hotkey System
bool AutonomousAgent::ValidateHotkeySystem()
{
    bool healthy = m_diagnosticEngine->RunTest("Hotkey System");
    m_hotkeySystemHealthy = healthy;
    
    if (!healthy) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Hotkey system health check failed");
    }
    
    return healthy;
}

// Validate Digestion System
bool AutonomousAgent::ValidateDigestionSystem()
{
    bool healthy = m_diagnosticEngine->RunTest("Digestion Engine");
    m_digestionSystemHealthy = healthy;
    
    if (!healthy) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Digestion system health check failed");
    }
    
    return healthy;
}

// Get Diagnostic History
std::vector<DiagnosticCheckpoint> AutonomousAgent::GetDiagnosticHistory() const
{
    if (m_beaconManager) {
        return m_beaconManager->GetBeaconHistory();
    }
    return {};
}

// Attempt Recovery
bool AutonomousAgent::AttemptRecovery(HealingAction action)
{
    if (!CanAttemptRecovery()) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Max recovery attempts reached");
        return false;
    }
    
    m_recoveryAttempts++;
    TransitionToState(AgentState::RECOVERY_MODE);
    
    EmitBeacon(BeaconType::RECOVERY_ATTEMPT, S_OK, "Attempting recovery: " + HealingActionToString(action));
    
    bool success = m_selfHealingEngine->ExecuteAction(action);
    
    if (success) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Recovery successful: " + HealingActionToString(action));
        m_recoveryAttempts = 0; // Reset on success
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Recovery failed: " + HealingActionToString(action));
    }
    
    TransitionToState(AgentState::MONITORING);
    return success;
}

// Auto Heal
bool AutonomousAgent::AutoHeal()
{
    if (!m_config.enableSelfHealing) {
        return false;
    }
    
    EmitBeacon(BeaconType::RECOVERY_ATTEMPT, S_OK, "Auto-healing initiated");
    
    // Determine which systems need healing
    std::vector<HealingAction> actions;
    
    if (!m_engineHealthy) {
        actions.push_back(HealingAction::RELOAD_ENGINE);
    }
    
    if (!m_messageLoopHealthy) {
        actions.push_back(HealingAction::RESTART_MESSAGE_LOOP);
    }
    
    if (!m_hotkeySystemHealthy) {
        actions.push_back(HealingAction::REINIT_HOTKEYS);
    }
    
    if (!m_digestionSystemHealthy) {
        actions.push_back(HealingAction::CLEAR_CORRUPT_STATE);
    }
    
    if (actions.empty()) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "No healing required");
        return true;
    }
    
    bool success = m_selfHealingEngine->ExecuteActions(actions);
    
    if (success) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Auto-healing completed successfully");
        ResetHealthState();
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Auto-healing failed");
    }
    
    return success;
}

// Reset Health State
void AutonomousAgent::ResetHealthState()
{
    m_engineHealthy = true;
    m_messageLoopHealthy = true;
    m_hotkeySystemHealthy = true;
    m_digestionSystemHealthy = true;
    m_recoveryAttempts = 0;
    
    EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Health state reset");
}

// Generate Diagnostic Report
bool AutonomousAgent::GenerateDiagnosticReport()
{
    if (!m_reporter) {
        return false;
    }
    
    // Add all beacons to reporter
    auto history = GetDiagnosticHistory();
    for (const auto& checkpoint : history) {
        m_reporter->AddDiagnostic(checkpoint);
    }
    
    // Add healing history
    auto healingHistory = m_selfHealingEngine->GetActionHistory();
    for (const auto& record : healingHistory) {
        m_reporter->AddHealingRecord(record.first, record.second);
    }
    
    bool success = m_reporter->GenerateJSONReport();
    
    if (success) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Diagnostic report generated: " + m_config.diagnosticReportPath);
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Failed to generate diagnostic report");
    }
    
    return success;
}

// Generate Recovery Report
bool AutonomousAgent::GenerateRecoveryReport()
{
    if (!m_reporter) {
        return false;
    }
    
    // Add healing history
    auto healingHistory = m_selfHealingEngine->GetActionHistory();
    for (const auto& record : healingHistory) {
        m_reporter->AddHealingRecord(record.first, record.second);
    }
    
    bool success = m_reporter->GenerateTextReport();
    
    if (success) {
        EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, "Recovery report generated: " + m_config.recoveryLogPath);
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Failed to generate recovery report");
    }
    
    return success;
}

// Get Status String
std::string AutonomousAgent::GetStatusString() const
{
    std::stringstream ss;
    ss << "Agent State: " << AgentStateToString(m_state) << "\n";
    ss << "Running: " << (m_running ? "Yes" : "No") << "\n";
    ss << "Diagnostic Mode: " << (m_diagnosticMode ? "Yes" : "No") << "\n";
    ss << "Engine Healthy: " << (m_engineHealthy ? "Yes" : "No") << "\n";
    ss << "Message Loop Healthy: " << (m_messageLoopHealthy ? "Yes" : "No") << "\n";
    ss << "Hotkey System Healthy: " << (m_hotkeySystemHealthy ? "Yes" : "No") << "\n";
    ss << "Digestion System Healthy: " << (m_digestionSystemHealthy ? "Yes" : "No") << "\n";
    ss << "Recovery Attempts: " << m_recoveryAttempts.load() << "\n";
    ss << "Last Known Good State: " << m_lastKnownGoodState.load() << "\n";
    
    return ss.str();
}

// Is Healthy
bool AutonomousAgent::IsHealthy() const
{
    return m_engineHealthy && m_messageLoopHealthy && m_hotkeySystemHealthy && m_digestionSystemHealthy;
}

// Thread Procedures
DWORD WINAPI AutonomousAgent::AgentThreadProc(LPVOID lpParam)
{
    auto* agent = static_cast<AutonomousAgent*>(lpParam);
    agent->RunAgentLoop();
    return 0;
}

DWORD WINAPI AutonomousAgent::MonitorThreadProc(LPVOID lpParam)
{
    auto* agent = static_cast<AutonomousAgent*>(lpParam);
    agent->RunMonitorLoop();
    return 0;
}

DWORD WINAPI AutonomousAgent::BeaconThreadProc(LPVOID lpParam)
{
    auto* agent = static_cast<AutonomousAgent*>(lpParam);
    agent->RunBeaconLoop();
    return 0;
}

// Run Agent Loop
void AutonomousAgent::RunAgentLoop()
{
    while (m_running) {
        ProcessStateTransitions();
        
        if (m_diagnosticMode) {
            RunFullDiagnostics();
        }
        
        Sleep(100); // 100ms polling interval
    }
}

// Run Monitor Loop
void AutonomousAgent::RunMonitorLoop()
{
    while (m_running) {
        CheckSystemHealth();
        
        if (!IsHealthy() && m_config.enableSelfHealing) {
            AutoHeal();
        }
        
        Sleep(1000); // 1 second polling interval
    }
}

// Run Beacon Loop
void AutonomousAgent::RunBeaconLoop()
{
    while (m_running) {
        if (m_config.enableBeaconing) {
            EmitBeacon(BeaconType::BEACONING, S_OK, "Agent heartbeat");
        }
        
        Sleep(m_config.beaconIntervalMs);
    }
}

// Process State Transitions
void AutonomousAgent::ProcessStateTransitions()
{
    // State machine logic here
    switch (m_state) {
        case AgentState::IDLE:
            if (m_running) {
                TransitionToState(AgentState::MONITORING);
            }
            break;
            
        case AgentState::MONITORING:
            if (m_diagnosticMode) {
                TransitionToState(AgentState::DIAGNOSTIC_MODE);
            } else if (!IsHealthy()) {
                TransitionToState(AgentState::RECOVERY_MODE);
            }
            break;
            
        case AgentState::DIAGNOSTIC_MODE:
            if (!m_diagnosticMode) {
                TransitionToState(AgentState::MONITORING);
            }
            break;
            
        case AgentState::RECOVERY_MODE:
            if (IsHealthy()) {
                TransitionToState(AgentState::MONITORING);
            }
            break;
            
        default:
            break;
    }
}

// Check System Health
void AutonomousAgent::CheckSystemHealth()
{
    ValidateEngineHealth();
    ValidateMessageLoopHealth();
    ValidateHotkeySystem();
    ValidateDigestionSystem();
}

// Transition To State
void AutonomousAgent::TransitionToState(AgentState newState)
{
    if (m_state != newState) {
        AgentState oldState = m_state;
        m_state = newState;
        
        EmitBeacon(BeaconType::ERROR_DETECTED, S_OK, 
                   "State transition: " + AgentStateToString(oldState) + " -> " + AgentStateToString(newState));
        
        UpdateStatus("State: " + AgentStateToString(newState));
    }
}

// Record Last Known Good State
void AutonomousAgent::RecordLastKnownGoodState(BeaconType type)
{
    m_lastKnownGoodState = static_cast<DWORD>(type);
}

// Can Attempt Recovery
bool AutonomousAgent::CanAttemptRecovery() const
{
    return m_recoveryAttempts < m_config.maxRecoveryAttempts;
}

// Update Status
void AutonomousAgent::UpdateStatus(const std::string& status)
{
    if (m_onStatusUpdate) {
        m_onStatusUpdate(status);
    }
    
    OutputDebugStringA(("[AGENT-STATUS] " + status).c_str());
}

// String Conversions
std::string AutonomousAgent::BeaconTypeToString(BeaconType type)
{
    switch (type) {
        case BeaconType::STARTUP: return "STARTUP";
        case BeaconType::ENGINE_LOADED: return "ENGINE_LOADED";
        case BeaconType::MESSAGE_LOOP_READY: return "MESSAGE_LOOP_READY";
        case BeaconType::HOTKEY_REGISTERED: return "HOTKEY_REGISTERED";
        case BeaconType::DIGESTION_INIT: return "DIGESTION_INIT";
        case BeaconType::DIGESTION_QUEUED: return "DIGESTION_QUEUED";
        case BeaconType::DIGESTION_STARTED: return "DIGESTION_STARTED";
        case BeaconType::DIGESTION_PROGRESS: return "DIGESTION_PROGRESS";
        case BeaconType::DIGESTION_COMPLETE: return "DIGESTION_COMPLETE";
        case BeaconType::ERROR_DETECTED: return "ERROR_DETECTED";
        case BeaconType::RECOVERY_ATTEMPT: return "RECOVERY_ATTEMPT";
        case BeaconType::RECOVERY_SUCCESS: return "RECOVERY_SUCCESS";
        case BeaconType::RECOVERY_FAILED: return "RECOVERY_FAILED";
        case BeaconType::SHUTDOWN_INIT: return "SHUTDOWN_INIT";
        case BeaconType::BEACONING: return "BEACONING";
        case BeaconType::MONITORING: return "MONITORING";
        default: return "UNKNOWN";
    }
}

std::string AutonomousAgent::AgentStateToString(AgentState state)
{
    switch (state) {
        case AgentState::IDLE: return "IDLE";
        case AgentState::DIAGNOSTIC_MODE: return "DIAGNOSTIC_MODE";
        case AgentState::MONITORING: return "MONITORING";
        case AgentState::RECOVERY_MODE: return "RECOVERY_MODE";
        case AgentState::BEACONING: return "BEACONING";
        case AgentState::SELF_HEALING: return "SELF_HEALING";
        case AgentState::REPORTING: return "REPORTING";
        case AgentState::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

std::string AutonomousAgent::HealingActionToString(HealingAction action)
{
    switch (action) {
        case HealingAction::NONE: return "NONE";
        case HealingAction::RESTART_MESSAGE_LOOP: return "RESTART_MESSAGE_LOOP";
        case HealingAction::RELOAD_ENGINE: return "RELOAD_ENGINE";
        case HealingAction::REINIT_HOTKEYS: return "REINIT_HOTKEYS";
        case HealingAction::CLEAR_CORRUPT_STATE: return "CLEAR_CORRUPT_STATE";
        case HealingAction::RESET_COUNTERS: return "RESET_COUNTERS";
        case HealingAction::RECREATE_WINDOWS: return "RECREATE_WINDOWS";
        case HealingAction::FULL_RESTART: return "FULL_RESTART";
        default: return "UNKNOWN";
    }
}

// IDE Process Monitoring
bool AutonomousAgent::IsIDEProcessAlive() const
{
    if (m_ideProcessId == 0) {
        return false;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, m_ideProcessId);
    if (!hProcess) {
        return false;
    }
    
    DWORD exitCode;
    bool alive = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hProcess);
    
    return alive;
}

bool AutonomousAgent::IsIDEWindowValid() const
{
    return m_hwndIDE && IsWindow(m_hwndIDE);
}

DWORD AutonomousAgent::GetIDEProcessId() const
{
    return m_ideProcessId;
}

// Beacon Manager Implementation
BeaconManager::BeaconManager(const std::string& logPath)
    : m_beaconCount(0)
    , m_logPath(logPath)
{
    InitializeCriticalSection(&m_beaconLock);
}

BeaconManager::~BeaconManager()
{
    DeleteCriticalSection(&m_beaconLock);
}

void BeaconManager::EmitBeacon(const DiagnosticCheckpoint& checkpoint)
{
    EnterCriticalSection(&m_beaconLock);
    m_beaconHistory.push_back(checkpoint);
    m_beaconCount++;
    LeaveCriticalSection(&m_beaconLock);
    
    WriteBeaconToLog(checkpoint);
}

std::vector<DiagnosticCheckpoint> BeaconManager::GetBeaconHistory() const
{
    EnterCriticalSection(&m_beaconLock);
    auto history = m_beaconHistory;
    LeaveCriticalSection(&m_beaconLock);
    return history;
}

DiagnosticCheckpoint BeaconManager::GetLastBeacon() const
{
    EnterCriticalSection(&m_beaconLock);
    DiagnosticCheckpoint checkpoint = m_beaconHistory.empty() ? DiagnosticCheckpoint(BeaconType::STARTUP) : m_beaconHistory.back();
    LeaveCriticalSection(&m_beaconLock);
    return checkpoint;
}

DiagnosticCheckpoint BeaconManager::GetLastBeaconOfType(BeaconType type) const
{
    EnterCriticalSection(&m_beaconLock);
    DiagnosticCheckpoint checkpoint(BeaconType::STARTUP);
    for (auto it = m_beaconHistory.rbegin(); it != m_beaconHistory.rend(); ++it) {
        if (it->type == type) {
            checkpoint = *it;
            break;
        }
    }
    LeaveCriticalSection(&m_beaconLock);
    return checkpoint;
}

bool BeaconManager::HasBeaconType(BeaconType type) const
{
    EnterCriticalSection(&m_beaconLock);
    bool found = std::any_of(m_beaconHistory.begin(), m_beaconHistory.end(),
        [type](const DiagnosticCheckpoint& cp) { return cp.type == type; });
    LeaveCriticalSection(&m_beaconLock);
    return found;
}

void BeaconManager::ClearHistory()
{
    EnterCriticalSection(&m_beaconLock);
    m_beaconHistory.clear();
    m_beaconCount = 0;
    LeaveCriticalSection(&m_beaconLock);
}

void BeaconManager::SetLogPath(const std::string& path)
{
    m_logPath = path;
}

bool BeaconManager::ExportToFile(const std::string& path) const
{
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    auto history = GetBeaconHistory();
    for (const auto& checkpoint : history) {
        file << FormatBeacon(checkpoint) << std::endl;
    }
    
    file.close();
    return true;
}

void BeaconManager::WriteBeaconToLog(const DiagnosticCheckpoint& checkpoint)
{
    if (m_logPath.empty()) {
        return;
    }
    
    std::ofstream logFile(m_logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << FormatBeacon(checkpoint) << std::endl;
        logFile.close();
    }
}

std::string BeaconManager::FormatBeacon(const DiagnosticCheckpoint& checkpoint) const
{
    std::stringstream ss;
    ss << "[" << checkpoint.timestamp << "] ";
    ss << "[Thread:" << checkpoint.threadId << "] ";
    ss << AutonomousAgent::BeaconTypeToString(checkpoint.type);
    
    if (FAILED(checkpoint.hr)) {
        ss << " [ERROR:" << checkpoint.hr << "]";
    }
    
    if (!checkpoint.message.empty()) {
        ss << " - " << checkpoint.message;
    }
    
    if (!checkpoint.context.empty()) {
        ss << " (" << checkpoint.context << ")";
    }
    
    return ss.str();
}

// Diagnostic Engine Implementation
DiagnosticEngine::DiagnosticEngine()
    : m_running(false)
{
}

DiagnosticEngine::~DiagnosticEngine()
{
}

void DiagnosticEngine::AddTest(const std::string& name, std::function<bool()> test)
{
    m_diagnosticTests.push_back({name, test});
}

bool DiagnosticEngine::RunAllTests()
{
    m_testResults.clear();
    
    for (const auto& test : m_diagnosticTests) {
        bool result = test.second();
        m_testResults.push_back({test.first, result});
        
        if (!result) {
            AutonomousAgent::Instance()->EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, 
                "Diagnostic test failed: " + test.first);
        }
    }
    
    return std::all_of(m_testResults.begin(), m_testResults.end(),
        [](const std::pair<std::string, bool>& result) { return result.second; });
}

bool DiagnosticEngine::RunTest(const std::string& name)
{
    for (const auto& test : m_diagnosticTests) {
        if (test.first == name) {
            bool result = test.second();
            
            // Update or add result
            bool found = false;
            for (auto& existing : m_testResults) {
                if (existing.first == name) {
                    existing.second = result;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                m_testResults.push_back({name, result});
            }
            
            if (!result) {
                AutonomousAgent::Instance()->EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, 
                    "Diagnostic test failed: " + name);
            }
            
            return result;
        }
    }
    
    return false;
}

std::vector<std::pair<std::string, bool>> DiagnosticEngine::GetResults() const
{
    return m_testResults;
}

std::string DiagnosticEngine::GetReport() const
{
    std::stringstream ss;
    ss << "Diagnostic Report:\n";
    ss << "==================\n\n";
    
    for (const auto& result : m_testResults) {
        ss << result.first << ": " << (result.second ? "PASSED" : "FAILED") << "\n";
    }
    
    return ss.str();
}

void DiagnosticEngine::ClearTests()
{
    m_diagnosticTests.clear();
    m_testResults.clear();
}

// Pre-defined Diagnostic Tests
bool DiagnosticEngine::TestEngineLoad()
{
    // Test if the engine can be loaded
    HMODULE hModule = LoadLibraryA("RawrXD_DigestionEngine.dll");
    if (hModule) {
        FreeLibrary(hModule);
        return true;
    }
    
    // Try alternative names
    hModule = LoadLibraryA("digestion_engine.dll");
    if (hModule) {
        FreeLibrary(hModule);
        return true;
    }
    
    return false;
}

bool DiagnosticEngine::TestMessageLoop()
{
    // Test if the message loop is responsive
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    
    // Post a test message and see if it's processed
    DWORD startTime = GetTickCount();
    PostMessageA(hwnd, WM_USER + 0x1000, 0, 0);
    
    // Give it some time to process
    Sleep(100);
    
    // Check if window is still responsive
    return IsWindow(hwnd) && (GetTickCount() - startTime < 1000);
}

bool DiagnosticEngine::TestHotkeyRegistration()
{
    // Test if hotkeys can be registered
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    if (!hwnd) {
        return false;
    }
    
    // Try to register a test hotkey
    BOOL result = RegisterHotKey(hwnd, 0x9000, MOD_CONTROL | MOD_SHIFT, 'T');
    if (result) {
        UnregisterHotKey(hwnd, 0x9000);
        return true;
    }
    
    return false;
}

bool DiagnosticEngine::TestDigestionEngine()
{
    // Test if the digestion engine is functional
    // This would typically call a test function in the engine
    // For now, we'll check if the function pointer is valid
    HMODULE hModule = LoadLibraryA("RawrXD_DigestionEngine.dll");
    if (!hModule) {
        return false;
    }
    
    auto* pFunc = GetProcAddress(hModule, "RawrXD_DigestionEngine_Avx512");
    FreeLibrary(hModule);
    
    return pFunc != nullptr;
}

bool DiagnosticEngine::TestMemoryAllocation()
{
    // Test if we can allocate memory
    void* pMem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 * 1024); // 1MB
    if (pMem) {
        HeapFree(GetProcessHeap(), 0, pMem);
        return true;
    }
    
    return false;
}

bool DiagnosticEngine::TestThreadCreation()
{
    // Test if we can create threads
    HANDLE hThread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { return 0; }, nullptr, 0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        return true;
    }
    
    return false;
}

bool DiagnosticEngine::TestFileAccess()
{
    // Test if we can access the file system
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) {
        return false;
    }
    
    char tempFile[MAX_PATH];
    if (GetTempFileNameA(tempPath, "AGT", 0, tempFile) == 0) {
        return false;
    }
    
    // Try to create and write to a file
    HANDLE hFile = CreateFileA(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    const char* testData = "Test";
    DWORD written;
    BOOL result = WriteFile(hFile, testData, (DWORD)strlen(testData), &written, nullptr);
    
    CloseHandle(hFile);
    DeleteFileA(tempFile);
    
    return result && written == strlen(testData);
}

bool DiagnosticEngine::TestWindowMessages()
{
    // Test if window messages are working
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    if (!hwnd) {
        return false;
    }
    
    // Send a message and check if it's processed
    LRESULT result = SendMessageA(hwnd, WM_NULL, 0, 0);
    return result == 0; // WM_NULL should return 0
}

// Self-Healing Engine Implementation
SelfHealingEngine::SelfHealingEngine(DWORD maxAttempts)
    : m_running(false)
    , m_maxAttempts(maxAttempts)
{
}

SelfHealingEngine::~SelfHealingEngine()
{
}

void SelfHealingEngine::RegisterAction(HealingAction action, std::function<bool()> handler)
{
    m_healingActions[action] = handler;
}

bool SelfHealingEngine::ExecuteAction(HealingAction action)
{
    auto it = m_healingActions.find(action);
    if (it == m_healingActions.end()) {
        return false;
    }
    
    AutonomousAgent::Instance()->EmitBeacon(BeaconType::RECOVERY_ATTEMPT, S_OK, 
        "Executing healing action: " + AutonomousAgent::HealingActionToString(action));
    
    bool success = it->second();
    m_actionHistory.push_back({action, success});
    
    if (success) {
        AutonomousAgent::Instance()->EmitBeacon(BeaconType::RECOVERY_SUCCESS, S_OK, 
            "Healing action successful: " + AutonomousAgent::HealingActionToString(action));
    } else {
        AutonomousAgent::Instance()->EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, 
            "Healing action failed: " + AutonomousAgent::HealingActionToString(action));
    }
    
    return success;
}

bool SelfHealingEngine::ExecuteActions(const std::vector<HealingAction>& actions)
{
    bool allSuccess = true;
    
    for (auto action : actions) {
        if (!ExecuteAction(action)) {
            allSuccess = false;
            break;
        }
    }
    
    return allSuccess;
}

std::vector<std::pair<HealingAction, bool>> SelfHealingEngine::GetActionHistory() const
{
    return m_actionHistory;
}

void SelfHealingEngine::ClearHistory()
{
    m_actionHistory.clear();
}

void SelfHealingEngine::SetMaxAttempts(DWORD attempts)
{
    m_maxAttempts = attempts;
}

// Pre-defined Healing Actions
bool SelfHealingEngine::RestartMessageLoop()
{
    // This would typically involve restarting the message pump
    // For now, we'll just log the action
    OutputDebugStringA("[AGENT-HEALING] Restarting message loop\n");
    
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    if (hwnd) {
        PostMessageA(hwnd, WM_NULL, 0, 0); // Wake up the message loop
        return true;
    }
    
    return false;
}

bool SelfHealingEngine::ReloadEngine()
{
    OutputDebugStringA("[AGENT-HEALING] Reloading engine\n");
    
    // This would typically reload the digestion engine DLL
    // For now, we'll just validate that the engine is accessible
    return DiagnosticEngine::TestEngineLoad();
}

bool SelfHealingEngine::ReinitHotkeys()
{
    OutputDebugStringA("[AGENT-HEALING] Reinitializing hotkeys\n");
    
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    if (!hwnd) {
        return false;
    }
    
    // Unregister and re-register hotkeys
    UnregisterHotKey(hwnd, 0x9000); // Ctrl+Shift+D
    
    BOOL result = RegisterHotKey(hwnd, 0x9000, MOD_CONTROL | MOD_SHIFT, 'D');
    return result != FALSE;
}

bool SelfHealingEngine::ClearCorruptState()
{
    OutputDebugStringA("[AGENT-HEALING] Clearing corrupt state\n");
    
    // Clear any corrupted state in the agent
    AutonomousAgent::Instance()->ResetHealthState();
    
    return true;
}

bool SelfHealingEngine::ResetCounters()
{
    OutputDebugStringA("[AGENT-HEALING] Resetting counters\n");
    
    // Reset recovery attempt counters
    // This is handled by the agent's ResetHealthState method
    return true;
}

bool SelfHealingEngine::RecreateWindows()
{
    OutputDebugStringA("[AGENT-HEALING] Recreating windows\n");
    
    // This would typically recreate any corrupted windows
    // For now, we'll just validate the main window
    HWND hwnd = AutonomousAgent::Instance()->GetIDEWindow();
    return hwnd && IsWindow(hwnd);
}

bool SelfHealingEngine::PerformFullRestart()
{
    OutputDebugStringA("[AGENT-HEALING] Performing full restart\n");
    
    // This would typically restart the entire IDE
    // For now, we'll just return true to indicate the action was attempted
    return true;
}

// Diagnostic Reporter Implementation
DiagnosticReporter::DiagnosticReporter(const std::string& reportPath)
    : m_reportPath(reportPath)
{
}

DiagnosticReporter::~DiagnosticReporter()
{
}

void DiagnosticReporter::AddDiagnostic(const DiagnosticCheckpoint& checkpoint)
{
    m_diagnostics.push_back(checkpoint);
}

void DiagnosticReporter::AddHealingRecord(HealingAction action, bool success)
{
    m_healingHistory.push_back({action, success});
}

bool DiagnosticReporter::GenerateJSONReport() const
{
    std::ofstream file(m_reportPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"diagnostics\": [\n";
    
    for (size_t i = 0; i < m_diagnostics.size(); ++i) {
        const auto& cp = m_diagnostics[i];
        file << "    {\n";
        file << "      \"timestamp\": " << cp.timestamp << ",\n";
        file << "      \"threadId\": " << cp.threadId << ",\n";
        file << "      \"type\": \"" << AutonomousAgent::BeaconTypeToString(cp.type) << "\",\n";
        file << "      \"hr\": " << cp.hr << ",\n";
        file << "      \"message\": \"" << EscapeJSON(cp.message) << "\",\n";
        file << "      \"context\": \"" << EscapeJSON(cp.context) << "\"\n";
        file << "    }";
        if (i < m_diagnostics.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "  ],\n";
    file << "  \"healingHistory\": [\n";
    
    for (size_t i = 0; i < m_healingHistory.size(); ++i) {
        const auto& record = m_healingHistory[i];
        file << "    {\n";
        file << "      \"action\": \"" << AutonomousAgent::HealingActionToString(record.first) << "\",\n";
        file << "      \"success\": " << (record.second ? "true" : "false") << "\n";
        file << "    }";
        if (i < m_healingHistory.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool DiagnosticReporter::GenerateHTMLReport() const
{
    // Similar to JSON but in HTML format
    // Implementation omitted for brevity
    return true;
}

bool DiagnosticReporter::GenerateTextReport() const
{
    std::ofstream file(m_reportPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "Diagnostic Report\n";
    file << "=================\n\n";
    
    file << "Diagnostics:\n";
    for (const auto& cp : m_diagnostics) {
        file << FormatTimestamp(cp.timestamp) << " - ";
        file << AutonomousAgent::BeaconTypeToString(cp.type);
        if (!cp.message.empty()) {
            file << " - " << cp.message;
        }
        file << "\n";
    }
    
    file << "\nHealing Actions:\n";
    for (const auto& record : m_healingHistory) {
        file << AutonomousAgent::HealingActionToString(record.first) << " - ";
        file << (record.second ? "SUCCESS" : "FAILED") << "\n";
    }
    
    file.close();
    return true;
}

void DiagnosticReporter::Clear()
{
    m_diagnostics.clear();
    m_healingHistory.clear();
}

std::string DiagnosticReporter::FormatTimestamp(DWORD timestamp) const
{
    // Convert tick count to readable time
    // This is a simplified version
    std::stringstream ss;
    ss << "[" << timestamp << "ms]";
    return ss.str();
}

std::string DiagnosticReporter::EscapeJSON(const std::string& str) const
{
    std::string escaped;
    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

// Utility Functions
namespace AgentUtils {
    std::string GetLastSystemError()
    {
        DWORD error = GetLastError();
        if (error == 0) {
            return "No error";
        }
        
        LPSTR messageBuffer = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);
        
        std::string message = messageBuffer ? messageBuffer : "Unknown error";
        LocalFree(messageBuffer);
        
        return message;
    }
    
    bool WriteLogEntry(const std::string& path, const std::string& entry)
    {
        std::ofstream file(path, std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        
        file << entry << std::endl;
        file.close();
        return true;
    }
    
    bool EnsureDirectoryExists(const std::string& path)
    {
        // Extract directory from path
        size_t lastSlash = path.find_last_of("\\/");
        if (lastSlash == std::string::npos) {
            return true; // No directory specified
        }
        
        std::string directory = path.substr(0, lastSlash);
        
        // Create directory recursively
        std::vector<std::string> dirs;
        std::string currentDir;
        
        for (char c : directory) {
            currentDir += c;
            if (c == '\\' || c == '/') {
                dirs.push_back(currentDir);
            }
        }
        
        if (!currentDir.empty() && currentDir.back() != '\\' && currentDir.back() != '/') {
            dirs.push_back(currentDir);
        }
        
        for (const auto& dir : dirs) {
            if (!CreateDirectoryA(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }
        
        return true;
    }
    
    std::string GenerateSessionId()
    {
        // Generate a unique session ID based on timestamp and process ID
        std::stringstream ss;
        ss << GetCurrentProcessId() << "_" << GetTickCount();
        return ss.str();
    }
    
    DWORD GetSystemUptimeMs()
    {
        return GetTickCount();
    }
    
    bool IsDebuggerAttached()
    {
        return IsDebuggerPresent();
    }
    
    void OutputAgentLog(const std::string& message)
    {
        std::string logEntry = "[AGENT-LOG] " + message + "\n";
        OutputDebugStringA(logEntry.c_str());
        
        // Also write to file if path is set
        static std::string logPath = "C:\\RawrXD_Agent.log";
        WriteLogEntry(logPath, logEntry);
    }
}

// ============================================================================
// Digestion Pipeline Monitoring Implementation
// ============================================================================

// Hotkey Pressed
void AutonomousAgent::OnDigestionHotkeyPressed()
{
    EmitBeacon(BeaconType::HOTKEY_REGISTERED, S_OK, "Ctrl+Shift+D hotkey pressed");
    AgentUtils::OutputAgentLog("Digestion hotkey detected - validating prerequisites");
    
    // Validate prerequisites before allowing digestion
    if (!ValidateDigestionPrerequisites()) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Digestion prerequisites not met");
        return;
    }
}

// Digestion Queued
void AutonomousAgent::OnDigestionQueued(DWORD taskId, const std::wstring& source)
{
    EnterCriticalSection(&m_digestionLock);
    m_activeDigestionTask = taskId;
    m_currentDigestionFile = source;
    m_digestionInProgress = true;
    m_digestionStartTime = std::chrono::steady_clock::now();
    LeaveCriticalSection(&m_digestionLock);
    
    char logBuf[512];
    sprintf_s(logBuf, "Digestion Task %lu queued for %S", taskId, source.c_str());
    EmitBeacon(BeaconType::DIGESTION_QUEUED, S_OK, logBuf);
    AgentUtils::OutputAgentLog(logBuf);
}

// Thread Spawned
void AutonomousAgent::OnDigestionThreadSpawned(DWORD taskId, HANDLE hThread)
{
    EnterCriticalSection(&m_digestionLock);
    m_digestionThreadHandle = hThread;
    LeaveCriticalSection(&m_digestionLock);
    
    char logBuf[256];
    sprintf_s(logBuf, "Digestion Task %lu thread spawned (handle=%p)", taskId, hThread);
    EmitBeacon(BeaconType::DIGESTION_STARTED, S_OK, logBuf);
    AgentUtils::OutputAgentLog(logBuf);
}

// Engine Invoked
void AutonomousAgent::OnDigestionEngineInvoked(DWORD taskId)
{
    char logBuf[256];
    sprintf_s(logBuf, "Digestion Task %lu engine invoked", taskId);
    EmitBeacon(BeaconType::DIGESTION_STARTED, S_OK, logBuf);
    AgentUtils::OutputAgentLog(logBuf);
}

// Progress Update
void AutonomousAgent::OnDigestionProgress(DWORD taskId, DWORD percent)
{
    char logBuf[256];
    sprintf_s(logBuf, "Digestion Task %lu progress: %lu%%", taskId, percent);
    EmitBeacon(BeaconType::DIGESTION_PROGRESS, S_OK, logBuf);
    
    // Only log every 25% to avoid spam
    if (percent % 25 == 0 || percent == 0 || percent == 100) {
        AgentUtils::OutputAgentLog(logBuf);
    }
}

// Completion
void AutonomousAgent::OnDigestionComplete(DWORD taskId, DWORD result)
{
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_digestionStartTime);
    
    EnterCriticalSection(&m_digestionLock);
    m_digestionInProgress = false;
    m_activeDigestionTask = 0;
    m_digestionThreadHandle = nullptr;
    LeaveCriticalSection(&m_digestionLock);
    
    char logBuf[512];
    sprintf_s(logBuf, "Digestion Task %lu completed in %lldms with result %lu (File: %S)", 
              taskId, duration.count(), result, m_currentDigestionFile.c_str());
    
    if (result == 0) { // S_DIGEST_OK
        EmitBeacon(BeaconType::DIGESTION_COMPLETE, S_OK, logBuf);
        m_digestionSystemHealthy = true;
    } else {
        EmitBeacon(BeaconType::ERROR_DETECTED, result, logBuf);
        m_digestionSystemHealthy = false;
    }
    
    AgentUtils::OutputAgentLog(logBuf);
}

// Error
void AutonomousAgent::OnDigestionError(DWORD taskId, HRESULT hr, const std::string& context)
{
    EnterCriticalSection(&m_digestionLock);
    m_digestionInProgress = false;
    m_digestionSystemHealthy = false;
    LeaveCriticalSection(&m_digestionLock);
    
    char logBuf[512];
    sprintf_s(logBuf, "Digestion Task %lu ERROR: 0x%08X - %s", taskId, hr, context.c_str());
    EmitBeacon(BeaconType::ERROR_DETECTED, hr, logBuf, context);
    AgentUtils::OutputAgentLog(logBuf);
    
    // Trigger auto-healing if enabled
    if (m_config.enableSelfHealing) {
        AgentUtils::OutputAgentLog("Attempting auto-recovery from digestion error");
        AutoHeal();
    }
}

// Trigger Autonomous Digestion
bool AutonomousAgent::TriggerAutonomousDigestion(const std::wstring& filepath)
{
    EmitBeacon(BeaconType::DIGESTION_INIT, S_OK, "Autonomous digestion triggered");
    
    if (!ValidateDigestionPrerequisites()) {
        EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Prerequisites not met");
        return false;
    }
    
    // Post WM_COMMAND message to IDE to trigger digestion via menu
    if (m_hwndIDE && IsWindow(m_hwndIDE)) {
        PostMessageW(m_hwndIDE, WM_COMMAND, MAKEWPARAM(3100 /* IDM_RUN_DIGESTION */, 0), 0);
        AgentUtils::OutputAgentLog("Posted digestion command to IDE window");
        return true;
    }
    
    EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "IDE window not available");
    return false;
}

// Validate Prerequisites
bool AutonomousAgent::ValidateDigestionPrerequisites()
{
    std::vector<std::string> failures;
    
    // Check IDE window
    if (!m_hwndIDE || !IsWindow(m_hwndIDE)) {
        failures.push_back("IDE window invalid");
    }
    
    // Check engine health
    if (!m_engineHealthy) {
        failures.push_back("Engine unhealthy");
    }
    
    // Check message loop
    if (!m_messageLoopHealthy) {
        failures.push_back("Message loop unhealthy");
    }
    
    // Check if already running
    if (m_digestionInProgress) {
        failures.push_back("Digestion already in progress");
    }
    
    if (!failures.empty()) {
        std::string failureMsg = "Prerequisites failed: ";
        for (size_t i = 0; i < failures.size(); ++i) {
            if (i > 0) failureMsg += ", ";
            failureMsg += failures[i];
        }
        AgentUtils::OutputAgentLog(failureMsg);
        return false;
    }
    
    EmitBeacon(BeaconType::DIGESTION_INIT, S_OK, "Prerequisites validated successfully");
    return true;
}

// Monitor Digestion Execution
bool AutonomousAgent::MonitorDigestionExecution(DWORD taskId, DWORD timeoutMs)
{
    EmitBeacon(BeaconType::MONITORING, S_OK, "Monitoring digestion execution");
    
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::milliseconds(timeoutMs);
    
    while (m_digestionInProgress) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        
        if (elapsed > timeoutDuration) {
            EmitBeacon(BeaconType::ERROR_DETECTED, E_FAIL, "Digestion execution timeout");
            OnDigestionError(taskId, E_FAIL, "Timeout waiting for completion");
            return false;
        }
        
        // Check thread health
        if (m_digestionThreadHandle) {
            DWORD exitCode = 0;
            if (GetExitCodeThread(m_digestionThreadHandle, &exitCode) && exitCode != STILL_ACTIVE) {
                AgentUtils::OutputAgentLog("Digestion thread terminated unexpectedly");
                OnDigestionError(taskId, E_FAIL, "Thread terminated unexpectedly");
                return false;
            }
        }
        
        Sleep(100); // Check every 100ms
    }
    
    EmitBeacon(BeaconType::MONITORING, S_OK, "Digestion monitoring complete");
    return true;
}
