/**
 * @file shell_integration.cpp
 * @brief Shell integration for command execution implementation
 * Batch 5 - Item 65: Shell integration
 */

#include "terminal/shell_integration.h"
#include <windows.h>
#include <thread>
#include <sstream>

namespace RawrXD::Terminal {

ShellIntegration::ShellIntegration()
    : m_currentShell(ShellType::Cmd)
    , m_workingDirectory(".")
    , m_running(false) {
    m_process.handle = nullptr;
    m_process.stdinWrite = nullptr;
    m_process.stdoutRead = nullptr;
    m_process.stderrRead = nullptr;
}

ShellIntegration::~ShellIntegration() {
    shutdown();
}

bool ShellIntegration::initialize() {
    m_currentShell = detectShell();
    return true;
}

void ShellIntegration::shutdown() {
    stopShell();
}

ShellType ShellIntegration::detectShell() {
    // Check for PowerShell Core first
    wchar_t pwshPath[MAX_PATH];
    if (SearchPathW(nullptr, L"pwsh.exe", nullptr, MAX_PATH, pwshPath, nullptr)) {
        return ShellType::PowerShellCore;
    }
    
    // Check for Windows PowerShell
    wchar_t psPath[MAX_PATH];
    if (SearchPathW(nullptr, L"powershell.exe", nullptr, MAX_PATH, psPath, nullptr)) {
        return ShellType::PowerShell;
    }
    
    // Check for Git Bash
    if (SearchPathW(nullptr, L"bash.exe", nullptr, MAX_PATH, psPath, nullptr)) {
        return ShellType::GitBash;
    }
    
    // Default to Cmd
    return ShellType::Cmd;
}

std::vector<ShellType> ShellIntegration::getAvailableShells() {
    std::vector<ShellType> shells;
    
    shells.push_back(ShellType::Cmd);
    
    wchar_t path[MAX_PATH];
    if (SearchPathW(nullptr, L"powershell.exe", nullptr, MAX_PATH, path, nullptr)) {
        shells.push_back(ShellType::PowerShell);
    }
    
    if (SearchPathW(nullptr, L"pwsh.exe", nullptr, MAX_PATH, path, nullptr)) {
        shells.push_back(ShellType::PowerShellCore);
    }
    
    if (SearchPathW(nullptr, L"bash.exe", nullptr, MAX_PATH, path, nullptr)) {
        shells.push_back(ShellType::GitBash);
    }
    
    if (SearchPathW(nullptr, L"wsl.exe", nullptr, MAX_PATH, path, nullptr)) {
        shells.push_back(ShellType::Wsl);
    }
    
    return shells;
}

std::string ShellIntegration::getShellName(ShellType type) const {
    switch (type) {
        case ShellType::Cmd: return "Command Prompt";
        case ShellType::PowerShell: return "Windows PowerShell";
        case ShellType::PowerShellCore: return "PowerShell Core";
        case ShellType::Bash: return "Bash";
        case ShellType::Zsh: return "Zsh";
        case ShellType::Fish: return "Fish";
        case ShellType::Wsl: return "WSL";
        case ShellType::GitBash: return "Git Bash";
        default: return "Unknown";
    }
}

std::string ShellIntegration::getShellPath(ShellType type) const {
    switch (type) {
        case ShellType::Cmd:
            return "cmd.exe";
        case ShellType::PowerShell:
            return "powershell.exe";
        case ShellType::PowerShellCore:
            return "pwsh.exe";
        case ShellType::Bash:
        case ShellType::GitBash:
            return "bash.exe";
        case ShellType::Wsl:
            return "wsl.exe";
        default:
            return "";
    }
}

bool ShellIntegration::startShell(ShellType type) {
    if (m_running) {
        stopShell();
    }
    
    m_currentShell = type;
    std::string shellPath = getShellPath(type);
    
    if (shellPath.empty()) {
        return false;
    }
    
    // Create pipes for stdin, stdout, stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE stdinRead, stdinWrite;
    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;
    
    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
        !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        return false;
    }
    
    // Ensure write handles are not inherited
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Prepare command line
    std::wstring cmdLine = L"\"" + std::wstring(shellPath.begin(), shellPath.end()) + L"\"";
    if (type == ShellType::Cmd) {
        cmdLine += L" /K";
    } else if (type == ShellType::PowerShell || type == ShellType::PowerShellCore) {
        cmdLine += L" -NoExit -Command -";
    }
    
    // Create process
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    
    PROCESS_INFORMATION pi = {};
    
    std::wstring workingDir = std::wstring(m_workingDirectory.begin(), m_workingDirectory.end());
    
    BOOL success = CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.c_str(),
        &si,
        &pi
    );
    
    // Close inherited handles
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    
    if (!success) {
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);
        return false;
    }
    
    // Store handles
    m_process.handle = pi.hProcess;
    m_process.stdinWrite = stdinWrite;
    m_process.stdoutRead = stdoutRead;
    m_process.stderrRead = stderrRead;
    m_process.running = true;
    m_running = true;
    
    CloseHandle(pi.hThread);
    
    // Start output reader threads
    std::thread stdoutThread(&ShellIntegration::readOutputThread, this);
    stdoutThread.detach();
    
    std::thread stderrThread(&ShellIntegration::readErrorThread, this);
    stderrThread.detach();
    
    return true;
}

bool ShellIntegration::startShell(const std::string& command) {
    if (m_running) {
        stopShell();
    }
    
    // Create pipes
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE stdinRead, stdinWrite;
    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;
    
    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
        !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        return false;
    }
    
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create process
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    
    PROCESS_INFORMATION pi = {};
    
    std::string cmdLine = "cmd.exe /C \"" + command + "\"";
    
    BOOL success = CreateProcessA(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        m_workingDirectory.c_str(),
        &si,
        &pi
    );
    
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    
    if (!success) {
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);
        return false;
    }
    
    m_process.handle = pi.hProcess;
    m_process.stdinWrite = stdinWrite;
    m_process.stdoutRead = stdoutRead;
    m_process.stderrRead = stderrRead;
    m_process.running = true;
    m_running = true;
    
    CloseHandle(pi.hThread);
    
    std::thread stdoutThread(&ShellIntegration::readOutputThread, this);
    stdoutThread.detach();
    
    std::thread stderrThread(&ShellIntegration::readErrorThread, this);
    stderrThread.detach();
    
    return true;
}

void ShellIntegration::stopShell() {
    if (m_process.handle) {
        TerminateProcess(m_process.handle, 0);
        CloseHandle(m_process.handle);
        m_process.handle = nullptr;
    }
    
    if (m_process.stdinWrite) {
        CloseHandle(m_process.stdinWrite);
        m_process.stdinWrite = nullptr;
    }
    
    if (m_process.stdoutRead) {
        CloseHandle(m_process.stdoutRead);
        m_process.stdoutRead = nullptr;
    }
    
    if (m_process.stderrRead) {
        CloseHandle(m_process.stderrRead);
        m_process.stderrRead = nullptr;
    }
    
    m_process.running = false;
    m_running = false;
}

bool ShellIntegration::isRunning() const {
    if (!m_process.handle) return false;
    
    DWORD exitCode;
    if (GetExitCodeProcess(m_process.handle, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

bool ShellIntegration::executeCommand(const std::string& command) {
    if (!m_running || !m_process.stdinWrite) {
        return false;
    }
    
    std::string cmd = command + "\n";
    DWORD written;
    return WriteFile(m_process.stdinWrite, cmd.c_str(), static_cast<DWORD>(cmd.length()), &written, nullptr);
}

std::future<CommandResult> ShellIntegration::executeCommandAsync(const std::string& command) {
    return std::async(std::launch::async, [this, command]() {
        return executeCommandSync(command, 30000);
    });
}

CommandResult ShellIntegration::executeCommandSync(const std::string& command, int timeoutMs) {
    CommandResult result;
    
    // Create temporary shell for sync execution
    ShellIntegration tempShell;
    tempShell.setWorkingDirectory(m_workingDirectory);
    
    if (!tempShell.startShell(ShellType::Cmd)) {
        result.exitCode = -1;
        result.stderr = "Failed to start shell";
        return result;
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // Execute command
    tempShell.executeCommand(command);
    tempShell.executeCommand("exit");
    
    // Wait for completion
    std::string output, error;
    while (tempShell.isRunning()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            tempShell.stopShell();
            result.exitCode = -1;
            result.stderr = "Command timed out";
            return result;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Read remaining output
    char buffer[4096];
    DWORD read;
    
    if (tempShell.m_process.stdoutRead) {
        while (PeekNamedPipe(tempShell.m_process.stdoutRead, nullptr, 0, nullptr, &read, nullptr) && read > 0) {
            if (ReadFile(tempShell.m_process.stdoutRead, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
                buffer[read] = '\0';
                output += buffer;
            }
        }
    }
    
    if (tempShell.m_process.stderrRead) {
        while (PeekNamedPipe(tempShell.m_process.stderrRead, nullptr, 0, nullptr, &read, nullptr) && read > 0) {
            if (ReadFile(tempShell.m_process.stderrRead, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
                buffer[read] = '\0';
                error += buffer;
            }
        }
    }
    
    DWORD exitCode;
    GetExitCodeProcess(tempShell.m_process.handle, &exitCode);
    
    result.exitCode = static_cast<int>(exitCode);
    result.stdout = output;
    result.stderr = error;
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );
    
    return result;
}

void ShellIntegration::sendInput(const std::string& input) {
    executeCommand(input);
}

std::string ShellIntegration::readOutput() {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    std::string output = m_outputBuffer;
    m_outputBuffer.clear();
    return output;
}

std::string ShellIntegration::readError() {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    std::string error = m_errorBuffer;
    m_errorBuffer.clear();
    return error;
}

bool ShellIntegration::hasOutput() const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    return !m_outputBuffer.empty();
}

bool ShellIntegration::hasError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return !m_errorBuffer.empty();
}

void ShellIntegration::setEnvironmentVariable(const std::string& name, const std::string& value) {
    SetEnvironmentVariableA(name.c_str(), value.c_str());
}

void ShellIntegration::unsetEnvironmentVariable(const std::string& name) {
    SetEnvironmentVariableA(name.c_str(), nullptr);
}

std::string ShellIntegration::getEnvironmentVariable(const std::string& name) {
    char buffer[32767];
    DWORD len = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        return std::string(buffer, len);
    }
    return "";
}

std::vector<EnvironmentVariable> ShellIntegration::getEnvironmentVariables() {
    std::vector<EnvironmentVariable> vars;
    
    LPCH envStrings = GetEnvironmentStrings();
    if (envStrings) {
        LPCH current = envStrings;
        while (*current) {
            std::string entry(current);
            size_t pos = entry.find('=');
            if (pos != std::string::npos && pos > 0) {
                EnvironmentVariable var;
                var.name = entry.substr(0, pos);
                var.value = entry.substr(pos + 1);
                vars.push_back(var);
            }
            current += entry.length() + 1;
        }
        FreeEnvironmentStrings(envStrings);
    }
    
    return vars;
}

void ShellIntegration::setWorkingDirectory(const std::string& path) {
    m_workingDirectory = path;
}

std::string ShellIntegration::getWorkingDirectory() const {
    return m_workingDirectory;
}

void ShellIntegration::setShellArgs(const std::vector<std::string>& args) {
    m_shellArgs = args;
}

void ShellIntegration::setStartupCommands(const std::vector<std::string>& commands) {
    m_startupCommands = commands;
}

void ShellIntegration::onOutput(OutputCallback callback) {
    m_outputCallback = callback;
}

void ShellIntegration::onError(ErrorCallback callback) {
    m_errorCallback = callback;
}

void ShellIntegration::onExit(ExitCallback callback) {
    m_exitCallback = callback;
}

void ShellIntegration::readOutputThread() {
    char buffer[4096];
    DWORD read;
    
    while (m_running && m_process.stdoutRead) {
        if (ReadFile(m_process.stdoutRead, buffer, sizeof(buffer) - 1, &read, nullptr)) {
            if (read > 0) {
                buffer[read] = '\0';
                {
                    std::lock_guard<std::mutex> lock(m_outputMutex);
                    m_outputBuffer += buffer;
                }
                if (m_outputCallback) {
                    m_outputCallback(buffer);
                }
            }
        } else {
            break;
        }
    }
}

void ShellIntegration::readErrorThread() {
    char buffer[4096];
    DWORD read;
    
    while (m_running && m_process.stderrRead) {
        if (ReadFile(m_process.stderrRead, buffer, sizeof(buffer) - 1, &read, nullptr)) {
            if (read > 0) {
                buffer[read] = '\0';
                {
                    std::lock_guard<std::mutex> lock(m_errorMutex);
                    m_errorBuffer += buffer;
                }
                if (m_errorCallback) {
                    m_errorCallback(buffer);
                }
            }
        } else {
            break;
        }
    }
}

} // namespace RawrXD::Terminal
