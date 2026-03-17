/**
 * @file terminal_pool.cpp
 * @brief Implementation of terminal pool management
 */

#include "terminal_pool.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#endif

// =============================================================================
// Construction / Destruction
// =============================================================================

TerminalPool::TerminalPool(int poolSize)
    : m_poolSize(poolSize)
{
#ifdef _WIN32
    m_shell = "cmd.exe";
#else
    m_shell = "/bin/bash";
#endif
    
    char cwd[4096];
#ifdef _WIN32
    if (GetCurrentDirectoryA(sizeof(cwd), cwd)) {
        m_defaultWorkingDirectory = cwd;
    }
#else
    if (getcwd(cwd, sizeof(cwd))) {
        m_defaultWorkingDirectory = cwd;
    }
#endif
}

TerminalPool::~TerminalPool() {
    shutdown();
}

// =============================================================================
// Pool Management
// =============================================================================

void TerminalPool::initialize() {
    if (m_initialized.exchange(true)) {
        return; // Already initialized
    }
    
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    // Pre-create sessions
    for (int i = 0; i < m_poolSize; ++i) {
        auto session = std::make_unique<TerminalSession>();
        session->id = m_nextSessionId++;
        session->name = "Terminal " + std::to_string(session->id);
        session->workingDirectory = m_defaultWorkingDirectory;
        session->active = false;
        session->busy = false;
        m_sessions.push_back(std::move(session));
    }
}

void TerminalPool::shutdown() {
    if (!m_initialized.load()) {
        return;
    }
    
    m_shutdown.store(true);
    
    // Wake up any waiting threads
    m_sessionAvailable.notify_all();
    
    // Join reader threads
    for (auto& thread : m_readerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_readerThreads.clear();
    
    // Close all sessions
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& session : m_sessions) {
            if (session && session->active) {
                terminateProcess(*session);
            }
        }
        m_sessions.clear();
    }
    
    m_initialized.store(false);
}

int TerminalPool::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    int count = 0;
    for (const auto& session : m_sessions) {
        if (session && session->active) {
            ++count;
        }
    }
    return count;
}

int TerminalPool::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    int count = 0;
    for (const auto& session : m_sessions) {
        if (session && !session->busy) {
            ++count;
        }
    }
    return count;
}

// =============================================================================
// Terminal Session Management
// =============================================================================

int TerminalPool::createSession(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    auto session = std::make_unique<TerminalSession>();
    session->id = m_nextSessionId++;
    session->name = name.empty() ? ("Terminal " + std::to_string(session->id)) : name;
    session->workingDirectory = m_defaultWorkingDirectory;
    session->active = false;
    session->busy = false;
    
    int id = session->id;
    m_sessions.push_back(std::move(session));
    
    if (m_onSessionCreated) {
        m_onSessionCreated(id);
    }
    
    return id;
}

bool TerminalPool::closeSession(int sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (*it && (*it)->id == sessionId) {
            if ((*it)->active) {
                terminateProcess(**it);
            }
            m_sessions.erase(it);
            
            if (m_onSessionClosed) {
                m_onSessionClosed(sessionId);
            }
            
            m_sessionAvailable.notify_one();
            return true;
        }
    }
    
    return false;
}

TerminalSession* TerminalPool::getSession(int sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    for (auto& session : m_sessions) {
        if (session && session->id == sessionId) {
            return session.get();
        }
    }
    
    return nullptr;
}

std::vector<int> TerminalPool::getActiveSessionIds() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    std::vector<int> ids;
    for (const auto& session : m_sessions) {
        if (session && session->active) {
            ids.push_back(session->id);
        }
    }
    return ids;
}

// =============================================================================
// Command Execution
// =============================================================================

CommandResult TerminalPool::executeCommand(const std::string& command, int sessionId) {
    CommandResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int actualSessionId = (sessionId == -1) ? acquireSession() : sessionId;
    if (actualSessionId == -1) {
        result.success = false;
        result.error = "No available terminal session";
        return result;
    }
    
    result.terminalId = actualSessionId;
    
    TerminalSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& s : m_sessions) {
            if (s && s->id == actualSessionId) {
                session = s.get();
                break;
            }
        }
    }
    
    if (!session) {
        result.success = false;
        result.error = "Session not found";
        if (sessionId == -1) {
            releaseSession(actualSessionId);
        }
        return result;
    }
    
    // Spawn process if not active
    if (!session->active) {
        if (!spawnProcess(*session)) {
            result.success = false;
            result.error = "Failed to spawn terminal process";
            if (sessionId == -1) {
                releaseSession(actualSessionId);
            }
            return result;
        }
    }
    
    // Execute command
#ifdef _WIN32
    std::string fullCommand = command + "\r\n";
#else
    std::string fullCommand = command + "\n";
#endif
    
    if (!writeToPipe(*session, fullCommand)) {
        result.success = false;
        result.error = "Failed to write to terminal";
        if (sessionId == -1) {
            releaseSession(actualSessionId);
        }
        return result;
    }
    
    // Read output
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    result.output = readFromPipe(*session, false, 5000);
    result.error = readFromPipe(*session, true, 100);
    
    // Determine success (basic heuristic)
    result.success = result.error.empty() || 
                     result.error.find("error") == std::string::npos;
    result.exitCode = result.success ? 0 : 1;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.totalCommandsExecuted++;
        if (result.success) {
            m_stats.successfulCommands++;
        } else {
            m_stats.failedCommands++;
        }
        m_stats.totalExecutionTimeMs += result.executionTimeMs;
        m_stats.averageExecutionTimeMs = 
            m_stats.totalExecutionTimeMs / m_stats.totalCommandsExecuted;
    }
    
    if (sessionId == -1) {
        releaseSession(actualSessionId);
    }
    
    return result;
}

CommandResult TerminalPool::executeCommandAsync(const std::string& command, int sessionId) {
    // For async, we just return immediately after sending the command
    CommandResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int actualSessionId = (sessionId == -1) ? acquireSession() : sessionId;
    if (actualSessionId == -1) {
        result.success = false;
        result.error = "No available terminal session";
        return result;
    }
    
    result.terminalId = actualSessionId;
    
    TerminalSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& s : m_sessions) {
            if (s && s->id == actualSessionId) {
                session = s.get();
                break;
            }
        }
    }
    
    if (!session) {
        result.success = false;
        result.error = "Session not found";
        return result;
    }
    
    // Spawn process if not active
    if (!session->active) {
        if (!spawnProcess(*session)) {
            result.success = false;
            result.error = "Failed to spawn terminal process";
            return result;
        }
    }
    
    // Execute command (non-blocking)
#ifdef _WIN32
    std::string fullCommand = command + "\r\n";
#else
    std::string fullCommand = command + "\n";
#endif
    
    result.success = writeToPipe(*session, fullCommand);
    if (!result.success) {
        result.error = "Failed to write to terminal";
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    return result;
}

bool TerminalPool::sendInput(int sessionId, const std::string& input) {
    TerminalSession* session = getSession(sessionId);
    if (!session || !session->active) {
        return false;
    }
    return writeToPipe(*session, input);
}

std::string TerminalPool::readOutput(int sessionId, int timeoutMs) {
    TerminalSession* session = getSession(sessionId);
    if (!session || !session->active) {
        return "";
    }
    return readFromPipe(*session, false, timeoutMs);
}

std::string TerminalPool::readError(int sessionId, int timeoutMs) {
    TerminalSession* session = getSession(sessionId);
    if (!session || !session->active) {
        return "";
    }
    return readFromPipe(*session, true, timeoutMs);
}

// =============================================================================
// Working Directory
// =============================================================================

void TerminalPool::setDefaultWorkingDirectory(const std::string& path) {
    m_defaultWorkingDirectory = path;
}

std::string TerminalPool::getDefaultWorkingDirectory() const {
    return m_defaultWorkingDirectory;
}

bool TerminalPool::setSessionWorkingDirectory(int sessionId, const std::string& path) {
    TerminalSession* session = getSession(sessionId);
    if (!session) {
        return false;
    }
    
    session->workingDirectory = path;
    
    if (session->active) {
        // Execute cd command
#ifdef _WIN32
        std::string cmd = "cd /d \"" + path + "\"\r\n";
#else
        std::string cmd = "cd \"" + path + "\"\n";
#endif
        return writeToPipe(*session, cmd);
    }
    
    return true;
}

// =============================================================================
// Shell Configuration
// =============================================================================

void TerminalPool::setShell(const std::string& shell) {
    m_shell = shell;
}

std::string TerminalPool::getShell() const {
    return m_shell;
}

void TerminalPool::setEnvironmentVariable(const std::string& name, const std::string& value) {
    m_environment[name] = value;
}

void TerminalPool::clearEnvironmentVariable(const std::string& name) {
    m_environment.erase(name);
}

// =============================================================================
// Statistics
// =============================================================================

TerminalPool::Statistics TerminalPool::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void TerminalPool::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = Statistics{};
}

// =============================================================================
// Internal Helpers
// =============================================================================

int TerminalPool::acquireSession() {
    std::unique_lock<std::mutex> lock(m_sessionsMutex);
    
    // Wait for an available session
    m_sessionAvailable.wait_for(lock, std::chrono::seconds(30), [this]() {
        if (m_shutdown.load()) return true;
        for (const auto& session : m_sessions) {
            if (session && !session->busy) {
                return true;
            }
        }
        return false;
    });
    
    if (m_shutdown.load()) {
        return -1;
    }
    
    // Find and mark a session as busy
    for (auto& session : m_sessions) {
        if (session && !session->busy) {
            session->busy = true;
            return session->id;
        }
    }
    
    return -1;
}

void TerminalPool::releaseSession(int sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    for (auto& session : m_sessions) {
        if (session && session->id == sessionId) {
            session->busy = false;
            break;
        }
    }
    
    m_sessionAvailable.notify_one();
}

#ifdef _WIN32

bool TerminalPool::spawnProcess(TerminalSession& session) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStdinRead, hStdinWrite;
    
    // Create pipes
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return false;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdoutWrite;
    si.hStdOutput = hStdoutWrite;
    si.hStdInput = hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    std::string cmdLine = m_shell;
    
    if (!CreateProcessA(
        NULL,
        const_cast<char*>(cmdLine.c_str()),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        session.workingDirectory.empty() ? NULL : session.workingDirectory.c_str(),
        &si,
        &pi
    )) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        return false;
    }
    
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);
    CloseHandle(pi.hThread);
    
    session.processHandle = pi.hProcess;
    session.inputPipe = hStdinWrite;
    session.outputPipe = hStdoutRead;
    session.active = true;
    
    return true;
}

void TerminalPool::terminateProcess(TerminalSession& session) {
    if (session.processHandle) {
        TerminateProcess(static_cast<HANDLE>(session.processHandle), 0);
        CloseHandle(static_cast<HANDLE>(session.processHandle));
        session.processHandle = nullptr;
    }
    if (session.inputPipe) {
        CloseHandle(static_cast<HANDLE>(session.inputPipe));
        session.inputPipe = nullptr;
    }
    if (session.outputPipe) {
        CloseHandle(static_cast<HANDLE>(session.outputPipe));
        session.outputPipe = nullptr;
    }
    session.active = false;
}

std::string TerminalPool::readFromPipe(TerminalSession& session, bool isStderr, int timeoutMs) {
    if (!session.outputPipe) {
        return "";
    }
    
    std::string result;
    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvailable;
    
    auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    
    while (std::chrono::steady_clock::now() < endTime) {
        if (!PeekNamedPipe(static_cast<HANDLE>(session.outputPipe), NULL, 0, NULL, &bytesAvailable, NULL)) {
            break;
        }
        
        if (bytesAvailable == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (ReadFile(static_cast<HANDLE>(session.outputPipe), buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
            
            if (m_onOutput && !isStderr) {
                m_onOutput(session.id, buffer);
            } else if (m_onError && isStderr) {
                m_onError(session.id, buffer);
            }
        } else {
            break;
        }
    }
    
    return result;
}

bool TerminalPool::writeToPipe(TerminalSession& session, const std::string& data) {
    if (!session.inputPipe) {
        return false;
    }
    
    DWORD bytesWritten;
    return WriteFile(
        static_cast<HANDLE>(session.inputPipe),
        data.c_str(),
        static_cast<DWORD>(data.size()),
        &bytesWritten,
        NULL
    ) && bytesWritten == data.size();
}

#else // POSIX

bool TerminalPool::spawnProcess(TerminalSession& session) {
    int stdinPipe[2];
    int stdoutPipe[2];
    
    if (pipe(stdinPipe) < 0) {
        return false;
    }
    
    if (pipe(stdoutPipe) < 0) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        return false;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return false;
    }
    
    if (pid == 0) {
        // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        
        if (!session.workingDirectory.empty()) {
            chdir(session.workingDirectory.c_str());
        }
        
        // Set environment variables
        for (const auto& env : m_environment) {
            setenv(env.first.c_str(), env.second.c_str(), 1);
        }
        
        execl(m_shell.c_str(), m_shell.c_str(), "-i", nullptr);
        _exit(1);
    }
    
    // Parent process
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    
    // Set non-blocking
    int flags = fcntl(stdoutPipe[0], F_GETFL, 0);
    fcntl(stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);
    
    session.pid = pid;
    session.inputFd = stdinPipe[1];
    session.outputFd = stdoutPipe[0];
    session.active = true;
    
    return true;
}

void TerminalPool::terminateProcess(TerminalSession& session) {
    if (session.pid > 0) {
        kill(session.pid, SIGTERM);
        int status;
        waitpid(session.pid, &status, WNOHANG);
        session.pid = -1;
    }
    if (session.inputFd >= 0) {
        close(session.inputFd);
        session.inputFd = -1;
    }
    if (session.outputFd >= 0) {
        close(session.outputFd);
        session.outputFd = -1;
    }
    session.active = false;
}

std::string TerminalPool::readFromPipe(TerminalSession& session, bool isStderr, int timeoutMs) {
    if (session.outputFd < 0) {
        return "";
    }
    
    std::string result;
    char buffer[4096];
    
    auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    
    while (std::chrono::steady_clock::now() < endTime) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(session.outputFd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms
        
        int ret = select(session.outputFd + 1, &readfds, NULL, NULL, &tv);
        
        if (ret > 0 && FD_ISSET(session.outputFd, &readfds)) {
            ssize_t bytesRead = read(session.outputFd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
                
                if (m_onOutput && !isStderr) {
                    m_onOutput(session.id, buffer);
                }
            } else if (bytesRead == 0) {
                break; // EOF
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break; // Error
            }
        } else if (ret < 0 && errno != EINTR) {
            break; // Error
        }
    }
    
    return result;
}

bool TerminalPool::writeToPipe(TerminalSession& session, const std::string& data) {
    if (session.inputFd < 0) {
        return false;
    }
    
    ssize_t written = write(session.inputFd, data.c_str(), data.size());
    return written == static_cast<ssize_t>(data.size());
}

#endif

void TerminalPool::outputReaderThread(int sessionId) {
    while (!m_shutdown.load()) {
        TerminalSession* session = getSession(sessionId);
        if (!session || !session->active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        std::string output = readFromPipe(*session, false, 100);
        if (!output.empty()) {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            session->outputBuffer += output;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
