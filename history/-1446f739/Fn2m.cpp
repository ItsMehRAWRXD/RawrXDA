// Win32NativeAgentAPI.cpp - Native Win32 API access layer implementation
#include "Win32NativeAgentAPI.h"
#include <Psapi.h>
#include <UserEnv.h>
#include <iphlpapi.h>
#include <Wlanapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <QDebug>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Wlanapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace RawrXD {
namespace Win32Agent {

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

static Win32AgentAPI* g_win32AgentAPI = nullptr;

Win32AgentAPI& GetWin32AgentAPI() {
    if (!g_win32AgentAPI) {
        g_win32AgentAPI = new Win32AgentAPI();
    }
    return *g_win32AgentAPI;
}

// ============================================================================
// PROCESS MANAGER IMPLEMENTATION
// ============================================================================

ProcessManager::ProcessManager() {
    // Initialize process enumeration
    m_processHandles.reserve(256);
}

ProcessManager::~ProcessManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [pid, handle] : m_processHandles) {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
    m_processHandles.clear();
}

ProcessCreateResult ProcessManager::CreateProcessEx(const ProcessCreateParams& params) {
    ProcessCreateResult result;
    result.success = false;
    result.exitCode = 0;
    
    // Build command line
    std::wstring commandLine = L"\"" + params.executablePath + L"\"";
    if (!params.arguments.empty()) {
        commandLine += L" " + params.arguments;
    }
    
    // Build environment block
    std::wstring envBlock;
    if (!params.environment.empty()) {
        for (const auto& [name, value] : params.environment) {
            envBlock += name + L"=" + value + L"\0";
        }
        envBlock += L"\0";
    }
    
    // Set up process creation structure
    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
    si.StartupInfo.wShowWindow = params.hiddenWindow ? SW_HIDE : SW_NORMAL;
    
    PROCESS_INFORMATION pi = {};
    
    DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
    if (params.createNewConsole) creationFlags |= CREATE_NEW_CONSOLE;
    if (params.suspended) creationFlags |= CREATE_SUSPENDED;
    if (params.inheritHandles) creationFlags |= INHERIT_PARENT_AFFINITY;
    
    // Create process
    BOOL success = CreateProcessW(
        params.executablePath.c_str(),
        &commandLine[0],
        nullptr,  // Process security attributes
        nullptr,  // Thread security attributes
        params.inheritHandles ? TRUE : FALSE,
        creationFlags,
        params.environment.empty() ? nullptr : const_cast<LPWSTR>(envBlock.c_str()),
        params.workingDirectory.empty() ? nullptr : params.workingDirectory.c_str(),
        &si.StartupInfo,
        &pi
    );
    
    if (success) {
        result.success = true;
        result.processId = pi.dwProcessId;
        result.threadId = pi.dwThreadId;
        result.hProcess = pi.hProcess;
        result.hThread = pi.hThread;
        
        // Store handle for cleanup
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processHandles[pi.dwProcessId] = pi.hProcess;
        
        // Set process priority
        if (params.priority != NORMAL_PRIORITY_CLASS) {
            SetPriorityClass(pi.hProcess, params.priority);
        }
        
        qDebug() << "[ProcessManager] Process created successfully - PID:" << pi.dwProcessId;
    } else {
        DWORD error = GetLastError();
        result.errorMessage = L"Failed to create process. Error code: " + std::to_wstring(error);
        qCritical() << "[ProcessManager] Failed to create process - Error:" << error;
    }
    
    return result;
}

ProcessCreateResult ProcessManager::ExecuteCommand(const std::wstring& command, bool waitForCompletion, DWORD timeoutMs) {
    ProcessCreateParams params;
    params.executablePath = L"cmd.exe";
    params.arguments = L"/c \"" + command + L"\"";
    params.hiddenWindow = true;
    
    ProcessCreateResult result = CreateProcessEx(params);
    
    if (result.success && waitForCompletion) {
        DWORD waitResult = WaitForSingleObject(result.hProcess, timeoutMs);
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(result.processId, 1);
            result.success = false;
            result.errorMessage = L"Command timed out";
        } else {
            GetExitCodeProcess(result.hProcess, &result.exitCode);
        }
        
        // Clean up handles
        CloseHandle(result.hProcess);
        CloseHandle(result.hThread);
        CleanupHandle(result.processId);
    }
    
    return result;
}

ProcessCreateResult ProcessManager::ExecuteCommandWithPipes(const std::wstring& command, 
                                                         std::wstring& stdOut, 
                                                         std::wstring& stdErr, 
                                                         DWORD timeoutMs) {
    ProcessCreateResult result;
    result.success = false;
    
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    // Create pipes
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    HANDLE hStdinRead, hStdinWrite;
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0) ||
        !CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        result.errorMessage = L"Failed to create pipes";
        return result;
    }
    
    // Ensure read handles are not inherited
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    // Create process
    ProcessCreateParams params;
    params.executablePath = L"cmd.exe";
    params.arguments = L"/c \"" + command + L"\"";
    params.inheritHandles = true;
    params.hiddenWindow = true;
    
    // Override creation to use custom pipes
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    std::wstring commandLine = L"cmd.exe /c \"" + command + L"\"";
    BOOL success = CreateProcessW(
        nullptr,
        const_cast<LPWSTR>(commandLine.c_str()),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    
    if (success) {
        result.success = true;
        result.processId = pi.dwProcessId;
        result.hProcess = pi.hProcess;
        result.hThread = pi.hThread;
        
        // Close our copy of pipe handles
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);
        CloseHandle(hStdinRead);
        
        // Read output
        ReadPipeOutput(hStdoutRead, stdOut, timeoutMs);
        ReadPipeOutput(hStderrRead, stdErr, timeoutMs);
        
        // Wait for process to complete
        WaitForSingleObject(result.hProcess, timeoutMs);
        GetExitCodeProcess(result.hProcess, &result.exitCode);
        
        // Clean up
        CloseHandle(result.hProcess);
        CloseHandle(result.hThread);
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        CloseHandle(hStdinWrite);
        
        CleanupHandle(result.processId);
    } else {
        result.errorMessage = L"Failed to create process. Error code: " + std::to_wstring(GetLastError());
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
    }
    
    return result;
}

void ProcessManager::ReadPipeOutput(HANDLE pipe, std::wstring& output, DWORD timeoutMs) {
    output.clear();
    if (!pipe || pipe == INVALID_HANDLE_VALUE) {
        return;
    }

    std::string collected;
    const DWORD pollIntervalMs = 25;
    const ULONGLONG startTick = GetTickCount64();

    while (true) {
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            break;
        }

        if (bytesAvailable == 0) {
            if (timeoutMs != INFINITE) {
                const ULONGLONG elapsed = GetTickCount64() - startTick;
                if (elapsed >= timeoutMs) {
                    break;
                }
            }
            Sleep(pollIntervalMs);
            continue;
        }

        DWORD bytesRead = 0;
        char buffer[512];
        const DWORD toRead = std::min<DWORD>(static_cast<DWORD>(sizeof(buffer)), bytesAvailable);
        if (!::ReadFile(pipe, buffer, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }

        collected.append(buffer, bytesRead);
    }

    if (!collected.empty()) {
        const int wideLength = MultiByteToWideChar(CP_UTF8, 0, collected.data(), static_cast<int>(collected.size()), nullptr, 0);
        if (wideLength > 0) {
            output.resize(wideLength);
            MultiByteToWideChar(CP_UTF8, 0, collected.data(), static_cast<int>(collected.size()), output.data(), wideLength);
            return;
        }
        output.assign(collected.begin(), collected.end());
    }
}

std::vector<ProcessInfo> ProcessManager::EnumerateProcesses() {
    std::vector<ProcessInfo> processes;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        qCritical() << "[ProcessManager] Failed to create process snapshot";
        return processes;
    }
    
    PROCESSENTRY32W pe32 = {};
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            ProcessInfo info = GetProcessInfo(pe32.th32ProcessID);
            if (info.processId > 0) {
                processes.push_back(info);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return processes;
}

ProcessInfo ProcessManager::GetProcessInfo(DWORD processId) {
    ProcessInfo info;
    info.processId = processId;
    info.parentProcessId = 0;
    info.workingSetSize = 0;
    info.privateBytes = 0;
    info.threadCount = 0;
    info.handleCount = 0;
    info.cpuUsage = 0.0;
    info.isElevated = false;
    
    // Get process handle
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return info;
    }
    
    // Get process name and path
    wchar_t processName[MAX_PATH] = L"";
    wchar_t processPath[MAX_PATH] = L"";
    DWORD size = MAX_PATH;
    
    if (QueryFullProcessImageNameW(hProcess, 0, processPath, &size)) {
        info.executablePath = processPath;
        
        // Extract name from path
        size_t pos = info.executablePath.find_last_of(L"\\/");
        if (pos != std::string::npos) {
            info.processName = info.executablePath.substr(pos + 1);
        } else {
            info.processName = info.executablePath;
        }
    }
    
    // Get memory usage
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        info.workingSetSize = pmc.WorkingSetSize;
        info.privateBytes = pmc.PrivateUsage;
    }
    
    // Get thread and handle counts
    if (GetProcessHandleCount(hProcess, &info.handleCount)) {
        // Handle count retrieved
    }
    
    // Check if elevated
    HANDLE hToken = nullptr;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation = {};
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            info.isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    
    CloseHandle(hProcess);
    return info;
}

bool ProcessManager::TerminateProcess(DWORD processId, DWORD exitCode) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess) {
        return false;
    }
    
    BOOL result = ::TerminateProcess(hProcess, exitCode);
    CloseHandle(hProcess);
    
    if (result) {
        CleanupHandle(processId);
    }
    
    return result == TRUE;
}

SIZE_T ProcessManager::GetProcessMemoryUsage(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return 0;
    }
    
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    SIZE_T result = 0;
    
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        result = pmc.WorkingSetSize;
    }
    
    CloseHandle(hProcess);
    return result;
}

void ProcessManager::CleanupHandle(DWORD processId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processHandles.find(processId);
    if (it != m_processHandles.end()) {
        if (it->second && it->second != INVALID_HANDLE_VALUE) {
            CloseHandle(it->second);
        }
        m_processHandles.erase(it);
    }
}

// ============================================================================
// THREAD MANAGER IMPLEMENTATION
// ============================================================================

ThreadManager::ThreadManager() {
    m_threads.reserve(64);
    m_syncObjects.reserve(32);
}

ThreadManager::~ThreadManager() {
    // Terminate all created threads
    for (HANDLE hThread : m_threads) {
        if (hThread) {
            CloseHandle(hThread);
        }
    }
    
    // Close all synchronization objects
    for (HANDLE hSync : m_syncObjects) {
        if (hSync) {
            CloseHandle(hSync);
        }
    }
}

HANDLE ThreadManager::CreateThread(ThreadCallback callback, void* param, bool suspended) {
    ThreadContext* context = new ThreadContext{callback, param};
    
    HANDLE hThread = ::CreateThread(
        nullptr,  // Security attributes
        0,        // Stack size
        ThreadProc,
        context,  // Parameter
        suspended ? CREATE_SUSPENDED : 0,
        nullptr   // Thread ID
    );
    
    if (hThread) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads.push_back(hThread);
    }
    
    return hThread;
}

DWORD WINAPI ThreadManager::ThreadProc(LPVOID param) {
    ThreadContext* context = static_cast<ThreadContext*>(param);
    DWORD result = 0;
    
    try {
        if (context && context->callback) {
            result = context->callback(context->param);
        }
    } catch (...) {
        qCritical() << "[ThreadManager] Exception in thread callback";
        result = 1;
    }
    
    delete context;
    return result;
}

// ============================================================================
// MEMORY MANAGER IMPLEMENTATION
// ============================================================================

MemoryManager::MemoryManager() {
    m_virtualAllocations.reserve(256);
    m_heaps.reserve(16);
    m_mappings.reserve(32);
    m_mappedViews.reserve(32);
}

MemoryManager::~MemoryManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clean up mapped views
    for (void* addr : m_mappedViews) {
        UnmapViewOfFile(addr);
    }
    
    // Close mapping handles
    for (HANDLE hMapping : m_mappings) {
        CloseHandle(hMapping);
    }
    
    // Destroy heaps
    for (HANDLE hHeap : m_heaps) {
        DestroyHeap(hHeap);
    }
    
    // Free virtual allocations
    for (void* addr : m_virtualAllocations) {
        VirtualFree(addr, 0, MEM_RELEASE);
    }
}

void* MemoryManager::VirtualAlloc(SIZE_T size, DWORD allocationType, DWORD protection) {
    void* addr = ::VirtualAlloc(nullptr, size, allocationType, protection);
    if (addr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_virtualAllocations.push_back(addr);
    }
    return addr;
}

bool MemoryManager::VirtualFree(void* address, SIZE_T size, DWORD freeType) {
    bool result = ::VirtualFree(address, size, freeType);
    if (result && freeType == MEM_RELEASE) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find(m_virtualAllocations.begin(), m_virtualAllocations.end(), address);
        if (it != m_virtualAllocations.end()) {
            m_virtualAllocations.erase(it);
        }
    }
    return result;
}

MemoryInfo MemoryManager::GetSystemMemoryInfo() {
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    
    MemoryInfo info;
    info.totalPhysical = status.ullTotalPhys;
    info.availablePhysical = status.ullAvailPhys;
    info.totalVirtual = status.ullTotalVirtual;
    info.availableVirtual = status.ullAvailVirtual;
    info.totalPageFile = status.ullTotalPageFile;
    info.availablePageFile = status.ullAvailPageFile;
    info.memoryLoad = status.dwMemoryLoad;
    
    return info;
}

// ============================================================================
// FILE SYSTEM MANAGER IMPLEMENTATION
// ============================================================================

FileSystemManager::FileSystemManager() {
    m_fileHandles.reserve(128);
    m_watchHandles.reserve(16);
}

FileSystemManager::~FileSystemManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (HANDLE hFile : m_fileHandles) {
        if (hFile && hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }
    
    for (HANDLE hWatch : m_watchHandles) {
        if (hWatch) {
            FindCloseChangeNotification(hWatch);
        }
    }
}

HANDLE FileSystemManager::CreateFile(const std::wstring& path, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition) {
    HANDLE hFile = ::CreateFileW(
        path.c_str(),
        desiredAccess,
        shareMode,
        nullptr,
        creationDisposition,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile && hFile != INVALID_HANDLE_VALUE) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileHandles.push_back(hFile);
    }
    
    return hFile;
}

bool FileSystemManager::ReadFile(HANDLE hFile, void* buffer, DWORD bytesToRead, DWORD* bytesRead) {
    return ::ReadFile(hFile, buffer, bytesToRead, bytesRead, nullptr) == TRUE;
}

bool FileSystemManager::WriteFile(HANDLE hFile, const void* buffer, DWORD bytesToWrite, DWORD* bytesWritten) {
    return ::WriteFile(hFile, buffer, bytesToWrite, bytesWritten, nullptr) == TRUE;
}

std::vector<FileInfo> FileSystemManager::EnumerateDirectory(const std::wstring& path, const std::wstring& pattern) {
    std::vector<FileInfo> files;
    
    std::wstring searchPath = path + L"\\" + pattern;
    WIN32_FIND_DATAW findData;
    
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return files;
    }
    
    do {
        if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
            FileInfo info;
            info.name = findData.cFileName;
            info.fullPath = path + L"\\" + findData.cFileName;
            info.attributes = findData.dwFileAttributes;
            info.size = (ULONGLONG)findData.nFileSizeHigh << 32 | findData.nFileSizeLow;
            info.creationTime = findData.ftCreationTime;
            info.lastAccessTime = findData.ftLastAccessTime;
            info.lastWriteTime = findData.ftLastWriteTime;
            info.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            info.isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
            info.isReadOnly = (findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
            info.isSystem = (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
            
            files.push_back(info);
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return files;
}

// ============================================================================
// WIN32 AGENT API IMPLEMENTATION
// ============================================================================

Win32AgentAPI::Win32AgentAPI() {
    qInfo() << "[Win32AgentAPI] Initializing Win32 Native Agent API";
}

Win32AgentAPI::~Win32AgentAPI() {
    qInfo() << "[Win32AgentAPI] Shutting down Win32 Native Agent API";
}

std::wstring Win32AgentAPI::ExecuteShellCommand(const std::wstring& command, DWORD timeoutMs) {
    std::wstring output, error;
    ProcessCreateResult result = m_processManager.ExecuteCommandWithPipes(command, output, error, timeoutMs);
    
    std::wstringstream ss;
    if (!output.empty()) {
        ss << L"STDOUT:\n" << output << L"\n";
    }
    if (!error.empty()) {
        ss << L"STDERR:\n" << error << L"\n";
    }
    if (result.success && result.exitCode != 0) {
        ss << L"Exit Code: " << result.exitCode << L"\n";
    }
    
    return ss.str();
}

bool Win32AgentAPI::LaunchApplication(const std::wstring& path, const std::wstring& args) {
    ProcessCreateParams params;
    params.executablePath = path;
    params.arguments = args;
    params.hiddenWindow = false;
    
    ProcessCreateResult result = m_processManager.CreateProcessEx(params);
    return result.success;
}

bool Win32AgentAPI::KillProcess(const std::wstring& processName) {
    auto processes = m_processManager.EnumerateProcesses();
    
    for (const auto& process : processes) {
        if (_wcsicmp(process.processName.c_str(), processName.c_str()) == 0) {
            return m_processManager.TerminateProcess(process.processId);
        }
    }
    
    return false;
}

std::wstring Win32AgentAPI::ReadTextFile(const std::wstring& path) {
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return L"ERROR: Cannot open file: " + path;
    }
    
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return L"ERROR: Cannot get file size: " + path;
    }
    
    std::vector<char> buffer(fileSize + 1, 0);
    DWORD bytesRead = 0;
    
    if (ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return std::wstring(buffer.begin(), buffer.begin() + bytesRead);
    }
    
    CloseHandle(hFile);
    return L"ERROR: Cannot read file: " + path;
}

bool Win32AgentAPI::WriteTextFile(const std::wstring& path, const std::wstring& content) {
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    std::string narrowContent(content.begin(), content.end());
    DWORD bytesWritten = 0;
    
    bool result = WriteFile(hFile, narrowContent.data(), static_cast<DWORD>(narrowContent.length()), &bytesWritten, nullptr) == TRUE;
    CloseHandle(hFile);
    
    return result;
}

std::vector<std::wstring> Win32AgentAPI::ListFiles(const std::wstring& directory, const std::wstring& pattern) {
    auto fileInfos = m_fileSystemManager.EnumerateDirectory(directory, pattern);
    std::vector<std::wstring> files;
    
    for (const auto& fileInfo : fileInfos) {
        if (!fileInfo.isDirectory) {
            files.push_back(fileInfo.name);
        }
    }
    
    return files;
}

std::vector<std::pair<std::wstring, std::wstring>> Win32AgentAPI::GetRunningProcesses() {
    auto processes = m_processManager.EnumerateProcesses();
    std::vector<std::pair<std::wstring, std::wstring>> result;
    
    for (const auto& process : processes) {
        std::wstringstream ss;
        ss << process.processId;
        result.push_back({process.processName, ss.str()});
    }
    
    return result;
}

bool Win32AgentAPI::IsElevated() {
    HANDLE hToken = nullptr;
    BOOL elevated = FALSE;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation = {};
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            elevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    
    return elevated == TRUE;
}

// ============================================================================
// SYSTEM INFO MANAGER IMPLEMENTATION
// ============================================================================

SystemInfoManager::SystemInfoManager() 
    : m_lastIdleTime(0), m_lastKernelTime(0), m_lastUserTime(0) {
}

SystemInfoManager::~SystemInfoManager() {
}

SystemInfo SystemInfoManager::GetSystemInfo() {
    SystemInfo info;
    
    // Get computer name
    WCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = ARRAYSIZE(computerName);
    if (GetComputerNameW(computerName, &size)) {
        info.computerName = computerName;
    }
    
    // Get user name
    WCHAR userName[UNLEN + 1];
    size = ARRAYSIZE(userName);
    if (GetUserNameW(userName, &size)) {
        info.userName = userName;
    }
    
    // Get OS version
    OSVERSIONINFOW osvi = { sizeof(osvi) };
    if (GetVersionExW(&osvi)) {
        info.osVersion = std::to_wstring(osvi.dwMajorVersion) + L"." + std::to_wstring(osvi.dwMinorVersion);
        info.osBuild = std::to_wstring(osvi.dwBuildNumber);
    }
    
    // Get system info
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);
    info.processorCount = si.dwNumberOfProcessors;
    info.processorArchitecture = si.wProcessorArchitecture;
    info.pageSize = si.dwPageSize;
    info.allocationGranularity = si.dwAllocationGranularity;
    
    // Get processor name from registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR procName[256];
        DWORD dataSize = sizeof(procName);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, 
            (LPBYTE)procName, &dataSize) == ERROR_SUCCESS) {
            info.processorName = procName;
        }
        RegCloseKey(hKey);
    }
    
    // Get memory info
    MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalPhysicalMemory = memStatus.ullTotalPhys;
        info.availablePhysicalMemory = memStatus.ullAvailPhys;
    }
    
    return info;
}

CpuUsage SystemInfoManager::GetCpuUsage() {
    CpuUsage usage = {};
    usage.totalCpu = 0.0;
    usage.kernelCpu = 0.0;
    usage.userCpu = 0.0;
    return usage;
}

std::wstring SystemInfoManager::GetEnvironmentVariable(const std::wstring& name) {
    WCHAR buffer[32768];
    DWORD size = ::GetEnvironmentVariableW(name.c_str(), buffer, ARRAYSIZE(buffer));
    if (size > 0 && size < ARRAYSIZE(buffer)) {
        return buffer;
    }
    return L"";
}

bool SystemInfoManager::SetEnvironmentVariable(const std::wstring& name, const std::wstring& value) {
    return ::SetEnvironmentVariableW(name.c_str(), value.c_str()) == TRUE;
}

std::vector<std::pair<std::wstring, std::wstring>> SystemInfoManager::GetAllEnvironmentVariables() {
    std::vector<std::pair<std::wstring, std::wstring>> vars;
    LPWSTR envBlock = GetEnvironmentStringsW();
    if (envBlock) {
        LPWSTR envVar = envBlock;
        while (*envVar) {
            std::wstring var(envVar);
            size_t pos = var.find(L'=');
            if (pos != std::wstring::npos) {
                vars.emplace_back(var.substr(0, pos), var.substr(pos + 1));
            }
            envVar += var.length() + 1;
        }
        FreeEnvironmentStringsW(envBlock);
    }
    return vars;
}

bool SystemInfoManager::Shutdown(bool reboot, bool force) {
    DWORD flags = reboot ? EWX_REBOOT : EWX_SHUTDOWN;
    if (force) flags |= EWX_FORCE;
    return ExitWindowsEx(flags, 0) == TRUE;
}

bool SystemInfoManager::LockWorkstation() {
    return LockWorkStation() == TRUE;
}

bool SystemInfoManager::SetSystemTime(const SYSTEMTIME& time) {
    return ::SetSystemTime(&time) == TRUE;
}

ULONGLONG SystemInfoManager::GetSystemUptime() {
    return GetTickCount64();
}

ULONGLONG SystemInfoManager::GetIdleTime() {
    return GetTickCount64();
}

// ============================================================================
// REGISTRY MANAGER IMPLEMENTATION
// ============================================================================

RegistryManager::RegistryManager() {
}

RegistryManager::~RegistryManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto hKey : m_openKeys) {
        if (hKey) {
            RegCloseKey(hKey);
        }
    }
    m_openKeys.clear();
}

HKEY RegistryManager::OpenKey(HKEY hRoot, const std::wstring& subKey, REGSAM access) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, access, &hKey) == ERROR_SUCCESS) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_openKeys.push_back(hKey);
        return hKey;
    }
    return nullptr;
}

HKEY RegistryManager::CreateKey(HKEY hRoot, const std::wstring& subKey) {
    HKEY hKey;
    DWORD disposition;
    if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, 
        KEY_ALL_ACCESS, nullptr, &hKey, &disposition) == ERROR_SUCCESS) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_openKeys.push_back(hKey);
        return hKey;
    }
    return nullptr;
}

bool RegistryManager::DeleteKey(HKEY hRoot, const std::wstring& subKey) {
    return RegDeleteKeyW(hRoot, subKey.c_str()) == ERROR_SUCCESS;
}

bool RegistryManager::DeleteKeyRecursive(HKEY hRoot, const std::wstring& subKey) {
    return RegDeleteTreeW(hRoot, subKey.c_str()) == ERROR_SUCCESS;
}

void RegistryManager::CloseKey(HKEY hKey) {
    if (hKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        RegCloseKey(hKey);
        m_openKeys.erase(std::find(m_openKeys.begin(), m_openKeys.end(), hKey));
    }
}

RegistryValue RegistryManager::GetValue(HKEY hKey, const std::wstring& valueName) {
    RegistryValue value = {};
    value.name = valueName;
    value.type = REG_NONE;
    
    DWORD dataSize = 0;
    if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, &value.type, nullptr, &dataSize) == ERROR_SUCCESS) {
        value.data.resize(dataSize);
        RegQueryValueExW(hKey, valueName.c_str(), nullptr, &value.type, value.data.data(), &dataSize);
        
        if (value.type == REG_SZ || value.type == REG_EXPAND_SZ) {
            value.stringValue = reinterpret_cast<LPCWSTR>(value.data.data());
        } else if (value.type == REG_DWORD) {
            value.dwordValue = *reinterpret_cast<DWORD*>(value.data.data());
        } else if (value.type == REG_QWORD) {
            value.qwordValue = *reinterpret_cast<ULONGLONG*>(value.data.data());
        }
    }
    return value;
}

bool RegistryManager::SetStringValue(HKEY hKey, const std::wstring& valueName, const std::wstring& value) {
    return RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ, 
        reinterpret_cast<const BYTE*>(value.c_str()), (value.length() + 1) * sizeof(WCHAR)) == ERROR_SUCCESS;
}

bool RegistryManager::SetDwordValue(HKEY hKey, const std::wstring& valueName, DWORD value) {
    return RegSetValueExW(hKey, valueName.c_str(), 0, REG_DWORD, 
        reinterpret_cast<const BYTE*>(&value), sizeof(DWORD)) == ERROR_SUCCESS;
}

bool RegistryManager::SetQwordValue(HKEY hKey, const std::wstring& valueName, ULONGLONG value) {
    return RegSetValueExW(hKey, valueName.c_str(), 0, REG_QWORD, 
        reinterpret_cast<const BYTE*>(&value), sizeof(ULONGLONG)) == ERROR_SUCCESS;
}

bool RegistryManager::SetBinaryValue(HKEY hKey, const std::wstring& valueName, const std::vector<BYTE>& data) {
    return RegSetValueExW(hKey, valueName.c_str(), 0, REG_BINARY, data.data(), data.size()) == ERROR_SUCCESS;
}

bool RegistryManager::DeleteValue(HKEY hKey, const std::wstring& valueName) {
    return RegDeleteValueW(hKey, valueName.c_str()) == ERROR_SUCCESS;
}

std::vector<std::wstring> RegistryManager::EnumerateSubKeys(HKEY hKey) {
    std::vector<std::wstring> subKeys;
    WCHAR name[256];
    DWORD index = 0;
    while (RegEnumKeyW(hKey, index, name, ARRAYSIZE(name)) == ERROR_SUCCESS) {
        subKeys.push_back(name);
        index++;
    }
    return subKeys;
}

std::vector<RegistryValue> RegistryManager::EnumerateValues(HKEY hKey) {
    std::vector<RegistryValue> values;
    WCHAR valueName[256];
    DWORD index = 0;
    while (RegEnumValueW(hKey, index, valueName, (PDWORD)ARRAYSIZE(valueName), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        values.push_back(GetValue(hKey, valueName));
        index++;
    }
    return values;
}

std::wstring RegistryManager::ReadString(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = OpenKey(hRoot, subKey);
    if (hKey) {
        RegistryValue val = GetValue(hKey, valueName);
        CloseKey(hKey);
        return val.stringValue;
    }
    return L"";
}

DWORD RegistryManager::ReadDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = OpenKey(hRoot, subKey);
    if (hKey) {
        RegistryValue val = GetValue(hKey, valueName);
        CloseKey(hKey);
        return val.dwordValue;
    }
    return 0;
}

bool RegistryManager::WriteString(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value) {
    HKEY hKey = CreateKey(hRoot, subKey);
    if (hKey) {
        bool result = SetStringValue(hKey, valueName, value);
        CloseKey(hKey);
        return result;
    }
    return false;
}

bool RegistryManager::WriteDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD value) {
    HKEY hKey = CreateKey(hRoot, subKey);
    if (hKey) {
        bool result = SetDwordValue(hKey, valueName, value);
        CloseKey(hKey);
        return result;
    }
    return false;
}

// ============================================================================
// SERVICE MANAGER IMPLEMENTATION
// ============================================================================

ServiceManager::ServiceManager() : m_hSCManager(nullptr) {
    m_hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
}

ServiceManager::~ServiceManager() {
    if (m_hSCManager) {
        CloseServiceHandle(m_hSCManager);
        m_hSCManager = nullptr;
    }
}

std::vector<ServiceInfo> ServiceManager::EnumerateServices(DWORD serviceType) {
    std::vector<ServiceInfo> services;
    if (!m_hSCManager) return services;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    ENUM_SERVICE_STATUS_PROCESSW* pServices = nullptr;
    DWORD dwBytesNeeded = 0;
    DWORD dwServiceCount = 0;
    DWORD dwResumeHandle = 0;
    
    if (EnumServicesStatusExW(m_hSCManager, SC_ENUM_PROCESS_INFO, serviceType, 
        SERVICE_STATE_ALL, nullptr, 0, &dwBytesNeeded, &dwServiceCount, &dwResumeHandle, nullptr)) {
        return services;
    }
    
    pServices = (ENUM_SERVICE_STATUS_PROCESSW*)malloc(dwBytesNeeded);
    if (!pServices) return services;
    
    if (EnumServicesStatusExW(m_hSCManager, SC_ENUM_PROCESS_INFO, serviceType, 
        SERVICE_STATE_ALL, (LPBYTE)pServices, dwBytesNeeded, &dwBytesNeeded, &dwServiceCount, &dwResumeHandle, nullptr)) {
        
        for (DWORD i = 0; i < dwServiceCount; i++) {
            ServiceInfo info;
            info.serviceName = pServices[i].lpServiceName;
            info.displayName = pServices[i].lpDisplayName;
            info.serviceType = pServices[i].ServiceStatusProcess.dwServiceType;
            info.currentState = pServices[i].ServiceStatusProcess.dwCurrentState;
            info.startType = pServices[i].ServiceStatusProcess.dwControlsAccepted;
            info.processId = pServices[i].ServiceStatusProcess.dwProcessId;
            info.isRunning = (pServices[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING);
            services.push_back(info);
        }
    }
    
    free(pServices);
    return services;
}

ServiceInfo ServiceManager::GetServiceInfo(const std::wstring& serviceName) {
    ServiceInfo info = {};
    if (!m_hSCManager) return info;
    
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (hService) {
        SERVICE_STATUS_PROCESS ssp = {};
        DWORD dwBytesNeeded = 0;
        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &dwBytesNeeded)) {
            info.serviceName = serviceName;
            info.currentState = ssp.dwCurrentState;
            info.processId = ssp.dwProcessId;
            info.isRunning = (ssp.dwCurrentState == SERVICE_RUNNING);
        }
        CloseServiceHandle(hService);
    }
    return info;
}

bool ServiceManager::StartService(const std::wstring& serviceName, const std::vector<std::wstring>& args) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_START);
    if (hService) {
        LPCWSTR* lpServiceArgVectors = nullptr;
        DWORD dwArgCount = 0;
        if (!args.empty()) {
            lpServiceArgVectors = new LPCWSTR[args.size()];
            for (size_t i = 0; i < args.size(); i++) {
                lpServiceArgVectors[i] = args[i].c_str();
            }
            dwArgCount = static_cast<DWORD>(args.size());
        }
        
        bool result = ::StartServiceW(hService, dwArgCount, lpServiceArgVectors) == TRUE;
        
        if (lpServiceArgVectors) {
            delete[] lpServiceArgVectors;
        }
        CloseServiceHandle(hService);
        return result;
    }
    return false;
}

bool ServiceManager::StopService(const std::wstring& serviceName) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_STOP);
    if (hService) {
        SERVICE_STATUS serviceStatus = {};
        bool result = ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus) == TRUE;
        CloseServiceHandle(hService);
        return result;
    }
    return false;
}

bool ServiceManager::RestartService(const std::wstring& serviceName) {
    return StopService(serviceName) && StartService(serviceName);
}

bool ServiceManager::PauseService(const std::wstring& serviceName) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_PAUSE_CONTINUE);
    if (hService) {
        SERVICE_STATUS serviceStatus = {};
        bool result = ControlService(hService, SERVICE_CONTROL_PAUSE, &serviceStatus) == TRUE;
        CloseServiceHandle(hService);
        return result;
    }
    return false;
}

bool ServiceManager::ContinueService(const std::wstring& serviceName) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_PAUSE_CONTINUE);
    if (hService) {
        SERVICE_STATUS serviceStatus = {};
        bool result = ControlService(hService, SERVICE_CONTROL_CONTINUE, &serviceStatus) == TRUE;
        CloseServiceHandle(hService);
        return result;
    }
    return false;
}

bool ServiceManager::CreateService(const std::wstring& serviceName, const std::wstring& displayName, 
                                   const std::wstring& binaryPath, DWORD startType) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = ::CreateServiceW(m_hSCManager, serviceName.c_str(), displayName.c_str(), 
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, startType, SERVICE_ERROR_NORMAL, 
        binaryPath.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);
    
    if (hService) {
        CloseServiceHandle(hService);
        return true;
    }
    return false;
}

bool ServiceManager::DeleteService(const std::wstring& serviceName) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), DELETE);
    if (hService) {
        BOOL result = ::DeleteService(hService);
        CloseServiceHandle(hService);
        return result == TRUE;
    }
    return false;
}

bool ServiceManager::SetServiceDescription(const std::wstring& serviceName, const std::wstring& description) {
    if (!m_hSCManager) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_CHANGE_CONFIG);
    if (hService) {
        SERVICE_DESCRIPTION sd = {};
        sd.lpDescription = const_cast<LPWSTR>(description.c_str());
        bool result = ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd) == TRUE;
        CloseServiceHandle(hService);
        return result;
    }
    return false;
}

DWORD ServiceManager::GetServiceState(const std::wstring& serviceName) {
    if (!m_hSCManager) return SERVICE_STOPPED;
    
    SC_HANDLE hService = OpenServiceW(m_hSCManager, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (hService) {
        SERVICE_STATUS_PROCESS ssp = {};
        DWORD dwBytesNeeded = 0;
        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &dwBytesNeeded)) {
            CloseServiceHandle(hService);
            return ssp.dwCurrentState;
        }
        CloseServiceHandle(hService);
    }
    return SERVICE_STOPPED;
}

bool ServiceManager::WaitForServiceState(const std::wstring& serviceName, DWORD desiredState, DWORD timeoutMs) {
    ULONGLONG startTime = GetTickCount64();
    while (GetTickCount64() - startTime < timeoutMs) {
        if (GetServiceState(serviceName) == desiredState) {
            return true;
        }
        Sleep(500);
    }
    return false;
}

// ============================================================================
// WINDOW MANAGER IMPLEMENTATION
// ============================================================================

WindowManager::WindowManager() {
}

WindowManager::~WindowManager() {
}

std::vector<WindowInfo> WindowManager::EnumerateWindows(bool visibleOnly) {
    std::vector<WindowInfo> windows;
    // For simplicity, return empty list - full implementation would require static callback
    return windows;
}

std::vector<WindowInfo> WindowManager::FindWindowsByTitle(const std::wstring& title, bool partialMatch) {
    std::vector<WindowInfo> results;
    auto allWindows = EnumerateWindows();
    for (const auto& window : allWindows) {
        if (partialMatch) {
            if (window.title.find(title) != std::wstring::npos) {
                results.push_back(window);
            }
        } else {
            if (window.title == title) {
                results.push_back(window);
            }
        }
    }
    return results;
}

std::vector<WindowInfo> WindowManager::FindWindowsByClass(const std::wstring& className) {
    std::vector<WindowInfo> results;
    // Implementation would search by window class
    return results;
}

std::vector<WindowInfo> WindowManager::GetProcessWindows(DWORD processId) {
    std::vector<WindowInfo> results;
    auto allWindows = EnumerateWindows();
    for (const auto& window : allWindows) {
        if (window.processId == processId) {
            results.push_back(window);
        }
    }
    return results;
}

WindowInfo WindowManager::GetWindowInfo(HWND hwnd) {
    WindowInfo info = {};
    info.hwnd = hwnd;
    GetWindowTextW(hwnd, (LPWSTR)info.title.c_str(), MAX_PATH);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.processId = pid;
    info.isVisible = IsWindowVisible(hwnd) == TRUE;
    return info;
}

bool WindowManager::ShowWindow(HWND hwnd, int showCmd) {
    return ::ShowWindow(hwnd, showCmd) != 0;
}

bool WindowManager::HideWindow(HWND hwnd) {
    return ::ShowWindow(hwnd, SW_HIDE) != 0;
}

bool WindowManager::MinimizeWindow(HWND hwnd) {
    return ::ShowWindow(hwnd, SW_MINIMIZE) != 0;
}

bool WindowManager::MaximizeWindow(HWND hwnd) {
    return ::ShowWindow(hwnd, SW_MAXIMIZE) != 0;
}

bool WindowManager::RestoreWindow(HWND hwnd) {
    return ::ShowWindow(hwnd, SW_RESTORE) != 0;
}

bool WindowManager::CloseWindow(HWND hwnd) {
    return PostMessageW(hwnd, WM_CLOSE, 0, 0) == TRUE;
}

bool WindowManager::MoveWindow(HWND hwnd, int x, int y, int width, int height) {
    return ::MoveWindow(hwnd, x, y, width, height, TRUE) == TRUE;
}

bool WindowManager::SetWindowTitle(HWND hwnd, const std::wstring& title) {
    return SetWindowTextW(hwnd, title.c_str()) == TRUE;
}

bool WindowManager::SetForegroundWindow(HWND hwnd) {
    return ::SetForegroundWindow(hwnd) != NULL;
}

bool WindowManager::EnableWindow(HWND hwnd, bool enable) {
    return ::EnableWindow(hwnd, enable ? TRUE : FALSE) == TRUE;
}

LRESULT WindowManager::SendMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return SendMessageW(hwnd, msg, wParam, lParam);
}

bool WindowManager::PostMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return PostMessageW(hwnd, msg, wParam, lParam) == TRUE;
}

HWND WindowManager::GetDesktopWindow() {
    return ::GetDesktopWindow();
}

HWND WindowManager::GetForegroundWindow() {
    return ::GetForegroundWindow();
}

HWND WindowManager::GetActiveWindow() {
    return ::GetActiveWindow();
}

BOOL WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    return TRUE;
}

// ============================================================================
// NETWORK MANAGER IMPLEMENTATION
// ============================================================================

NetworkManager::NetworkManager() {
}

NetworkManager::~NetworkManager() {
}

// ============================================================================
// PIPE MANAGER IMPLEMENTATION
// ============================================================================

PipeManager::PipeManager() {
}

PipeManager::~PipeManager() {
}

} // namespace Win32Agent
} // namespace RawrXD
