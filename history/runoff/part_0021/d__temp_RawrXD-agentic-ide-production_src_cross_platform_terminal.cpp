#include "cross_platform_terminal.h"
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#ifdef __linux__
#include <glib.h>
#endif
#endif

CrossPlatformTerminal::CrossPlatformTerminal()
    : m_running(false)
    , m_currentShell(PWSH)
{
#ifdef _WIN32
    m_hChildStd_IN_Rd = nullptr;
    m_hChildStd_IN_Wr = nullptr;
    m_hChildStd_OUT_Rd = nullptr;
    m_hChildStd_OUT_Wr = nullptr;
    m_hChildStd_ERR_Rd = nullptr;
    m_hChildStd_ERR_Wr = nullptr;
    ZeroMemory(&m_pi, sizeof(m_pi));
#else
    m_pid = -1;
    m_stdin_pipe[0] = m_stdin_pipe[1] = -1;
    m_stdout_pipe[0] = m_stdout_pipe[1] = -1;
    m_stderr_pipe[0] = m_stderr_pipe[1] = -1;
#endif
}

CrossPlatformTerminal::~CrossPlatformTerminal() {
    stop();
}

bool CrossPlatformTerminal::createPipes() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &sa, 0)) {
        std::cerr << "StdoutRd CreatePipe failed" << std::endl;
        return false;
    }

    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "Stdout SetHandleInformation failed" << std::endl;
        return false;
    }

    if (!CreatePipe(&m_hChildStd_ERR_Rd, &m_hChildStd_ERR_Wr, &sa, 0)) {
        std::cerr << "StderrRd CreatePipe failed" << std::endl;
        return false;
    }

    if (!SetHandleInformation(m_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "Stderr SetHandleInformation failed" << std::endl;
        return false;
    }

    if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &sa, 0)) {
        std::cerr << "Stdin CreatePipe failed" << std::endl;
        return false;
    }

    if (!SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "Stdin SetHandleInformation failed" << std::endl;
        return false;
    }

    return true;
#else
    if (pipe(m_stdin_pipe) == -1 || pipe(m_stdout_pipe) == -1 || pipe(m_stderr_pipe) == -1) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }
    return true;
#endif
}

bool CrossPlatformTerminal::spawnProcess(const std::string& command) {
#ifdef _WIN32
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = m_hChildStd_ERR_Wr;
    siStartInfo.hStdOutput = m_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = m_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmdLine;
    if (m_currentShell == PWSH) {
        cmdLine = "pwsh.exe -NoExit";
    } else if (m_currentShell == CMD) {
        cmdLine = "cmd.exe";
    }

    if (!CreateProcessA(nullptr, (LPSTR)cmdLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &siStartInfo, &piProcInfo)) {
        std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
        return false;
    }

    m_pi = piProcInfo;
    CloseHandle(m_hChildStd_OUT_Wr);
    CloseHandle(m_hChildStd_ERR_Wr);
    CloseHandle(m_hChildStd_IN_Rd);
    m_running = true;
    return true;
#elif defined(__APPLE__)
    // macOS: Use posix_spawn for better process management
    extern char** environ;  // Environment variables
    posix_spawn_file_actions_t file_actions;
    posix_spawnattr_t attr;
    
    posix_spawn_file_actions_init(&file_actions);
    posix_spawnattr_init(&attr);
    
    // Setup file descriptor inheritance
    posix_spawn_file_actions_adddup2(&file_actions, m_stdin_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, m_stdout_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, m_stderr_pipe[1], STDERR_FILENO);
    
    // Close the write ends in parent
    posix_spawn_file_actions_addclose(&file_actions, m_stdin_pipe[1]);
    posix_spawn_file_actions_addclose(&file_actions, m_stdout_pipe[0]);
    posix_spawn_file_actions_addclose(&file_actions, m_stderr_pipe[0]);
    
    const char* shell = "/bin/zsh";
    char* argv[] = {(char*)shell, (char*)"-i", nullptr};
    
    if (posix_spawn(&m_pid, shell, &file_actions, &attr, argv, environ) != 0) {
        std::cerr << "posix_spawn failed" << std::endl;
        posix_spawn_file_actions_destroy(&file_actions);
        posix_spawnattr_destroy(&attr);
        return false;
    }
    
    posix_spawn_file_actions_destroy(&file_actions);
    posix_spawnattr_destroy(&attr);
    
    // Parent process: close child's file descriptors
    close(m_stdin_pipe[0]);
    close(m_stdout_pipe[1]);
    close(m_stderr_pipe[1]);
    
    // Set non-blocking I/O on pipes
    int flags;
    flags = fcntl(m_stdout_pipe[0], F_GETFL);
    fcntl(m_stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(m_stderr_pipe[0], F_GETFL);
    fcntl(m_stderr_pipe[0], F_SETFL, flags | O_NONBLOCK);
    
    m_running = true;
    return true;
#else
    // Linux: Use fork/exec with non-blocking I/O
    m_pid = fork();
    if (m_pid == 0) {
        // Child process
        close(m_stdin_pipe[1]);
        close(m_stdout_pipe[0]);
        close(m_stderr_pipe[0]);
        
        dup2(m_stdin_pipe[0], STDIN_FILENO);
        dup2(m_stdout_pipe[1], STDOUT_FILENO);
        dup2(m_stderr_pipe[1], STDERR_FILENO);
        
        const char* shell = "/bin/bash";
        char* argv[] = {(char*)shell, (char*)"-i", nullptr};
        
        execv(shell, argv);
        exit(1);
    } else if (m_pid > 0) {
        // Parent process
        close(m_stdin_pipe[0]);
        close(m_stdout_pipe[1]);
        close(m_stderr_pipe[1]);
        
        // Set non-blocking I/O on pipes
        int flags;
        flags = fcntl(m_stdout_pipe[0], F_GETFL);
        fcntl(m_stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(m_stderr_pipe[0], F_GETFL);
        fcntl(m_stderr_pipe[0], F_SETFL, flags | O_NONBLOCK);
        
        m_running = true;
        return true;
    }
    return false;
#endif
}

bool CrossPlatformTerminal::startShell(ShellType shellType) {
    if (m_running) {
        stop();
    }

    m_currentShell = shellType;

    if (!createPipes()) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    if (!spawnProcess("")) {
        std::cerr << "Failed to spawn shell process" << std::endl;
        return false;
    }

    std::cout << "[Terminal] Shell started: " << static_cast<int>(shellType) << std::endl;
    return true;
}

bool CrossPlatformTerminal::sendCommand(const std::string& command) {
    if (!m_running) {
        return false;
    }

#ifdef _WIN32
    DWORD dwWritten = 0;
    std::string cmdWithNewline = command + "\n";
    
    if (!WriteFile(m_hChildStd_IN_Wr, cmdWithNewline.c_str(), cmdWithNewline.length(), &dwWritten, nullptr)) {
        std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
        return false;
    }
    
    return dwWritten > 0;
#else
    ssize_t written = write(m_stdin_pipe[1], (command + "\n").c_str(), command.length() + 1);
    return written > 0;
#endif
}

std::string CrossPlatformTerminal::readOutput() {
#ifdef _WIN32
    DWORD dwRead = 0;
    char chBuf[4096];
    std::string output;

    while (ReadFile(m_hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, nullptr) && dwRead > 0) {
        output.append(chBuf, dwRead);
    }

    return output;
#else
    std::string output;
    char buffer[4096];
    ssize_t nread;
    
    while ((nread = read(m_stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, nread);
    }
    
    return output;
#endif
}

std::string CrossPlatformTerminal::readError() {
#ifdef _WIN32
    DWORD dwRead = 0;
    char chBuf[4096];
    std::string error;

    while (ReadFile(m_hChildStd_ERR_Rd, chBuf, sizeof(chBuf), &dwRead, nullptr) && dwRead > 0) {
        error.append(chBuf, dwRead);
    }

    return error;
#else
    std::string error;
    char buffer[4096];
    ssize_t nread;
    
    while ((nread = read(m_stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        error.append(buffer, nread);
    }
    
    return error;
#endif
}

bool CrossPlatformTerminal::isRunning() const {
    return m_running;
}

void CrossPlatformTerminal::stop() {
    if (!m_running) {
        return;
    }

#ifdef _WIN32
    if (m_pi.hProcess) {
        TerminateProcess(m_pi.hProcess, 0);
        WaitForSingleObject(m_pi.hProcess, INFINITE);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
    }

    if (m_hChildStd_IN_Wr) CloseHandle(m_hChildStd_IN_Wr);
    if (m_hChildStd_OUT_Rd) CloseHandle(m_hChildStd_OUT_Rd);
    if (m_hChildStd_ERR_Rd) CloseHandle(m_hChildStd_ERR_Rd);
#else
    if (m_pid > 0) {
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
    }

    if (m_stdin_pipe[1] >= 0) close(m_stdin_pipe[1]);
    if (m_stdout_pipe[0] >= 0) close(m_stdout_pipe[0]);
    if (m_stderr_pipe[0] >= 0) close(m_stderr_pipe[0]);
#endif

    m_running = false;
}

std::string CrossPlatformTerminal::getDefaultShell() {
#ifdef _WIN32
    return "pwsh";
#elif defined(__APPLE__)
    return "zsh";
#else
    return "bash";
#endif
}

std::vector<CrossPlatformTerminal::ShellType> CrossPlatformTerminal::getAvailableShells() {
    std::vector<ShellType> shells;

#ifdef _WIN32
    shells.push_back(PWSH);
    shells.push_back(CMD);
#else
    shells.push_back(BASH);
    shells.push_back(ZSH);
    shells.push_back(FISH);
#endif

    return shells;
}