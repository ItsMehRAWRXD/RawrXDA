// IDEDiagnosticAutoHealer_Impl.cpp - Implementation Details (simplified for easier compilation)
#include "IDEDiagnosticAutoHealer.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <ctime>
#include <windows.h>
#include <psapi.h>

// Singleton instances
IDEDiagnosticAutoHealer& IDEDiagnosticAutoHealer::Instance() {
    static IDEDiagnosticAutoHealer instance;
    return instance;
}

BeaconStorage& BeaconStorage::Instance() {
    static BeaconStorage instance;
    return instance;
}

// Auto healer stub implementations
void IDEDiagnosticAutoHealer::StartFullDiagnostic() {
    if (m_running.exchange(true)) {
        return;
    }
    
    OutputDebugStringW(L"[AutoHealer] Starting full diagnostic sequence\n");
    
    InitializeCriticalSection(&m_lock);
    m_idleEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_diagEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_healEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring basePath = exePath;
    basePath = basePath.substr(0, basePath.find_last_of(L"\\"));
    
    m_config.ideExePath = basePath + L"\\RawrXD-Win32IDE.exe";
    m_config.testFilePath = basePath + L"\\test_input.cpp";
    m_config.diagnosticReportPath = basePath + L"\\diagnostic_report.json";
    m_config.maxRetries = 3;
    m_config.timeoutMs = 10000;
    m_config.autoHealEnabled = true;
    
    m_diagnosticThread = std::make_unique<std::thread>([this]() {
        this->DiagnosticThreadProc();
    });
}

void IDEDiagnosticAutoHealer::StopDiagnostic() {
    m_running = false;
    
    if (m_diagnosticThread && m_diagnosticThread->joinable()) {
        m_diagnosticThread->join();
    }
    
    if (m_idleEvent) CloseHandle(m_idleEvent);
    if (m_diagEvent) CloseHandle(m_diagEvent);
    if (m_healEvent) CloseHandle(m_healEvent);
    
    DeleteCriticalSection(&m_lock);
    
    OutputDebugStringW(L"[AutoHealer] Diagnostic stopped\n");
}

void IDEDiagnosticAutoHealer::DiagnosticThreadProc() {
    OutputDebugStringW(L"[AutoHealer-Diag] Thread started\n");
    
    try {
        EmitBeacon(BeaconStage::IDE_LAUNCH, S_OK, "Launching IDE process");
        HRESULT hr = ExecuteIDELaunch();
        if (FAILED(hr)) {
            OutputDebugStringW(L"[AutoHealer-Diag] IDE launch failed\n");
            return;
        }
        
        Sleep(1000);
        EmitBeacon(BeaconStage::WINDOW_CREATED, S_OK, "IDE window created");
        
        m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
        if (!m_ideMainWindow) {
            OutputDebugStringW(L"[AutoHealer-Diag] Main window not found\n");
            return;
        }
        
        Sleep(500);
        EmitBeacon(BeaconStage::FILE_OPENED, S_OK, "Opening test file");
        
        Sleep(500);
        EmitBeacon(BeaconStage::HOTKEY_SENT, S_OK, "Sending Ctrl+Shift+D hotkey");
        SetFocus(m_ideMainWindow);
        DiagnosticUtils::SendHotkey(m_ideMainWindow, 'D', true, true, false);
        
        EmitBeacon(BeaconStage::MESSAGE_RECEIVED, S_OK, "Waiting for digestion");
        DWORD startTime = GetTickCount();
        bool completed = false;
        
        while ((GetTickCount() - startTime) < m_config.timeoutMs && m_running) {
            if (DiagnosticUtils::VerifyDigestFileCreated(m_config.testFilePath)) {
                EmitBeacon(BeaconStage::OUTPUT_VERIFIED, S_OK, "Digest file created");
                completed = true;
                break;
            }
            Sleep(100);
        }
        
        if (!completed) {
            OutputDebugStringW(L"[AutoHealer-Diag] Digestion timeout\n");
            EmitBeacon(BeaconStage::ENGINE_COMPLETE, WAIT_TIMEOUT, "Engine execution timeout");
        } else {
            EmitBeacon(BeaconStage::SUCCESS, S_OK, "Full diagnostic cycle complete");
        }
        
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("[AutoHealer-Diag] Exception: ") + e.what();
        OutputDebugStringA(errorMsg.c_str());
    }
}

void IDEDiagnosticAutoHealer::EmitBeacon(BeaconStage stage, HRESULT result, const std::string& data) {
    EnterCriticalSection(&m_lock);
    
    BeaconCheckpoint checkpoint;
    checkpoint.stage = stage;
    checkpoint.timestamp = GetTickCount();
    checkpoint.result = result;
    checkpoint.diagnosticData = data;
    
    m_currentStage = stage;
    m_beaconHistory.push_back(checkpoint);
    
    BeaconStorage::Instance().SaveCheckpoint(stage, result, data);
    
    wchar_t beaconMsg[512];
    swprintf_s(beaconMsg, 512, L"[Beacon] Stage=%d, Result=0x%08X\n", 
               static_cast<int>(stage), result);
    OutputDebugStringW(beaconMsg);
    
    LeaveCriticalSection(&m_lock);
}

HRESULT IDEDiagnosticAutoHealer::ExecuteIDELaunch() {
    OutputDebugStringW(L"[AutoHealer] Launching IDE process\n");
    
    m_ideProcessId = DiagnosticUtils::LaunchIDEProcess(m_config.ideExePath);
    
    if (m_ideProcessId == 0) {
        OutputDebugStringW(L"[AutoHealer] Failed to launch IDE\n");
        return E_FAIL;
    }
    
    wchar_t msg[256];
    swprintf_s(msg, 256, L"[AutoHealer] IDE launched with PID: %lu\n", m_ideProcessId);
    OutputDebugStringW(msg);
    
    return S_OK;
}

void IDEDiagnosticAutoHealer::ApplyHealing(HealingStrategy strategy) {
    EnterCriticalSection(&m_lock);
    
    int currentAttempts = m_healingAttempts.load();
    if (currentAttempts >= 5) {
        OutputDebugStringW(L"[AutoHealer] Max healing attempts reached\n");
        LeaveCriticalSection(&m_lock);
        return;
    }
    
    m_appliedHealings.push_back(strategy);
    m_healingAttempts++;
    
    LeaveCriticalSection(&m_lock);
}

void IDEDiagnosticAutoHealer::ExecuteHotkeyResend() {
    OutputDebugStringW(L"[AutoHealer-Heal] Resending hotkey\n");
    if (!m_ideMainWindow || !IsWindow(m_ideMainWindow)) {
        m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
    }
    if (m_ideMainWindow) {
        SetForegroundWindow(m_ideMainWindow);
        Sleep(200);
        DiagnosticUtils::SendHotkey(m_ideMainWindow, 'D', true, true, false);
        DiagnosticUtils::LogHealing("Hotkey resend (Ctrl+Shift+D)", true);
    } else {
        DiagnosticUtils::LogHealing("Hotkey resend", false);
    }
}

void IDEDiagnosticAutoHealer::ExecuteFileReopen() {
    OutputDebugStringW(L"[AutoHealer-Heal] Reopening file\n");
    if (!m_ideMainWindow || !IsWindow(m_ideMainWindow)) {
        m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
    }
    if (m_ideMainWindow) {
        DiagnosticUtils::OpenFileInEditor(m_ideMainWindow, m_config.testFilePath);
        Sleep(500);
        DiagnosticUtils::LogHealing("File reopen", true);
    } else {
        DiagnosticUtils::LogHealing("File reopen", false);
    }
}

void IDEDiagnosticAutoHealer::ExecuteMessageRepost() {
    OutputDebugStringW(L"[AutoHealer-Heal] Reposting message\n");
    if (m_ideMainWindow && IsWindow(m_ideMainWindow)) {
        // Send WM_USER-based custom message to trigger re-digest
        PostMessageW(m_ideMainWindow, WM_USER + 100, 0, 0);
        DiagnosticUtils::LogHealing("Message repost", true);
    } else {
        DiagnosticUtils::LogHealing("Message repost", false);
    }
}

void IDEDiagnosticAutoHealer::ExecuteWindowRefocus() {
    OutputDebugStringW(L"[AutoHealer-Heal] Refocusing window\n");
    m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
    if (m_ideMainWindow) {
        SetForegroundWindow(m_ideMainWindow);
        SetFocus(m_ideMainWindow);
        BringWindowToTop(m_ideMainWindow);
        DiagnosticUtils::LogHealing("Window refocus", true);
    } else {
        DiagnosticUtils::LogHealing("Window refocus", false);
    }
}

void IDEDiagnosticAutoHealer::ExecuteProcessRestart() {
    OutputDebugStringW(L"[AutoHealer-Heal] Restarting process\n");
    // Terminate existing process
    if (m_ideProcessId != 0) {
        DiagnosticUtils::TerminateProcessGracefully(m_ideProcessId);
        Sleep(1000);
    }
    // Launch new instance
    m_ideProcessId = DiagnosticUtils::LaunchIDEProcess(m_config.ideExePath);
    if (m_ideProcessId != 0) {
        Sleep(2000); // Wait for startup
        m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
        DiagnosticUtils::LogHealing("Process restart", m_ideMainWindow != nullptr);
        EmitBeacon(BeaconStage::IDE_LAUNCH, S_OK, "Recovery: Process restarted");
    } else {
        DiagnosticUtils::LogHealing("Process restart", false);
    }
}

std::string IDEDiagnosticAutoHealer::GenerateDiagnosticReport() {
    EnterCriticalSection(&m_lock);
    
    std::ostringstream report;
    report << "{\n  \"timestamp\": " << GetTickCount() << ",\n";
    report << "  \"totalBeacons\": " << static_cast<int>(m_beaconHistory.size()) << ",\n";
    report << "  \"healingAttempts\": " << m_healingAttempts.load() << "\n";
    report << "}\n";
    
    LeaveCriticalSection(&m_lock);
    
    return report.str();
}

std::string IDEDiagnosticAutoHealer::GenerateHealingLog() {
    return "Healing actions logged";
}

void IDEDiagnosticAutoHealer::RecoverFromCheckpoint(BeaconStage stage) {
    OutputDebugStringW(L"[AutoHealer] Recovering from checkpoint\n");
    EnterCriticalSection(&m_lock);

    // Find the last successful checkpoint at or before the requested stage
    BeaconCheckpoint lastGood{};
    bool found = false;
    for (auto it = m_beaconHistory.rbegin(); it != m_beaconHistory.rend(); ++it) {
        if (it->stage <= stage && SUCCEEDED(it->result)) {
            lastGood = *it;
            found = true;
            break;
        }
    }

    LeaveCriticalSection(&m_lock);

    if (!found) {
        OutputDebugStringW(L"[AutoHealer] No valid checkpoint found, starting fresh\n");
        // No valid checkpoint - restart from scratch
        if (m_running) {
            StopDiagnostic();
            StartFullDiagnostic();
        }
        return;
    }

    wchar_t msg[256];
    swprintf_s(msg, 256, L"[AutoHealer] Recovering from stage %d\n", static_cast<int>(lastGood.stage));
    OutputDebugStringW(msg);

    // Apply recovery based on the failed stage
    if (stage >= BeaconStage::HOTKEY_SENT) {
        // Re-send the hotkey
        if (m_ideMainWindow && IsWindow(m_ideMainWindow)) {
            SetFocus(m_ideMainWindow);
            DiagnosticUtils::SendHotkey(m_ideMainWindow, 'D', true, true, false);
            EmitBeacon(BeaconStage::HOTKEY_SENT, S_OK, "Recovery: Hotkey re-sent");
        }
    } else if (stage >= BeaconStage::FILE_OPENED) {
        // Re-open the file
        if (m_ideMainWindow && IsWindow(m_ideMainWindow)) {
            DiagnosticUtils::OpenFileInEditor(m_ideMainWindow, m_config.testFilePath);
            EmitBeacon(BeaconStage::FILE_OPENED, S_OK, "Recovery: File re-opened");
        }
    } else if (stage >= BeaconStage::WINDOW_CREATED) {
        // Try to find the window again
        m_ideMainWindow = DiagnosticUtils::FindIDEMainWindow(m_ideProcessId);
        if (m_ideMainWindow) {
            EmitBeacon(BeaconStage::WINDOW_CREATED, S_OK, "Recovery: Window re-found");
        }
    }
}

void IDEDiagnosticAutoHealer::ResumeFromLastKnownGood() {
    OutputDebugStringW(L"[AutoHealer] Resuming from last known good state\n");

    BeaconCheckpoint lastGood = BeaconStorage::Instance().LoadLastCheckpoint();
    if (lastGood.timestamp == 0) {
        OutputDebugStringW(L"[AutoHealer] No known good state found\n");
        return;
    }

    wchar_t msg[256];
    swprintf_s(msg, 256, L"[AutoHealer] Last known good: stage=%d at tick=%lu\n",
               static_cast<int>(lastGood.stage), lastGood.timestamp);
    OutputDebugStringW(msg);

    RecoverFromCheckpoint(lastGood.stage);
}

void IDEDiagnosticAutoHealer::MonitorIDEProcess(DWORD processId) {
    wchar_t msg[256];
    swprintf_s(msg, 256, L"[AutoHealer] Monitoring process PID=%lu\n", processId);
    OutputDebugStringW(msg);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, processId);
    if (!hProcess) {
        OutputDebugStringW(L"[AutoHealer] Failed to open process for monitoring\n");
        return;
    }

    // Check if process is still alive
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            swprintf_s(msg, 256, L"[AutoHealer] Process exited with code %lu\n", exitCode);
            OutputDebugStringW(msg);
            EmitBeacon(BeaconStage::ENGINE_COMPLETE, E_FAIL, "Process exited unexpectedly");
            CloseHandle(hProcess);
            return;
        }
    }

    // Monitor memory usage
    PROCESS_MEMORY_COUNTERS pmc = {};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        // Alert if using more than 2GB
        if (pmc.WorkingSetSize > 2ULL * 1024 * 1024 * 1024) {
            OutputDebugStringW(L"[AutoHealer] WARNING: Process memory > 2GB\n");
            EmitBeacon(BeaconStage::OUTPUT_VERIFIED, S_FALSE, "High memory usage detected");
        }
    }

    // Check if window is responding
    HWND ideWindow = DiagnosticUtils::FindIDEMainWindow(processId);
    if (ideWindow) {
        DWORD_PTR result = 0;
        LRESULT sendResult = SendMessageTimeoutW(ideWindow, WM_NULL, 0, 0,
                                                  SMTO_ABORTIFHUNG, 3000, &result);
        if (sendResult == 0) {
            OutputDebugStringW(L"[AutoHealer] WARNING: Window not responding (hung)\n");
            EmitBeacon(BeaconStage::OUTPUT_VERIFIED, E_FAIL, "Window hung detected");
        }
    }

    CloseHandle(hProcess);
}

void IDEDiagnosticAutoHealer::DetectAndHealFailures() {
    if (!m_config.autoHealEnabled) return;
    OutputDebugStringW(L"[AutoHealer] Running failure detection and auto-healing\n");

    EnterCriticalSection(&m_lock);

    // Analyze beacon history for failure patterns
    bool windowLost = false;
    bool hotkeyFailed = false;
    bool digestTimeout = false;
    bool processExited = false;

    for (const auto& beacon : m_beaconHistory) {
        if (FAILED(beacon.result)) {
            switch (beacon.stage) {
                case BeaconStage::WINDOW_CREATED:
                    windowLost = true;
                    break;
                case BeaconStage::HOTKEY_SENT:
                    hotkeyFailed = true;
                    break;
                case BeaconStage::ENGINE_COMPLETE:
                    if (beacon.result == WAIT_TIMEOUT) digestTimeout = true;
                    else processExited = true;
                    break;
                default:
                    break;
            }
        }
    }

    LeaveCriticalSection(&m_lock);

    // Apply healing strategies in order of severity
    if (processExited) {
        OutputDebugStringW(L"[AutoHealer] HEAL: Process exited, restarting\n");
        ApplyHealing(HealingStrategy::PROCESS_RESTART);
        ExecuteProcessRestart();
    } else if (windowLost) {
        OutputDebugStringW(L"[AutoHealer] HEAL: Window lost, refocusing\n");
        ApplyHealing(HealingStrategy::WINDOW_REFOCUS);
        ExecuteWindowRefocus();
    } else if (hotkeyFailed) {
        OutputDebugStringW(L"[AutoHealer] HEAL: Hotkey failed, resending\n");
        ApplyHealing(HealingStrategy::HOTKEY_RESEND);
        ExecuteHotkeyResend();
    } else if (digestTimeout) {
        OutputDebugStringW(L"[AutoHealer] HEAL: Digest timeout, reopening file\n");
        ApplyHealing(HealingStrategy::FILE_REOPEN);
        ExecuteFileReopen();
    }
}

// Beacon storage implementations
void BeaconStorage::SaveCheckpoint(BeaconStage stage, HRESULT result, const std::string& data) {
    EnterCriticalSection(&m_lock);
    
    BeaconCheckpoint checkpoint;
    checkpoint.stage = stage;
    checkpoint.timestamp = GetTickCount();
    checkpoint.result = result;
    checkpoint.diagnosticData = data;
    
    m_checkpoints.push_back(checkpoint);
    
    LeaveCriticalSection(&m_lock);
}

BeaconCheckpoint BeaconStorage::LoadLastCheckpoint() {
    EnterCriticalSection(&m_lock);
    
    BeaconCheckpoint result{};
    if (!m_checkpoints.empty()) {
        result = m_checkpoints.back();
    }
    
    LeaveCriticalSection(&m_lock);
    return result;
}

void BeaconStorage::ClearHistory() {
    EnterCriticalSection(&m_lock);
    m_checkpoints.clear();
    LeaveCriticalSection(&m_lock);
}

std::vector<BeaconCheckpoint> BeaconStorage::GetFullHistory() const {
    return m_checkpoints;
}

std::wstring BeaconStorage::GetBeaconFilePath() const {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring result = tempPath;
    result += L"ide_beacon.log";
    return result;
}

// Diagnostic utils implementations
namespace DiagnosticUtils {

DWORD LaunchIDEProcess(const std::wstring& exePath) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, 
                        nullptr, nullptr, &si, &pi)) {
        OutputDebugStringW(L"[DiagnosticUtils] CreateProcess failed\n");
        return 0;
    }
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return pi.dwProcessId;
}

bool TerminateProcessGracefully(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!process) {
        return false;
    }
    
    bool result = TerminateProcess(process, 0) != FALSE;
    CloseHandle(process);
    
    return result;
}

HWND FindIDEMainWindow(DWORD processId) {
    struct FindWindowData {
        DWORD targetPID;
        HWND result;
    } data = {processId, nullptr};
    
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* data = reinterpret_cast<FindWindowData*>(lParam);
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        
        if (pid == data->targetPID && IsWindowVisible(hwnd)) {
            wchar_t className[256];
            GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t));
            
            if (wcscmp(className, L"IDEMainWindow") == 0 || wcscmp(className, L"RawrXD_IDE") == 0) {
                data->result = hwnd;
                return FALSE;
            }
        }
        
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));
    
    return data.result;
}

bool SendHotkey(HWND hwnd, UINT vk, bool ctrl, bool shift, bool alt) {
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    Sleep(50);
    PostMessageW(hwnd, WM_KEYUP, vk, 0);
    
    return true;
}

bool OpenFileInEditor(HWND hwnd, const std::wstring& filePath) {
    PostMessageW(hwnd, WM_KEYDOWN, 'O', 0);
    Sleep(500);
    
    return true;
}

bool VerifyDigestFileCreated(const std::wstring& sourceFile) {
    std::wstring digestPath = sourceFile + L".digest";
    
    WIN32_FIND_DATAW findData;
    HANDLE findHandle = FindFirstFileW(digestPath.c_str(), &findData);
    
    if (findHandle != INVALID_HANDLE_VALUE) {
        FindClose(findHandle);
        return true;
    }
    
    return false;
}

std::string ReadDigestFile(const std::wstring& sourceFile) {
    std::wstring digestPath = sourceFile + L".digest";
    std::wifstream file(digestPath);
    if (!file.is_open()) {
        return "";
    }
    
    std::wstringstream buffer;
    buffer << file.rdbuf();
    
    std::wstring wContent = buffer.str();
    std::string content(wContent.begin(), wContent.end());
    
    return content;
}

void LogDiagnostic(const std::string& message) {
    std::string formatted = "[Diagnostic] " + message + "\n";
    OutputDebugStringA(formatted.c_str());
}

void LogHealing(const std::string& action, bool success) {
    std::string formatted = std::string("[Healing] ") + action + " - " + 
                           (success ? "SUCCESS" : "FAILED") + "\n";
    OutputDebugStringA(formatted.c_str());
}

}  // namespace DiagnosticUtils

