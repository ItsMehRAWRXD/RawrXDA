/*
 * RawrXD_TerminalManager_Win32.cpp
 * Pure Win32 replacement for Qt QProcess/terminal widgets
 * Uses: CreateProcessW, pipes, console API
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef RAWRXD_WIN32_STATIC_BUILD
#define RAWRXD_SHIP_EXPORT __declspec(dllexport)
#else
#define RAWRXD_SHIP_EXPORT
#endif
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <memory>

struct ProcessInfo {
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdOut;
    HANDLE hStdErr;
    HANDLE hStdInWrite;
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
                CloseHandle(pair.second.hProcess);
            }
            if (pair.second.hThread) CloseHandle(pair.second.hThread);
            if (pair.second.hStdOut) CloseHandle(pair.second.hStdOut);
            if (pair.second.hStdErr) CloseHandle(pair.second.hStdErr);
            if (pair.second.hStdInWrite) CloseHandle(pair.second.hStdInWrite);
        }
        LeaveCriticalSection(&m_criticalSection);
        DeleteCriticalSection(&m_criticalSection);
    }
    
    DWORD ExecuteCommand(const wchar_t* command, const wchar_t* workingDir = nullptr) {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
        HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
        HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
        
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) return 0;
        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
            CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
            return 0;
        }
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOW si = {0};
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStdoutWrite;
        si.hStdInput = hStdinRead;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {0};
        wchar_t cmdLine[32768];
        wcscpy_s(cmdLine, sizeof(cmdLine) / sizeof(wchar_t), L"cmd.exe /c ");
        wcscat_s(cmdLine, sizeof(cmdLine) / sizeof(wchar_t), command);
        
        if (!CreateProcessW(nullptr, cmdLine, nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, workingDir, &si, &pi)) {
            CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
            CloseHandle(hStdinRead); CloseHandle(hStdinWrite);
            return 0;
        }
        
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        
        ProcessInfo pinfo;
        pinfo.hProcess = pi.hProcess;
        pinfo.hThread = pi.hThread;
        pinfo.hStdOut = hStdoutRead;
        pinfo.hStdErr = nullptr;
        pinfo.hStdInWrite = hStdinWrite;
        pinfo.dwProcessId = pi.dwProcessId;
        pinfo.running = true;
        
        EnterCriticalSection(&m_criticalSection);
        m_processes[pi.dwProcessId] = pinfo;
        LeaveCriticalSection(&m_criticalSection);
        
        return pi.dwProcessId;
    }
    
    bool ReadOutput(DWORD processId, char* buffer, size_t bufSize, size_t& bytesRead) {
        EnterCriticalSection(&m_criticalSection);
        
        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        HANDLE hPipe = it->second.hStdOut;
        LeaveCriticalSection(&m_criticalSection);
        
        DWORD dwRead;
        if (!ReadFile(hPipe, buffer, (DWORD)bufSize, &dwRead, nullptr)) {
            return false;
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
        
        LeaveCriticalSection(&m_criticalSection);
        return running;
    }
    
    DWORD GetExitCode(DWORD processId) {
        EnterCriticalSection(&m_criticalSection);
        
        auto it = m_processes.find(processId);
        if (it == m_processes.end()) {
            LeaveCriticalSection(&m_criticalSection);
            return -1;
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
        return m ? m->GetExitCode(processId) : -1;
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
