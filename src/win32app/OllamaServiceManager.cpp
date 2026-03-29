#include "OllamaServiceManager.h"
#include "Win32IDE.h"
#include "IDELogger.h"
#include <winhttp.h>
#include <tlhelp32.h>
#include <winnt.h>
#include <richedit.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

OllamaServiceManager::OllamaServiceManager(Win32IDE* ide)
    : m_ide(ide)
    , m_state(ServiceState::Stopped)
    , m_shutdownRequested(false)
    , m_processHandle(nullptr)
    , m_processId(0)
    , m_stdoutRead(nullptr)
    , m_stdoutWrite(nullptr)
    , m_stderrRead(nullptr)
    , m_stderrWrite(nullptr)
    , m_restartAttempts(0)
    , m_isHealthy(false)
    , m_lastResponseTime(std::chrono::milliseconds::zero())
    , m_hwndTerminalPane(nullptr)
{
    addLogEntry(LogLevel::Info, "OllamaServiceManager initialized");
}

OllamaServiceManager::~OllamaServiceManager() {
    shutdown();
}

// ============================================================================
// COMPONENT LIFECYCLE
// ============================================================================

bool OllamaServiceManager::initialize() {
    addLogEntry(LogLevel::Info, "Initializing Ollama service manager");

    // Setup directory structure
    m_ollamaBinaryPath = fs::current_path() / "bin" / "ollama.exe";
    m_modelsDirectory = fs::current_path() / "models";
    m_configDirectory = fs::current_path() / "config" / "ollama";

    try {
        fs::create_directories(fs::path(m_ollamaBinaryPath).parent_path());
        fs::create_directories(m_modelsDirectory);
        fs::create_directories(m_configDirectory);
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to create directories: " + std::string(e.what()));
        return false;
    }

    // Setup Ollama environment (production-hardened setup)
    if (!setupOllamaEnvironment()) {
        addLogEntry(LogLevel::Error, "Failed to setup Ollama environment");
        return false;
    }

    // Check if Ollama binary exists, download if needed
    if (!isOllamaBinaryAvailable() && m_config.downloadOllamaIfMissing) {
        addLogEntry(LogLevel::Info, "Ollama binary not found, downloading...");
        if (!downloadOllamaBinary()) {
            addLogEntry(LogLevel::Error, "Failed to download Ollama binary");
            return false;
        }
    }

    // Start service if configured to auto-start
    if (m_config.autoStart && isOllamaBinaryAvailable()) {
        if (!startService()) {
            addLogEntry(LogLevel::Warning, "Failed to auto-start Ollama service");
        }
    }

    return true;
}

void OllamaServiceManager::shutdown() {
    m_shutdownRequested = true;
    addLogEntry(LogLevel::Info, "Shutting down Ollama service manager");

    // Stop the service
    stopService();

    // Join background threads
    if (m_healthCheckThread.joinable()) {
        m_healthCheckThread.join();
    }
    if (m_processWatchThread.joinable()) {
        m_processWatchThread.join();
    }
    if (m_outputReaderThread.joinable()) {
        m_outputReaderThread.join();
    }

    // Close handles
    if (m_stdoutRead) { CloseHandle(m_stdoutRead); m_stdoutRead = nullptr; }
    if (m_stdoutWrite) { CloseHandle(m_stdoutWrite); m_stdoutWrite = nullptr; }
    if (m_stderrRead) { CloseHandle(m_stderrRead); m_stderrRead = nullptr; }
    if (m_stderrWrite) { CloseHandle(m_stderrWrite); m_stderrWrite = nullptr; }

    addLogEntry(LogLevel::Info, "Ollama service manager shutdown complete");
}

// ============================================================================
// SERVICE MANAGEMENT
// ============================================================================

bool OllamaServiceManager::startService() {
    if (m_state == ServiceState::Running || m_state == ServiceState::Starting) {
        return true;
    }

    m_state = ServiceState::Starting;
    if (m_stateCallback) {
        m_stateCallback(ServiceState::Starting, "Starting Ollama service...");
    }

    addLogEntry(LogLevel::Info, "Starting Ollama service on " + m_config.host + ":" + std::to_string(m_config.port));

    if (!isOllamaBinaryAvailable()) {
        addLogEntry(LogLevel::Error, "Ollama binary not available at: " + m_ollamaBinaryPath);
        m_state = ServiceState::Error;
        return false;
    }

    if (!launchOllamaProcess()) {
        addLogEntry(LogLevel::Error, "Failed to launch Ollama process");
        m_state = ServiceState::Error;
        return false;
    }

    // Wait for process to start and become healthy
    int attempts = 0;
    const int maxAttempts = 30; // 30 seconds
    while (attempts < maxAttempts && m_state == ServiceState::Starting) {
        Sleep(1000);
        if (performHealthCheck()) {
            m_state = ServiceState::Running;
            m_isHealthy = true;
            addLogEntry(LogLevel::Info, "Ollama service started successfully");
            
            // Start monitoring threads
            if (!m_healthCheckThread.joinable()) {
                m_healthCheckThread = std::thread(&OllamaServiceManager::performHealthChecks, this);
            }
            if (!m_processWatchThread.joinable()) {
                m_processWatchThread = std::thread(&OllamaServiceManager::watchOllamaProcess, this);
            }
            if (!m_outputReaderThread.joinable()) {
                m_outputReaderThread = std::thread(&OllamaServiceManager::readOllamaStdout, this);
            }

            if (m_stateCallback) {
                m_stateCallback(ServiceState::Running, "Ollama service running on " + getServiceEndpoint());
            }
            return true;
        }
        ++attempts;
    }

    addLogEntry(LogLevel::Error, "Ollama service failed to become healthy within timeout");
    terminateOllamaProcess();
    m_state = ServiceState::Error;
    return false;
}

bool OllamaServiceManager::stopService() {
    if (m_state == ServiceState::Stopped || m_state == ServiceState::Stopping) {
        return true;
    }

    m_state = ServiceState::Stopping;
    if (m_stateCallback) {
        m_stateCallback(ServiceState::Stopping, "Stopping Ollama service...");
    }

    addLogEntry(LogLevel::Info, "Stopping Ollama service");

    if (!terminateOllamaProcess()) {
        addLogEntry(LogLevel::Error, "Failed to terminate Ollama process gracefully");
        return false;
    }

    m_state = ServiceState::Stopped;
    m_isHealthy = false;
    addLogEntry(LogLevel::Info, "Ollama service stopped successfully");
    
    if (m_stateCallback) {
        m_stateCallback(ServiceState::Stopped, "Ollama service stopped");
    }
    return true;
}

bool OllamaServiceManager::restartService() {
    addLogEntry(LogLevel::Info, "Restarting Ollama service");
    
    if (!stopService()) {
        return false;
    }

    Sleep(2000); // Brief delay between stop/start

    return startService();
}

bool OllamaServiceManager::isServiceRunning() const {
    return m_state == ServiceState::Running;
}

bool OllamaServiceManager::isServiceHealthy() const {
    return m_isHealthy.load() && m_state == ServiceState::Running;
}

OllamaServiceManager::ServiceState OllamaServiceManager::getServiceState() const {
    return m_state.load();
}

DWORD OllamaServiceManager::getServicePID() const {
    return m_processId;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void OllamaServiceManager::setConfig(const ServiceConfig& config) {
    m_config = config;
}

OllamaServiceManager::ServiceConfig OllamaServiceManager::getConfig() const {
    return m_config;
}

std::string OllamaServiceManager::getServiceEndpoint() const {
    return "http://" + m_config.host + ":" + std::to_string(m_config.port);
}

// ============================================================================
// BINARY MANAGEMENT
// ============================================================================

bool OllamaServiceManager::isOllamaBinaryAvailable() const {
    return fs::exists(m_ollamaBinaryPath) && fs::is_regular_file(m_ollamaBinaryPath);
}

bool OllamaServiceManager::downloadOllamaBinary() {
    m_state = ServiceState::Downloading;
    if (m_stateCallback) {
        m_stateCallback(ServiceState::Downloading, "Downloading Ollama binary...");
    }

    addLogEntry(LogLevel::Info, "Downloading Ollama from: " + m_config.ollamaDownloadUrl);

    std::string tempPath = m_ollamaBinaryPath + ".tmp";
    
    auto progressCallback = [this](int progress) {
        if (m_stateCallback) {
            m_stateCallback(ServiceState::Downloading, 
                          "Downloading Ollama binary... " + std::to_string(progress) + "%");
        }
    };

    if (!downloadFile(m_config.ollamaDownloadUrl, tempPath, progressCallback)) {
        addLogEntry(LogLevel::Error, "Failed to download Ollama binary");
        m_state = ServiceState::Error;
        return false;
    }

    // Move temp file to final location and verify integrity
    try {
        // First verify the downloaded binary is valid
        std::ifstream tempFile(tempPath, std::ios::binary);
        if (!tempFile) {
            addLogEntry(LogLevel::Error, "Downloaded file inaccessible");
            return false;
        }
        
        // Read DOS header for basic PE validation
        IMAGE_DOS_HEADER dosHeader = {};
        tempFile.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
        
        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
            addLogEntry(LogLevel::Error, "Downloaded file is not a valid Windows executable");
            fs::remove(tempPath);
            return false;
        }
        
        // Check file size constraints  
        tempFile.seekg(0, std::ios::end);
        std::streamsize fileSize = tempFile.tellg();
        
        if (fileSize < 1024 * 1024) { // Less than 1MB seems too small for Ollama
            addLogEntry(LogLevel::Error, "Downloaded binary suspiciously small: " + std::to_string(fileSize) + " bytes");
            fs::remove(tempPath);
            return false;
        }
        
        if (fileSize > 500 * 1024 * 1024) { // Greater than 500MB seems too large
            addLogEntry(LogLevel::Warning, "Downloaded binary very large: " + std::to_string(fileSize) + " bytes");
        }
        
        tempFile.close();
        
        // Move to final location
        fs::rename(tempPath, m_ollamaBinaryPath);
        addLogEntry(LogLevel::Info, "Ollama binary downloaded and verified successfully (" + 
                   std::to_string(fileSize) + " bytes)");
        return true;
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to verify/move downloaded binary: " + std::string(e.what()));
        fs::remove(tempPath); // Cleanup temp file
        return false;
    }
}

bool OllamaServiceManager::installOllamaBinary() {
    // For embedded use, installation is just ensuring the binary is executable
    if (!isOllamaBinaryAvailable()) {
        return downloadOllamaBinary();
    }
    return true;
}

bool OllamaServiceManager::verifyBinaryIntegrity(const std::string& path) const {
    // Production-hardened binary integrity verification
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        addLogEntry(LogLevel::Error, "Binary file does not exist or is not a regular file: " + path);
        return false;
    }

    // Check file size (Ollama binaries are typically 50-200MB)
    auto fileSize = fs::file_size(path);
    if (fileSize < 10 * 1024 * 1024) { // Less than 10MB is suspicious
        addLogEntry(LogLevel::Error, "Binary file suspiciously small: " + std::to_string(fileSize) + " bytes");
        return false;
    }

    // Verify it's a Windows executable by checking PE header
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        addLogEntry(LogLevel::Error, "Cannot open binary file for integrity check: " + path);
        return false;
    }

    // Read DOS header
    IMAGE_DOS_HEADER dosHeader = {};
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    if (!file || dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        addLogEntry(LogLevel::Error, "Invalid DOS header in binary file: " + path);
        return false;
    }

    // Seek to PE header
    file.seekg(dosHeader.e_lfanew, std::ios::beg);
    DWORD peSignature = 0;
    file.read(reinterpret_cast<char*>(&peSignature), sizeof(peSignature));
    if (!file || peSignature != IMAGE_NT_SIGNATURE) {
        addLogEntry(LogLevel::Error, "Invalid PE signature in binary file: " + path);
        return false;
    }

    // Read COFF header
    IMAGE_FILE_HEADER coffHeader = {};
    file.read(reinterpret_cast<char*>(&coffHeader), sizeof(coffHeader));
    if (!file) {
        addLogEntry(LogLevel::Error, "Cannot read COFF header from binary file: " + path);
        return false;
    }

    // Verify it's a Windows executable (not just any PE file)
    if (coffHeader.Machine != IMAGE_FILE_MACHINE_AMD64 && 
        coffHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        addLogEntry(LogLevel::Error, "Binary is not a Windows x86/x64 executable: " + path);
        return false;
    }

    // Check for executable characteristics
    if (!(coffHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)) {
        addLogEntry(LogLevel::Error, "Binary does not have executable characteristics: " + path);
        return false;
    }

    addLogEntry(LogLevel::Info, "Binary integrity verification passed for: " + path);
    return true;
}

bool OllamaServiceManager::createModelsDirectory() {
    try {
        // Create models directory with proper permissions
        fs::create_directories(m_modelsDirectory);
        
        // Verify directory was created and is writable
        if (!fs::exists(m_modelsDirectory) || !fs::is_directory(m_modelsDirectory)) {
            addLogEntry(LogLevel::Error, "Failed to create models directory: " + m_modelsDirectory);
            return false;
        }

        // Test write permissions by creating a temporary test file
        std::string testFile = m_modelsDirectory + "\\.write_test";
        std::ofstream testStream(testFile);
        if (!testStream) {
            addLogEntry(LogLevel::Error, "Models directory is not writable: " + m_modelsDirectory);
            return false;
        }
        testStream.close();
        fs::remove(testFile); // Clean up test file

        addLogEntry(LogLevel::Info, "Models directory created and verified: " + m_modelsDirectory);
        return true;

    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Exception creating models directory: " + std::string(e.what()));
        return false;
    }
}

bool OllamaServiceManager::setupOllamaEnvironment() {
    // Production-hardened Ollama environment setup
    try {
        // Ensure models directory exists
        if (!createModelsDirectory()) {
            return false;
        }

        // Create config directory if it doesn't exist
        fs::create_directories(m_configDirectory);

        // Set up environment variables for Ollama
        std::string ollamaHome = m_configDirectory;
        std::string modelsPath = m_modelsDirectory;

        // Set OLLAMA_HOME environment variable
        if (!SetEnvironmentVariableA("OLLAMA_HOME", ollamaHome.c_str())) {
            addLogEntry(LogLevel::Warning, "Failed to set OLLAMA_HOME environment variable");
            // Continue anyway, as Ollama might work without it
        }

        // Set OLLAMA_MODELS environment variable
        if (!SetEnvironmentVariableA("OLLAMA_MODELS", modelsPath.c_str())) {
            addLogEntry(LogLevel::Warning, "Failed to set OLLAMA_MODELS environment variable");
            // Continue anyway
        }

        // Verify Ollama binary is available and executable
        if (!isOllamaBinaryAvailable()) {
            addLogEntry(LogLevel::Error, "Ollama binary not available during environment setup");
            return false;
        }

        // Verify binary integrity
        if (!verifyBinaryIntegrity(m_ollamaBinaryPath)) {
            addLogEntry(LogLevel::Error, "Ollama binary integrity check failed");
            return false;
        }

        // Test that Ollama can start (quick version check)
        std::string version = getOllamaVersion();
        if (version == "Not installed" || version.empty()) {
            addLogEntry(LogLevel::Error, "Ollama version check failed during environment setup");
            return false;
        }

        addLogEntry(LogLevel::Info, "Ollama environment setup completed successfully");
        addLogEntry(LogLevel::Info, "OLLAMA_HOME: " + ollamaHome);
        addLogEntry(LogLevel::Info, "OLLAMA_MODELS: " + modelsPath);
        addLogEntry(LogLevel::Info, "Ollama version: " + version);

        return true;

    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Exception during Ollama environment setup: " + std::string(e.what()));
        return false;
    }
}

std::string OllamaServiceManager::getOllamaBinaryPath() const {
    return m_ollamaBinaryPath;
}

std::string OllamaServiceManager::getOllamaVersion() const {
    if (!isOllamaBinaryAvailable()) {
        return "Not installed";
    }

    // Execute ollama version command
    std::string command = "\"" + m_ollamaBinaryPath + "\" --version";
    
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    // Create pipes for output capture
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return "Unknown";
    }

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    if (CreateProcessA(nullptr, const_cast<char*>(command.c_str()), 
                       nullptr, nullptr, TRUE, CREATE_NO_WINDOW, 
                       nullptr, nullptr, &si, &pi)) {
        
        CloseHandle(hWritePipe);
        WaitForSingleObject(pi.hProcess, 5000); // 5 second timeout
        
        // Read output
        char buffer[1024] = {};
        DWORD bytesRead = 0;
        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            std::string output(buffer, bytesRead);
            // Parse version from output
            size_t vPos = output.find("version ");
            if (vPos != std::string::npos) {
                size_t start = vPos + 8;
                size_t end = output.find('\n', start);
                if (end == std::string::npos) end = output.length();
                return output.substr(start, end - start);
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hReadPipe);
    return "Unknown";
}

// ============================================================================
// LOGGING AND MONITORING
// ============================================================================

void OllamaServiceManager::setLogCallback(LogCallback callback) {
    m_logCallback = std::move(callback);
}

void OllamaServiceManager::setStateCallback(StateCallback callback) {
    m_stateCallback = std::move(callback);
}

std::vector<OllamaServiceManager::LogEntry> OllamaServiceManager::getRecentLogs(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logs.size() <= maxCount) {
        return m_logs;
    }
    return std::vector<LogEntry>(m_logs.end() - maxCount, m_logs.end());
}

void OllamaServiceManager::clearLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.clear();
}

// ============================================================================
// HEALTH MONITORING
// ============================================================================

bool OllamaServiceManager::performHealthCheck() {
    auto start = std::chrono::high_resolution_clock::now();
    
    bool healthy = testHttpEndpoint() && testOllamaAPI();
    
    auto end = std::chrono::high_resolution_clock::now();
    m_lastResponseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    m_lastHealthCheck = std::chrono::system_clock::now();
    
    return healthy;
}

std::string OllamaServiceManager::getHealthStatus() const {
    if (m_state != ServiceState::Running) {
        return OllamaUtils::serviceStateToString(m_state.load());
    }

    std::ostringstream oss;
    oss << "Healthy: " << (m_isHealthy ? "Yes" : "No")
        << ", Response time: " << m_lastResponseTime.count() << "ms"
        << ", PID: " << m_processId;
    return oss.str();
}

std::chrono::milliseconds OllamaServiceManager::getLastResponseTime() const {
    return m_lastResponseTime;
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

bool OllamaServiceManager::launchOllamaProcess() {
    // Create pipes for stdout/stderr
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&m_stdoutRead, &m_stdoutWrite, &sa, 0) ||
        !CreatePipe(&m_stderrRead, &m_stderrWrite, &sa, 0)) {
        addLogEntry(LogLevel::Error, "Failed to create pipes for Ollama process");
        return false;
    }

    SetHandleInformation(m_stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_stderrRead, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    std::ostringstream cmdLine;
    cmdLine << "\"" << m_ollamaBinaryPath << "\" serve";
    
    // Set environment variables
    std::ostringstream envVars;
    envVars << "OLLAMA_HOST=" << m_config.host << ":" << m_config.port << '\0';
    envVars << "OLLAMA_MODELS=" << m_modelsDirectory << '\0';
    envVars << "OLLAMA_LOGS=1" << '\0';
    envVars << '\0';
    
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = m_stdoutWrite;
    si.hStdError = m_stderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::string command = cmdLine.str();
    std::string envStr = envVars.str();

    if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()),
                       nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                       const_cast<char*>(envStr.c_str()), nullptr, &si, &pi)) {
        DWORD error = GetLastError();
        addLogEntry(LogLevel::Error, "Failed to create Ollama process. Error: " + std::to_string(error));
        CloseHandle(m_stdoutRead); m_stdoutRead = nullptr;
        CloseHandle(m_stdoutWrite); m_stdoutWrite = nullptr;
        CloseHandle(m_stderrRead); m_stderrRead = nullptr;
        CloseHandle(m_stderrWrite); m_stderrWrite = nullptr;
        return false;
    }

    m_processHandle = pi.hProcess;
    m_processId = pi.dwProcessId;
    CloseHandle(pi.hThread);
    
    // Close write ends of pipes (parent doesn't write)
    CloseHandle(m_stdoutWrite); m_stdoutWrite = nullptr;
    CloseHandle(m_stderrWrite); m_stderrWrite = nullptr;

    addLogEntry(LogLevel::Info, "Ollama process launched with PID: " + std::to_string(m_processId));
    return true;
}

bool OllamaServiceManager::terminateOllamaProcess() {
    if (!m_processHandle) {
        return true;
    }

    addLogEntry(LogLevel::Info, "Terminating Ollama process (PID: " + std::to_string(m_processId) + ")");

    // Try graceful termination first
    if (!TerminateProcess(m_processHandle, 0)) {
        DWORD error = GetLastError();
        addLogEntry(LogLevel::Warning, "Failed to terminate process gracefully. Error: " + std::to_string(error));
    }

    // Wait for process to exit
    DWORD result = WaitForSingleObject(m_processHandle, 5000); // 5 second timeout
    if (result != WAIT_OBJECT_0) {
        addLogEntry(LogLevel::Warning, "Process did not exit within timeout, force killing");
        TerminateProcess(m_processHandle, 1);
    }

    CloseHandle(m_processHandle);
    m_processHandle = nullptr;
    m_processId = 0;

    return true;
}

void OllamaServiceManager::watchOllamaProcess() {
    while (!m_shutdownRequested && m_processHandle) {
        DWORD result = WaitForSingleObject(m_processHandle, 1000);
        
        if (result == WAIT_OBJECT_0) {
            // Process exited
            DWORD exitCode = 0;
            GetExitCodeProcess(m_processHandle, &exitCode);
            handleProcessExit(exitCode);
            break;
        } else if (result == WAIT_FAILED) {
            addLogEntry(LogLevel::Error, "Failed to wait for Ollama process");
            break;
        }
        // WAIT_TIMEOUT is normal, continue monitoring
    }
}

void OllamaServiceManager::performHealthChecks() {
    while (!m_shutdownRequested) {
        if (m_state == ServiceState::Running) {
            bool healthy = performHealthCheck();
            if (healthy != m_isHealthy.load()) {
                m_isHealthy = healthy;
                if (!healthy) {
                    addLogEntry(LogLevel::Warning, "Ollama service became unhealthy - initiating auto-recovery");

                    // Auto-recovery: try to restart unhealthy service
                    if (m_restartAttempts < m_config.maxRestartAttempts) {
                        addLogEntry(LogLevel::Info, "Attempting auto-recovery restart (attempt " +
                                   std::to_string(m_restartAttempts + 1) + "/" +
                                   std::to_string(m_config.maxRestartAttempts) + ")");

                        if (restartService()) {
                            addLogEntry(LogLevel::Info, "Auto-recovery successful - service restored");
                            m_restartAttempts = 0; // Reset on successful recovery
                        } else {
                            ++m_restartAttempts;
                            addLogEntry(LogLevel::Error, "Auto-recovery failed - service remains unhealthy");
                        }
                    } else {
                        addLogEntry(LogLevel::Error, "Auto-recovery exhausted - manual intervention required");
                    }
                } else {
                    addLogEntry(LogLevel::Info, "Ollama service restored to healthy state");
                }
            }
        }

        Sleep(m_config.healthCheckIntervalMs);
    }
}

void OllamaServiceManager::handleProcessExit(DWORD exitCode) {
    addLogEntry(LogLevel::Warning, "Ollama process exited with code: " + std::to_string(exitCode));
    
    m_isHealthy = false;
    m_state = ServiceState::Stopped;
    
    if (m_stateCallback) {
        m_stateCallback(ServiceState::Stopped, "Ollama process exited unexpectedly");
    }

    // Auto-restart if configured and not shutting down
    if (!m_shutdownRequested && m_restartAttempts < m_config.maxRestartAttempts) {
        addLogEntry(LogLevel::Info, "Attempting auto-restart (attempt " + 
                   std::to_string(m_restartAttempts + 1) + "/" + 
                   std::to_string(m_config.maxRestartAttempts) + ")");
        
        ++m_restartAttempts;
        Sleep(2000); // Brief delay before restart
        
        if (startService()) {
            m_restartAttempts = 0; // Reset on successful restart
        }
    }
}

bool OllamaServiceManager::downloadFile(const std::string& url, const std::string& localPath,
                                       std::function<void(int)> progressCallback) {
    // WinHTTP implementation for downloading files
    HINTERNET hSession = WinHttpOpen(L"RawrXD-OllamaManager/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return false;
    }

    // Parse URL
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    
    WCHAR hostname[256] = {};
    WCHAR urlPath[1024] = {};
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = ARRAYSIZE(hostname);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostname, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Get content length for progress tracking
    DWORD contentLength = 0;
    DWORD bufferSize = sizeof(contentLength);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &bufferSize, WINHTTP_NO_HEADER_INDEX);

    // Download and save file
    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD totalDownloaded = 0;
    DWORD bytesAvailable = 0;
    
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<BYTE> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            outFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
            totalDownloaded += bytesRead;
            
            if (progressCallback && contentLength > 0) {
                int progress = static_cast<int>((totalDownloaded * 100) / contentLength);
                progressCallback(progress);
            }
        }
    }

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return totalDownloaded > 0;
}

bool OllamaServiceManager::testHttpEndpoint() const {
    // Simple HTTP GET to /api/version or /api/tags
    std::string endpoint = getServiceEndpoint() + "/api/tags";
    
    // Use WinHTTP for quick test
    HINTERNET hSession = WinHttpOpen(L"RawrXD-HealthCheck/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set a short timeout for health checks
    DWORD timeout = 3000; // 3 seconds
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    bool success = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                   WinHttpReceiveResponse(hRequest, NULL);

    if (success) {
        DWORD statusCode = 0;
        DWORD bufferSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &bufferSize, WINHTTP_NO_HEADER_INDEX);
        
        success = (statusCode == 200);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return success;
}

bool OllamaServiceManager::testOllamaAPI() const {
    return testHttpEndpoint(); // For now, just test HTTP connectivity
    // Could be enhanced to test model loading, etc.
}

void OllamaServiceManager::addLogEntry(LogLevel level, const std::string& message, const std::string& source) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.message = message;
    entry.source = source;

    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logs.push_back(entry);
        
        // Keep only recent logs (prevent memory growth)
        if (m_logs.size() > 1000) {
            m_logs.erase(m_logs.begin(), m_logs.begin() + 200);
        }
    }

    // Send to terminal pane if attached
    sendToTerminalPane(OllamaUtils::formatLogEntry(entry));

    // Callback for UI updates
    if (m_logCallback) {
        m_logCallback(entry);
    }
}

void OllamaServiceManager::readOllamaStdout() {
    if (!m_stdoutRead) return;

    char buffer[4096];
    DWORD bytesRead;
    
    while (!m_shutdownRequested && m_stdoutRead) {
        if (ReadFile(m_stdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string output(buffer, bytesRead);
            processOllamaOutput(output);
        } else {
            Sleep(100);
        }
    }
}

void OllamaServiceManager::processOllamaOutput(const std::string& output) {
    // Parse Ollama output and convert to structured log entries
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        LogLevel level = LogLevel::Info;
        
        // Parse log level from Ollama output
        if (line.find("[ERROR]") != std::string::npos || line.find("error") != std::string::npos) {
            level = LogLevel::Error;
        } else if (line.find("[WARN") != std::string::npos || line.find("warning") != std::string::npos) {
            level = LogLevel::Warning;
        } else if (line.find("[DEBUG]") != std::string::npos) {
            level = LogLevel::Debug;
        }
        
        addLogEntry(level, line, "Ollama");
    }
}

void OllamaServiceManager::sendToTerminalPane(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_terminalMutex);
    if (m_hwndTerminalPane && IsWindow(m_hwndTerminalPane)) {
        // Use proper Win32 console or text control APIs
        std::string formattedMsg = message + "\r\n";
        
        // Try to identify control type and use appropriate API
        WCHAR className[256] = {};
        GetClassNameW(m_hwndTerminalPane, className, ARRAYSIZE(className));
        
        if (wcscmp(className, L"Edit") == 0 || wcscmp(className, L"RichEdit") == 0) {
            // Rich edit or edit control - append text properly
            int textLength = GetWindowTextLengthA(m_hwndTerminalPane);
            SendMessageA(m_hwndTerminalPane, EM_SETSEL, textLength, textLength);
            SendMessageA(m_hwndTerminalPane, EM_REPLACESEL, FALSE, (LPARAM)formattedMsg.c_str());
            SendMessageA(m_hwndTerminalPane, EM_SCROLLCARET, 0, 0);
        } else if (wcscmp(className, L"ConsoleWindowClass") == 0) {
            // Console window - use console APIs
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteConsoleA(hConsole, formattedMsg.c_str(), (DWORD)formattedMsg.length(), &written, nullptr);
            }
        } else {
            // Custom control - try generic text append
            SendMessageA(m_hwndTerminalPane, WM_SETTEXT, 0, (LPARAM)formattedMsg.c_str());
        }
    }
}

void OllamaServiceManager::attachTerminalPane(HWND hwndTerminal) {
    std::lock_guard<std::mutex> lock(m_terminalMutex);
    
    if (hwndTerminal && IsWindow(hwndTerminal)) {
        m_hwndTerminalPane = hwndTerminal;
        addLogEntry(LogLevel::Info, "Terminal pane attached for live logging");
        
        // Send initial status message
        sendToTerminalPane("[OllamaService] Terminal logging enabled");
        sendToTerminalPane("[OllamaService] Current state: " + OllamaUtils::serviceStateToString(m_state.load()));
        
        if (m_state == ServiceState::Running) {
            sendToTerminalPane("[OllamaService] Endpoint: " + getServiceEndpoint());
            sendToTerminalPane("[OllamaService] PID: " + std::to_string(m_processId));
        }
    } else {
        addLogEntry(LogLevel::Warning, "Invalid terminal pane handle provided");
    }
}

void OllamaServiceManager::detachTerminalPane() {
    std::lock_guard<std::mutex> lock(m_terminalMutex);
    
    if (m_hwndTerminalPane) {
        sendToTerminalPane("[OllamaService] Terminal logging disabled");
        m_hwndTerminalPane = nullptr;
        addLogEntry(LogLevel::Info, "Terminal pane detached");
    }
}

// ============================================================================
// MODEL MANAGEMENT INTEGRATION
// ============================================================================

bool OllamaServiceManager::isModelLoaded(const std::string& modelName) const {
    if (!isServiceHealthy()) {
        return false;
    }

    // Query Ollama API for loaded models
    std::string endpoint = getServiceEndpoint() + "/api/tags";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-ModelCheck/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse response and check if model is loaded
    try {
        json resp = json::parse(response);
        if (resp.contains("models") && resp["models"].is_array()) {
            for (const auto& model : resp["models"]) {
                if (model.contains("name") && model["name"] == modelName) {
                    return true;
                }
            }
        }
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to parse model list response: " + std::string(e.what()));
    }

    return false;
}

std::vector<std::string> OllamaServiceManager::getLoadedModels() const {
    std::vector<std::string> models;

    if (!isServiceHealthy()) {
        return models;
    }

    // Query Ollama API for loaded models
    std::string endpoint = getServiceEndpoint() + "/api/tags";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-ModelList/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return models;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return models;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return models;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return models;
    }

    // Read response
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse response and extract model names
    try {
        json resp = json::parse(response);
        if (resp.contains("models") && resp["models"].is_array()) {
            for (const auto& model : resp["models"]) {
                if (model.contains("name") && model["name"].is_string()) {
                    models.push_back(model["name"]);
                }
            }
        }
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to parse model list response: " + std::string(e.what()));
    }

    return models;
}

bool OllamaServiceManager::preloadModel(const std::string& modelName) {
    if (!isServiceHealthy()) {
        addLogEntry(LogLevel::Error, "Cannot preload model: service not healthy");
        return false;
    }

    addLogEntry(LogLevel::Info, "Preloading model: " + modelName);

    // Use Ollama's generate API with a minimal prompt to load the model
    std::string endpoint = getServiceEndpoint() + "/api/generate";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-ModelPreload/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Add content-type header
    if (!WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Build preload request (minimal prompt to load model without generating much)
    json preloadRequest;
    preloadRequest["model"] = modelName;
    preloadRequest["prompt"] = " ";  // Minimal prompt
    preloadRequest["stream"] = false;
    preloadRequest["num_predict"] = 1;  // Generate just 1 token

    std::string bodyStr = preloadRequest.dump();

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length(),
                           (DWORD)bodyStr.length(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        addLogEntry(LogLevel::Error, "Failed to send preload request for model: " + modelName);
        return false;
    }

    // Check response status
    DWORD statusCode = 0;
    DWORD bufferSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &bufferSize, WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    bool success = (statusCode == 200);
    if (success) {
        addLogEntry(LogLevel::Info, "Successfully preloaded model: " + modelName);
    } else {
        addLogEntry(LogLevel::Error, "Failed to preload model: " + modelName + " (HTTP " + std::to_string(statusCode) + ")");
    }

    return success;
}

bool OllamaServiceManager::unloadModel(const std::string& modelName) {
    // Ollama doesn't have a direct "unload" API, but we can suggest it via logging
    addLogEntry(LogLevel::Info, "Model unloading requested for: " + modelName +
                               " (Note: Ollama manages model unloading automatically based on memory pressure)");

    // For now, just return true since Ollama handles this automatically
    // In the future, this could implement model eviction strategies
    return true;
}

// ============================================================================
// ADVANCED MODEL MANAGEMENT (PRODUCTION FEATURES)
// ============================================================================

bool OllamaServiceManager::cacheModelLocally(const std::string& modelName) {
    if (!isServiceHealthy()) {
        addLogEntry(LogLevel::Error, "Cannot cache model: service not healthy");
        return false;
    }

    addLogEntry(LogLevel::Info, "Caching model locally: " + modelName);

    // Use Ollama's pull API to download and cache the model
    std::string endpoint = getServiceEndpoint() + "/api/pull";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-ModelCache/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/pull", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Add content-type header
    if (!WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Build pull request
    json pullRequest;
    pullRequest["name"] = modelName;
    pullRequest["stream"] = false;

    std::string bodyStr = pullRequest.dump();

    // Set longer timeout for model downloads
    DWORD timeout = 300000; // 5 minutes
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length(),
                           (DWORD)bodyStr.length(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        addLogEntry(LogLevel::Error, "Failed to send cache request for model: " + modelName);
        return false;
    }

    // Check response status
    DWORD statusCode = 0;
    DWORD bufferSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &bufferSize, WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    bool success = (statusCode == 200);
    if (success) {
        addLogEntry(LogLevel::Info, "Successfully cached model: " + modelName);
    } else {
        addLogEntry(LogLevel::Error, "Failed to cache model: " + modelName + " (HTTP " + std::to_string(statusCode) + ")");
    }

    return success;
}

bool OllamaServiceManager::isModelCached(const std::string& modelName) const {
    // Check if model exists in local model storage
    std::string modelPath = m_modelsDirectory + "/" + modelName;
    return fs::exists(modelPath) || isModelLoaded(modelName);
}

std::vector<std::string> OllamaServiceManager::getRecommendedModels() const {
    // Production-grade model recommendations based on system capabilities
    std::vector<std::string> recommendations;

    // Get system memory
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    
    DWORDLONG totalMemGB = memStatus.ullTotalPhys / (1024 * 1024 * 1024);
    DWORDLONG availMemGB = memStatus.ullAvailPhys / (1024 * 1024 * 1024);

    addLogEntry(LogLevel::Info, "System has " + std::to_string(totalMemGB) + "GB total, " + 
               std::to_string(availMemGB) + "GB available");

    // Recommend models based on available memory
    if (availMemGB >= 32) {
        recommendations.push_back("codestral:22b-v0.1-q4_K_S");
        recommendations.push_back("llama3.1:70b-instruct-q4_K_M");
        recommendations.push_back("qwen2.5-coder:32b-instruct-q4_K_M");
    } else if (availMemGB >= 16) {
        recommendations.push_back("codestral:22b-v0.1-q4_K_S"); 
        recommendations.push_back("llama3.1:8b-instruct-q8_0");
        recommendations.push_back("qwen2.5-coder:14b-instruct-q4_K_M");
    } else if (availMemGB >= 8) {
        recommendations.push_back("llama3.2:8b-instruct-q4_K_M");
        recommendations.push_back("qwen2.5-coder:7b-instruct-q4_K_M");
        recommendations.push_back("mistral:7b-instruct-q4_K_M");
    } else {
        recommendations.push_back("llama3.2:3b-instruct-q4_K_M");
        recommendations.push_back("codegemma:2b-code-q4_K_M");
        recommendations.push_back("qwen2.5:3b-instruct-q4_K_M");
    }

    return recommendations;
}

std::string OllamaServiceManager::getOptimalModelForTask(const std::string& taskType) const {
    // Task-optimized model selection logic
    if (taskType == "code" || taskType == "coding") {
        return "codestral:22b-v0.1-q4_K_S";
    } else if (taskType == "chat" || taskType == "conversation") {
        return "llama3.1:8b-instruct-q8_0";
    } else if (taskType == "analysis" || taskType == "reasoning") {
        return "qwen2.5-coder:14b-instruct-q4_K_M";
    } else if (taskType == "creative" || taskType == "writing") {
        return "mistral:7b-instruct-q4_K_M";
    } else if (taskType == "fast" || taskType == "quick") {
        return "llama3.2:3b-instruct-q4_K_M";
    } else {
        return "llama3.1:8b-instruct-q8_0"; // general purpose
    }
}

bool OllamaServiceManager::purgeModelCache() {
    addLogEntry(LogLevel::Info, "Purging model cache directory");
    
    try {
        if (fs::exists(m_modelsDirectory)) {
            std::uintmax_t removedBytes = 0;
            for (const auto& entry : fs::recursive_directory_iterator(m_modelsDirectory)) {
                if (entry.is_regular_file()) {
                    removedBytes += entry.file_size();
                }
            }
            
            fs::remove_all(m_modelsDirectory);
            fs::create_directories(m_modelsDirectory);
            
            addLogEntry(LogLevel::Info, "Purged " + std::to_string(removedBytes / (1024*1024)) + " MB from model cache");
            return true;
        }
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to purge model cache: " + std::string(e.what()));
    }
    
    return false;
}

std::size_t OllamaServiceManager::getCacheSize() const {
    std::size_t totalSize = 0;
    
    try {
        if (fs::exists(m_modelsDirectory)) {
            for (const auto& entry : fs::recursive_directory_iterator(m_modelsDirectory)) {
                if (entry.is_regular_file()) {
                    totalSize += entry.file_size();
                }
            }
        }
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to calculate cache size: " + std::string(e.what()));
    }
    
    return totalSize;
}

bool OllamaServiceManager::optimizeModelSelection(const std::string& taskType, std::string& recommendedModel) {
    // Intelligent model selection based on task type
    // This implements production-hardened model selection logic

    std::map<std::string, std::vector<std::string>> taskModelMap = {
        {"code", {"codellama:13b", "codellama:7b", "llama2:13b", "mistral:7b"}},
        {"chat", {"llama2:7b", "mistral:7b", "llama2:13b", "gemma:7b"}},
        {"analysis", {"llama2:13b", "codellama:13b", "mistral:7b", "llama2:7b"}},
        {"creative", {"llama2:13b", "mistral:7b", "gemma:7b", "llama2:7b"}},
        {"fast", {"orca-mini:3b", "phi:2.7b", "gemma:2b", "mistral:7b"}},
        {"general", {"llama2:7b", "mistral:7b", "gemma:7b", "llama2:13b"}}
    };

    auto it = taskModelMap.find(taskType);
    if (it == taskModelMap.end()) {
        // Default to general purpose
        it = taskModelMap.find("general");
    }

    // Find the best available model for this task
    for (const auto& model : it->second) {
        if (isModelCached(model)) {
            recommendedModel = model;
            addLogEntry(LogLevel::Info, "Optimized model selection for task '" + taskType +
                                       "': " + recommendedModel);
            return true;
        }
    }

    // If no cached models for this task, recommend the first one
    if (!it->second.empty()) {
        recommendedModel = it->second[0];
        addLogEntry(LogLevel::Info, "No cached models for task '" + taskType +
                                   "', recommending: " + recommendedModel);
        return true;
    }

    addLogEntry(LogLevel::Warning, "Could not optimize model selection for task: " + taskType);
    return false;
}

// ============================================================================
// PRODUCTION PERFORMANCE MONITORING (PUBLIC RELEASE FEATURES)
// ============================================================================

std::vector<OllamaServiceManager::ModelPerformanceMetrics> OllamaServiceManager::getModelPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(m_performanceMutex);
    std::vector<ModelPerformanceMetrics> metrics;

    for (const auto& pair : m_modelMetrics) {
        metrics.push_back(pair.second);
    }

    // Sort by average response time (fastest first)
    std::sort(metrics.begin(), metrics.end(),
              [](const ModelPerformanceMetrics& a, const ModelPerformanceMetrics& b) {
                  return a.averageResponseTimeMs < b.averageResponseTimeMs;
              });

    return metrics;
}

bool OllamaServiceManager::optimizeModelForPerformance(const std::string& modelName) {
    if (!isServiceHealthy()) {
        addLogEntry(LogLevel::Error, "Cannot optimize model performance: service not healthy");
        return false;
    }

    addLogEntry(LogLevel::Info, "Optimizing performance for model: " + modelName);

    // Preload the model to ensure it's ready
    if (!preloadModel(modelName)) {
        addLogEntry(LogLevel::Error, "Failed to preload model for performance optimization: " + modelName);
        return false;
    }

    // Run a performance test with a small prompt
    auto start = std::chrono::high_resolution_clock::now();

    json testRequest;
    testRequest["model"] = modelName;
    testRequest["prompt"] = "Hello, how are you? Please respond briefly.";
    testRequest["temperature"] = 0.1; // Low temperature for consistent results
    testRequest["num_predict"] = 50;  // Small response for quick test
    testRequest["stream"] = false;

    std::string bodyStr = testRequest.dump();
    std::string endpoint = getServiceEndpoint() + "/api/generate";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-PerformanceTest/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wHost(m_config.host.begin(), m_config.host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), m_config.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Add content-type header
    if (!WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
                                 (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.length(),
                           (DWORD)bodyStr.length(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse response and update metrics
    try {
        json resp = json::parse(response);
        if (resp.contains("response")) {
            std::string result = resp["response"];
            size_t tokenCount = 0;

            // Rough token estimation (words * 1.3 for subword tokens)
            std::istringstream iss(result);
            std::string word;
            while (iss >> word) {
                ++tokenCount;
            }
            tokenCount = static_cast<size_t>(tokenCount * 1.3);

            // Update performance metrics
            std::lock_guard<std::mutex> lock(m_performanceMutex);
            auto& metrics = m_modelMetrics[modelName];
            metrics.modelName = modelName;
            metrics.lastUsed = std::chrono::system_clock::now();

            // Update running averages
            double responseTimeMs = static_cast<double>(duration.count());
            if (metrics.totalRequests == 0) {
                metrics.averageResponseTimeMs = responseTimeMs;
                metrics.tokensPerSecond = tokenCount / (responseTimeMs / 1000.0);
            } else {
                // Exponential moving average
                double alpha = 0.1; // 10% weight for new measurements
                metrics.averageResponseTimeMs = (1 - alpha) * metrics.averageResponseTimeMs + alpha * responseTimeMs;
                metrics.tokensPerSecond = (1 - alpha) * metrics.tokensPerSecond + alpha * (tokenCount / (responseTimeMs / 1000.0));
            }

            ++metrics.totalRequests;
            ++metrics.successfulRequests;

            addLogEntry(LogLevel::Info, "Performance test completed for " + modelName +
                       ": " + std::to_string(responseTimeMs) + "ms, " +
                       std::to_string(metrics.tokensPerSecond) + " tokens/sec");

            return true;
        }
    } catch (const std::exception& e) {
        addLogEntry(LogLevel::Error, "Failed to parse performance test response: " + std::string(e.what()));
    }

    return false;
}

std::string OllamaServiceManager::getBestPerformingModel(const std::string& taskType) const {
    // Get task-appropriate models
    std::vector<std::string> candidateModels;
    std::map<std::string, std::vector<std::string>> taskModelMap = {
        {"code", {"codestral:22b-v0.1-q4_K_S", "qwen2.5-coder:14b-instruct-q4_K_M", "codestral:7b"}},
        {"chat", {"llama3.1:8b-instruct-q8_0", "mistral:7b-instruct-q4_K_M", "llama3.2:3b-instruct-q4_K_M"}},
        {"analysis", {"qwen2.5-coder:14b-instruct-q4_K_M", "llama3.1:8b-instruct-q8_0", "codestral:22b-v0.1-q4_K_S"}},
        {"creative", {"mistral:7b-instruct-q4_K_M", "llama3.1:8b-instruct-q8_0", "llama3.2:3b-instruct-q4_K_M"}},
        {"fast", {"llama3.2:3b-instruct-q4_K_M", "codegemma:2b-code-q4_K_M", "qwen2.5:3b-instruct-q4_K_M"}}
    };

    auto it = taskModelMap.find(taskType);
    if (it != taskModelMap.end()) {
        candidateModels = it->second;
    } else {
        // Default to general purpose models
        candidateModels = {"llama3.1:8b-instruct-q8_0", "mistral:7b-instruct-q4_K_M", "llama3.2:3b-instruct-q4_K_M"};
    }

    // Find the best performing model among candidates
    std::lock_guard<std::mutex> lock(m_performanceMutex);
    std::string bestModel;
    double bestScore = std::numeric_limits<double>::max(); // Lower response time is better

    for (const auto& model : candidateModels) {
        auto metricIt = m_modelMetrics.find(model);
        if (metricIt != m_modelMetrics.end() && metricIt->second.successfulRequests > 0) {
            // Score based on response time, weighted by success rate
            double successRate = static_cast<double>(metricIt->second.successfulRequests) /
                               static_cast<double>(metricIt->second.totalRequests);
            double score = metricIt->second.averageResponseTimeMs / successRate;

            if (score < bestScore) {
                bestScore = score;
                bestModel = model;
            }
        }
    }

    // If no performance data available, return the first available cached model
    if (bestModel.empty()) {
        for (const auto& model : candidateModels) {
            if (isModelCached(model)) {
                bestModel = model;
                break;
            }
        }
    }

    // If still no model found, return the first candidate
    if (bestModel.empty() && !candidateModels.empty()) {
        bestModel = candidateModels[0];
    }

    return bestModel;
}

namespace OllamaUtils {
    std::string formatLogEntry(const OllamaServiceManager::LogEntry& entry) {
        std::ostringstream oss;
        
        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()) % 1000;
        
        oss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        
        // Format level
        switch (entry.level) {
            case OllamaServiceManager::LogLevel::Error:   oss << "[ERROR] "; break;
            case OllamaServiceManager::LogLevel::Warning: oss << "[WARN]  "; break;
            case OllamaServiceManager::LogLevel::Info:    oss << "[INFO]  "; break;
            case OllamaServiceManager::LogLevel::Debug:   oss << "[DEBUG] "; break;
        }
        
        oss << entry.message;
        return oss.str();
    }

    std::string serviceStateToString(OllamaServiceManager::ServiceState state) {
        switch (state) {
            case OllamaServiceManager::ServiceState::Stopped:     return "Stopped";
            case OllamaServiceManager::ServiceState::Starting:    return "Starting";
            case OllamaServiceManager::ServiceState::Running:     return "Running";
            case OllamaServiceManager::ServiceState::Stopping:    return "Stopping";
            case OllamaServiceManager::ServiceState::Error:       return "Error";
            case OllamaServiceManager::ServiceState::Downloading: return "Downloading";
            case OllamaServiceManager::ServiceState::Installing:  return "Installing";
            default: return "Unknown";
        }
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    bool isPortAvailable(int port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }

        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<u_short>(port));

        bool available = (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        
        closesocket(sock);
        WSACleanup();
        return available;
    }

    bool killProcessByPort(int port) {
        // Use netstat to find process using the port, then terminate it
        std::ostringstream cmdStream;
        cmdStream << "netstat -ano | findstr ::" << port;
        
        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        
        // Create pipes for output capture
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return false;
        }
        
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;
        
        std::string cmd = "cmd.exe /c " + cmdStream.str();
        
        if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), 
                           nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                           nullptr, nullptr, &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return false;
        }
        
        CloseHandle(hWritePipe);
        WaitForSingleObject(pi.hProcess, 5000);
        
        // Read netstat output
        char buffer[4096] = {};
        DWORD bytesRead = 0;
        std::string output;
        
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            output.append(buffer, bytesRead);
        }
        
        CloseHandle(hReadPipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Parse PID from netstat output
        std::istringstream iss(output);
        std::string line;
        DWORD targetPID = 0;
        
        while (std::getline(iss, line)) {\n            // Look for lines containing the port and extract PID\n            if (line.find(\":\" + std::to_string(port)) != std::string::npos) {\n                size_t lastSpace = line.find_last_of(' ');\n                if (lastSpace != std::string::npos) {\n                    std::string pidStr = line.substr(lastSpace + 1);\n                    try {\n                        targetPID = std::stoul(pidStr);\n                        break;\n                    } catch (const std::exception&) {\n                        continue;\n                    }\n                }\n            }\n        }\n        \n        if (targetPID == 0) {\n            return false; // No process found using the port\n        }\n        \n        // Terminate the process\n        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, targetPID);\n        if (hProcess) {\n            bool success = TerminateProcess(hProcess, 1);\n            CloseHandle(hProcess);\n            return success;\n        }\n        \n        return false;\n    }

    std::vector<std::string> parseOllamaModels(const std::string& apiResponse) {
        std::vector<std::string> models;
        
        try {
            json response = json::parse(apiResponse);
            if (response.contains("models") && response["models"].is_array()) {
                for (const auto& model : response["models"]) {
                    if (model.contains("name") && model["name"].is_string()) {
                        models.push_back(model["name"]);
                    }
                }
            }
        } catch (const std::exception&) {
            // Parse error, return empty vector
        }
        
        return models;
    }
}