/**
 * @file OSShellIntegration.h
 * @brief OS Explorer interceptor (stub declarations for patchable build).
 */
#ifndef OS_SHELL_INTEGRATION_H
#define OS_SHELL_INTEGRATION_H

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

typedef void (*OSInterceptorCallback)(void* logEntry);

struct OSIntegrationCtx {
    ULONGLONG magic;
    DWORD version;
    DWORD targetPID;
    HANDLE hTargetProcess;
    void* hookTable;
    void* callLog;
    OSInterceptorCallback callback;
    void* stats;
};

struct CallLogEntry {
    ULONGLONG timestamp;
    DWORD threadID;
    void* apiFunction;
    ULONGLONG parameters[8];
    ULONGLONG returnValue;
    ULONGLONG callStack[16];
    CallLogEntry* next;
};

struct InterceptionStats {
    ULONGLONG totalCalls, fileIOCalls, registryCalls, networkCalls;
    ULONGLONG processCalls, memoryCalls, windowCalls, comCalls, cryptoCalls, otherCalls;
    ULONGLONG bytesLogged;
};

class OSShellIntegration {
public:
    OSShellIntegration();
    ~OSShellIntegration();
    bool Initialize(DWORD targetPID, OSInterceptorCallback callback);
    bool StartInterception();
    bool StopInterception();
    InterceptionStats GetStatistics();
    void ProcessLogQueue();
    std::vector<CallLogEntry*> GetInterceptedCalls();
    void ClearLog();
    bool IsRunning() const { return m_isRunning; }
    DWORD GetTargetPID() const { return m_targetPID; }
private:
    bool RegisterOSCallbacks();
    bool UnregisterOSCallbacks();
    void LogCall(CallLogEntry* entry);
    void StreamCall(CallLogEntry* entry);
    OSIntegrationCtx* m_interceptor;
    DWORD m_targetPID;
    HANDLE m_hTargetProcess;
    bool m_isRunning;
    OSInterceptorCallback m_callback;
    std::unique_ptr<std::thread> m_logThread;
    std::atomic<bool> m_stopLogThread;
};

extern std::unique_ptr<OSShellIntegration> g_osInterceptor;
bool InitOSExplorerInterceptor(DWORD targetPID, OSInterceptorCallback callback);
void CleanupOSExplorerInterceptor();
OSShellIntegration* GetOSExplorerInterceptor();
bool AttachToTaskManager();
bool MonitorTaskManager();
bool AddContextMenuItems();
bool AttachToExplorer();
bool MonitorExplorer();
bool MonitorFileOperations();
bool StartNetworkInterception();
bool StopNetworkInterception();
bool MonitorNetworkAPIs();
bool ApplyLiveUpdate(void* address, void* patch, size_t size);
bool RemoveLiveUpdate(void* address, size_t size);
bool InitializeCheckpoints();
bool PlaceCheckpoints();
bool ScanForCheckpoints();
void* LoadModuleManual(HANDLE hProcess, void* dllData, size_t dllSize);
bool ApplyRelocations(HANDLE hProcess, void* mappedBase, void* dllData);
bool BindImports(HANDLE hProcess, void* mappedBase, void* dllData);
std::string GetFunctionName(void* function);
std::string FormatCallInfo(CallLogEntry* entry);
bool IsSensitiveFile(const std::string& path);
bool IsSensitiveRegistryKey(const std::string& key);

#endif
