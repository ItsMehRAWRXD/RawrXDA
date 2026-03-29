#include "OllamaManager.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <winhttp.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

using json = nlohmann::json;

OllamaManager::OllamaManager() : m_state(State::Stopped) {
    // Try to find Ollama automatically
    if (m_ollamaExePath.empty()) {
        EnsureOllamaAvailable(m_ollamaExePath);
    }
}

OllamaManager::~OllamaManager() {
    Stop();
}

std::string OllamaManager::GetDefaultOllamaPath() {
    // Check common Windows installation paths
    std::vector<std::string> paths = {
        "C:\\Program Files\\Ollama\\ollama.exe",
        "C:\\Program Files (x86)\\Ollama\\ollama.exe",
        "C:\\Users\\" + std::string(std::getenv("USERNAME") ? std::getenv("USERNAME") : "") + "\\AppData\\Local\\Programs\\Ollama\\ollama.exe"
    };

    for (const auto& path : paths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return "";
}

bool OllamaManager::EnsureOllamaAvailable(std::string& outPath) {
    // First try to find existing Ollama
    outPath = GetDefaultOllamaPath();
    if (!outPath.empty()) {
        return true;
    }

    // Could implement auto-download here in future
    // For now, just log that Ollama needs to be installed
    return false;
}

bool OllamaManager::Start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != State::Stopped) {
        LogOutput("[Ollama] Already started or starting...");
        return false;
    }

    if (m_ollamaExePath.empty()) {
        LogOutput("[Ollama ERROR] Ollama executable path not found. Please install Ollama from https://ollama.ai");
        SetState(State::Error);
        return false;
    }

    SetState(State::Starting);
    LogOutput("[Ollama] Starting Ollama server on port " + std::to_string(m_port) + "...");

    if (!LaunchOllamaProcess()) {
        LogOutput("[Ollama ERROR] Failed to launch Ollama process");
        SetState(State::Error);
        return false;
    }

    // Wait for health check
    if (!WaitForHealthcheck()) {
        LogOutput("[Ollama ERROR] Ollama failed to become healthy");
        TerminateOllamaProcess();
        SetState(State::Error);
        return false;
    }

    // Start monitoring thread
    m_shouldMonitor = true;
    m_monitorThread = std::thread(&OllamaManager::MonitorProcess, this);

    m_version = QueryOllamaVersion();
    LogOutput("[Ollama] Server running - Version: " + m_version);
    SetState(State::Running);

    return true;
}

bool OllamaManager::Stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == State::Stopped) {
        return true;
    }

    SetState(State::Stopping);
    LogOutput("[Ollama] Stopping Ollama server...");

    m_shouldMonitor = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }

    if (!TerminateOllamaProcess()) {
        LogOutput("[Ollama ERROR] Failed to terminate Ollama process");
        SetState(State::Error);
        return false;
    }

    LogOutput("[Ollama] Server stopped");
    SetState(State::Stopped);
    return true;
}

bool OllamaManager::IsHealthy() const {
    if (m_state != State::Running) {
        return false;
    }

    try {
        HINTERNET hSession = WinHttpOpen(
            L"RawrXD/OllamaManager",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(
            hSession, L"localhost", m_port, 0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, L"GET", L"/api/tags",
            NULL, WINHTTP_NO_REFERER, NULL,
            WINHTTP_FLAG_REFRESH);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        BOOL result = WinHttpSendRequest(
            hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);

        if (!result) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return dwStatusCode == 200;
    } catch (...) {
        return false;
    }
}

std::string OllamaManager::GetEndpointURL() const {
    return "http://localhost:" + std::to_string(m_port);
}

std::string OllamaManager::GetStateString() const {
    switch (m_state.load()) {
        case State::Stopped: return "Stopped";
        case State::Starting: return "Starting";
        case State::Running: return "Running";
        case State::Stopping: return "Stopping";
        case State::Error: return "Error";
        default: return "Unknown";
    }
}

void OllamaManager::SetState(State newState) {
    m_state = newState;
    if (m_stateChangeCallback) {
        m_stateChangeCallback(newState);
    }
}

bool OllamaManager::LaunchOllamaProcess() {
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    std::string cmdLine = m_ollamaExePath + " serve";
    
    // Set OLLAMA_HOST environment variable
    std::string envVar = "OLLAMA_HOST=localhost:" + std::to_string(m_port);

    if (!CreateProcessA(
        nullptr,
        (char*)cmdLine.c_str(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE | CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi)) {

        LogOutput("[Ollama ERROR] CreateProcessA failed: " + std::to_string(GetLastError()));
        return false;
    }

    m_processId = pi.dwProcessId;
    m_processHandle = pi.hProcess;

    CloseHandle(pi.hThread);
    return true;
}

bool OllamaManager::TerminateOllamaProcess() {
    if (!m_processHandle || m_processHandle == INVALID_HANDLE_VALUE) {
        return true;
    }

    // Try graceful termination first (let the process clean up)
    if (!TerminateProcess(m_processHandle, 0)) {
        LogOutput("[Ollama ERROR] TerminateProcess failed: " + std::to_string(GetLastError()));
        return false;
    }

    // Wait for process to exit
    DWORD dwWaitResult = WaitForSingleObject(m_processHandle, 5000);
    if (dwWaitResult == WAIT_TIMEOUT) {
        LogOutput("[Ollama WARNING] Process did not terminate within timeout");
    }

    CloseHandle(m_processHandle);
    m_processHandle = nullptr;
    m_processId = 0;

    return true;
}

std::string OllamaManager::QueryOllamaVersion() {
    try {
        HINTERNET hSession = WinHttpOpen(
            L"RawrXD/OllamaManager",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession) return "unknown";

        HINTERNET hConnect = WinHttpConnect(
            hSession, L"localhost", m_port, 0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "unknown";
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, L"GET", L"/api/version",
            NULL, WINHTTP_NO_REFERER, NULL,
            WINHTTP_FLAG_REFRESH);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "unknown";
        }

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "unknown";
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "unknown";
        }

        std::string response;
        DWORD bytes_available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
            std::vector<char> buffer(bytes_available + 1, 0);
            DWORD bytes_read = 0;
            if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
                response.append(buffer.data(), bytes_read);
            } else {
                break;
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!response.empty()) {
            try {
                json j = json::parse(response);
                if (j.contains("version")) {
                    return j["version"].get<std::string>();
                }
            } catch (...) {
                return "unknown";
            }
        }

        return "unknown";
    } catch (...) {
        return "unknown";
    }
}

bool OllamaManager::WaitForHealthcheck(int maxAttempts) {
    for (int i = 0; i < maxAttempts; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (IsHealthy()) {
            return true;
        }

        LogOutput("[Ollama] Waiting for server to be ready... (" + std::to_string(i + 1) + "/" + std::to_string(maxAttempts) + ")");
    }

    return false;
}

void OllamaManager::MonitorProcess() {
    while (m_shouldMonitor) {
        if (IsHealthy()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } else {
            LogOutput("[Ollama WARNING] Health check failed");
            SetState(State::Error);
            break;
        }
    }
}

void OllamaManager::LogOutput(const std::string& message) {
    if (m_outputCallback) {
        m_outputCallback(message + "\r\n");
    }
}

}  // namespace RawrXD
