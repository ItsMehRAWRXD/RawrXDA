/*
 * RawrXD_TerminalManager_Win32.cpp
 * Pure Win32 replacement for Qt QProcess/terminal widgets (integrated into Win32 IDE)
 * Uses: CreateProcessW, pipes, console API
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <memory>

#ifndef RAWRXD_WIN32_STATIC_BUILD
#define RAWRXD_SHIP_EXPORT __declspec(dllexport)
#else
#define RAWRXD_SHIP_EXPORT
#endif

struct ProcessInfo {
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdOut;
    HANDLE hStdErr;
    DWORD dwProcessId;
    bool running;
};

class RawrXDTerminalManager {
private:
    std::map<DWORD, ProcessInfo> m_processes;
    mutable CRITICAL_SECTION m_criticalSection;

public:
    RawrXDTerminalManager() {
        InitializeCriticalSection(&m_criticalSection);
    }

    ~RawrXDTerminalManager() {
        EnterCriticalSection(&m_criticalSection);
        for (auto& pair : m_processes) {
            if (pair.second.hProcess) {
                ::TerminateProcess(pair.second.hProcess, 1);
                WaitForSingleObject(pair.second.hProcess, 500);  // Wait up to 500ms for termination
                CloseHandle(pair.second.hProcess);
            }
            if (pair.second.hThread) CloseHandle(pair.second.hThread);
            if (pair.second.hStdOut) CloseHandle(pair.second.hStdOut);
            if (pair.second.hStdErr) CloseHandle(pair.second.hStdErr);
        }
        m_processes.clear();
        LeaveCriticalSection(&m_criticalSection);
        DeleteCriticalSection(&m_criticalSection);
    }

    DWORD ExecuteCommand(const wchar_t* command, const wchar_t* workingDir = nullptr) {
        // Create pipes for stdout/stderr capture
        HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
        HANDLE hStdErrRead = nullptr, hStdErrWrite = nullptr;
        
        SECURITY_ATTRIBUTES sa = {0};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
            !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
            if (hStdOutRead) CloseHandle(hStdOutRead);
            if (hStdOutWrite) CloseHandle(hStdOutWrite);
            if (hStdErrRead) CloseHandle(hStdErrRead);
            if (hStdErrWrite) CloseHandle(hStdErrWrite);
            return 0;
        }

        // Make read ends non-inheritable
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = {0};
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdErrWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = {0};

        wchar_t cmdLine[32768];
        wcscpy_s(cmdLine, 32768, L"cmd.exe /c ");
        if (command) wcscat_s(cmdLine, 32768, command);

        // Cast to non-const for CreateProcessW compatibility
        wchar_t* mutableCmdLine = cmdLine;

        if (!CreateProcessW(nullptr, mutableCmdLine, nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, workingDir, &si, &pi)) {
            CloseHandle(hStdOutRead);
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdErrRead);
            CloseHandle(hStdErrWrite);
            return 0;
        }

        // Close write ends in parent process
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);

        ProcessInfo pinfo;
        pinfo.hProcess = pi.hProcess;
        pinfo.hThread = pi.hThread;
        pinfo.hStdOut = hStdOutRead;
        pinfo.hStdErr = hStdErrRead;
        pinfo.dwProcessId = pi.dwProcessId;
        pinfo.running = true;

        EnterCriticalSection(&m_criticalSection);
        m_processes[pi.dwProcessId] = pinfo;
        LeaveCriticalSection(&m_criticalSection);

        return pi.dwProcessId;
    }

    bool ReadOutput(DWORD processId, char* buffer, size_t bufSize, size_t& bytesRead) {
        if (!buffer || bufSize == 0) {
            bytesRead = 0;
            return false;
        }

        EnterCriticalSection(&m_criticalSection);

        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            bytesRead = 0;
            return false;
        }

        HANDLE hPipe = it->second.hStdOut;
        LeaveCriticalSection(&m_criticalSection);

        if (!hPipe) {
            bytesRead = 0;
            return false;
        }

        DWORD dwRead = 0;
        // Use non-blocking read with PeekNamedPipe first
        DWORD dwAvailable = 0;
        if (PeekNamedPipe(hPipe, nullptr, 0, nullptr, &dwAvailable, nullptr) && dwAvailable > 0) {
            DWORD toRead = (dwAvailable < (DWORD)bufSize) ? dwAvailable : (DWORD)bufSize;
            if (!ReadFile(hPipe, buffer, toRead, &dwRead, nullptr)) {
                bytesRead = 0;
                return false;
            }
        }

        bytesRead = dwRead;
        return true;
    }

    bool IsProcessRunning(DWORD processId) {
        EnterCriticalSection(&m_criticalSection);

        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }

        DWORD dwExitCode;
        bool running = GetExitCodeProcess(it->second.hProcess, &dwExitCode) &&
            (dwExitCode == STILL_ACTIVE);

        // If process has finished, clean up its resources
        if (!running) {
            if (it->second.hProcess) CloseHandle(it->second.hProcess);
            if (it->second.hThread) CloseHandle(it->second.hThread);
            if (it->second.hStdOut) CloseHandle(it->second.hStdOut);
            if (it->second.hStdErr) CloseHandle(it->second.hStdErr);
            m_processes.erase(it);
        }

        LeaveCriticalSection(&m_criticalSection);
        return running;
    }

    DWORD GetExitCode(DWORD processId) {
        EnterCriticalSection(&m_criticalSection);

        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return (DWORD)-1;
        }

        DWORD dwExitCode;
        GetExitCodeProcess(it->second.hProcess, &dwExitCode);

        LeaveCriticalSection(&m_criticalSection);
        return dwExitCode;
    }

    bool TerminateProcess(DWORD processId) {
        EnterCriticalSection(&m_criticalSection);

        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }

        bool result = ::TerminateProcess(it->second.hProcess, 1);
        WaitForSingleObject(it->second.hProcess, 500);

        // Properly close all handles
        if (it->second.hProcess) CloseHandle(it->second.hProcess);
        if (it->second.hThread) CloseHandle(it->second.hThread);
        if (it->second.hStdOut) CloseHandle(it->second.hStdOut);
        if (it->second.hStdErr) CloseHandle(it->second.hStdErr);
        
        m_processes.erase(it);

        LeaveCriticalSection(&m_criticalSection);
        return result;
    }

    void KillAllProcesses() {
        EnterCriticalSection(&m_criticalSection);

        for (auto& pair : m_processes) {
            if (pair.second.hProcess) {
                ::TerminateProcess(pair.second.hProcess, 1);
            }
        }

        LeaveCriticalSection(&m_criticalSection);
    }
};

static RawrXDTerminalManager* g_terminalManager = nullptr;

extern "C" {
    RAWRXD_SHIP_EXPORT void* __stdcall CreateTerminalManager() {
        if (!g_terminalManager) {
            g_terminalManager = new RawrXDTerminalManager();
        }
        return g_terminalManager;
    }

    RAWRXD_SHIP_EXPORT void __stdcall DestroyTerminalManager(void* mgr) {
        if (mgr && mgr == g_terminalManager) {
            g_terminalManager->KillAllProcesses();
            delete g_terminalManager;
            g_terminalManager = nullptr;
        }
    }

    RAWRXD_SHIP_EXPORT DWORD __stdcall Terminal_ExecuteCommand(void* mgr, const wchar_t* command, const wchar_t* workingDir) {
        RawrXDTerminalManager* m = static_cast<RawrXDTerminalManager*>(mgr);
        return m ? m->ExecuteCommand(command, workingDir) : 0;
    }

    RAWRXD_SHIP_EXPORT bool __stdcall Terminal_ReadOutput(void* mgr, DWORD processId, char* buffer, size_t bufSize, size_t* bytesRead) {
        RawrXDTerminalManager* m = static_cast<RawrXDTerminalManager*>(mgr);
        size_t read = 0;
        bool result = m ? m->ReadOutput(processId, buffer, bufSize, read) : false;
        if (bytesRead) *bytesRead = read;
        return result;
    }

    RAWRXD_SHIP_EXPORT bool __stdcall Terminal_IsProcessRunning(void* mgr, DWORD processId) {
        RawrXDTerminalManager* m = static_cast<RawrXDTerminalManager*>(mgr);
        return m ? m->IsProcessRunning(processId) : false;
    }

    RAWRXD_SHIP_EXPORT DWORD __stdcall Terminal_GetExitCode(void* mgr, DWORD processId) {
        RawrXDTerminalManager* m = static_cast<RawrXDTerminalManager*>(mgr);
        return m ? m->GetExitCode(processId) : (DWORD)-1;
    }

    RAWRXD_SHIP_EXPORT bool __stdcall Terminal_TerminateProcess(void* mgr, DWORD processId) {
        RawrXDTerminalManager* m = static_cast<RawrXDTerminalManager*>(mgr);
        return m ? m->TerminateProcess(processId) : false;
    }
}

#ifndef RAWRXD_WIN32_STATIC_BUILD
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_TerminalManager_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_terminalManager) {
        g_terminalManager->KillAllProcesses();
        delete g_terminalManager;
        g_terminalManager = nullptr;
    }
    return TRUE;
}
#endif
