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
    if (params.inheritHandles) creationFlags |= INHERIT_PARENT_PROCESSOR_AFFINITY;
    
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
    
    BOOL success = CreateProcessW(
        nullptr,
        const_cast<LPWSTR>((L"cmd.exe /c \"" + command + L"\"").c_str()),
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
        result.errorMessage = L"Failed to create process";
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
    }
    
    return result;
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
    
    HANDLE hThread = CreateThread(
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
    HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
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
    HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    std::string narrowContent(content.begin(), content.end());
    DWORD bytesWritten = 0;
    
    bool result = WriteFile(hFile, narrowContent.data(), narrowContent.length(), &bytesWritten) == TRUE;
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

} // namespace Win32Agent
} // namespace RawrXD
