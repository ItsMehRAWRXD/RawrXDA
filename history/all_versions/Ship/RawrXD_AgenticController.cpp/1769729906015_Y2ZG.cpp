// RawrXD Agentic Controller - Pure Win32/C++ Implementation  
// Replaces: agentic_controller.cpp and all Qt-dependent controller components
// Zero Qt dependencies - just Win32 API + STL

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

// ============================================================================
// AGENTIC RESULT
// ============================================================================

struct AgenticResult {
    bool success;
    std::wstring message;
    int errorCode;
    std::chrono::steady_clock::time_point timestamp;
    
    AgenticResult(bool success = true, const std::wstring& message = L"", int errorCode = 0)
        : success(success), message(message), errorCode(errorCode), 
          timestamp(std::chrono::steady_clock::now()) {}
    
    static AgenticResult Ok(const std::wstring& message = L"Operation completed") {
        return AgenticResult(true, message, 0);
    }
    
    static AgenticResult Error(const std::wstring& message, int code = -1) {
        return AgenticResult(false, message, code);
    }
    
    static AgenticResult Timeout(const std::wstring& operation) {
        return AgenticResult(false, L"Timeout: " + operation, -2);
    }
    
    bool IsOk() const { return success; }
    bool IsError() const { return !success; }
};

// ============================================================================
// EVENT SYSTEM (REPLACES QT SIGNALS/SLOTS)
// ============================================================================

template<typename... Args>
class Event {
private:
    std::vector<std::function<void(Args...)>> m_handlers;
    std::mutex m_mutex;
    
public:
    void Connect(std::function<void(Args...)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.push_back(handler);
    }
    
    void Emit(Args... args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& handler : m_handlers) {
            try {
                handler(args...);
            } catch (...) {
                // Log error but continue processing other handlers
                OutputDebugStringW(L"[AgenticController] Handler exception in event emission\n");
            }
        }
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers.clear();
    }
    
    size_t HandlerCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handlers.size();
    }
};

// ============================================================================
// WIN32 TIMER (REPLACES QT QTIMER)  
// ============================================================================

class Win32Timer {
private:
    UINT_PTR m_timerId;
    DWORD m_interval;
    std::function<void()> m_callback;
    bool m_running;
    bool m_singleShot;
    static std::map<UINT_PTR, Win32Timer*> s_timers;
    static std::mutex s_timerMutex;
    
public:
    Win32Timer() : m_timerId(0), m_interval(1000), m_running(false), m_singleShot(false) {}
    
    ~Win32Timer() {
        Stop();
    }
    
    void SetInterval(DWORD intervalMs) {
        m_interval = intervalMs;
    }
    
    void SetSingleShot(bool singleShot) {
        m_singleShot = singleShot;
    }
    
    void SetCallback(std::function<void()> callback) {
        m_callback = callback;
    }
    
    bool Start() {
        if (m_running) Stop();
        
        m_timerId = SetTimer(NULL, 0, m_interval, TimerProc);
        if (m_timerId != 0) {
            std::lock_guard<std::mutex> lock(s_timerMutex);
            s_timers[m_timerId] = this;
            m_running = true;
            return true;
        }
        return false;
    }
    
    void Stop() {
        if (m_running && m_timerId != 0) {
            KillTimer(NULL, m_timerId);
            
            std::lock_guard<std::mutex> lock(s_timerMutex);
            s_timers.erase(m_timerId);
            
            m_timerId = 0;
            m_running = false;
        }
    }
    
    bool IsRunning() const {
        return m_running;
    }
    
    DWORD GetInterval() const {
        return m_interval;
    }
    
private:
    static void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
        (void)hwnd; (void)uMsg; (void)dwTime;
        
        std::lock_guard<std::mutex> lock(s_timerMutex);
        auto it = s_timers.find(idEvent);
        if (it != s_timers.end()) {
            Win32Timer* timer = it->second;
            
            if (timer->m_callback) {
                timer->m_callback();
            }
            
            if (timer->m_singleShot) {
                timer->Stop();
            }
        }
    }
};

std::map<UINT_PTR, Win32Timer*> Win32Timer::s_timers;
std::mutex Win32Timer::s_timerMutex;

// ============================================================================
// AGENTIC CONTROLLER
// ============================================================================

class AgenticController {
private:
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    std::chrono::steady_clock::time_point m_bootTime;
    std::wstring m_workspaceRoot;
    std::wstring m_configPath;
    
    // Timing
    Win32Timer m_heartbeatTimer;
    std::chrono::steady_clock::time_point m_lastHeartbeat;
    DWORD m_heartbeatInterval; // ms
    
    // Threading
    std::thread m_controllerThread;
    std::mutex m_stateMutex;
    
    // Components (forward declarations)
    // AgenticCoordinator* m_coordinator;  
    // ProductionConfigManager* m_configManager;
    
    // Layout preferences
    std::wstring m_snapshotPreference;
    std::map<std::wstring, std::wstring> m_layoutHints;
    
public:
    // Events (replace Qt signals)
    Event<std::wstring> ControllerError;           // emit controllerError(QString)
    Event<> ControllerReady;                       // emit controllerReady()
    Event<std::wstring> LayoutHydrationRequested;  // emit layoutHydrationRequested(QString)
    Event<std::wstring> HeartbeatPublished;        // emit heartbeatPublished()
    
    AgenticController() 
        : m_initialized(false), 
          m_running(false),
          m_heartbeatInterval(15000) { // 15 seconds
        
        m_heartbeatTimer.SetInterval(m_heartbeatInterval);
        m_heartbeatTimer.SetCallback([this]() { PublishHeartbeat(); });
        
        InitializePaths();
        
        OutputDebugStringW(L"[AgenticController] Initialized\n");
    }
    
    ~AgenticController() {
        Shutdown();
        OutputDebugStringW(L"[AgenticController] Destroyed\n");
    }
    
    // ========================================================================
    // BOOTSTRAP & LIFECYCLE
    // ========================================================================
    
    AgenticResult Bootstrap() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        
        if (m_initialized.load()) {
            return AgenticResult::Error(L"Controller already initialized");
        }
        
        m_bootTime = std::chrono::steady_clock::now();
        
        LogInfo(L"Starting agentic controller bootstrap...");
        
        // Step 1: Initialize coordinator
        AgenticResult coordResult = EnsureCoordinator();
        if (!coordResult.IsOk()) {
            LogError(L"Coordinator initialization failed: " + coordResult.message);
            ControllerError.Emit(coordResult.message);
            return coordResult;
        }
        
        // Step 2: Load configuration
        AgenticResult configResult = LoadConfiguration();
        if (!configResult.IsOk()) {
            LogWarning(L"Configuration loading had issues: " + configResult.message);
            // Continue anyway with defaults
        }
        
        // Step 3: Initialize layout system
        std::wstring snapshotHint = ResolveSnapshotPreference();
        LayoutHydrationRequested.Emit(snapshotHint);
        
        // Step 4: Start heartbeat
        if (!m_heartbeatTimer.Start()) {
            LogWarning(L"Failed to start heartbeat timer - continuing without heartbeat");
        }
        
        // Step 5: Set running state
        m_running = true;
        m_initialized = true;
        
        // Calculate boot time
        auto bootDuration = std::chrono::steady_clock::now() - m_bootTime;
        auto bootMs = std::chrono::duration_cast<std::chrono::milliseconds>(bootDuration).count();
        
        std::wstring successMsg = L"Agentic controller bootstrap completed in " + 
                                 std::to_wstring(bootMs) + L" ms";
        LogInfo(successMsg);
        
        // Notify ready
        ControllerReady.Emit();
        
        return AgenticResult::Ok(successMsg);
    }
    
    void Shutdown() {
        if (!m_running.load()) return;
        
        LogInfo(L"Shutting down agentic controller...");
        
        // Stop heartbeat
        m_heartbeatTimer.Stop();
        
        // Stop thread if running
        m_running = false;
        if (m_controllerThread.joinable()) {
            m_controllerThread.join();
        }
        
        // Clear event handlers
        ControllerError.Clear();
        ControllerReady.Clear();
        LayoutHydrationRequested.Clear();
        HeartbeatPublished.Clear();
        
        m_initialized = false;
        
        LogInfo(L"Agentic controller shutdown complete");
    }
    
    // ========================================================================
    // COORDINATOR MANAGEMENT
    // ========================================================================
    
    AgenticResult EnsureCoordinator() {
        // TODO: Initialize actual coordinator when available
        // For now, simulate successful coordinator setup
        
        LogInfo(L"Initializing agentic coordinator...");
        
        // Simulate coordinator initialization
        Sleep(10); // Small delay to simulate work
        
        LogInfo(L"Agentic coordinator initialized successfully");
        return AgenticResult::Ok(L"Coordinator ready");
    }
    
    // ========================================================================
    // CONFIGURATION MANAGEMENT
    // ========================================================================
    
    AgenticResult LoadConfiguration() {
        LogInfo(L"Loading agentic configuration...");
        
        // Load configuration from file or registry
        std::wstring configFile = m_configPath + L"\\agentic_config.json";
        
        DWORD attrs = GetFileAttributesW(configFile.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            LogInfo(L"No config file found, creating defaults");
            return CreateDefaultConfiguration();
        }
        
        // Read configuration file
        HANDLE hFile = CreateFileW(configFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return AgenticResult::Error(L"Cannot open config file: " + configFile);
        }
        
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
            CloseHandle(hFile);
            return CreateDefaultConfiguration();
        }
        
        std::vector<char> buffer(fileSize);
        DWORD bytesRead;
        bool success = ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
        CloseHandle(hFile);
        
        if (!success) {
            return AgenticResult::Error(L"Failed to read config file");
        }
        
        // Convert to wide string and parse
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
        std::vector<wchar_t> wideBuffer(wideSize);
        MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, wideBuffer.data(), wideSize);
        
        std::wstring configJson(wideBuffer.data(), wideSize);
        return ParseConfiguration(configJson);
    }
    
    AgenticResult CreateDefaultConfiguration() {
        // Create default layout hints
        m_layoutHints[L"default_layout"] = L"standard";
        m_layoutHints[L"panel_visibility"] = L"auto";
        m_layoutHints[L"sidebar_width"] = L"300";
        m_layoutHints[L"terminal_height"] = L"200";
        
        m_snapshotPreference = L"last_session";
        
        LogInfo(L"Created default configuration");
        return AgenticResult::Ok(L"Default configuration loaded");
    }
    
    AgenticResult ParseConfiguration(const std::wstring& json) {
        // Simple JSON parsing for basic configuration
        // In production, use a proper JSON parser
        
        if (json.find(L"\"snapshot_preference\"") != std::wstring::npos) {
            size_t start = json.find(L"\"snapshot_preference\"");
            size_t valueStart = json.find(L":", start);
            size_t quoteStart = json.find(L"\"", valueStart);
            size_t quoteEnd = json.find(L"\"", quoteStart + 1);
            
            if (quoteStart != std::wstring::npos && quoteEnd != std::wstring::npos) {
                m_snapshotPreference = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }
        
        LogInfo(L"Parsed configuration from JSON");
        return AgenticResult::Ok(L"Configuration parsed successfully");
    }
    
    // ========================================================================
    // LAYOUT & SNAPSHOT MANAGEMENT
    // ========================================================================
    
    std::wstring ResolveSnapshotPreference() {
        if (!m_snapshotPreference.empty()) {
            return m_snapshotPreference;
        }
        
        // Check for existing session data
        std::wstring sessionFile = m_configPath + L"\\last_session.dat";
        DWORD attrs = GetFileAttributesW(sessionFile.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            return L"restore_session";
        }
        
        return L"default_layout";
    }
    
    void SetSnapshotPreference(const std::wstring& preference) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_snapshotPreference = preference;
        LogInfo(L"Snapshot preference set to: " + preference);
    }
    
    std::wstring GetLayoutHint(const std::wstring& key) const {
        auto it = m_layoutHints.find(key);
        return (it != m_layoutHints.end()) ? it->second : L"";
    }
    
    void SetLayoutHint(const std::wstring& key, const std::wstring& value) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_layoutHints[key] = value;
    }
    
    // ========================================================================
    // HEARTBEAT SYSTEM
    // ========================================================================
    
    void PublishHeartbeat() {
        m_lastHeartbeat = std::chrono::steady_clock::now();
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        
        wchar_t timeStr[64];
        wcsftime(timeStr, sizeof(timeStr) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &timeinfo);
        
        std::wstring heartbeatMsg = L"Controller heartbeat: " + std::wstring(timeStr);
        
        LogInfo(heartbeatMsg);
        HeartbeatPublished.Emit(heartbeatMsg);
    }
    
    void SetHeartbeatInterval(DWORD intervalMs) {
        if (intervalMs < 1000) intervalMs = 1000; // Min 1 second
        if (intervalMs > 300000) intervalMs = 300000; // Max 5 minutes
        
        m_heartbeatInterval = intervalMs;
        m_heartbeatTimer.SetInterval(intervalMs);
        
        if (m_heartbeatTimer.IsRunning()) {
            m_heartbeatTimer.Stop();
            m_heartbeatTimer.Start();
        }
        
        LogInfo(L"Heartbeat interval set to " + std::to_wstring(intervalMs) + L" ms");
    }
    
    std::chrono::milliseconds GetTimeSinceLastHeartbeat() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeartbeat);
    }
    
    // ========================================================================
    // STATUS & MONITORING
    // ========================================================================
    
    bool IsInitialized() const {
        return m_initialized.load();
    }
    
    bool IsRunning() const {
        return m_running.load();
    }
    
    std::chrono::milliseconds GetUptime() const {
        if (!m_initialized.load()) return std::chrono::milliseconds(0);
        
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_bootTime);
    }
    
    std::wstring GetStatusReport() const {
        std::wstring report = L"AgenticController Status:\n";
        report += L"  Initialized: " + std::wstring(m_initialized.load() ? L"Yes" : L"No") + L"\n";
        report += L"  Running: " + std::wstring(m_running.load() ? L"Yes" : L"No") + L"\n";
        
        if (m_initialized.load()) {
            auto uptimeMs = GetUptime().count();
            report += L"  Uptime: " + std::to_wstring(uptimeMs) + L" ms\n";
            
            auto heartbeatMs = GetTimeSinceLastHeartbeat().count();
            report += L"  Last Heartbeat: " + std::to_wstring(heartbeatMs) + L" ms ago\n";
            
            report += L"  Heartbeat Interval: " + std::to_wstring(m_heartbeatInterval) + L" ms\n";
            report += L"  Snapshot Preference: " + m_snapshotPreference + L"\n";
        }
        
        return report;
    }
    
private:
    void InitializePaths() {
        // Get workspace root
        wchar_t buffer[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buffer);
        m_workspaceRoot = buffer;
        
        // Set config path
        if (GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH)) {
            m_configPath = std::wstring(buffer) + L"\\.rawrxd";
        } else {
            m_configPath = m_workspaceRoot + L"\\.rawrxd";
        }
        
        // Ensure config directory exists
        CreateDirectoryW(m_configPath.c_str(), NULL);
    }
    
    void LogInfo(const std::wstring& message) {
        OutputDebugStringW((L"[AgenticController] INFO: " + message + L"\n").c_str());
    }
    
    void LogWarning(const std::wstring& message) {
        OutputDebugStringW((L"[AgenticController] WARNING: " + message + L"\n").c_str());
    }
    
    void LogError(const std::wstring& message) {
        OutputDebugStringW((L"[AgenticController] ERROR: " + message + L"\n").c_str());
    }
};

// ============================================================================
// C INTERFACE FOR DLL EXPORT
// ============================================================================

extern "C" {

__declspec(dllexport) AgenticController* CreateAgenticController() {
    return new AgenticController();
}

__declspec(dllexport) void DestroyAgenticController(AgenticController* controller) {
    delete controller;
}

__declspec(dllexport) BOOL AgenticController_Bootstrap(AgenticController* controller) {
    if (!controller) return FALSE;
    AgenticResult result = controller->Bootstrap();
    return result.IsOk() ? TRUE : FALSE;
}

__declspec(dllexport) void AgenticController_Shutdown(AgenticController* controller) {
    if (controller) {
        controller->Shutdown();
    }
}

__declspec(dllexport) BOOL AgenticController_IsInitialized(AgenticController* controller) {
    return (controller && controller->IsInitialized()) ? TRUE : FALSE;
}

__declspec(dllexport) BOOL AgenticController_IsRunning(AgenticController* controller) {
    return (controller && controller->IsRunning()) ? TRUE : FALSE;
}

__declspec(dllexport) DWORD AgenticController_GetUptime(AgenticController* controller) {
    if (!controller) return 0;
    return static_cast<DWORD>(controller->GetUptime().count());
}

__declspec(dllexport) void AgenticController_SetHeartbeatInterval(AgenticController* controller, DWORD intervalMs) {
    if (controller) {
        controller->SetHeartbeatInterval(intervalMs);
    }
}

__declspec(dllexport) const wchar_t* AgenticController_GetStatusReport(AgenticController* controller) {
    if (!controller) return L"Controller not available";
    
    static std::wstring report;
    report = controller->GetStatusReport();
    return report.c_str();
}

} // extern "C"

// ============================================================================
// DLL ENTRY POINT
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_AgenticController] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_AgenticController] DLL unloaded\n");
        break;
    }
    return TRUE;
}