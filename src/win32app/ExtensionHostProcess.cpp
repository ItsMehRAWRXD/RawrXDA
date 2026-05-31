// ExtensionHostProcess.cpp
// Phase 2 Day 6: Extension Host Runtime Foundation
// Implementation of real Win32 process lifecycle management

#include "ExtensionHostProcess.h"
#include "IPC_Channel.h"
#include "IDELogger.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <sstream>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "kernel32.lib")

// MASM Bridge
extern "C" HANDLE MASM_CreateIsolatedProcess(const char* AppName, const char* CmdLine, const char* WorkDir, DWORD MemLimitMB);

namespace RawrXD::Extensions {

    // ============================================================================
    // ExtensionHostProcess Implementation
    // ============================================================================

    ExtensionHostProcess::ExtensionHostProcess(
        const ExtensionMetadata& metadata,
        const ResourceQuota& quota)
        : m_metadata(metadata), m_quota(quota)
    {
        LogEvent("CTOR", "Extension host process manager initialized for: " + metadata.extensionId);
    }

    ExtensionHostProcess::~ExtensionHostProcess()
    {
        if (IsAlive()) {
            Shutdown(3000);
        }
        CloseIPCChannel();
        CleanupResources();
    }

    HRESULT ExtensionHostProcess::Startup()
    {
        if (m_processState != HostProcessState::Uninitialized && 
            m_processState != HostProcessState::Failed) {
            LogEvent("STARTUP_ERROR", "Process already started or in invalid state");
            return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
        }

        m_processState = HostProcessState::Starting;

        // Create the isolated process via MASM Broker (includes job setup)
        HRESULT hrProcess = CreateIsolatedProcess();
        if (FAILED(hrProcess)) {
            LogEvent("STARTUP_ERROR", "Failed to create isolated process via broker: " + std::to_string(hrProcess));
            m_processState = HostProcessState::Failed;
            return hrProcess;
        }

        // Get current time for telemetry
        GetSystemTimeAsFileTime(&m_startTime);

        m_processState = HostProcessState::Running;
        LogEvent("STARTUP_SUCCESS", "Process started via MASM Broker with PID: " + std::to_string(m_processId));

        // Fire callback
        if (m_onStartedCallback) {
            m_onStartedCallback(m_processId);
        }

        // Setup crash monitoring
        SetupCrashMonitoring();

        return S_OK;
    }

    HRESULT ExtensionHostProcess::Shutdown(DWORD timeoutMs)
    {
        if (m_processState == HostProcessState::Uninitialized ||
            m_processState == HostProcessState::Shutdown) {
            return S_OK;
        }

        // Stop monitoring
        m_monitoring = false;
        if (m_monitoringThread) {
            WaitForSingleObject(m_monitoringThread, 2000);
            CloseHandle(m_monitoringThread);
            m_monitoringThread = nullptr;
        }

        // Close IPC channel first
        CloseIPCChannel();

        m_processState = HostProcessState::Shutdown;

        if (!m_processHandle || !IsAlive()) {
            GetSystemTimeAsFileTime(&m_shutdownTime);
            LogEvent("SHUTDOWN_CLEAN", "Process already terminated");
            return S_OK;
        }

        // Try graceful shutdown first
        DWORD dwWait = WaitForSingleObject(m_processHandle, timeoutMs);
        
        if (dwWait == WAIT_TIMEOUT) {
            LogEvent("SHUTDOWN_FORCED", "Graceful shutdown timeout, forcing termination");
            ForceTerminate();
        }

        DWORD dwExitCode = 0;
        if (m_processHandle) {
            if (GetExitCodeProcess(m_processHandle, &dwExitCode)) {
                LogEvent("SHUTDOWN_SUCCESS", "Exit code: " + std::to_string(dwExitCode));
            }
        }

        // Fire callback
        if (m_onShutdownCallback) {
            m_onShutdownCallback(m_processId, (int)dwExitCode);
        }

        CleanupResources();
        GetSystemTimeAsFileTime(&m_shutdownTime);

        return S_OK;
    }

    void ExtensionHostProcess::ForceTerminate()
    {
        if (m_processHandle && IsAlive()) {
            TerminateProcess(m_processHandle, 1);
            LogEvent("TERMINATE_FORCED", "Process forcefully terminated");
        }
    }

    bool ExtensionHostProcess::IsAlive() const
    {
        if (!m_processHandle) {
            return false;
        }

        DWORD dwExitCode;
        if (!GetExitCodeProcess(m_processHandle, &dwExitCode)) {
            return false;
        }

        return (dwExitCode == STILL_ACTIVE);
    }

    HRESULT ExtensionHostProcess::CreateIsolatedProcess()
    {
        std::string hostExePath = m_metadata.extensionPath + "\\extension_host.exe";
        std::string cmdLine = m_metadata.extensionId;

        // Call MASM Process Broker for secure, isolated creation
        HANDLE hProcess = MASM_CreateIsolatedProcess(
            hostExePath.c_str(),
            cmdLine.c_str(),
            m_metadata.extensionPath.c_str(),
            m_quota.maxMemoryMB
        );

        if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE) {
            DWORD dwError = GetLastError();
            LogEvent("BROKER_ERROR", "MASM Process Broker failed to create isolated process. Error: " + std::to_string(dwError));
            return HRESULT_FROM_WIN32(dwError);
        }

        m_processHandle = hProcess;
        m_processId = GetProcessId(hProcess);
        
        // Note: Thread handle is not currently returned by this version of the MASM broker.
        // If needed, we could update the MASM routine to return the PROCESS_INFORMATION struct.
        m_threadHandle = nullptr; 

        return S_OK;
    }

    HRESULT ExtensionHostProcess::SetupJobObject()
    {
        m_jobObject = CreateJobObjectA(nullptr, nullptr);
        if (!m_jobObject) {
            DWORD dwError = GetLastError();
            return HRESULT_FROM_WIN32(dwError);
        }

        // Set resource limits
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = 
            JOB_OBJECT_LIMIT_JOB_MEMORY |
            JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        
        // Memory limit (in bytes)
        jeli.JobMemoryLimit = (SIZE_T)m_quota.maxMemoryMB * 1024 * 1024;
        jeli.BasicLimitInformation.ActiveProcessLimit = 1;

        if (!SetInformationJobObject(
            m_jobObject,
            JobObjectExtendedLimitInformation,
            &jeli,
            sizeof(jeli))) {
            
            DWORD dwError = GetLastError();
            CloseHandle(m_jobObject);
            m_jobObject = nullptr;
            return HRESULT_FROM_WIN32(dwError);
        }

        return S_OK;
    }

    IPC_Channel* ExtensionHostProcess::EstablishIPCChannel()
    {
        if (!IsAlive()) {
            LogEvent("IPC_ERROR", "Cannot establish IPC with dead process");
            return nullptr;
        }

        try {
            std::string channelName = "RawrXD_Ext_" + m_metadata.extensionId;
            m_ipcChannel = std::make_unique<RequestResponseChannel>(channelName, m_processId);

            HRESULT hr = m_ipcChannel->Connect(5000);
            if (FAILED(hr)) {
                LogEvent("IPC_ERROR", "Failed to connect IPC channel: " + std::to_string(hr));
                m_ipcChannel.reset();
                return nullptr;
            }

            LogEvent("IPC_SUCCESS", "IPC channel established for process");
            return m_ipcChannel.get();
        }
        catch (const std::exception& ex) {
            LogEvent("IPC_ERROR", std::string("Exception: ") + ex.what());
            m_ipcChannel.reset();
            return nullptr;
        }
    }

    void ExtensionHostProcess::CloseIPCChannel()
    {
        if (m_ipcChannel) {
            m_ipcChannel->Close();
            m_ipcChannel.reset();
            LogEvent("IPC_CLOSED", "IPC channel closed");
        }
    }

    void ExtensionHostProcess::OnProcessCrashed(int exitCode)
    {
        m_crashCount++;
        m_consecutiveCrashes++;
        m_lastCrashTime = GetTickCount();

        LogEvent("CRASH", "Process crashed - count: " + std::to_string(m_crashCount) +
                         ", exit code: " + std::to_string(exitCode));

        // Fire callback
        if (m_onCrashedCallback) {
            m_onCrashedCallback(m_processId, exitCode);
        }

        // Determine if we should restart
        constexpr DWORD MAX_RESTARTS = 3;
        if (m_consecutiveCrashes <= MAX_RESTARTS) {
            // Exponential backoff: 500ms, 1s, 2s
            DWORD backoffMs = 500 * (1 << (m_consecutiveCrashes - 1));
            LogEvent("RESTART_SCHEDULED", "Restart scheduled after " + std::to_string(backoffMs) + "ms");
            
            // In production, would schedule restart via timer
        }
    }

    void ExtensionHostProcess::SetupCrashMonitoring()
    {
        if (m_monitoringThread || m_monitoring) {
            return;
        }

        m_monitoring = true;
        m_monitoringThread = CreateThread(
            nullptr, 0,
            ExtensionHostProcess::MonitoringThreadProc,
            this, 0, nullptr);

        if (!m_monitoringThread) {
            m_monitoring = false;
            LogEvent("MONITOR_ERROR", "Failed to create monitoring thread");
        }
    }

    DWORD WINAPI ExtensionHostProcess::MonitoringThreadProc(LPVOID param)
    {
        ExtensionHostProcess* pThis = static_cast<ExtensionHostProcess*>(param);
        if (pThis) {
            pThis->MonitoringThreadMain();
        }
        return 0;
    }

    void ExtensionHostProcess::MonitoringThreadMain()
    {
        while (m_monitoring && m_processHandle) {
            DWORD dwWait = WaitForSingleObject(m_processHandle, 1000);

            if (dwWait == WAIT_OBJECT_0) {
                // Process has exited
                DWORD dwExitCode = 0;
                GetExitCodeProcess(m_processHandle, &dwExitCode);
                OnProcessCrashed((int)dwExitCode);
                break;
            }

            // Check resource quotas
            if (m_processState == HostProcessState::Running) {
                EnforceResourceQuotas();
            }
        }
    }

    bool ExtensionHostProcess::EnforceResourceQuotas()
    {
        if (!m_processHandle) {
            return false;
        }

        SIZE_T memUsage = GetCurrentMemoryUsage();
        SIZE_T maxMemory = (SIZE_T)m_quota.maxMemoryMB * 1024 * 1024;

        if (memUsage > maxMemory) {
            LogEvent("QUOTA_EXCEEDED", "Memory quota exceeded: " + std::to_string(memUsage / 1024 / 1024) + " MB");
            TerminateProcess(m_processHandle, 1);
            return false;
        }

        return true;
    }

    SIZE_T ExtensionHostProcess::GetCurrentMemoryUsage() const
    {
        if (!m_processHandle) {
            return 0;
        }

        PROCESS_MEMORY_COUNTERS pmc = {0};
        if (GetProcessMemoryInfo(m_processHandle, &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }

        return 0;
    }

    FILETIME ExtensionHostProcess::GetProcessCPUTime() const
    {
        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        FILETIME ftCPUTime = {0, 0};

        if (m_processHandle && GetProcessTimes(m_processHandle, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
            // Sum kernel and user time
            ULARGE_INTEGER uliKernel, uliUser, uliTotal;
            uliKernel.LowPart = ftKernel.dwLowDateTime;
            uliKernel.HighPart = ftKernel.dwHighDateTime;
            uliUser.LowPart = ftUser.dwLowDateTime;
            uliUser.HighPart = ftUser.dwHighDateTime;
            uliTotal.QuadPart = uliKernel.QuadPart + uliUser.QuadPart;
            
            ftCPUTime.dwLowDateTime = uliTotal.LowPart;
            ftCPUTime.dwHighDateTime = uliTotal.HighPart;
        }

        return ftCPUTime;
    }

    void ExtensionHostProcess::CleanupResources()
    {
        if (m_threadHandle) {
            CloseHandle(m_threadHandle);
            m_threadHandle = nullptr;
        }

        if (m_processHandle) {
            CloseHandle(m_processHandle);
            m_processHandle = nullptr;
        }

        if (m_jobObject) {
            CloseHandle(m_jobObject);
            m_jobObject = nullptr;
        }

        m_processId = 0;
        m_processState = HostProcessState::Uninitialized;
    }

    void ExtensionHostProcess::LogEvent(const std::string& eventType, const std::string& details)
    {
        // Format: YYYY-MM-DD HH:MM:SS [EVENTTYPE] ExtensionID: details
        auto now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);

        std::ostringstream oss;
        oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
            << " [" << eventType << "] "
            << m_metadata.extensionId << ": "
            << details;

        LOG_INFO(oss.str());
    }

    // ============================================================================
    // Extension Process Factory Implementation
    // ============================================================================

    std::shared_ptr<ExtensionHostProcess> ExtensionProcessFactory::CreateHostProcess(
        const ExtensionMetadata& metadata,
        const ResourceQuota& quota)
    {
        return std::make_shared<ExtensionHostProcess>(metadata, quota);
    }

} // namespace RawrXD::Extensions
