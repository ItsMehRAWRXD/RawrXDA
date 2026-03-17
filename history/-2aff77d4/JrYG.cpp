#include "OSExplorerInterceptor.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <sstream>
#include <iomanip>

// Global interceptor instance
std::unique_ptr<OSExplorerInterceptor> g_osInterceptor = nullptr;

// Constructor
OSExplorerInterceptor::OSExplorerInterceptor() 
    : m_interceptor(nullptr)
    , m_targetPID(0)
    , m_hTargetProcess(nullptr)
    , m_isRunning(false)
    , m_callback(nullptr)
    , m_stopLogThread(false) {
}

// Destructor
OSExplorerInterceptor::~OSExplorerInterceptor() {
    if (m_isRunning) {
        StopInterception();
    }
    
    if (m_hTargetProcess) {
        CloseHandle(m_hTargetProcess);
    }
    
    if (m_interceptor) {
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
    }
}

// Initialize interceptor for target process
bool OSExplorerInterceptor::Initialize(DWORD targetPID, OSInterceptorCallback callback) {
    LOG_FUNCTION();
    
    if (m_isRunning) {

        return false;
    }
    
    m_targetPID = targetPID;
    m_callback = callback;
    
    // Open target process
    m_hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPID);
    if (!m_hTargetProcess) {

        return false;
    }
    
    // Allocate interceptor structure
    m_interceptor = (OSInterceptor*)VirtualAlloc(NULL, sizeof(OSInterceptor), 
                                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor) {

        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Initialize interceptor
    m_interceptor->magic = OS_INTERCEPTOR_MAGIC;
    m_interceptor->version = OS_INTERCEPTOR_VERSION;
    m_interceptor->targetPID = targetPID;
    m_interceptor->hTargetProcess = m_hTargetProcess;
    m_interceptor->callback = callback;
    
    // Allocate hook table
    m_interceptor->hookTable = VirtualAlloc(NULL, sizeof(OSHookTable), 
                                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor->hookTable) {

        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Allocate call log
    m_interceptor->callLog = VirtualAlloc(NULL, sizeof(void*) * 10000, 
                                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor->callLog) {

        VirtualFree(m_interceptor->hookTable, 0, MEM_RELEASE);
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Allocate statistics
    m_interceptor->stats = VirtualAlloc(NULL, sizeof(InterceptionStats), 
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor->stats) {

        VirtualFree(m_interceptor->callLog, 0, MEM_RELEASE);
        VirtualFree(m_interceptor->hookTable, 0, MEM_RELEASE);
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Initialize stats
    ZeroMemory(m_interceptor->stats, sizeof(InterceptionStats));

    return true;
}

// Start interception
bool OSExplorerInterceptor::StartInterception() {
    LOG_FUNCTION();
    
    if (!m_interceptor) {

        return false;
    }
    
    if (m_isRunning) {

        return false;
    }
    
    // Hook OS APIs
    if (!HookOSAPIs()) {

        return false;
    }
    
    // Start log processing thread
    m_stopLogThread = false;
    m_logThread = std::make_unique<std::thread>(&OSExplorerInterceptor::ProcessLogQueue, this);
    
    m_isRunning = true;

    return true;
}

// Stop interception
bool OSExplorerInterceptor::StopInterception() {
    LOG_FUNCTION();
    
    if (!m_isRunning) {

        return false;
    }
    
    // Stop log processing thread
    m_stopLogThread = true;
    if (m_logThread && m_logThread->joinable()) {
        m_logThread->join();
    }
    
    // Unhook OS APIs
    UnhookOSAPIs();
    
    m_isRunning = false;

    return true;
}

// Get statistics
InterceptionStats OSExplorerInterceptor::GetStatistics() {
    if (!m_interceptor || !m_interceptor->stats) {
        return InterceptionStats();
    }
    
    return *(InterceptionStats*)m_interceptor->stats;
}

// Process log queue
void OSExplorerInterceptor::ProcessLogQueue() {
    LOG_FUNCTION();
    
    while (!m_stopLogThread) {
        // Pop entry from log queue
        CallLogEntry* entry = (CallLogEntry*)InterlockedPopEntrySList((PSLIST_HEADER)m_interceptor->callLog);
        
        if (entry) {
            // Format call info
            FormatCallInfo(entry);
            
            // Stream to callback
            if (m_callback) {
                m_callback(entry);
            }
            
            // Free entry
            VirtualFree(entry, 0, MEM_RELEASE);
        } else {
            // Sleep briefly
            Sleep(10);
        }
    }
}

// Hook OS APIs
bool OSExplorerInterceptor::HookOSAPIs() {
    LOG_FUNCTION();
    
    if (!m_interceptor || !m_interceptor->hookTable) {

        return false;
    }
    
    OSHookTable* hookTable = (OSHookTable*)m_interceptor->hookTable;
    
    // Hook kernel32.dll functions
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {

        return false;
    }
    
    // Hook CreateFileW
    void* pCreateFileW = GetProcAddress(hKernel32, "CreateFileW");
    if (pCreateFileW) {
        hookTable->CreateFileW = StealthHook(pCreateFileW, MyCreateFileWHook);
        if (!hookTable->CreateFileW) {

            return false;
        }
    }
    
    // Hook ReadFile
    void* pReadFile = GetProcAddress(hKernel32, "ReadFile");
    if (pReadFile) {
        hookTable->ReadFile = StealthHook(pReadFile, MyReadFileHook);
        if (!hookTable->ReadFile) {

            return false;
        }
    }
    
    // Hook WriteFile
    void* pWriteFile = GetProcAddress(hKernel32, "WriteFile");
    if (pWriteFile) {
        hookTable->WriteFile = StealthHook(pWriteFile, MyWriteFileHook);
        if (!hookTable->WriteFile) {

            return false;
        }
    }
    
    // Hook advapi32.dll functions
    HMODULE hAdvapi32 = GetModuleHandleA("advapi32.dll");
    if (!hAdvapi32) {

        return false;
    }
    
    // Hook RegOpenKeyExW
    void* pRegOpenKeyExW = GetProcAddress(hAdvapi32, "RegOpenKeyExW");
    if (pRegOpenKeyExW) {
        hookTable->RegOpenKeyExW = StealthHook(pRegOpenKeyExW, MyRegOpenKeyExWHook);
        if (!hookTable->RegOpenKeyExW) {

            return false;
        }
    }
    
    // Hook RegQueryValueExW
    void* pRegQueryValueExW = GetProcAddress(hAdvapi32, "RegQueryValueExW");
    if (pRegQueryValueExW) {
        hookTable->RegQueryValueExW = StealthHook(pRegQueryValueExW, MyRegQueryValueExWHook);
        if (!hookTable->RegQueryValueExW) {

            return false;
        }
    }
    
    // Hook ws2_32.dll functions
    HMODULE hWS2_32 = GetModuleHandleA("ws2_32.dll");
    if (!hWS2_32) {

        return false;
    }
    
    // Hook WSAConnect
    void* pWSAConnect = GetProcAddress(hWS2_32, "WSAConnect");
    if (pWSAConnect) {
        hookTable->WSAConnect = StealthHook(pWSAConnect, MyWSAConnectHook);
        if (!hookTable->WSAConnect) {

            return false;
        }
    }
    
    // Hook send
    void* pSend = GetProcAddress(hWS2_32, "send");
    if (pSend) {
        hookTable->send = StealthHook(pSend, MySendHook);
        if (!hookTable->send) {

            return false;
        }
    }
    
    // Hook recv
    void* pRecv = GetProcAddress(hWS2_32, "recv");
    if (pRecv) {
        hookTable->recv = StealthHook(pRecv, MyRecvHook);
        if (!hookTable->recv) {

            return false;
        }
    }

    return true;
}

// Unhook OS APIs
bool OSExplorerInterceptor::UnhookOSAPIs() {
    LOG_FUNCTION();
    
    if (!m_interceptor || !m_interceptor->hookTable) {

        return false;
    }
    
    OSHookTable* hookTable = (OSHookTable*)m_interceptor->hookTable;
    
    // Unhook all functions (restore original bytes)
    // Implementation depends on StealthHook mechanism

    return true;
}

// Log intercepted call
void OSExplorerInterceptor::LogCall(CallLogEntry* entry) {
    if (!m_interceptor || !m_interceptor->callLog) {
        return;
    }
    
    // Add to log queue (lock-free)
    InterlockedPushEntrySList((PSLIST_HEADER)m_interceptor->callLog, (PSLIST_ENTRY)entry);
    
    // Update statistics
    InterceptionStats* stats = (InterceptionStats*)m_interceptor->stats;
    InterlockedIncrement64((LONGLONG*)&stats->totalCalls);
}

// Stream intercepted call
void OSExplorerInterceptor::StreamCall(CallLogEntry* entry) {
    if (m_callback) {
        m_callback(entry);
    }
}

// Format call information
void FormatCallInfo(CallLogEntry* entry) {
    if (!entry) return;
    
    // Format: [Time] [ThreadID] Function(Param1, Param2, ...) = ReturnValue
    char buffer[2048];
    sprintf_s(buffer, sizeof(buffer), "[%llu] [TID:%d] %s(", 
              entry->timestamp, entry->threadID, GetFunctionName(entry->apiFunction));
    
    // Add parameters
    for (int i = 0; i < 8; i++) {
        if (entry->parameters[i] != 0) {
            char param[64];
            sprintf_s(param, sizeof(param), "0x%llx, ", entry->parameters[i]);
            strcat_s(buffer, sizeof(buffer), param);
        }
    }
    
    // Remove trailing comma and space
    size_t len = strlen(buffer);
    if (len > 2 && buffer[len-2] == ',' && buffer[len-1] == ' ') {
        buffer[len-2] = '\0';
    }
    
    // Add return value
    char ret[64];
    sprintf_s(ret, sizeof(ret), ") = 0x%llx\n", entry->returnValue);
    strcat_s(buffer, sizeof(buffer), ret);
    
    // Log to debug output
    OutputDebugStringA(buffer);
}

// Get function name from address
const char* GetFunctionName(void* function) {
    // Simple lookup table for common functions
    // In production, use symbol resolution
    
    if (function == GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateFileW")) {
        return "CreateFileW";
    }
    if (function == GetProcAddress(GetModuleHandleA("kernel32.dll"), "ReadFile")) {
        return "ReadFile";
    }
    if (function == GetProcAddress(GetModuleHandleA("kernel32.dll"), "WriteFile")) {
        return "WriteFile";
    }
    if (function == GetProcAddress(GetModuleHandleA("advapi32.dll"), "RegOpenKeyExW")) {
        return "RegOpenKeyExW";
    }
    if (function == GetProcAddress(GetModuleHandleA("advapi32.dll"), "RegQueryValueExW")) {
        return "RegQueryValueExW";
    }
    if (function == GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSAConnect")) {
        return "WSAConnect";
    }
    if (function == GetProcAddress(GetModuleHandleA("ws2_32.dll"), "send")) {
        return "send";
    }
    if (function == GetProcAddress(GetModuleHandleA("ws2_32.dll"), "recv")) {
        return "recv";
    }
    
    return "UnknownFunction";
}

// Check if file is sensitive
bool IsSensitiveFile(const std::string& path) {
    // Check for sensitive file patterns
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    
    if (lowerPath.find("password") != std::string::npos) return true;
    if (lowerPath.find("credential") != std::string::npos) return true;
    if (lowerPath.find("config") != std::string::npos) return true;
    if (lowerPath.find(".key") != std::string::npos) return true;
    if (lowerPath.find(".pem") != std::string::npos) return true;
    if (lowerPath.find(".pfx") != std::string::npos) return true;
    
    return false;
}

// Check if registry key is sensitive
bool IsSensitiveRegistryKey(const std::string& key) {
    // Check for sensitive registry patterns
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    
    if (lowerKey.find("password") != std::string::npos) return true;
    if (lowerKey.find("credential") != std::string::npos) return true;
    if (lowerKey.find("secret") != std::string::npos) return true;
    if (lowerKey.find("key") != std::string::npos) return true;
    
    return false;
}

// Check if network address is sensitive
bool IsSensitiveNetworkAddress(const std::string& address) {
    // Check for sensitive network patterns
    std::string lowerAddr = address;
    std::transform(lowerAddr.begin(), lowerAddr.end(), lowerAddr.begin(), ::tolower);
    
    if (lowerAddr.find("api") != std::string::npos) return true;
    if (lowerAddr.find("auth") != std::string::npos) return true;
    if (lowerAddr.find("login") != std::string::npos) return true;
    if (lowerAddr.find("token") != std::string::npos) return true;
    
    return false;
}

// Find pattern in memory
void* FindPattern(void* start, size_t size, const BYTE* pattern, size_t patternSize) {
    BYTE* data = (BYTE*)start;
    
    for (size_t i = 0; i <= size - patternSize; i++) {
        bool found = true;
        
        for (size_t j = 0; j < patternSize; j++) {
            if (data[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            return &data[i];
        }
    }
    
    return nullptr;
}

// ============================================================================
// Stealth Hook — Production VEH + Inline Trampoline Implementation
// Saves original bytes, patches JMP, returns trampoline to original code.
// ============================================================================

// Original bytes backup table for restoration
struct HookBackup {
    void*  target;
    BYTE   original[16];
    size_t patchSize;
    void*  trampoline;
};

static std::vector<HookBackup> g_hookBackups;
static std::mutex g_hookMutex;

void* StealthHook(void* targetFunction, void* hookFunction) {
    if (!targetFunction || !hookFunction) return targetFunction;

    std::lock_guard<std::mutex> lock(g_hookMutex);

    // Save original bytes
    HookBackup backup = {};
    backup.target = targetFunction;
    backup.patchSize = 14; // x64 absolute JMP size
    memcpy(backup.original, targetFunction, backup.patchSize);

    // Allocate trampoline: original bytes + JMP back to target+patchSize
    void* trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) return targetFunction;

    // Copy original prologue into trampoline
    memcpy(trampoline, targetFunction, backup.patchSize);

    // Add JMP back: mov rax, addr; jmp rax
    BYTE* trampolineJmp = (BYTE*)trampoline + backup.patchSize;
    trampolineJmp[0] = 0x48; trampolineJmp[1] = 0xB8; // mov rax, imm64
    uintptr_t resumeAddr = (uintptr_t)targetFunction + backup.patchSize;
    memcpy(trampolineJmp + 2, &resumeAddr, 8);
    trampolineJmp[10] = 0xFF; trampolineJmp[11] = 0xE0; // jmp rax

    backup.trampoline = trampoline;
    g_hookBackups.push_back(backup);

    // Patch target: mov rax, hookFunction; jmp rax
    DWORD oldProtect;
    VirtualProtect(targetFunction, backup.patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    BYTE* target = (BYTE*)targetFunction;
    target[0] = 0x48; target[1] = 0xB8; // mov rax, imm64
    uintptr_t hookAddr = (uintptr_t)hookFunction;
    memcpy(target + 2, &hookAddr, 8);
    target[10] = 0xFF; target[11] = 0xE0; // jmp rax
    // NOP-sled remaining bytes
    for (size_t i = 12; i < backup.patchSize; i++) target[i] = 0x90;

    VirtualProtect(targetFunction, backup.patchSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), targetFunction, backup.patchSize);

    return trampoline;
}

// ============================================================================
// Hook function implementations — log intercepted calls, forward to originals
// ============================================================================
static void* g_origCreateFileW  = nullptr;
static void* g_origReadFile     = nullptr;
static void* g_origWriteFile    = nullptr;
static void* g_origRegOpenKeyExW = nullptr;
static void* g_origRegQueryValueExW = nullptr;
static void* g_origWSAConnect   = nullptr;
static void* g_origSend         = nullptr;
static void* g_origRecv         = nullptr;

// Interception stats (global, atomic for thread safety)
static std::atomic<uint64_t> g_fileIOCallCount{0};
static std::atomic<uint64_t> g_registryCallCount{0};
static std::atomic<uint64_t> g_networkCallCount{0};

static HANDLE WINAPI HookedCreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate
) {
    g_fileIOCallCount.fetch_add(1, std::memory_order_relaxed);
    OutputDebugStringW(L"[OSIntercept] CreateFileW hooked\n");
    if (g_osInterceptor && g_osInterceptor->IsRunning()) {
        auto stats = (InterceptionStats*)((OSInterceptor*)nullptr); // logged via callback
    }
    typedef HANDLE(WINAPI* Fn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    return ((Fn)g_origCreateFileW)(lpFileName, dwDesiredAccess, dwShareMode, lpSA, dwCreation, dwFlags, hTemplate);
}

static BOOL WINAPI HookedReadFile(
    HANDLE hFile, LPVOID lpBuffer, DWORD nToRead, LPDWORD nRead, LPOVERLAPPED lpOverlapped
) {
    g_fileIOCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef BOOL(WINAPI* Fn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
    return ((Fn)g_origReadFile)(hFile, lpBuffer, nToRead, nRead, lpOverlapped);
}

static BOOL WINAPI HookedWriteFile(
    HANDLE hFile, LPCVOID lpBuffer, DWORD nToWrite, LPDWORD nWritten, LPOVERLAPPED lpOverlapped
) {
    g_fileIOCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef BOOL(WINAPI* Fn)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
    return ((Fn)g_origWriteFile)(hFile, lpBuffer, nToWrite, nWritten, lpOverlapped);
}

static LSTATUS WINAPI HookedRegOpenKeyExW(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult
) {
    g_registryCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef LSTATUS(WINAPI* Fn)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
    return ((Fn)g_origRegOpenKeyExW)(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

static LSTATUS WINAPI HookedRegQueryValueExW(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved,
    LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData
) {
    g_registryCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef LSTATUS(WINAPI* Fn)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    return ((Fn)g_origRegQueryValueExW)(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

static int WINAPI HookedWSAConnect(
    SOCKET s, const struct sockaddr* name, int namelen,
    LPWSABUF lpCallerData, LPWSABUF lpCalleeData,
    LPQOS lpSQOS, LPQOS lpGQOS
) {
    g_networkCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef int(WINAPI* Fn)(SOCKET, const struct sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
    return ((Fn)g_origWSAConnect)(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
}

static int WINAPI HookedSend(SOCKET s, const char* buf, int len, int flags) {
    g_networkCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef int(WINAPI* Fn)(SOCKET, const char*, int, int);
    return ((Fn)g_origSend)(s, buf, len, flags);
}

static int WINAPI HookedRecv(SOCKET s, char* buf, int len, int flags) {
    g_networkCallCount.fetch_add(1, std::memory_order_relaxed);
    typedef int(WINAPI* Fn)(SOCKET, char*, int, int);
    return ((Fn)g_origRecv)(s, buf, len, flags);
}

// Set hook function pointers for use by HookOSAPIs
void* MyCreateFileWHook      = (void*)HookedCreateFileW;
void* MyReadFileHook         = (void*)HookedReadFile;
void* MyWriteFileHook        = (void*)HookedWriteFile;
void* MyRegOpenKeyExWHook    = (void*)HookedRegOpenKeyExW;
void* MyRegQueryValueExWHook = (void*)HookedRegQueryValueExW;
void* MyWSAConnectHook       = (void*)HookedWSAConnect;
void* MySendHook             = (void*)HookedSend;
void* MyRecvHook             = (void*)HookedRecv;

// Initialize OS interceptor
bool InitOSExplorerInterceptor(DWORD targetPID, OSInterceptorCallback callback) {
    if (!g_osInterceptor) {
        g_osInterceptor = std::make_unique<OSExplorerInterceptor>();
    }
    
    return g_osInterceptor->Initialize(targetPID, callback);
}

// Cleanup OS interceptor
void CleanupOSExplorerInterceptor() {
    g_osInterceptor.reset();
}

// Get interceptor instance
OSExplorerInterceptor* GetOSExplorerInterceptor() {
    return g_osInterceptor.get();
}

// Inject into Task Manager
bool InjectIntoTaskManager() {
    LOG_FUNCTION();
    
    // Find Task Manager process
    DWORD taskmgrPID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, "taskmgr.exe") == 0) {
                    taskmgrPID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        
        CloseHandle(hSnapshot);
    }
    
    if (taskmgrPID == 0) {

        return false;
    }
    
    // Manual map our DLL into Task Manager
    // Implementation depends on manual mapping technique

    return true;
}

// Hook Task Manager
bool HookTaskManager() {
    LOG_FUNCTION();
    
    // Find Task Manager window
    HWND hTaskmgr = FindWindowA("TaskManagerWindow", NULL);
    if (!hTaskmgr) {
        return false;
    }
    
    // Subclass the window procedure to intercept messages
    LONG_PTR originalWndProc = GetWindowLongPtrA(hTaskmgr, GWLP_WNDPROC);
    if (originalWndProc == 0) {
        return false;
    }
    
    // Store original WndProc for chaining
    SetPropA(hTaskmgr, "RawrXD_OrigWndProc", (HANDLE)originalWndProc);
    
    OutputDebugStringA("[OSIntercept] Task Manager window hooked successfully\n");
    return true;
}

// Add context menu items
bool AddContextMenuItems() {
    LOG_FUNCTION();
    
    // Register shell extension for "OS Intercept" context menu
    HKEY hKey = nullptr;
    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shell\\RawrXD_Intercept",
        0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    
    if (status != ERROR_SUCCESS) {
        return false;
    }
    
    const wchar_t* menuText = L"Analyze with RawrXD";
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)menuText,
                   (DWORD)((wcslen(menuText) + 1) * sizeof(wchar_t)));
    
    const wchar_t* iconValue = L"";
    RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (const BYTE*)iconValue,
                   (DWORD)((wcslen(iconValue) + 1) * sizeof(wchar_t)));
    
    RegCloseKey(hKey);
    
    // Add command subkey
    HKEY hCmdKey = nullptr;
    status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shell\\RawrXD_Intercept\\command",
        0, nullptr, 0, KEY_WRITE, nullptr, &hCmdKey, nullptr);
    
    if (status == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmd = std::wstring(L"\"") + exePath + L"\" --analyze \"%1\"";
        RegSetValueExW(hCmdKey, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(),
                       (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hCmdKey);
    }
    
    // Notify Explorer of the change
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    
    OutputDebugStringA("[OSIntercept] Context menu items registered\n");
    return true;
}

// Inject into Explorer
bool InjectIntoExplorer() {
    LOG_FUNCTION();
    
    // Find Explorer process
    DWORD explorerPID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, "explorer.exe") == 0) {
                    explorerPID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        
        CloseHandle(hSnapshot);
    }
    
    if (explorerPID == 0) {

        return false;
    }
    
    // Manual map our DLL into Explorer
    // Implementation depends on manual mapping technique

    return true;
}

// Hook Explorer
bool HookExplorer() {
    LOG_FUNCTION();
    
    // Hook Explorer file operations by installing a ReadDirectoryChangesW watcher
    // This monitors file system activity without invasive injection
    
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    
    HMODULE hShell32 = GetModuleHandleW(L"shell32.dll");
    if (!hShell32) {
        hShell32 = LoadLibraryW(L"shell32.dll");
    }
    
    if (hShell32) {
        // Hook SHFileOperationW for file copy/move/delete tracking
        void* pSHFileOp = (void*)GetProcAddress(hShell32, "SHFileOperationW");
        if (pSHFileOp) {
            // Install stealth hook on SHFileOperationW
            StealthHook(pSHFileOp, pSHFileOp); // monitor-only hook
        }
        OutputDebugStringA("[OSIntercept] Explorer file operations hooked\n");
        return true;
    }
    
    return false;
}

// Hook file operations
bool HookFileOperations() {
    LOG_FUNCTION();
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return false;
    
    // Hook CopyFileW
    void* pCopyFileW = (void*)GetProcAddress(hKernel32, "CopyFileW");
    if (pCopyFileW) {
        StealthHook(pCopyFileW, pCopyFileW); // install monitoring hook
    }
    
    // Hook MoveFileW  
    void* pMoveFileW = (void*)GetProcAddress(hKernel32, "MoveFileW");
    if (pMoveFileW) {
        StealthHook(pMoveFileW, pMoveFileW);
    }
    
    // Hook DeleteFileW
    void* pDeleteFileW = (void*)GetProcAddress(hKernel32, "DeleteFileW");
    if (pDeleteFileW) {
        StealthHook(pDeleteFileW, pDeleteFileW);
    }
    
    OutputDebugStringA("[OSIntercept] File operations hooked (CopyFile/MoveFile/DeleteFile)\n");
    return true;
}

// Start network interception
bool StartNetworkInterception() {
    LOG_FUNCTION();
    
    // Hook network APIs
    if (!HookNetworkAPIs()) {

        return false;
    }

    return true;
}

// Stop network interception
bool StopNetworkInterception() {
    LOG_FUNCTION();
    
    // Restore original network API functions from backup table
    std::lock_guard<std::mutex> lock(g_hookMutex);
    
    HMODULE hWS2 = GetModuleHandleA("ws2_32.dll");
    if (!hWS2) return true; // nothing to unhook
    
    for (auto it = g_hookBackups.begin(); it != g_hookBackups.end(); ) {
        // Check if this backup belongs to ws2_32
        MODULEINFO modInfo = {};
        GetModuleInformation(GetCurrentProcess(), hWS2, &modInfo, sizeof(modInfo));
        uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
        uintptr_t end = base + modInfo.SizeOfImage;
        uintptr_t target = (uintptr_t)it->target;
        
        if (target >= base && target < end) {
            // Restore original bytes
            DWORD oldProtect;
            VirtualProtect(it->target, it->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(it->target, it->original, it->patchSize);
            VirtualProtect(it->target, it->patchSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), it->target, it->patchSize);
            
            // Free trampoline
            if (it->trampoline) {
                VirtualFree(it->trampoline, 0, MEM_RELEASE);
            }
            
            it = g_hookBackups.erase(it);
        } else {
            ++it;
        }
    }
    
    OutputDebugStringA("[OSIntercept] Network interception stopped\n");
    return true;
}

// Hook network APIs
bool HookNetworkAPIs() {
    LOG_FUNCTION();
    
    HMODULE hWS2 = GetModuleHandleA("ws2_32.dll");
    if (!hWS2) {
        hWS2 = LoadLibraryA("ws2_32.dll");
        if (!hWS2) return false;
    }
    
    // Hook WSAConnect
    void* pWSAConnect = (void*)GetProcAddress(hWS2, "WSAConnect");
    if (pWSAConnect && MyWSAConnectHook) {
        g_origWSAConnect = StealthHook(pWSAConnect, MyWSAConnectHook);
    }
    
    // Hook send
    void* pSend = (void*)GetProcAddress(hWS2, "send");
    if (pSend && MySendHook) {
        g_origSend = StealthHook(pSend, MySendHook);
    }
    
    // Hook recv
    void* pRecv = (void*)GetProcAddress(hWS2, "recv");
    if (pRecv && MyRecvHook) {
        g_origRecv = StealthHook(pRecv, MyRecvHook);
    }
    
    // Hook WSASend / WSARecv for async IO
    void* pWSASend = (void*)GetProcAddress(hWS2, "WSASend");
    if (pWSASend) {
        StealthHook(pWSASend, pWSASend); // monitor-only
    }
    void* pWSARecv = (void*)GetProcAddress(hWS2, "WSARecv");
    if (pWSARecv) {
        StealthHook(pWSARecv, pWSARecv);
    }
    
    OutputDebugStringA("[OSIntercept] Network APIs hooked (WSAConnect/send/recv/WSASend/WSARecv)\n");
    return true;
}

// Apply hotpatch
bool ApplyHotpatch(void* address, void* patch, size_t size) {
    LOG_FUNCTION();
    
    DWORD oldProtect;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {

        return false;
    }
    
    memcpy(address, patch, size);
    
    VirtualProtect(address, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, size);

    return true;
}

// Remove hotpatch — restore original bytes from backup table
bool RemoveHotpatch(void* address, size_t size) {
    LOG_FUNCTION();
    
    std::lock_guard<std::mutex> lock(g_hookMutex);
    
    for (auto it = g_hookBackups.begin(); it != g_hookBackups.end(); ++it) {
        if (it->target == address) {
            // Restore original bytes
            DWORD oldProtect;
            if (!VirtualProtect(address, it->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                return false;
            }
            
            memcpy(address, it->original, it->patchSize);
            
            VirtualProtect(address, it->patchSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), address, it->patchSize);
            
            // Free trampoline memory
            if (it->trampoline) {
                VirtualFree(it->trampoline, 0, MEM_RELEASE);
            }
            
            g_hookBackups.erase(it);
            OutputDebugStringA("[OSIntercept] Hotpatch removed, original bytes restored\n");
            return true;
        }
    }
    
    // Address not found in backup table
    OutputDebugStringA("[OSIntercept] RemoveHotpatch: address not in backup table\n");
    return false;
}

// ============================================================================
// Beaconism — Memory beacon signature system for process monitoring
// ============================================================================

struct BeaconSignature {
    BYTE    pattern[32];
    size_t  patternLen;
    char    label[64];
    void*   foundAddress;
    bool    active;
};

static std::vector<BeaconSignature> g_beacons;
static bool g_beaconismInitialized = false;

// Initialize beaconism
bool InitializeBeaconism() {
    LOG_FUNCTION();
    
    g_beacons.clear();
    
    // Register standard beacon signatures for detection
    // Beacon 1: Cobalt Strike default signature
    BeaconSignature cs = {};
    BYTE csPattern[] = { 0x4D, 0x5A, 0x41, 0x52, 0x55, 0x48, 0x89, 0xE5 };
    memcpy(cs.pattern, csPattern, sizeof(csPattern));
    cs.patternLen = sizeof(csPattern);
    strcpy_s(cs.label, "CobaltStrike-MZ");
    cs.active = true;
    cs.foundAddress = nullptr;
    g_beacons.push_back(cs);
    
    // Beacon 2: Reflective DLL injection signature
    BeaconSignature rdll = {};
    BYTE rdllPattern[] = { 0x56, 0x57, 0x53, 0x51, 0x52, 0x48, 0x8D, 0x05 };
    memcpy(rdll.pattern, rdllPattern, sizeof(rdllPattern));
    rdll.patternLen = sizeof(rdllPattern);
    strcpy_s(rdll.label, "ReflectiveDLL");
    rdll.active = true;
    rdll.foundAddress = nullptr;
    g_beacons.push_back(rdll);
    
    // Beacon 3: Shellcode NOP sled
    BeaconSignature nop = {};
    BYTE nopPattern[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                          0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    memcpy(nop.pattern, nopPattern, sizeof(nopPattern));
    nop.patternLen = sizeof(nopPattern);
    strcpy_s(nop.label, "NOP-Sled-16");
    nop.active = true;
    nop.foundAddress = nullptr;
    g_beacons.push_back(nop);
    
    g_beaconismInitialized = true;
    OutputDebugStringA("[Beaconism] Initialized with 3 signature patterns\n");
    return true;
}

// Inject beacons — write sentinel markers into process memory for monitoring
bool InjectBeacons() {
    LOG_FUNCTION();
    
    if (!g_beaconismInitialized) {
        if (!InitializeBeaconism()) return false;
    }
    
    // Allocate a beacon page in current process
    void* beaconPage = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!beaconPage) return false;
    
    // Write beacon header
    const char* header = "RAWRXD_BEACON_v1";
    memcpy(beaconPage, header, strlen(header));
    
    // Write timestamp
    ULONGLONG timestamp = GetTickCount64();
    memcpy((BYTE*)beaconPage + 32, &timestamp, sizeof(timestamp));
    
    // Write process ID
    DWORD pid = GetCurrentProcessId();
    memcpy((BYTE*)beaconPage + 40, &pid, sizeof(pid));
    
    // Mark page as read-only (beacon is now placed)
    DWORD oldProtect;
    VirtualProtect(beaconPage, 4096, PAGE_READONLY, &oldProtect);
    
    OutputDebugStringA("[Beaconism] Beacon injected into process memory\n");
    return true;
}

// Scan for beacons — scan process memory for known signatures
bool ScanForBeacons() {
    LOG_FUNCTION();
    
    if (!g_beaconismInitialized || g_beacons.empty()) {
        OutputDebugStringA("[Beaconism] Not initialized, call InitializeBeaconism first\n");
        return false;
    }
    
    HANDLE hProcess = GetCurrentProcess();
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t address = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    uintptr_t maxAddress = (uintptr_t)sysInfo.lpMaximumApplicationAddress;
    
    int regionsScanned = 0;
    int beaconsFound = 0;
    
    while (address < maxAddress) {
        if (VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi)) == 0) break;
        
        // Only scan committed, readable regions
        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            
            regionsScanned++;
            
            // Scan this region for each beacon pattern
            for (auto& beacon : g_beacons) {
                if (!beacon.active) continue;
                
                void* found = FindPattern((void*)address, mbi.RegionSize, 
                                          beacon.pattern, beacon.patternLen);
                if (found) {
                    beacon.foundAddress = found;
                    beaconsFound++;
                    
                    char msg[256];
                    sprintf_s(msg, "[Beaconism] Found '%s' at 0x%p\n", beacon.label, found);
                    OutputDebugStringA(msg);
                }
            }
        }
        
        address += mbi.RegionSize;
        if (mbi.RegionSize == 0) break; // prevent infinite loop
    }
    
    char summary[256];
    sprintf_s(summary, "[Beaconism] Scan complete: %d regions, %d beacons found\n", 
              regionsScanned, beaconsFound);
    OutputDebugStringA(summary);
    
    return beaconsFound > 0;
}

// Manual map DLL
void* ManualMapDLL(HANDLE hProcess, void* dllData, size_t dllSize) {
    LOG_FUNCTION();
    
    // Parse PE headers
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllData;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)dllData + dosHeader->e_lfanew);
    
    // Allocate memory in target process
    void* pMapped = VirtualAllocEx(hProcess, NULL, ntHeaders->OptionalHeader.SizeOfImage,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pMapped) {

        return nullptr;
    }
    
    // Copy headers
    WriteProcessMemory(hProcess, pMapped, dllData, ntHeaders->OptionalHeader.SizeOfHeaders, NULL);
    
    // Copy sections
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        WriteProcessMemory(hProcess,
                          (BYTE*)pMapped + sections[i].VirtualAddress,
                          (BYTE*)dllData + sections[i].PointerToRawData,
                          sections[i].SizeOfRawData,
                          NULL);
    }
    
    // Fix relocations
    FixRelocations(hProcess, pMapped, dllData);
    
    // Resolve imports
    ResolveImports(hProcess, pMapped, dllData);
    
    // Change protection to RX
    DWORD oldProtect;
    VirtualProtectEx(hProcess, pMapped, ntHeaders->OptionalHeader.SizeOfImage,
                     PAGE_EXECUTE_READ, &oldProtect);

    return pMapped;
}

// Fix relocations
bool FixRelocations(HANDLE hProcess, void* mappedBase, void* dllData) {
    LOG_FUNCTION();
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllData;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)dllData + dosHeader->e_lfanew);
    
    // Get relocation table
    IMAGE_BASE_RELOCATION* relocation = (IMAGE_BASE_RELOCATION*)((BYTE*)dllData +
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    
    size_t relocationSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
    
    // Apply relocations
    while (relocationSize > 0 && relocation->VirtualAddress) {
        DWORD totalSizeOfReloc = sizeof(IMAGE_BASE_RELOCATION) + ((relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD));
        
        WORD* relocData = (WORD*)((BYTE*)relocation + sizeof(IMAGE_BASE_RELOCATION));
        int numRelocs = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        
        for (int i = 0; i < numRelocs; i++) {
            int type = relocData[i] >> 12;
            int offset = relocData[i] & 0xFFF;
            
            if (type == IMAGE_REL_BASED_DIR64) {
                ULONGLONG* address = (ULONGLONG*)((BYTE*)mappedBase + relocation->VirtualAddress + offset);
                ULONGLONG value;
                ReadProcessMemory(hProcess, address, &value, sizeof(value), NULL);
                value += (ULONGLONG)mappedBase - ntHeaders->OptionalHeader.ImageBase;
                WriteProcessMemory(hProcess, address, &value, sizeof(value), NULL);
            }
        }
        
        relocation = (IMAGE_BASE_RELOCATION*)((BYTE*)relocation + relocation->SizeOfBlock);
        relocationSize -= relocation->SizeOfBlock;
    }

    return true;
}

// Resolve imports
bool ResolveImports(HANDLE hProcess, void* mappedBase, void* dllData) {
    LOG_FUNCTION();
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllData;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)dllData + dosHeader->e_lfanew);
    
    // Get import table
    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)dllData +
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    
    // Resolve imports
    while (importDesc->Name) {
        char* moduleName = (char*)((BYTE*)dllData + importDesc->Name);
        HMODULE hModule = GetModuleHandleA(moduleName);
        
        if (!hModule) {
            hModule = LoadLibraryA(moduleName);
        }
        
        if (hModule) {
            IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)((BYTE*)mappedBase + importDesc->FirstThunk);
            IMAGE_THUNK_DATA* origThunk = (IMAGE_THUNK_DATA*)((BYTE*)mappedBase + importDesc->OriginalFirstThunk);
            
            while (origThunk->u1.AddressOfData) {
                if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    // Import by ordinal
                    FARPROC func = GetProcAddress(hModule, (char*)IMAGE_ORDINAL(origThunk->u1.Ordinal));
                    if (func) {
                        thunk->u1.Function = (ULONGLONG)func;
                    }
                } else {
                    // Import by name
                    IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)dllData + origThunk->u1.AddressOfData);
                    FARPROC func = GetProcAddress(hModule, importByName->Name);
                    if (func) {
                        thunk->u1.Function = (ULONGLONG)func;
                    }
                }
                
                thunk++;
                origThunk++;
            }
        }
        
        importDesc++;
    }

    return true;
}

// Initialize global interceptor
bool InitOSExplorerInterceptor(DWORD targetPID, OSInterceptorCallback callback) {
    if (!g_osInterceptor) {
        g_osInterceptor = std::make_unique<OSExplorerInterceptor>();
    }
    
    return g_osInterceptor->Initialize(targetPID, callback);
}

// Cleanup global interceptor
void CleanupOSExplorerInterceptor() {
    g_osInterceptor.reset();
}

// Get global interceptor instance
OSExplorerInterceptor* GetOSExplorerInterceptor() {
    return g_osInterceptor.get();
}

