#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

// OS Interceptor structures and definitions

// Interceptor callback function type
typedef void (*OSInterceptorCallback)(void* logEntry);

// OS Interceptor structure
struct OSInterceptor {
    ULONGLONG magic;           // Signature (0x0SINT3RC3PT0R)
    DWORD version;             // Version number
    DWORD targetPID;           // Process being intercepted
    HANDLE hTargetProcess;     // Target process handle
    void* hookTable;           // Table of all hooks
    void* callLog;             // Log of intercepted calls
    OSInterceptorCallback callback; // User callback for streaming
    void* stats;               // Interception statistics
};

// Call log entry structure
struct CallLogEntry {
    ULONGLONG timestamp;       // When call was made
    DWORD threadID;            // Thread that made call
    void* apiFunction;         // Function called
    ULONGLONG parameters[8];   // First 8 parameters
    ULONGLONG returnValue;     // Return value
    ULONGLONG callStack[16];   // Call stack
    CallLogEntry* next;        // Next entry in queue
};

// Hook table for OS APIs
struct OSHookTable {
    // File I/O
    void* CreateFileW;
    void* ReadFile;
    void* WriteFile;
    void* DeleteFileW;
    void* MoveFileW;
    void* CopyFileW;
    
    // Registry
    void* RegOpenKeyExW;
    void* RegQueryValueExW;
    void* RegSetValueExW;
    void* RegCreateKeyExW;
    void* RegDeleteKeyW;
    void* RegCloseKey;
    
    // Process/Thread
    void* CreateProcessW;
    void* CreateThread;
    void* TerminateProcess;
    void* TerminateThread;
    void* OpenProcess;
    void* CloseHandle;
    
    // Memory
    void* VirtualAlloc;
    void* VirtualFree;
    void* VirtualProtect;
    void* HeapAlloc;
    void* HeapFree;
    
    // Network
    void* WSAConnect;
    void* send;
    void* recv;
    void* WSASend;
    void* WSARecv;
    void* getaddrinfo;
    void* gethostbyname;
    
    // Window/Graphics
    void* CreateWindowExW;
    void* SendMessageW;
    void* PostMessageW;
    void* BitBlt;
    void* StretchBlt;
    
    // COM/OLE
    void* CoCreateInstance;
    void* CoInitialize;
    void* CoUninitialize;
    
    // Crypto
    void* CryptEncrypt;
    void* CryptDecrypt;
    void* CryptHashData;
    
    // Misc
    void* LoadLibraryW;
    void* GetProcAddress;
    void* SetWindowsHookExW;
    void* UnhookWindowsHookEx;
};

// Interception statistics
struct InterceptionStats {
    ULONGLONG totalCalls;      // Total intercepted calls
    ULONGLONG fileIOCalls;     // File I/O calls
    ULONGLONG registryCalls;   // Registry calls
    ULONGLONG networkCalls;    // Network calls
    ULONGLONG processCalls;    // Process/thread calls
    ULONGLONG memoryCalls;     // Memory calls
    ULONGLONG windowCalls;     // Window/graphics calls
    ULONGLONG comCalls;        // COM/OLE calls
    ULONGLONG cryptoCalls;     // Crypto calls
    ULONGLONG otherCalls;      // Other calls
    ULONGLONG bytesLogged;     // Total bytes logged
};

// OS Explorer Interceptor class
class OSExplorerInterceptor {
public:
    OSExplorerInterceptor();
    ~OSExplorerInterceptor();
    
    // Initialize interceptor for target process
    bool Initialize(DWORD targetPID, OSInterceptorCallback callback);
    
    // Start interception
    bool StartInterception();
    
    // Stop interception
    bool StopInterception();
    
    // Get statistics
    InterceptionStats GetStatistics();
    
    // Process log queue
    void ProcessLogQueue();
    
    // Get intercepted calls
    std::vector<CallLogEntry*> GetInterceptedCalls();
    
    // Clear log
    void ClearLog();
    
    // Check if running
    bool IsRunning() const { return m_isRunning; }
    
    // Get target process ID
    DWORD GetTargetPID() const { return m_targetPID; }
    
private:
    // Private implementation
    bool HookOSAPIs();
    bool UnhookOSAPIs();
    void LogCall(CallLogEntry* entry);
    void StreamCall(CallLogEntry* entry);
    
    OSInterceptor* m_interceptor;
    DWORD m_targetPID;
    HANDLE m_hTargetProcess;
    bool m_isRunning;
    OSInterceptorCallback m_callback;
    
    // Thread for processing log queue
    std::unique_ptr<std::thread> m_logThread;
    std::atomic<bool> m_stopLogThread;
};

// Global interceptor instance
extern std::unique_ptr<OSExplorerInterceptor> g_osInterceptor;

// Initialize OS interceptor
bool InitOSExplorerInterceptor(DWORD targetPID, OSInterceptorCallback callback);

// Cleanup OS interceptor
void CleanupOSExplorerInterceptor();

// Get interceptor instance
OSExplorerInterceptor* GetOSExplorerInterceptor();

// Task Manager integration
bool InjectIntoTaskManager();
bool HookTaskManager();
bool AddContextMenuItems();

// Explorer integration
bool InjectIntoExplorer();
bool HookExplorer();
bool HookFileOperations();

// Network interception
bool StartNetworkInterception();
bool StopNetworkInterception();
bool HookNetworkAPIs();

// Hotpatching
bool ApplyHotpatch(void* address, void* patch, size_t size);
bool RemoveHotpatch(void* address, size_t size);

// Beaconism integration
bool InitializeBeaconism();
bool InjectBeacons();
bool ScanForBeacons();

// Manual mapping
void* ManualMapDLL(HANDLE hProcess, void* dllData, size_t dllSize);
bool FixRelocations(HANDLE hProcess, void* mappedBase, void* dllData);
bool ResolveImports(HANDLE hProcess, void* mappedBase, void* dllData);

// Utility functions
std::string GetFunctionName(void* function);
void FormatCallInfo(CallLogEntry* entry);
bool IsSensitiveFile(const std::string& path);
bool IsSensitiveRegistryKey(const std::string& key);
bool IsSensitiveNetworkAddress(const std::string& address);
void* FindPattern(void* start, size_t size, const BYTE* pattern, size_t patternSize);

// Constants (magic: "OSINT" + "3RC3" in hex)
const ULONGLONG OS_INTERCEPTOR_MAGIC = 0x05314E5433524333ULL;
const DWORD OS_INTERCEPTOR_VERSION = 1;

// Menu IDs
const UINT MENU_ID_OS_INTERCEPT = 5001;
const UINT MENU_ID_NETWORK_INTERCEPT = 5002;
const UINT MENU_ID_HOTPATCH = 5003;
const UINT MENU_ID_BEACONISM = 5004;
const UINT MENU_ID_DUAL_ENGINE = 5005;

// Error codes
const DWORD ERROR_OSINT_SUCCESS = 0;
const DWORD ERROR_OSINT_INVALID_PID = 1;
const DWORD ERROR_OSINT_ACCESS_DENIED = 2;
const DWORD ERROR_OSINT_HOOK_FAILED = 3;
const DWORD ERROR_OSINT_ALREADY_RUNNING = 4;