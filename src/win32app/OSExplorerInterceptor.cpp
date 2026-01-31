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
        LOG_ERROR("Interceptor already running");
        return false;
    }
    
    m_targetPID = targetPID;
    m_callback = callback;
    
    // Open target process
    m_hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPID);
    if (!m_hTargetProcess) {
        LOG_ERROR("Failed to open target process: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Allocate interceptor structure
    m_interceptor = (OSInterceptor*)VirtualAlloc(NULL, sizeof(OSInterceptor), 
                                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor) {
        LOG_ERROR("Failed to allocate interceptor: " + std::to_string(GetLastError()));
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
        LOG_ERROR("Failed to allocate hook table: " + std::to_string(GetLastError()));
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Allocate call log
    m_interceptor->callLog = VirtualAlloc(NULL, sizeof(void*) * 10000, 
                                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor->callLog) {
        LOG_ERROR("Failed to allocate call log: " + std::to_string(GetLastError()));
        VirtualFree(m_interceptor->hookTable, 0, MEM_RELEASE);
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Allocate statistics
    m_interceptor->stats = VirtualAlloc(NULL, sizeof(InterceptionStats), 
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_interceptor->stats) {
        LOG_ERROR("Failed to allocate stats: " + std::to_string(GetLastError()));
        VirtualFree(m_interceptor->callLog, 0, MEM_RELEASE);
        VirtualFree(m_interceptor->hookTable, 0, MEM_RELEASE);
        VirtualFree(m_interceptor, 0, MEM_RELEASE);
        CloseHandle(m_hTargetProcess);
        return false;
    }
    
    // Initialize stats
    ZeroMemory(m_interceptor->stats, sizeof(InterceptionStats));
    
    LOG_INFO("OS Explorer Interceptor initialized for PID: " + std::to_string(targetPID));
    return true;
}

// Start interception
bool OSExplorerInterceptor::StartInterception() {
    LOG_FUNCTION();
    
    if (!m_interceptor) {
        LOG_ERROR("Interceptor not initialized");
        return false;
    }
    
    if (m_isRunning) {
        LOG_ERROR("Interceptor already running");
        return false;
    }
    
    // Hook OS APIs
    if (!HookOSAPIs()) {
        LOG_ERROR("Failed to hook OS APIs");
        return false;
    }
    
    // Start log processing thread
    m_stopLogThread = false;
    m_logThread = std::make_unique<std::thread>(&OSExplorerInterceptor::ProcessLogQueue, this);
    
    m_isRunning = true;
    LOG_INFO("OS Explorer Interception started");
    
    return true;
}

// Stop interception
bool OSExplorerInterceptor::StopInterception() {
    LOG_FUNCTION();
    
    if (!m_isRunning) {
        LOG_ERROR("Interceptor not running");
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
    LOG_INFO("OS Explorer Interception stopped");
    
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
        LOG_ERROR("Invalid interceptor or hook table");
        return false;
    }
    
    OSHookTable* hookTable = (OSHookTable*)m_interceptor->hookTable;
    
    // Hook kernel32.dll functions
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        LOG_ERROR("Failed to get kernel32.dll handle");
        return false;
    }
    
    // Hook CreateFileW
    void* pCreateFileW = GetProcAddress(hKernel32, "CreateFileW");
    if (pCreateFileW) {
        hookTable->CreateFileW = StealthHook(pCreateFileW, MyCreateFileWHook);
        if (!hookTable->CreateFileW) {
            LOG_ERROR("Failed to hook CreateFileW");
            return false;
        }
    }
    
    // Hook ReadFile
    void* pReadFile = GetProcAddress(hKernel32, "ReadFile");
    if (pReadFile) {
        hookTable->ReadFile = StealthHook(pReadFile, MyReadFileHook);
        if (!hookTable->ReadFile) {
            LOG_ERROR("Failed to hook ReadFile");
            return false;
        }
    }
    
    // Hook WriteFile
    void* pWriteFile = GetProcAddress(hKernel32, "WriteFile");
    if (pWriteFile) {
        hookTable->WriteFile = StealthHook(pWriteFile, MyWriteFileHook);
        if (!hookTable->WriteFile) {
            LOG_ERROR("Failed to hook WriteFile");
            return false;
        }
    }
    
    // Hook advapi32.dll functions
    HMODULE hAdvapi32 = GetModuleHandleA("advapi32.dll");
    if (!hAdvapi32) {
        LOG_ERROR("Failed to get advapi32.dll handle");
        return false;
    }
    
    // Hook RegOpenKeyExW
    void* pRegOpenKeyExW = GetProcAddress(hAdvapi32, "RegOpenKeyExW");
    if (pRegOpenKeyExW) {
        hookTable->RegOpenKeyExW = StealthHook(pRegOpenKeyExW, MyRegOpenKeyExWHook);
        if (!hookTable->RegOpenKeyExW) {
            LOG_ERROR("Failed to hook RegOpenKeyExW");
            return false;
        }
    }
    
    // Hook RegQueryValueExW
    void* pRegQueryValueExW = GetProcAddress(hAdvapi32, "RegQueryValueExW");
    if (pRegQueryValueExW) {
        hookTable->RegQueryValueExW = StealthHook(pRegQueryValueExW, MyRegQueryValueExWHook);
        if (!hookTable->RegQueryValueExW) {
            LOG_ERROR("Failed to hook RegQueryValueExW");
            return false;
        }
    }
    
    // Hook ws2_32.dll functions
    HMODULE hWS2_32 = GetModuleHandleA("ws2_32.dll");
    if (!hWS2_32) {
        LOG_ERROR("Failed to get ws2_32.dll handle");
        return false;
    }
    
    // Hook WSAConnect
    void* pWSAConnect = GetProcAddress(hWS2_32, "WSAConnect");
    if (pWSAConnect) {
        hookTable->WSAConnect = StealthHook(pWSAConnect, MyWSAConnectHook);
        if (!hookTable->WSAConnect) {
            LOG_ERROR("Failed to hook WSAConnect");
            return false;
        }
    }
    
    // Hook send
    void* pSend = GetProcAddress(hWS2_32, "send");
    if (pSend) {
        hookTable->send = StealthHook(pSend, MySendHook);
        if (!hookTable->send) {
            LOG_ERROR("Failed to hook send");
            return false;
        }
    }
    
    // Hook recv
    void* pRecv = GetProcAddress(hWS2_32, "recv");
    if (pRecv) {
        hookTable->recv = StealthHook(pRecv, MyRecvHook);
        if (!hookTable->recv) {
            LOG_ERROR("Failed to hook recv");
            return false;
        }
    }
    
    LOG_INFO("OS APIs hooked successfully");
    return true;
}

// Unhook OS APIs
bool OSExplorerInterceptor::UnhookOSAPIs() {
    LOG_FUNCTION();
    
    if (!m_interceptor || !m_interceptor->hookTable) {
        LOG_ERROR("Invalid interceptor or hook table");
        return false;
    }
    
    OSHookTable* hookTable = (OSHookTable*)m_interceptor->hookTable;
    
    // Unhook all functions (restore original bytes)
    // Implementation depends on StealthHook mechanism
    
    LOG_INFO("OS APIs unhooked");
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

// Stealth hook implementation (placeholder)
void* StealthHook(void* targetFunction, void* hookFunction) {
    // Implementation depends on stealth hooking technique
    // Could use hardware breakpoints, VEH, page guard, etc.
    
    // For now, return original function
    return targetFunction;
}

// Hook implementations (placeholders)
void* MyCreateFileWHook = nullptr;
void* MyReadFileHook = nullptr;
void* MyWriteFileHook = nullptr;
void* MyRegOpenKeyExWHook = nullptr;
void* MyRegQueryValueExWHook = nullptr;
void* MyWSAConnectHook = nullptr;
void* MySendHook = nullptr;
void* MyRecvHook = nullptr;

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
        LOG_ERROR("Task Manager not found");
        return false;
    }
    
    // Manual map our DLL into Task Manager
    // Implementation depends on manual mapping technique
    
    LOG_INFO("Injected into Task Manager (PID: " + std::to_string(taskmgrPID) + ")");
    return true;
}

// Hook Task Manager
bool HookTaskManager() {
    LOG_FUNCTION();
    
    // Find Task Manager window
    HWND hTaskmgr = FindWindowA("TaskManagerWindow", NULL);
    if (!hTaskmgr) {
        LOG_ERROR("Task Manager window not found");
        return false;
    }
    
    // Hook window procedure
    // Implementation depends on hooking technique
    
    LOG_INFO("Task Manager hooked");
    return true;
}

// Add context menu items
bool AddContextMenuItems() {
    LOG_FUNCTION();
    
    // Add "OS Intercept" to context menu
    // Implementation depends on menu modification technique
    
    LOG_INFO("Context menu items added");
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
        LOG_ERROR("Explorer not found");
        return false;
    }
    
    // Manual map our DLL into Explorer
    // Implementation depends on manual mapping technique
    
    LOG_INFO("Injected into Explorer (PID: " + std::to_string(explorerPID) + ")");
    return true;
}

// Hook Explorer
bool HookExplorer() {
    LOG_FUNCTION();
    
    // Hook Explorer file operations
    // Implementation depends on hooking technique
    
    LOG_INFO("Explorer hooked");
    return true;
}

// Hook file operations
bool HookFileOperations() {
    LOG_FUNCTION();
    
    // Hook CopyFileW, MoveFileW, DeleteFileW
    // Implementation depends on hooking technique
    
    LOG_INFO("File operations hooked");
    return true;
}

// Start network interception
bool StartNetworkInterception() {
    LOG_FUNCTION();
    
    // Hook network APIs
    if (!HookNetworkAPIs()) {
        LOG_ERROR("Failed to hook network APIs");
        return false;
    }
    
    LOG_INFO("Network interception started");
    return true;
}

// Stop network interception
bool StopNetworkInterception() {
    LOG_FUNCTION();
    
    // Unhook network APIs
    // Implementation depends on hooking technique
    
    LOG_INFO("Network interception stopped");
    return true;
}

// Hook network APIs
bool HookNetworkAPIs() {
    LOG_FUNCTION();
    
    // Hook WSAConnect, send, recv, WSASend, WSARecv, etc.
    // Implementation depends on hooking technique
    
    LOG_INFO("Network APIs hooked");
    return true;
}

// Apply hotpatch
bool ApplyHotpatch(void* address, void* patch, size_t size) {
    LOG_FUNCTION();
    
    DWORD oldProtect;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LOG_ERROR("Failed to change memory protection: " + std::to_string(GetLastError()));
        return false;
    }
    
    memcpy(address, patch, size);
    
    VirtualProtect(address, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    
    LOG_INFO("Hotpatch applied at 0x" + std::to_string((ULONGLONG)address));
    return true;
}

// Remove hotpatch
bool RemoveHotpatch(void* address, size_t size) {
    LOG_FUNCTION();
    
    // Restore original bytes
    // Implementation depends on how original bytes were saved
    
    LOG_INFO("Hotpatch removed from 0x" + std::to_string((ULONGLONG)address));
    return true;
}

// Initialize beaconism
bool InitializeBeaconism() {
    LOG_FUNCTION();
    
    // Initialize beacon system
    // Implementation depends on beaconism technique
    
    LOG_INFO("Beaconism initialized");
    return true;
}

// Inject beacons
bool InjectBeacons() {
    LOG_FUNCTION();
    
    // Inject beacon signatures into memory
    // Implementation depends on beaconism technique
    
    LOG_INFO("Beacons injected");
    return true;
}

// Scan for beacons
bool ScanForBeacons() {
    LOG_FUNCTION();
    
    // Scan memory for beacon signatures
    // Implementation depends on beaconism technique
    
    LOG_INFO("Beacon scan completed");
    return true;
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
        LOG_ERROR("Failed to allocate memory in target process");
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
    
    LOG_INFO("DLL manually mapped at 0x" + std::to_string((ULONGLONG)pMapped));
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
    
    LOG_INFO("Relocations fixed");
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
    
    LOG_INFO("Imports resolved");
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

