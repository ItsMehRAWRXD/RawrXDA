#include "win32_agent_tools.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <memory>
#include <sstream>
#include <cstdint>
#include <cstring>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
static inline void rawrxd_cpuid(int info[4], int leaf) {
    __cpuid_count(leaf, 0, info[0], info[1], info[2], info[3]);
}
#define __cpuid(info, leaf) rawrxd_cpuid(info, leaf)
#endif

// ============================================================================
// Implementation of header's static API (rawrxd::agent:: classes)
// Rewritten to match header declarations — preserves ALL original Win32 logic
// ============================================================================

namespace rawrxd {
namespace agent {

// ============================================================================
// ProcessManager Implementation (static methods)
// ============================================================================

HANDLE ProcessManager::CreateAgenticProcess(
    const std::wstring& executable,
    const std::wstring& arguments,
    const std::wstring& working_dir,
    bool inherit_handles
) {
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::wstring cmd_line = executable;
    if (!arguments.empty()) {
        cmd_line += L" " + arguments;
    }

    const wchar_t* work_dir_ptr = working_dir.empty() ? nullptr : working_dir.c_str();

    if (!::CreateProcessW(
        executable.c_str(),
        const_cast<LPWSTR>(cmd_line.c_str()),
        nullptr,
        nullptr,
        inherit_handles ? TRUE : FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        work_dir_ptr,
        &si,
        &pi
    )) {
        return INVALID_HANDLE_VALUE;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

std::vector<ProcessManager::ProcessInfo> ProcessManager::EnumerateProcesses() {
    std::vector<ProcessInfo> processes;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.name = pe.szExeFile;
            info.memory_mb = 0;
            info.cpu_percent = 0.0f;

            // Try to get module path and memory info
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (hProcess && hProcess != INVALID_HANDLE_VALUE) {
                wchar_t path_buf[MAX_PATH] = {};
                if (GetModuleFileNameExW(hProcess, nullptr, path_buf, MAX_PATH)) {
                    info.path = path_buf;
                }
                PROCESS_MEMORY_COUNTERS pmc = {};
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    info.memory_mb = pmc.WorkingSetSize / (1024 * 1024);
                }
                CloseHandle(hProcess);
            }

            processes.push_back(info);
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return processes;
}

bool ProcessManager::ReadProcessMemory(
    HANDLE process,
    void* address,
    void* buffer,
    size_t size
) {
    SIZE_T bytes_read = 0;
    BOOL success = ::ReadProcessMemory(process, address, buffer, size, &bytes_read);
    return success && bytes_read == size;
}

bool ProcessManager::WriteProcessMemory(
    HANDLE process,
    void* address,
    const void* buffer,
    size_t size
) {
    SIZE_T bytes_written = 0;
    BOOL success = ::WriteProcessMemory(process, address, const_cast<void*>(buffer), size, &bytes_written);
    return success && bytes_written == size;
}

bool ProcessManager::TerminateProcess(HANDLE process, UINT exit_code) {
    return ::TerminateProcess(process, exit_code) != 0;
}

HANDLE ProcessManager::FindProcessByName(const std::wstring& name) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (name == pe.szExeFile) {
                CloseHandle(hSnapshot);
                return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID);
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return INVALID_HANDLE_VALUE;
}

bool ProcessManager::InjectDLL(HANDLE process, const std::wstring& dll_path) {
    // Allocate memory for DLL path in target process
    size_t path_size = (dll_path.length() + 1) * sizeof(wchar_t);
    LPVOID remote_buffer = VirtualAllocEx(process, nullptr, path_size, MEM_COMMIT, PAGE_READWRITE);
    if (!remote_buffer) {
        return false;
    }

    // Write DLL path to target process
    if (!::WriteProcessMemory(process, remote_buffer, dll_path.c_str(), path_size, nullptr)) {
        VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
        return false;
    }

    // Get address of LoadLibraryW in kernel32.dll
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC load_library_addr = GetProcAddress(kernel32, "LoadLibraryW");

    // Create remote thread to call LoadLibraryW
    HANDLE hThread = CreateRemoteThread(
        process,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)load_library_addr,
        remote_buffer,
        0,
        nullptr
    );

    bool success = (hThread != nullptr && hThread != INVALID_HANDLE_VALUE);

    if (success) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    VirtualFreeEx(process, remote_buffer, 0, MEM_RELEASE);
    return success;
}

// ============================================================================
// FileSystemTools Implementation (static methods)
// ============================================================================

std::vector<uint8_t> FileSystemTools::ReadFileMapped(const std::wstring& path) {
    std::vector<uint8_t> data;

    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) return data;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size)) {
        CloseHandle(hFile);
        return data;
    }

    HANDLE hMapFile = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapFile) {
        CloseHandle(hFile);
        return data;
    }

    LPVOID pFile = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (pFile) {
        data.resize(static_cast<size_t>(file_size.QuadPart));
        memcpy(data.data(), pFile, data.size());
        UnmapViewOfFile(pFile);
    }

    CloseHandle(hMapFile);
    CloseHandle(hFile);
    return data;
}

bool FileSystemTools::WriteFileAtomic(
    const std::wstring& path,
    const void* data,
    size_t size
) {
    std::wstring temp_path = path + L".tmp";

    HANDLE hFile = CreateFileW(
        temp_path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD bytes_written = 0;
    if (!::WriteFile(hFile, data, static_cast<DWORD>(size), &bytes_written, nullptr) ||
        bytes_written != static_cast<DWORD>(size)) {
        CloseHandle(hFile);
        DeleteFileW(temp_path.c_str());
        return false;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    // Atomic rename
    if (!MoveFileExW(temp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp_path.c_str());
        return false;
    }

    return true;
}

bool FileSystemTools::CreateDirectoryRecursive(const std::wstring& path) {
    if (::CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }

    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return false;

    std::wstring parent = path.substr(0, pos);
    if (!CreateDirectoryRecursive(parent)) return false;

    return ::CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool FileSystemTools::DeleteDirectoryRecursive(const std::wstring& path) {
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW((path + L"\\*").c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        std::wstring name = ffd.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring full = path + L"\\" + name;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            DeleteDirectoryRecursive(full);
        } else {
            DeleteFileW(full.c_str());
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
    return RemoveDirectoryW(path.c_str()) != 0;
}

HANDLE FileSystemTools::MonitorDirectory(
    const std::wstring& path,
    std::function<void(const std::wstring&, DWORD)> callback
) {
    HANDLE hDir = CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    (void)callback; // Actual monitoring requires ReadDirectoryChangesW in a thread
    return hDir;
}

bool FileSystemTools::GetFileInfo(
    const std::wstring& path,
    WIN32_FILE_ATTRIBUTE_DATA& info
) {
    return GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) != 0;
}

bool FileSystemTools::CopyFileFast(
    const std::wstring& source,
    const std::wstring& dest,
    bool overwrite
) {
    return ::CopyFileW(source.c_str(), dest.c_str(), overwrite ? FALSE : TRUE) != 0;
}

// ============================================================================
// RegistryTools Implementation (static methods)
// ============================================================================

static HKEY rootKeyToHKEY(RegistryTools::RootKey root) {
    switch (root) {
        case RegistryTools::RootKey::CurrentUser:   return HKEY_CURRENT_USER;
        case RegistryTools::RootKey::LocalMachine:  return HKEY_LOCAL_MACHINE;
        case RegistryTools::RootKey::ClassesRoot:   return HKEY_CLASSES_ROOT;
        case RegistryTools::RootKey::Users:          return HKEY_USERS;
        default: return HKEY_CURRENT_USER;
    }
}

std::wstring RegistryTools::ReadString(
    RootKey root,
    const std::wstring& subkey,
    const std::wstring& value_name
) {
    HKEY hKey;
    HKEY hRoot = rootKeyToHKEY(root);
    if (RegOpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return L"";
    }

    wchar_t buffer[1024] = {};
    DWORD size = sizeof(buffer);
    if (RegQueryValueExW(hKey, value_name.c_str(), nullptr, nullptr, (LPBYTE)buffer, &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return L"";
    }

    RegCloseKey(hKey);
    return buffer;
}

DWORD RegistryTools::ReadDWORD(
    RootKey root,
    const std::wstring& subkey,
    const std::wstring& value_name
) {
    HKEY hKey;
    HKEY hRoot = rootKeyToHKEY(root);
    if (RegOpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return 0;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    RegQueryValueExW(hKey, value_name.c_str(), nullptr, nullptr, (LPBYTE)&value, &size);
    RegCloseKey(hKey);
    return value;
}

bool RegistryTools::WriteString(
    RootKey root,
    const std::wstring& subkey,
    const std::wstring& value_name,
    const std::wstring& value
) {
    HKEY hKey;
    HKEY hRoot = rootKeyToHKEY(root);
    DWORD dwDisposition;

    if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, nullptr, &hKey, &dwDisposition) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegSetValueExW(hKey, value_name.c_str(), 0, REG_SZ,
                                  (const BYTE*)value.c_str(),
                                  (DWORD)((value.length() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool RegistryTools::WriteDWORD(
    RootKey root,
    const std::wstring& subkey,
    const std::wstring& value_name,
    DWORD value
) {
    HKEY hKey;
    HKEY hRoot = rootKeyToHKEY(root);
    DWORD dwDisposition;

    if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, nullptr, &hKey, &dwDisposition) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegSetValueExW(hKey, value_name.c_str(), 0, REG_DWORD,
                                  (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool RegistryTools::DeleteKey(
    RootKey root,
    const std::wstring& subkey
) {
    HKEY hRoot = rootKeyToHKEY(root);
    return RegDeleteKeyW(hRoot, subkey.c_str()) == ERROR_SUCCESS;
}

std::vector<std::wstring> RegistryTools::EnumerateSubkeys(
    RootKey root,
    const std::wstring& subkey
) {
    std::vector<std::wstring> result;
    HKEY hKey;
    HKEY hRoot = rootKeyToHKEY(root);

    if (RegOpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return result;
    }

    wchar_t name_buf[256];
    DWORD index = 0;
    DWORD name_len;

    while (true) {
        name_len = 256;
        LONG status = RegEnumKeyExW(hKey, index++, name_buf, &name_len, nullptr, nullptr, nullptr, nullptr);
        if (status != ERROR_SUCCESS) break;
        result.push_back(name_buf);
    }

    RegCloseKey(hKey);
    return result;
}

// ============================================================================
// MemoryTools Implementation (static methods)
// ============================================================================

void* MemoryTools::AllocateVirtual(size_t size, DWORD protect) {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
}

bool MemoryTools::FreeVirtual(void* address) {
    return VirtualFree(address, 0, MEM_RELEASE) != 0;
}

bool MemoryTools::ProtectMemory(void* address, size_t size, DWORD new_protect, DWORD* old_protect) {
    DWORD old_p;
    BOOL result = VirtualProtect(address, size, new_protect, &old_p);
    if (old_protect) *old_protect = old_p;
    return result != 0;
}

bool MemoryTools::LockMemory(void* address, size_t size) {
    return VirtualLock(address, size) != 0;
}

bool MemoryTools::UnlockMemory(void* address, size_t size) {
    return VirtualUnlock(address, size) != 0;
}

MEMORYSTATUSEX MemoryTools::GetMemoryStatus() {
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return mem;
}

PROCESS_MEMORY_COUNTERS MemoryTools::GetProcessMemory(HANDLE process) {
    PROCESS_MEMORY_COUNTERS pmc = {};
    pmc.cb = sizeof(pmc);
    ::GetProcessMemoryInfo(process, &pmc, sizeof(pmc));
    return pmc;
}

// ============================================================================
// IPCTools Implementation (static methods)
// ============================================================================

HANDLE IPCTools::CreateNamedPipeServer(const std::wstring& pipe_name, size_t buffer_size) {
    return ::CreateNamedPipeW(
        (L"\\\\.\\pipe\\" + pipe_name).c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        static_cast<DWORD>(buffer_size),
        static_cast<DWORD>(buffer_size),
        0,
        nullptr
    );
}

HANDLE IPCTools::ConnectNamedPipe(const std::wstring& pipe_name) {
    return CreateFileW(
        (L"\\\\.\\pipe\\" + pipe_name).c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
}

std::vector<uint8_t> IPCTools::ReadPipe(HANDLE pipe, size_t max_size) {
    std::vector<uint8_t> data(max_size);
    DWORD bytes_read = 0;
    if (::ReadFile(pipe, data.data(), static_cast<DWORD>(max_size), &bytes_read, nullptr)) {
        data.resize(bytes_read);
    } else {
        data.clear();
    }
    return data;
}

bool IPCTools::WritePipe(HANDLE pipe, const void* data, size_t size) {
    DWORD bytes_written = 0;
    return ::WriteFile(pipe, data, static_cast<DWORD>(size), &bytes_written, nullptr) &&
           bytes_written == static_cast<DWORD>(size);
}

HANDLE IPCTools::CreateSharedMemory(const std::wstring& name, size_t size) {
    return CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        static_cast<DWORD>(size >> 32),
        static_cast<DWORD>(size & 0xFFFFFFFF),
        name.c_str()
    );
}

HANDLE IPCTools::OpenSharedMemory(const std::wstring& name) {
    return OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
}

void* IPCTools::MapSharedMemory(HANDLE mapping) {
    return MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
}

bool IPCTools::UnmapSharedMemory(void* address) {
    return UnmapViewOfFile(address) != 0;
}

// ============================================================================
// SystemInfo Implementation (static methods)
// ============================================================================

SystemInfo::CPUInfo SystemInfo::GetCPUInfo() {
    CPUInfo info;
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);
    info.cores = si.dwNumberOfProcessors;
    info.logical_processors = si.dwNumberOfProcessors;

    // Get CPU vendor string
    int cpu_info[4] = {};
    __cpuid(cpu_info, 0);
    char vendor[13] = {};
    memcpy(vendor, &cpu_info[1], 4);
    memcpy(vendor + 4, &cpu_info[3], 4);
    memcpy(vendor + 8, &cpu_info[2], 4);
    info.vendor = std::wstring(vendor, vendor + 12);

    // Get CPU brand string
    char brand[49] = {};
    for (int i = 0; i < 3; i++) {
        __cpuid(cpu_info, 0x80000002 + i);
        memcpy(brand + i * 16, cpu_info, 16);
    }
    info.brand = std::wstring(brand, brand + strlen(brand));

    return info;
}

std::vector<SystemInfo::GPUInfo> SystemInfo::GetGPUInfo() {
    // Minimal implementation — full version requires DXGI or WMI
    std::vector<GPUInfo> infos;
    GPUInfo info;
    info.name = L"Unknown GPU";
    info.memory_mb = 0;
    info.driver_version = L"Unknown";
    infos.push_back(info);
    return infos;
}

uint64_t SystemInfo::GetSystemUptimeMS() {
    return GetTickCount64();
}

std::wstring SystemInfo::GetCurrentUser() {
    wchar_t buf[256] = {};
    DWORD size = 256;
    GetUserNameW(buf, &size);
    return buf;
}

std::wstring SystemInfo::GetComputerName() {
    wchar_t buf[256] = {};
    DWORD size = 256;
    GetComputerNameW(buf, &size);
    return buf;
}

std::wstring SystemInfo::GetOSVersion() {
    return L"Windows 10+";
}

bool SystemInfo::IsElevated() {
    BOOL fElevated = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fElevated != 0;
}

// ============================================================================
// DebugTools Implementation (stub — requires DbgHelp)
// ============================================================================

bool DebugTools::AttachDebugger(DWORD pid) {
    return DebugActiveProcess(pid) != 0;
}

bool DebugTools::DetachDebugger() {
    return TRUE; // Stub — DebugActiveProcessStop requires PID
}

bool DebugTools::SetBreakpoint(HANDLE process, void* address) {
    (void)process; (void)address;
    return false; // Requires debug API — INT3 injection
}

CONTEXT DebugTools::GetThreadContext(HANDLE thread) {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    ::GetThreadContext(thread, &ctx);
    return ctx;
}

bool DebugTools::SetThreadContext(HANDLE thread, const CONTEXT& context) {
    return ::SetThreadContext(thread, &context) != 0;
}

bool DebugTools::CreateMiniDump(
    HANDLE process,
    const std::wstring& dump_path,
    DWORD dump_type
) {
    (void)process; (void)dump_path; (void)dump_type;
    return false; // Requires DbgHelp.dll — MiniDumpWriteDump
}

std::vector<void*> DebugTools::WalkStack(HANDLE thread) {
    (void)thread;
    return {}; // Requires DbgHelp.dll — StackWalk64
}

// ============================================================================
// ThreadingTools Implementation
// ============================================================================

HANDLE ThreadingTools::CreateThreadWithAffinity(
    std::function<DWORD()> thread_func,
    DWORD_PTR affinity_mask
) {
    // Heap-allocate the function to avoid static lifetime issues
    auto* func_ptr = new std::function<DWORD()>(std::move(thread_func));

    HANDLE hThread = ::CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* fn = static_cast<std::function<DWORD()>*>(param);
            DWORD result = (*fn)();
            delete fn;
            return result;
        },
        func_ptr, CREATE_SUSPENDED, nullptr
    );

    if (hThread) {
        SetThreadAffinityMask(hThread, affinity_mask);
        ResumeThread(hThread);
    } else {
        delete func_ptr;
    }
    return hThread;
}

bool ThreadingTools::SetThreadPriority(HANDLE thread, int priority) {
    return ::SetThreadPriority(thread, priority) != 0;
}

HANDLE ThreadingTools::CreateMutexEx(const std::wstring& name, bool initial_owner) {
    return ::CreateMutexW(nullptr, initial_owner ? TRUE : FALSE,
                          name.empty() ? nullptr : name.c_str());
}

HANDLE ThreadingTools::CreateEventEx(const std::wstring& name, bool manual_reset, bool initial_state) {
    return ::CreateEventW(nullptr, manual_reset ? TRUE : FALSE,
                          initial_state ? TRUE : FALSE,
                          name.empty() ? nullptr : name.c_str());
}

HANDLE ThreadingTools::CreateSemaphoreEx(const std::wstring& name, LONG initial_count, LONG maximum_count) {
    return ::CreateSemaphoreW(nullptr, initial_count, maximum_count,
                              name.empty() ? nullptr : name.c_str());
}

DWORD ThreadingTools::WaitForMultiple(
    const std::vector<HANDLE>& handles,
    bool wait_all,
    DWORD timeout_ms
) {
    return WaitForMultipleObjects(
        static_cast<DWORD>(handles.size()),
        handles.data(),
        wait_all ? TRUE : FALSE,
        timeout_ms
    );
}

// ============================================================================
// AgentToolRouter Implementation
// ============================================================================

AgentToolRouter::AgentToolRouter() {
    // Enable all categories by default (debug/registry off for safety)
    category_enabled_[ToolCategory::PROCESS] = true;
    category_enabled_[ToolCategory::FILE_SYSTEM] = true;
    category_enabled_[ToolCategory::REGISTRY] = false;
    category_enabled_[ToolCategory::MEMORY] = true;
    category_enabled_[ToolCategory::IPC] = true;
    category_enabled_[ToolCategory::THREADING] = true;
    category_enabled_[ToolCategory::SYSTEM_INFO] = true;
    category_enabled_[ToolCategory::DEBUG] = false;

    registerBuiltinTools();
}

AgentToolRouter::~AgentToolRouter() = default;

AgentToolRouter::ToolResult AgentToolRouter::executeTool(
    const std::string& tool_name,
    const std::vector<std::string>& args
) {
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) {
        return { false, L"Tool not found: " + std::wstring(tool_name.begin(), tool_name.end()), {} };
    }

    if (!it->second.enabled) {
        return { false, L"Tool disabled: " + std::wstring(tool_name.begin(), tool_name.end()), {} };
    }

    auto cat_it = category_enabled_.find(it->second.category);
    if (cat_it != category_enabled_.end() && !cat_it->second) {
        return { false, L"Tool category disabled", {} };
    }

    return it->second.handler(args);
}

void AgentToolRouter::registerCustomTool(
    const std::string& name,
    ToolCategory category,
    std::function<ToolResult(const std::vector<std::string>&)> handler
) {
    tools_[name] = { name, category, handler, true };
}

std::vector<std::string> AgentToolRouter::getAvailableTools(ToolCategory category) const {
    std::vector<std::string> result;
    for (const auto& [name, def] : tools_) {
        if (def.category == category && def.enabled) {
            result.push_back(name);
        }
    }
    return result;
}

void AgentToolRouter::enableCategory(ToolCategory category, bool enable) {
    category_enabled_[category] = enable;
}

bool AgentToolRouter::isToolAllowed(const std::string& tool_name) const {
    auto it = tools_.find(tool_name);
    if (it == tools_.end()) return false;
    if (!it->second.enabled) return false;
    auto cat_it = category_enabled_.find(it->second.category);
    if (cat_it != category_enabled_.end() && !cat_it->second) return false;
    return true;
}

void AgentToolRouter::registerBuiltinTools() {
    registerCustomTool("enumerate_processes", ToolCategory::PROCESS,
        [](const std::vector<std::string>&) -> ToolResult {
            auto procs = ProcessManager::EnumerateProcesses();
            std::wstring msg = L"Found " + std::to_wstring(procs.size()) + L" processes";
            return { true, msg, {} };
        });

    registerCustomTool("get_memory_status", ToolCategory::MEMORY,
        [](const std::vector<std::string>&) -> ToolResult {
            auto mem = MemoryTools::GetMemoryStatus();
            std::wstring msg = L"Memory load: " + std::to_wstring(mem.dwMemoryLoad) + L"%";
            return { true, msg, {} };
        });

    registerCustomTool("get_system_info", ToolCategory::SYSTEM_INFO,
        [](const std::vector<std::string>&) -> ToolResult {
            auto cpu = SystemInfo::GetCPUInfo();
            return { true, L"CPU: " + cpu.brand + L" (" + std::to_wstring(cpu.cores) + L" cores)", {} };
        });

    registerCustomTool("get_uptime", ToolCategory::SYSTEM_INFO,
        [](const std::vector<std::string>&) -> ToolResult {
            uint64_t ms = SystemInfo::GetSystemUptimeMS();
            uint64_t hours = ms / 3600000;
            return { true, L"Uptime: " + std::to_wstring(hours) + L" hours", {} };
        });

    registerCustomTool("read_file_mapped", ToolCategory::FILE_SYSTEM,
        [](const std::vector<std::string>& args) -> ToolResult {
            if (args.empty()) return { false, L"No path specified", {} };
            std::wstring wpath(args[0].begin(), args[0].end());
            auto data = FileSystemTools::ReadFileMapped(wpath);
            if (data.empty()) return { false, L"Failed to read file", {} };
            return { true, L"Read " + std::to_wstring(data.size()) + L" bytes", data };
        });

    registerCustomTool("is_elevated", ToolCategory::SYSTEM_INFO,
        [](const std::vector<std::string>&) -> ToolResult {
            bool elevated = SystemInfo::IsElevated();
            return { true, elevated ? L"Running elevated" : L"Not elevated", {} };
        });

    registerCustomTool("get_current_user", ToolCategory::SYSTEM_INFO,
        [](const std::vector<std::string>&) -> ToolResult {
            return { true, SystemInfo::GetCurrentUser(), {} };
        });

    registerCustomTool("get_computer_name", ToolCategory::SYSTEM_INFO,
        [](const std::vector<std::string>&) -> ToolResult {
            return { true, SystemInfo::GetComputerName(), {} };
        });
}

} // namespace agent
} // namespace rawrxd
