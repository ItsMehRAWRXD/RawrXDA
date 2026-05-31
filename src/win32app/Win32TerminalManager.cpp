#include "Win32TerminalManager.h"
#include <iostream>
#include <string>
#include <vector>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

#ifndef HPCON
typedef HANDLE HPCON;
#endif

namespace
{
using CreatePseudoConsoleFn = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
using ClosePseudoConsoleFn = VOID(WINAPI*)(HPCON);
}

Win32TerminalManager::Win32TerminalManager()
    : m_hProcess(nullptr), m_hThread(nullptr), m_processId(0), m_hStdInRead(nullptr), m_hStdInWrite(nullptr),
    m_hStdOutRead(nullptr), m_hStdOutWrite(nullptr), m_hStdErrRead(nullptr), m_hStdErrWrite(nullptr),
    m_hPseudoConsole(nullptr), m_hPtyInputWrite(nullptr), m_hPtyOutputRead(nullptr), m_running(false), m_destroying(false),
    m_outputRingBytes(0)
{
}

Win32TerminalManager::~Win32TerminalManager()
{
    stop();
}

void Win32TerminalManager::resetStaleStateFromExitedProcess()
{
    if (!m_hProcess && !m_outputThread.joinable() && !m_errorThread.joinable() && !m_monitorThread.joinable())
        return;

    m_running = false;

    if (m_outputThread.joinable())
    {
        if (m_outputThread.get_id() == std::this_thread::get_id())
            m_outputThread.detach();
        else
            m_outputThread.join();
    }
    if (m_errorThread.joinable())
    {
        if (m_errorThread.get_id() == std::this_thread::get_id())
            m_errorThread.detach();
        else
            m_errorThread.join();
    }
    if (m_monitorThread.joinable())
    {
        if (m_monitorThread.get_id() == std::this_thread::get_id())
            m_monitorThread.detach();
        else
            m_monitorThread.join();
    }

    if (m_hProcess)
    {
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    if (m_hThread)
    {
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    auto closeIf = [](HANDLE& h)
    {
        if (h)
        {
            CloseHandle(h);
            h = nullptr;
        }
    };

    HANDLE hStdInWrite = m_hStdInWrite;
    HANDLE hStdOutRead = m_hStdOutRead;
    HANDLE hStdErrRead = m_hStdErrRead;
    HANDLE hPtyInputWrite = m_hPtyInputWrite;
    HANDLE hPtyOutputRead = m_hPtyOutputRead;

    m_hStdInWrite = nullptr;
    m_hStdOutRead = nullptr;
    m_hStdErrRead = nullptr;
    m_hPtyInputWrite = nullptr;
    m_hPtyOutputRead = nullptr;

    closeIf(hStdInWrite);
    if (hPtyInputWrite && hPtyInputWrite != hStdInWrite)
        closeIf(hPtyInputWrite);
    closeIf(hStdOutRead);
    if (hPtyOutputRead && hPtyOutputRead != hStdOutRead)
        closeIf(hPtyOutputRead);
    closeIf(hStdErrRead);
    if (m_hPseudoConsole)
    {
        HMODULE kernel = GetModuleHandleA("kernel32.dll");
        if (kernel)
        {
            auto closePseudoConsole = reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel, "ClosePseudoConsole"));
            if (closePseudoConsole)
                closePseudoConsole(reinterpret_cast<HPCON>(m_hPseudoConsole));
        }
        m_hPseudoConsole = nullptr;
    }
}

bool Win32TerminalManager::startWithConPTY(const std::string& cmd, const char* workingDirectoryUtf8)
{
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    if (!kernel)
        return false;

    auto createPseudoConsole = reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(kernel, "CreatePseudoConsole"));
    auto closePseudoConsole = reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel, "ClosePseudoConsole"));
    if (!createPseudoConsole || !closePseudoConsole)
        return false;

    HANDLE hPtyInputRead = nullptr;
    HANDLE hPtyInputWrite = nullptr;
    HANDLE hPtyOutputRead = nullptr;
    HANDLE hPtyOutputWrite = nullptr;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hPtyInputRead, &hPtyInputWrite, &sa, 0) || !CreatePipe(&hPtyOutputRead, &hPtyOutputWrite, &sa, 0))
    {
        if (hPtyInputRead)
            CloseHandle(hPtyInputRead);
        if (hPtyInputWrite)
            CloseHandle(hPtyInputWrite);
        if (hPtyOutputRead)
            CloseHandle(hPtyOutputRead);
        if (hPtyOutputWrite)
            CloseHandle(hPtyOutputWrite);
        return false;
    }

    SetHandleInformation(hPtyInputWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPtyOutputRead, HANDLE_FLAG_INHERIT, 0);

    HPCON hPc = nullptr;
    const HRESULT hr = createPseudoConsole({120, 3000}, hPtyInputRead, hPtyOutputWrite, 0, &hPc);
    CloseHandle(hPtyInputRead);
    CloseHandle(hPtyOutputWrite);
    if (FAILED(hr) || !hPc)
    {
        CloseHandle(hPtyInputWrite);
        CloseHandle(hPtyOutputRead);
        return false;
    }

    SIZE_T attrSize = 0;
    STARTUPINFOEXA siEx;
    ZeroMemory(&siEx, sizeof(siEx));
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attrSize));
    if (!siEx.lpAttributeList)
    {
        closePseudoConsole(hPc);
        CloseHandle(hPtyInputWrite);
        CloseHandle(hPtyOutputRead);
        return false;
    }

    bool attrOk = false;
    if (InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrSize) &&
        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPc, sizeof(HPCON),
                                  nullptr, nullptr))
    {
        attrOk = true;
    }

    if (!attrOk)
    {
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        closePseudoConsole(hPc);
        CloseHandle(hPtyInputWrite);
        CloseHandle(hPtyOutputRead);
        return false;
    }

    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXA);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    const char* lpCurrentDirectory = nullptr;
    char resolvedDir[1024];
    resolvedDir[0] = '\0';
    if (workingDirectoryUtf8 && workingDirectoryUtf8[0])
    {
        const DWORD n =
            GetFullPathNameA(workingDirectoryUtf8, static_cast<DWORD>(sizeof(resolvedDir)), resolvedDir, nullptr);
        if (n > 0 && n < sizeof(resolvedDir))
        {
            const DWORD attr = GetFileAttributesA(resolvedDir);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                lpCurrentDirectory = resolvedDir;
        }
    }

    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

    const BOOL created = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                                        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, lpCurrentDirectory,
                                        &siEx.StartupInfo, &pi);

    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

    if (!created)
    {
        closePseudoConsole(hPc);
        CloseHandle(hPtyInputWrite);
        CloseHandle(hPtyOutputRead);
        return false;
    }

    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    m_processId = pi.dwProcessId;
    m_hPseudoConsole = reinterpret_cast<HANDLE>(hPc);
    m_hPtyInputWrite = hPtyInputWrite;
    m_hPtyOutputRead = hPtyOutputRead;
    m_hStdInWrite = m_hPtyInputWrite;
    m_hStdOutRead = m_hPtyOutputRead;
    m_hStdErrRead = nullptr;
    m_running = true;

    m_outputThread = std::thread(&Win32TerminalManager::readOutputThread, this);
    m_monitorThread = std::thread(&Win32TerminalManager::monitorProcessThread, this);

    return true;
}

bool Win32TerminalManager::startWithPipes(const std::string& cmd, const char* workingDirectoryUtf8)
{
    // Create pipes for stdin, stdout, stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&m_hStdOutRead, &m_hStdOutWrite, &sa, 0) || !CreatePipe(&m_hStdErrRead, &m_hStdErrWrite, &sa, 0) ||
        !CreatePipe(&m_hStdInRead, &m_hStdInWrite, &sa, 0))
    {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    // Ensure the PARENT's pipe ends are NOT inherited by the child process.
    // The child inherits: m_hStdInRead (its stdin), m_hStdOutWrite (its stdout), m_hStdErrWrite (its stderr)
    // The parent keeps: m_hStdInWrite (write to child), m_hStdOutRead (read child out), m_hStdErrRead (read child err)
    SetHandleInformation(m_hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hStdErrRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hStdInWrite, HANDLE_FLAG_INHERIT, 0);

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

    const char* lpCurrentDirectory = nullptr;
    char resolvedDir[1024];
    resolvedDir[0] = '\0';
    if (workingDirectoryUtf8 && workingDirectoryUtf8[0])
    {
        const DWORD n =
            GetFullPathNameA(workingDirectoryUtf8, static_cast<DWORD>(sizeof(resolvedDir)), resolvedDir, nullptr);
        if (n > 0 && n < sizeof(resolvedDir))
        {
            const DWORD attr = GetFileAttributesA(resolvedDir);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                lpCurrentDirectory = resolvedDir;
        }
    }

    // Writable command line buffer — CreateProcess may modify the string
    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

    // Create the process
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, lpCurrentDirectory,
                        &si, &pi))
    {
        std::cerr << "Failed to create process: " << GetLastError() << std::endl;
        if (m_hStdOutRead)
            CloseHandle(m_hStdOutRead);
        if (m_hStdOutWrite)
            CloseHandle(m_hStdOutWrite);
        if (m_hStdErrRead)
            CloseHandle(m_hStdErrRead);
        if (m_hStdErrWrite)
            CloseHandle(m_hStdErrWrite);
        if (m_hStdInRead)
            CloseHandle(m_hStdInRead);
        if (m_hStdInWrite)
            CloseHandle(m_hStdInWrite);
        m_hStdOutRead = nullptr;
        m_hStdOutWrite = nullptr;
        m_hStdErrRead = nullptr;
        m_hStdErrWrite = nullptr;
        m_hStdInRead = nullptr;
        m_hStdInWrite = nullptr;
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
    m_hStdOutWrite = nullptr;
    m_hStdErrWrite = nullptr;
    m_hStdInRead = nullptr;

    // Start threads to read output
    m_outputThread = std::thread(&Win32TerminalManager::readOutputThread, this);
    m_errorThread = std::thread(&Win32TerminalManager::readErrorThread, this);
    m_monitorThread = std::thread(&Win32TerminalManager::monitorProcessThread, this);

    return true;
}

bool Win32TerminalManager::start(ShellType shell, const char* workingDirectoryUtf8)
{
    if (m_running)
        return false;
    m_destroying.store(false, std::memory_order_release);
    resetStaleStateFromExitedProcess();

    m_shellType = shell;

    // Choose shell — try pwsh (PS7) first, fall back to powershell.exe (PS5)
    std::string cmd;
    if (shell == PowerShell)
    {
        // -NoExit keeps session alive, -NoLogo suppresses banner
        // -ExecutionPolicy Bypass prevents script restrictions
        // Try pwsh.exe first (PowerShell 7), fall back to powershell.exe
        char pwshPath[MAX_PATH];
        // Prompt hook: emit RAWRXD_CWD|<path>|END on each prompt so the IDE status bar tracks `cd` (stripped before
        // display).
        const char* psTail = " -Command \"function global:prompt { $p=(Get-Location).ProviderPath; "
                             "[Console]::Out.WriteLine(('RAWRXD_CWD|' + $p + '|END')); return ('PS ' + $p + '> ') }\"";
        if (SearchPathA(nullptr, "pwsh.exe", nullptr, MAX_PATH, pwshPath, nullptr))
        {
            cmd = std::string("pwsh.exe -NoExit -NoLogo -NoProfile") + psTail;
        }
        else
        {
            cmd = std::string("powershell.exe -NoExit -NoLogo -NoProfile") + psTail;
        }
    }
    else
    {
        // CMD: strip RAWRXD_CWD|path|END in the IDE (same wire format as PowerShell). Use ^| so echo is not parsed as
        // a shell pipe. doskey macros ($T = "then") emit a line after cd/pushd/popd so the status bar tracks cwd.
        cmd = "cmd.exe /K \"doskey cd=cd $* $T echo RAWRXD_CWD^|%CD%^|END & "
              "doskey pushd=pushd $* $T echo RAWRXD_CWD^|%CD%^|END & "
              "doskey popd=popd $T echo RAWRXD_CWD^|%CD%^|END & "
              "echo RAWRXD_CWD^|%CD%^|END\"";
    }

    bool started = startWithConPTY(cmd, workingDirectoryUtf8);
    if (!started)
        started = startWithPipes(cmd, workingDirectoryUtf8);

    if (started && onStarted)
        onStarted();
    return started;
}

void Win32TerminalManager::stop()
{
    m_destroying.store(true, std::memory_order_release);

    if (m_running)
    {
        m_running = false;

        TerminateProcess(m_hProcess, 0);
        WaitForSingleObject(m_hProcess, 5000);  // 5s max instead of INFINITE

        if (m_outputThread.joinable())
        {
            if (m_outputThread.get_id() == std::this_thread::get_id())
                m_outputThread.detach();
            else
                m_outputThread.join();
        }
        if (m_errorThread.joinable())
        {
            if (m_errorThread.get_id() == std::this_thread::get_id())
                m_errorThread.detach();
            else
                m_errorThread.join();
        }
        if (m_monitorThread.joinable())
        {
            if (m_monitorThread.get_id() == std::this_thread::get_id())
                m_monitorThread.detach();
            else
                m_monitorThread.join();
        }

        HANDLE hStdInWrite = m_hStdInWrite;
        HANDLE hStdOutRead = m_hStdOutRead;
        HANDLE hStdErrRead = m_hStdErrRead;
        HANDLE hPtyInputWrite = m_hPtyInputWrite;
        HANDLE hPtyOutputRead = m_hPtyOutputRead;

        if (m_hProcess)
            CloseHandle(m_hProcess);
        if (m_hThread)
            CloseHandle(m_hThread);
        if (hStdInWrite)
            CloseHandle(hStdInWrite);
        if (hPtyInputWrite && hPtyInputWrite != hStdInWrite)
            CloseHandle(hPtyInputWrite);
        if (hStdOutRead)
            CloseHandle(hStdOutRead);
        if (hPtyOutputRead && hPtyOutputRead != hStdOutRead)
            CloseHandle(hPtyOutputRead);
        if (hStdErrRead)
            CloseHandle(hStdErrRead);
        if (m_hPseudoConsole)
        {
            HMODULE kernel = GetModuleHandleA("kernel32.dll");
            if (kernel)
            {
                auto closePseudoConsole =
                    reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel, "ClosePseudoConsole"));
                if (closePseudoConsole)
                    closePseudoConsole(reinterpret_cast<HPCON>(m_hPseudoConsole));
            }
            m_hPseudoConsole = nullptr;
        }

        m_hProcess = nullptr;
        m_hThread = nullptr;
        m_hStdInWrite = nullptr;
        m_hStdOutRead = nullptr;
        m_hStdErrRead = nullptr;
        m_hPtyInputWrite = nullptr;
        m_hPtyOutputRead = nullptr;
    }
    else
    {
        // Process exited on its own; monitor cleared m_running but handles were never released.
        resetStaleStateFromExitedProcess();
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
    HANDLE targetInput = m_hPtyInputWrite ? m_hPtyInputWrite : m_hStdInWrite;
    if (!m_running || !targetInput)
        return;

    std::lock_guard<std::mutex> lock(m_stdinWriteMutex);
    DWORD written = 0;
    WriteFile(targetInput, data.c_str(), static_cast<DWORD>(data.size()), &written, nullptr);
}

void Win32TerminalManager::appendToOutputRing(const char* data, size_t size)
{
    if (!data || size == 0)
        return;

    std::lock_guard<std::mutex> lock(m_outputRingMutex);
    m_outputRing.emplace_back(data, size);
    m_outputRingBytes += size;

    while (m_outputRingBytes > kMaxOutputRingBytes && !m_outputRing.empty())
    {
        std::string& head = m_outputRing.front();
        const size_t overflow = m_outputRingBytes - kMaxOutputRingBytes;
        if (overflow >= head.size())
        {
            m_outputRingBytes -= head.size();
            m_outputRing.pop_front();
            continue;
        }

        head.erase(0, overflow);
        m_outputRingBytes -= overflow;
        break;
    }
}

void Win32TerminalManager::readOutputThread()
{
    char buffer[4096];
    DWORD bytesRead = 0;
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

    while (m_running)
    {
        if (m_destroying.load(std::memory_order_acquire))
            break;

        if (ReadFile(m_hStdOutRead, buffer, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0)
        {
            const size_t safeBytes =
                (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
            buffer[safeBytes] = '\0';
            appendToOutputRing(buffer, safeBytes);
            if (!m_destroying.load(std::memory_order_acquire) && onOutput)
            {
                onOutput(std::string(buffer, safeBytes));
            }
        }
        else
        {
            break;
        }
    }
}

void Win32TerminalManager::readErrorThread()
{
    char buffer[4096];
    DWORD bytesRead = 0;
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

    while (m_running)
    {
        if (m_destroying.load(std::memory_order_acquire))
            break;

        if (ReadFile(m_hStdErrRead, buffer, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0)
        {
            const size_t safeBytes =
                (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
            buffer[safeBytes] = '\0';
            appendToOutputRing(buffer, safeBytes);
            if (!m_destroying.load(std::memory_order_acquire) && onError)
            {
                onError(std::string(buffer, safeBytes));
            }
        }
        else
        {
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

    if (!m_destroying.load(std::memory_order_acquire) && onFinished)
    {
        onFinished(exitCode);
    }
}
