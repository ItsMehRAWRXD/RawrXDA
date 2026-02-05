#include "Win32TerminalManager.h"
#include <iostream>
#include <vector>

Win32TerminalManager::Win32TerminalManager()
    : m_hProcess(nullptr), m_hThread(nullptr), m_processId(0),
      m_hStdInRead(nullptr), m_hStdInWrite(nullptr),
      m_hStdOutRead(nullptr), m_hStdOutWrite(nullptr),
      m_hStdErrRead(nullptr), m_hStdErrWrite(nullptr),
      m_running(false)
{
}

Win32TerminalManager::~Win32TerminalManager()
{
    stop();
}

bool Win32TerminalManager::start(ShellType shell)
{
    m_shellType = shell;

    // Create pipes for stdin, stdout, stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&m_hStdOutRead, &m_hStdOutWrite, &sa, 0) ||
        !CreatePipe(&m_hStdErrRead, &m_hStdErrWrite, &sa, 0) ||
        !CreatePipe(&m_hStdInRead, &m_hStdInWrite, &sa, 0)) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    // Ensure write handles are not inherited
    SetHandleInformation(m_hStdOutWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hStdErrWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hStdInRead, HANDLE_FLAG_INHERIT, 0);

    // Set up process startup info
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = m_hStdInRead;
    si.hStdOutput = m_hStdOutWrite;
    si.hStdError = m_hStdErrWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Choose shell
    std::string cmd;
    if (shell == PowerShell) {
        cmd = "powershell.exe -NoExit -Command -";
    } else {
        cmd = "cmd.exe";
    }

    // Create the process
    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr,
                       TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        std::cerr << "Failed to create process: " << GetLastError() << std::endl;
        return false;
    }

    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    m_processId = pi.dwProcessId;
    m_running = true;

    // Close unnecessary handles
    CloseHandle(m_hStdOutWrite);
    CloseHandle(m_hStdErrWrite);
    CloseHandle(m_hStdInRead);

    // Start threads to read output
    m_outputThread = std::thread(&Win32TerminalManager::readOutputThread, this);
    m_errorThread = std::thread(&Win32TerminalManager::readErrorThread, this);
    m_monitorThread = std::thread(&Win32TerminalManager::monitorProcessThread, this);

    if (onStarted) {
        onStarted();
    }

    return true;
}

void Win32TerminalManager::stop()
{
    if (m_running) {
        m_running = false;
        TerminateProcess(m_hProcess, 0);
        WaitForSingleObject(m_hProcess, INFINITE);

        if (m_outputThread.joinable()) m_outputThread.join();
        if (m_errorThread.joinable()) m_errorThread.join();
        if (m_monitorThread.joinable()) m_monitorThread.join();

        CloseHandle(m_hProcess);
        CloseHandle(m_hThread);
        CloseHandle(m_hStdInWrite);
        CloseHandle(m_hStdOutRead);
        CloseHandle(m_hStdErrRead);

        m_hProcess = nullptr;
        m_hThread = nullptr;
    }
}

DWORD Win32TerminalManager::pid() const
{
    return m_processId;
}

bool Win32TerminalManager::isRunning() const
{
    return m_running;
}

void Win32TerminalManager::writeInput(const std::string& data)
{
    if (!m_running || !m_hStdInWrite) return;

    DWORD written;
    WriteFile(m_hStdInWrite, data.c_str(), data.size(), &written, nullptr);
}

void Win32TerminalManager::readOutputThread()
{
    char buffer[4096];
    DWORD bytesRead;

    while (m_running) {
        if (ReadFile(m_hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (onOutput) {
                onOutput(std::string(buffer, bytesRead));
            }
        } else {
            break;
        }
    }
}

void Win32TerminalManager::readErrorThread()
{
    char buffer[4096];
    DWORD bytesRead;

    while (m_running) {
        if (ReadFile(m_hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (onError) {
                onError(std::string(buffer, bytesRead));
            }
        } else {
            break;
        }
    }
}

void Win32TerminalManager::monitorProcessThread()
{
    WaitForSingleObject(m_hProcess, INFINITE);
    m_running = false;

    DWORD exitCode;
    GetExitCodeProcess(m_hProcess, &exitCode);

    if (onFinished) {
        onFinished(exitCode);
    }
}
