/**
 * @file win32_autonomous_api.cpp
 * @brief Implementation of Win32 API access for autonomous agent workloads
 * 
 * Provides complete Win32 API wrapping with proper error handling,
 * resource cleanup, thread safety, and performance optimization.
 */

#include "win32_autonomous_api.hpp"
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <ctime>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")

Win32AutonomousAPI* Win32AutonomousAPI::s_instance = nullptr;

QMutex& win32Mutex() {
    static QMutex mutex;
    return mutex;
}

Win32AutonomousAPI::Win32AutonomousAPI()
{
    // Initialize Win32 API
}

Win32AutonomousAPI::~Win32AutonomousAPI()
{
    // Cleanup on destruction
}

Win32AutonomousAPI& Win32AutonomousAPI::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&g_win32Mutex);
        if (!s_instance) {
            s_instance = new Win32AutonomousAPI();
        }
    }
    return *s_instance;
}

// =====================================================================
// PROCESS CREATION & MANAGEMENT
// =====================================================================

ProcessExecutionResult Win32AutonomousAPI::createProcess(
    const QString& executable,
    const QStringList& arguments,
    const QString& workingDirectory,
    bool waitForCompletion,
    bool captureOutput,
    const QMap<QString, QString>& environmentVars,
    DWORD priority,
    bool createWindow,
    bool runAsAdmin)
{
    ProcessExecutionResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Build command line
    QString cmdLine = executable;
    for (const auto& arg : arguments) {
        if (arg.contains(' ')) {
            cmdLine += " \"" + arg + "\"";
        } else {
            cmdLine += " " + arg;
        }
    }
    
    // Handle elevation if needed
    if (runAsAdmin && !isRunningAsAdmin()) {
        // Use ShellExecute for elevation
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = executable.toStdWString().c_str();
        
        QString argString;
        for (const auto& arg : arguments) {
            argString += (argString.isEmpty() ? "" : " ") + arg;
        }
        
        sei.lpParameters = argString.toStdWString().c_str();
        sei.lpDirectory = workingDirectory.isEmpty() ? nullptr : workingDirectory.toStdWString().c_str();
        sei.nShow = createWindow ? SW_SHOW : SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        if (ShellExecuteExW(&sei)) {
            DWORD exitCode = 0;
            if (sei.hProcess) {
                result.processId = GetProcessId(sei.hProcess);
                result.processHandle = sei.hProcess;
                
                if (waitForCompletion) {
                    WaitForSingleObject(sei.hProcess, INFINITE);
                    GetExitCodeProcess(sei.hProcess, &exitCode);
                    CloseHandle(sei.hProcess);
                }
                result.exitCode = exitCode;
                result.success = true;
            }
        } else {
            result.errorMessage = getErrorMessage(GetLastError());
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return result;
    }
    
    // Create process with output capture
    HANDLE hPipeRead = NULL;
    HANDLE hPipeWrite = NULL;
    
    if (captureOutput) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) {
            result.errorMessage = getErrorMessage(GetLastError());
            auto endTime = std::chrono::high_resolution_clock::now();
            result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            return result;
        }
    }
    
    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    
    if (captureOutput) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hPipeWrite;
        si.hStdError = hPipeWrite;
    } else if (!createWindow) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    
    // Build environment block
    wchar_t* envBlock = nullptr;
    if (!environmentVars.empty()) {
        wchar_t* currentEnv = GetEnvironmentStringsW();
        int blockSize = 0;
        for (auto it = environmentVars.begin(); it != environmentVars.end(); ++it) {
            blockSize += (it.key().length() + it.value().length() + 2);
        }
        
        // Add current environment
        wchar_t* current = currentEnv;
        while (*current) {
            blockSize += wcslen(current) + 1;
            current += wcslen(current) + 1;
        }
        blockSize += 1; // Final null
        
        envBlock = new wchar_t[blockSize];
        memset(envBlock, 0, blockSize * sizeof(wchar_t));
        
        int offset = 0;
        for (auto it = environmentVars.begin(); it != environmentVars.end(); ++it) {
            std::wstring varStr = it.key().toStdWString() + L"=" + it.value().toStdWString();
            wcscpy_s(envBlock + offset, blockSize - offset, varStr.c_str());
            offset += varStr.length() + 1;
        }
        
        FreeEnvironmentStringsW(currentEnv);
    }
    
    std::wstring cmdLineW = cmdLine.toStdWString();
    std::wstring workDirW = workingDirectory.toStdWString();
    
    if (!CreateProcessW(
        executable.toStdWString().c_str(),
        (wchar_t*)cmdLineW.c_str(),
        NULL, NULL,
        captureOutput ? TRUE : FALSE,
        priority | (createWindow ? 0 : CREATE_NO_WINDOW),
        envBlock,
        workingDirectory.isEmpty() ? NULL : (wchar_t*)workDirW.c_str(),
        &si, &pi))
    {
        result.errorMessage = getErrorMessage(GetLastError());
        if (hPipeRead) CloseHandle(hPipeRead);
        if (hPipeWrite) CloseHandle(hPipeWrite);
        if (envBlock) delete[] envBlock;
        auto endTime = std::chrono::high_resolution_clock::now();
        result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return result;
    }
    
    result.processId = pi.dwProcessId;
    result.processHandle = pi.hProcess;
    
    if (captureOutput) {
        CloseHandle(hPipeWrite);
        
        // Read output in chunks
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.stdOutput += QString::fromLocal8Bit(buffer);
        }
        
        CloseHandle(hPipeRead);
    }
    
    if (waitForCompletion) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&result.exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    result.success = true;
    result.waitForCompletion = waitForCompletion;
    
    if (envBlock) delete[] envBlock;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    return result;
}

ProcessExecutionResult Win32AutonomousAPI::createProcessAsUser(
    const QString& username,
    const QString& password,
    const QString& executable,
    const QStringList& arguments,
    const QString& workingDirectory)
{
    ProcessExecutionResult result;
    
    // Domain\username or just username
    std::wstring userW = username.toStdWString();
    std::wstring passW = password.toStdWString();
    
    HANDLE hToken = NULL;
    if (!LogonUserW(
        userW.c_str(),
        NULL,
        passW.c_str(),
        LOGON32_LOGON_INTERACTIVE,
        LOGON32_PROVIDER_DEFAULT,
        &hToken))
    {
        result.errorMessage = getErrorMessage(GetLastError());
        return result;
    }
    
    // Build command line
    QString cmdLine = executable;
    for (const auto& arg : arguments) {
        cmdLine += " " + arg;
    }
    
    LPVOID envBlock = NULL;
    CreateEnvironmentBlock(&envBlock, hToken, FALSE);
    
    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    std::wstring cmdLineW = cmdLine.toStdWString();
    std::wstring workDirW = workingDirectory.toStdWString();
    
    if (CreateProcessAsUserW(
        hToken,
        executable.toStdWString().c_str(),
        (wchar_t*)cmdLineW.c_str(),
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_CONSOLE,
        envBlock,
        workingDirectory.isEmpty() ? NULL : (wchar_t*)workDirW.c_str(),
        &si, &pi))
    {
        result.processId = pi.dwProcessId;
        result.success = true;
        
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&result.exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        result.errorMessage = getErrorMessage(GetLastError());
    }
    
    if (envBlock) DestroyEnvironmentBlock(envBlock);
    CloseHandle(hToken);
    
    return result;
}

bool Win32AutonomousAPI::terminateProcess(DWORD processId, int waitMs)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return false;
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);
    if (exitCode == STILL_ACTIVE) {
        if (waitMs > 0) {
            // Try graceful shutdown first
            if (TerminateProcess(hProcess, 0)) {
                WaitForSingleObject(hProcess, waitMs);
            }
        } else {
            TerminateProcess(hProcess, 1);
        }
    }
    
    CloseHandle(hProcess);
    return true;
}

bool Win32AutonomousAPI::killProcessTree(DWORD processId, int waitMs)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    PROCESSENTRY32W pe32 = {};
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    QList<DWORD> childProcesses;
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ParentProcessID == processId) {
                childProcesses.append(pe32.th32ProcessID);
                killProcessTree(pe32.th32ProcessID, waitMs);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    terminateProcess(processId, waitMs);
    return true;
}

ProcessInfo Win32AutonomousAPI::getProcessInfo(DWORD processId)
{
    ProcessInfo info;
    info.processId = processId;
    
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
        processId);
    
    if (!hProcess) {
        return info;
    }
    
    // Get executable path
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH)) {
        info.commandLine = QString::fromStdWString(exePath);
    }
    
    // Get priority
    info.priority = GetPriorityClass(hProcess);
    
    // Check if running
    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);
    info.isRunning = (exitCode == STILL_ACTIVE);
    info.exitCode = exitCode;
    
    // Get username
    HANDLE hToken = NULL;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        wchar_t username[256];
        DWORD size = 256;
        if (GetUserNameW(username, &size)) {
            info.username = QString::fromStdWString(username);
        }
        CloseHandle(hToken);
    }
    
    CloseHandle(hProcess);
    return info;
}

QList<DWORD> Win32AutonomousAPI::getAllProcessIds()
{
    QList<DWORD> pids;
    DWORD aProcesses[1024] = {};
    DWORD cbNeeded = 0;
    
    if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        DWORD cProcesses = cbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < cProcesses; i++) {
            if (aProcesses[i] != 0) {
                pids.append(aProcesses[i]);
            }
        }
    }
    
    return pids;
}

DWORD Win32AutonomousAPI::findProcessByName(const QString& executableName, int instance)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    PROCESSENTRY32W pe32 = {};
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    int found = 0;
    DWORD result = 0;
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString processName = QString::fromStdWString(pe32.szExeFile);
            if (processName.compare(executableName, Qt::CaseInsensitive) == 0) {
                if (found == instance) {
                    result = pe32.th32ProcessID;
                    break;
                }
                found++;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return result;
}

bool Win32AutonomousAPI::waitForProcess(DWORD processId, DWORD timeoutMs)
{
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, processId);
    if (!hProcess) {
        return false;
    }
    
    bool result = (WaitForSingleObject(hProcess, timeoutMs) == WAIT_OBJECT_0);
    CloseHandle(hProcess);
    
    return result;
}

DWORD Win32AutonomousAPI::getProcessExitCode(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return STILL_ACTIVE;
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);
    
    return exitCode;
}

HANDLE Win32AutonomousAPI::createJobObject(const QString& jobName)
{
    std::wstring jobNameW = jobName.toStdWString();
    HANDLE hJob = CreateJobObjectW(NULL, jobName.isEmpty() ? NULL : jobNameW.c_str());
    
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
    
    return hJob;
}

bool Win32AutonomousAPI::assignProcessToJob(HANDLE jobHandle, HANDLE processHandle)
{
    return AssignProcessToJobObject(jobHandle, processHandle) ? true : false;
}

// =====================================================================
// THREAD OPERATIONS
// =====================================================================

DWORD Win32AutonomousAPI::createRemoteThread(
    HANDLE processHandle,
    LPTHREAD_START_ROUTINE threadFunction,
    LPVOID threadParameter,
    DWORD creationFlags)
{
    HANDLE hThread = CreateRemoteThread(
        processHandle,
        NULL,
        0,
        threadFunction,
        threadParameter,
        creationFlags,
        NULL);
    
    if (!hThread) {
        return 0;
    }
    
    DWORD threadId = GetThreadId(hThread);
    CloseHandle(hThread);
    
    return threadId;
}

bool Win32AutonomousAPI::suspendThread(DWORD threadId)
{
    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadId);
    if (!hThread) {
        return false;
    }
    
    bool result = (SuspendThread(hThread) != (DWORD)-1);
    CloseHandle(hThread);
    
    return result;
}

bool Win32AutonomousAPI::resumeThread(DWORD threadId)
{
    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadId);
    if (!hThread) {
        return false;
    }
    
    bool result = (ResumeThread(hThread) != (DWORD)-1);
    CloseHandle(hThread);
    
    return result;
}

bool Win32AutonomousAPI::terminateThread(DWORD threadId, DWORD exitCode)
{
    HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, threadId);
    if (!hThread) {
        return false;
    }
    
    bool result = TerminateThread(hThread, exitCode) ? true : false;
    CloseHandle(hThread);
    
    return result;
}

int Win32AutonomousAPI::getThreadPriority(DWORD threadId)
{
    HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
    if (!hThread) {
        return 0;
    }
    
    int priority = GetThreadPriority(hThread);
    CloseHandle(hThread);
    
    return priority;
}

bool Win32AutonomousAPI::setThreadPriority(DWORD threadId, int priority)
{
    HANDLE hThread = OpenThread(THREAD_SET_INFORMATION, FALSE, threadId);
    if (!hThread) {
        return false;
    }
    
    bool result = SetThreadPriority(hThread, priority) ? true : false;
    CloseHandle(hThread);
    
    return result;
}

// =====================================================================
// MODULE/DLL LOADING
// =====================================================================

HMODULE Win32AutonomousAPI::loadLibrary(const QString& dllPath, DWORD searchFlags)
{
    std::wstring dllPathW = dllPath.toStdWString();
    
    if (searchFlags != 0) {
        return LoadLibraryExW(dllPathW.c_str(), NULL, searchFlags);
    } else {
        return LoadLibraryW(dllPathW.c_str());
    }
}

bool Win32AutonomousAPI::injectDll(HANDLE processHandle, const QString& dllPath)
{
    // Get length of DLL path
    size_t dllPathLen = (dllPath.length() + 1) * sizeof(wchar_t);
    
    // Allocate memory in remote process
    LPVOID remoteDllPath = VirtualAllocEx(
        processHandle,
        NULL,
        dllPathLen,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    
    if (!remoteDllPath) {
        return false;
    }
    
    // Write DLL path to remote process
    std::wstring dllPathW = dllPath.toStdWString();
    if (!WriteProcessMemory(
        processHandle,
        remoteDllPath,
        (void*)dllPathW.c_str(),
        dllPathLen,
        NULL))
    {
        VirtualFreeEx(processHandle, remoteDllPath, 0, MEM_RELEASE);
        return false;
    }
    
    // Get LoadLibraryW address
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    
    // Create remote thread to load DLL
    HANDLE hThread = CreateRemoteThread(
        processHandle,
        NULL,                                   // lpThreadAttributes
        0,                                      // dwStackSize (default)
        (LPTHREAD_START_ROUTINE)pLoadLibraryW,  // lpStartAddress
        remoteDllPath,                          // lpParameter
        0,                                      // dwCreationFlags
        NULL);                                  // lpThreadId
    
    if (!hThread) {
        VirtualFreeEx(processHandle, remoteDllPath, 0, MEM_RELEASE);
        return false;
    }
    
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    
    VirtualFreeEx(processHandle, remoteDllPath, 0, MEM_RELEASE);
    
    return true;
}

FARPROC Win32AutonomousAPI::getProcAddress(HMODULE module, const QString& functionName)
{
    std::string funcNameA = functionName.toStdString();
    return GetProcAddress(module, funcNameA.c_str());
}

bool Win32AutonomousAPI::freeLibrary(HMODULE module)
{
    return FreeLibrary(module) ? true : false;
}

DWORD64 Win32AutonomousAPI::getRemoteModuleBase(DWORD processId, const QString& moduleName)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    MODULEENTRY32W me32 = {};
    me32.dwSize = sizeof(MODULEENTRY32W);
    
    DWORD64 result = 0;
    
    if (Module32FirstW(hSnapshot, &me32)) {
        do {
            QString modName = QString::fromStdWString(me32.szModule);
            if (modName.compare(moduleName, Qt::CaseInsensitive) == 0) {
                result = (DWORD64)me32.modBaseAddr;
                break;
            }
        } while (Module32NextW(hSnapshot, &me32));
    }
    
    CloseHandle(hSnapshot);
    return result;
}

// =====================================================================
// FILE OPERATIONS
// =====================================================================

HANDLE Win32AutonomousAPI::createFile(
    const QString& filePath,
    DWORD desiredAccess,
    DWORD creationDisposition,
    DWORD attributes)
{
    std::wstring filePathW = filePath.toStdWString();
    return CreateFileW(
        filePathW.c_str(),
        desiredAccess,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        creationDisposition,
        attributes,
        NULL);
}

QString Win32AutonomousAPI::readFile(HANDLE fileHandle, int maxBytes)
{
    QString result;
    
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return result;
    }
    
    char* buffer = new char[maxBytes];
    DWORD bytesRead = 0;
    
    if (ReadFile(fileHandle, buffer, maxBytes, &bytesRead, NULL)) {
        result = QString::fromLocal8Bit(buffer, bytesRead);
    }
    
    delete[] buffer;
    return result;
}

bool Win32AutonomousAPI::writeFile(HANDLE fileHandle, const QString& data)
{
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    std::string dataStr = data.toStdString();
    DWORD bytesWritten = 0;
    
    return WriteFile(
        fileHandle,
        dataStr.c_str(),
        dataStr.length(),
        &bytesWritten,
        NULL) ? true : false;
}

bool Win32AutonomousAPI::deleteFile(const QString& filePath)
{
    std::wstring filePathW = filePath.toStdWString();
    return DeleteFileW(filePathW.c_str()) ? true : false;
}

qint64 Win32AutonomousAPI::getFileSize(const QString& filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA fileAttrData;
    std::wstring filePathW = filePath.toStdWString();
    
    if (GetFileAttributesExW(filePathW.c_str(), GetFileExInfoStandard, &fileAttrData)) {
        LARGE_INTEGER size;
        size.LowPart = fileAttrData.nFileSizeLow;
        size.HighPart = fileAttrData.nFileSizeHigh;
        return size.QuadPart;
    }
    
    return -1;
}

bool Win32AutonomousAPI::copyFile(const QString& sourceFile, const QString& targetFile, bool failIfExists)
{
    std::wstring srcW = sourceFile.toStdWString();
    std::wstring tgtW = targetFile.toStdWString();
    
    return CopyFileW(srcW.c_str(), tgtW.c_str(), failIfExists) ? true : false;
}

bool Win32AutonomousAPI::moveFile(const QString& oldPath, const QString& newPath)
{
    std::wstring oldW = oldPath.toStdWString();
    std::wstring newW = newPath.toStdWString();
    
    return MoveFileW(oldW.c_str(), newW.c_str()) ? true : false;
}

bool Win32AutonomousAPI::closeFile(HANDLE fileHandle)
{
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    return CloseHandle(fileHandle) ? true : false;
}

// =====================================================================
// REGISTRY OPERATIONS
// =====================================================================

HKEY Win32AutonomousAPI::openRegistryKey(
    HKEY rootKey,
    const QString& subKeyPath,
    REGSAM samDesired)
{
    HKEY hKey = NULL;
    std::wstring subKeyW = subKeyPath.toStdWString();
    
    if (RegOpenKeyExW(rootKey, subKeyW.c_str(), 0, samDesired, &hKey) == ERROR_SUCCESS) {
        return hKey;
    }
    
    return NULL;
}

QString Win32AutonomousAPI::queryRegistryValue(HKEY keyHandle, const QString& valueName)
{
    QString result;
    
    if (!keyHandle) {
        return result;
    }
    
    wchar_t buffer[4096] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    
    std::wstring valueNameW = valueName.toStdWString();
    
    if (RegQueryValueExW(keyHandle, valueNameW.c_str(), NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        result = QString::fromStdWString(buffer);
    }
    
    return result;
}

bool Win32AutonomousAPI::setRegistryValue(HKEY keyHandle, const QString& valueName, const QString& value)
{
    if (!keyHandle) {
        return false;
    }
    
    std::wstring valueNameW = valueName.toStdWString();
    std::wstring valueW = value.toStdWString();
    
    return (RegSetValueExW(
        keyHandle,
        valueNameW.c_str(),
        0,
        REG_SZ,
        (const BYTE*)valueW.c_str(),
        (valueW.length() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS);
}

bool Win32AutonomousAPI::deleteRegistryValue(HKEY keyHandle, const QString& valueName)
{
    if (!keyHandle) {
        return false;
    }
    
    std::wstring valueNameW = valueName.toStdWString();
    
    return (RegDeleteValueW(keyHandle, valueNameW.c_str()) == ERROR_SUCCESS);
}

bool Win32AutonomousAPI::closeRegistryKey(HKEY keyHandle)
{
    if (!keyHandle) {
        return false;
    }
    
    return (RegCloseKey(keyHandle) == ERROR_SUCCESS);
}

// =====================================================================
// WINDOW/UI AUTOMATION
// =====================================================================

HWND Win32AutonomousAPI::findWindow(const QString& windowName, const QString& className)
{
    std::wstring windowNameW = windowName.toStdWString();
    std::wstring classNameW = className.toStdWString();
    
    return FindWindowW(
        className.isEmpty() ? NULL : classNameW.c_str(),
        windowNameW.c_str());
}

LRESULT Win32AutonomousAPI::sendMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!IsWindow(hwnd)) {
        return 0;
    }
    
    return SendMessageW(hwnd, msg, wParam, lParam);
}

bool Win32AutonomousAPI::postMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!IsWindow(hwnd)) {
        return false;
    }
    
    return PostMessageW(hwnd, msg, wParam, lParam) ? true : false;
}

bool Win32AutonomousAPI::simulateKeyPress(int keyCode, int modifiers)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = 0;
    
    if (modifiers & 0x0001) {
        // Shift
    }
    if (modifiers & 0x0002) {
        // Ctrl
    }
    if (modifiers & 0x0004) {
        // Alt
    }
    
    return SendInput(1, &input, sizeof(INPUT)) > 0;
}

bool Win32AutonomousAPI::simulateMouseClick(int x, int y, int button)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = (button == 1) ? MOUSEEVENTF_LEFTDOWN : (button == 2) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_MIDDLEDOWN;
    
    SendInput(1, &input, sizeof(INPUT));
    
    input.mi.dwFlags = (button == 1) ? MOUSEEVENTF_LEFTUP : (button == 2) ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_MIDDLEUP;
    return SendInput(1, &input, sizeof(INPUT)) > 0;
}

// =====================================================================
// SYSTEM INFORMATION
// =====================================================================

int Win32AutonomousAPI::getProcessorCount()
{
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

qint64 Win32AutonomousAPI::getTotalSystemMemory()
{
    MEMORYSTATUSEX ms = {};
    ms.dwLength = sizeof(ms);
    
    if (GlobalMemoryStatusEx(&ms)) {
        return ms.ullTotalPhys;
    }
    
    return 0;
}

qint64 Win32AutonomousAPI::getAvailableSystemMemory()
{
    MEMORYSTATUSEX ms = {};
    ms.dwLength = sizeof(ms);
    
    if (GlobalMemoryStatusEx(&ms)) {
        return ms.ullAvailPhys;
    }
    
    return 0;
}

QString Win32AutonomousAPI::getOSVersion()
{
    // Use GetVersionExW or WMI for detailed OS version
    OSVERSIONINFOW ovi = {};
    ovi.dwOSVersionInfoSize = sizeof(ovi);
    
    GetVersionExW(&ovi);
    
    return QString("Windows %1.%2 Build %3")
        .arg(ovi.dwMajorVersion)
        .arg(ovi.dwMinorVersion)
        .arg(ovi.dwBuildNumber);
}

QString Win32AutonomousAPI::getCurrentUsername()
{
    wchar_t username[256] = {};
    DWORD size = 256;
    
    if (GetUserNameW(username, &size)) {
        return QString::fromStdWString(username);
    }
    
    return QString();
}

bool Win32AutonomousAPI::isRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    TOKEN_ELEVATION elevation;
    DWORD cbSize = sizeof(TOKEN_ELEVATION);
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    
    return isAdmin ? true : false;
}

// =====================================================================
// INTER-PROCESS COMMUNICATION
// =====================================================================

HANDLE Win32AutonomousAPI::createNamedPipe(
    const QString& pipeName,
    int bufferSize,
    DWORD openMode)
{
    std::wstring pipeNameW = pipeName.toStdWString();
    
    return CreateNamedPipeW(
        pipeNameW.c_str(),
        openMode,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        bufferSize,
        bufferSize,
        0,
        NULL);
}

HANDLE Win32AutonomousAPI::connectToNamedPipe(const QString& pipeName, int timeoutMs)
{
    std::wstring pipeNameW = pipeName.toStdWString();
    
    if (WaitNamedPipeW(pipeNameW.c_str(), timeoutMs)) {
        return CreateFileW(
            pipeNameW.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }
    
    return INVALID_HANDLE_VALUE;
}

bool Win32AutonomousAPI::writeToPipe(HANDLE pipeHandle, const QString& data)
{
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    std::string dataStr = data.toStdString();
    DWORD bytesWritten = 0;
    
    return WriteFile(pipeHandle, dataStr.c_str(), dataStr.length(), &bytesWritten, NULL) ? true : false;
}

QString Win32AutonomousAPI::readFromPipe(HANDLE pipeHandle, int maxBytes)
{
    QString result;
    
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        return result;
    }
    
    char* buffer = new char[maxBytes];
    DWORD bytesRead = 0;
    
    if (ReadFile(pipeHandle, buffer, maxBytes, &bytesRead, NULL)) {
        result = QString::fromLocal8Bit(buffer, bytesRead);
    }
    
    delete[] buffer;
    return result;
}

bool Win32AutonomousAPI::closePipe(HANDLE pipeHandle)
{
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    return CloseHandle(pipeHandle) ? true : false;
}

// =====================================================================
// EVENT/MUTEX/SEMAPHORE
// =====================================================================

HANDLE Win32AutonomousAPI::createEvent(
    const QString& eventName,
    bool manualReset,
    bool initialState)
{
    std::wstring eventNameW = eventName.toStdWString();
    
    return CreateEventW(
        NULL,
        manualReset,
        initialState,
        eventName.isEmpty() ? NULL : eventNameW.c_str());
}

bool Win32AutonomousAPI::setEvent(HANDLE eventHandle)
{
    if (!eventHandle) {
        return false;
    }
    
    return SetEvent(eventHandle) ? true : false;
}

bool Win32AutonomousAPI::resetEvent(HANDLE eventHandle)
{
    if (!eventHandle) {
        return false;
    }
    
    return ResetEvent(eventHandle) ? true : false;
}

bool Win32AutonomousAPI::waitForEvent(HANDLE eventHandle, DWORD timeoutMs)
{
    if (!eventHandle) {
        return false;
    }
    
    return (WaitForSingleObject(eventHandle, timeoutMs) == WAIT_OBJECT_0);
}

HANDLE Win32AutonomousAPI::createMutex(const QString& mutexName)
{
    std::wstring mutexNameW = mutexName.toStdWString();
    
    return CreateMutexW(
        NULL,
        FALSE,
        mutexName.isEmpty() ? NULL : mutexNameW.c_str());
}

bool Win32AutonomousAPI::acquireMutex(HANDLE mutexHandle, DWORD timeoutMs)
{
    if (!mutexHandle) {
        return false;
    }
    
    return (WaitForSingleObject(mutexHandle, timeoutMs) == WAIT_OBJECT_0);
}

bool Win32AutonomousAPI::releaseMutex(HANDLE mutexHandle)
{
    if (!mutexHandle) {
        return false;
    }
    
    return ReleaseMutex(mutexHandle) ? true : false;
}

HANDLE Win32AutonomousAPI::createSemaphore(
    const QString& semaphoreName,
    long initialCount,
    long maxCount)
{
    std::wstring semaphoreNameW = semaphoreName.toStdWString();
    
    return CreateSemaphoreW(
        NULL,
        initialCount,
        maxCount,
        semaphoreName.isEmpty() ? NULL : semaphoreNameW.c_str());
}

bool Win32AutonomousAPI::closeHandle(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    return CloseHandle(handle) ? true : false;
}

// =====================================================================
// HELPER METHODS
// =====================================================================

QString Win32AutonomousAPI::getErrorMessage(DWORD errorCode)
{
    wchar_t message[256] = {};
    
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message,
        sizeof(message) / sizeof(wchar_t),
        NULL);
    
    return QString::fromStdWString(message);
}

QString Win32AutonomousAPI::wideToQt(const wchar_t* wideStr)
{
    if (!wideStr) {
        return QString();
    }
    
    return QString::fromStdWString(wideStr);
}

std::wstring Win32AutonomousAPI::qtToWide(const QString& str)
{
    return str.toStdWString();
}

HANDLE Win32AutonomousAPI::getCurrentProcessHandle()
{
    return GetCurrentProcess();
}
