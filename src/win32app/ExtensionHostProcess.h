// ExtensionHostProcess.h
// Phase 2 Day 6: Extension Host Runtime Foundation
// Real Win32 process lifecycle management with crash handling, broker boundary, and resource quotas

#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <queue>
#include "IDELogger.h"

namespace RawrXD::Extensions {

    // Forward declarations
    class IProcessBroker;
    class IPC_Channel;
    class ExtensionHostProcess;

    // Extension host process state
    enum class HostProcessState {
        Uninitialized,
        Starting,
        Running,
        Suspended,
        Crashed,
        Shutdown,
        Failed
    };

    // Resource quota enforcement
    struct ResourceQuota {
        DWORD maxMemoryMB;      // Max working set in MB
        DWORD maxCPUPercent;    // Max CPU usage percentage
        DWORD timeoutMs;        // Max operation timeout
        
        ResourceQuota() 
            : maxMemoryMB(256), maxCPUPercent(80), timeoutMs(5000) {}
    };

    // Extension metadata
    struct ExtensionMetadata {
        std::string extensionId;
        std::string extensionPath;
        std::string extensionName;
        std::string version;
        bool isDevExtension;
        
        ExtensionMetadata() : isDevExtension(false) {}
    };

    // Callback types for process lifecycle events
    using OnProcessStartedCallback = std::function<void(DWORD processId)>;
    using OnProcessCrashedCallback = std::function<void(DWORD processId, int exitCode)>;
    using OnProcessShutdownCallback = std::function<void(DWORD processId, int exitCode)>;

    // ============================================================================
    // ExtensionHostProcess - Main process lifecycle manager
    // ============================================================================
    class ExtensionHostProcess {
    public:
        ExtensionHostProcess(
            const ExtensionMetadata& metadata,
            const ResourceQuota& quota = ResourceQuota());
        
        ~ExtensionHostProcess();

        // --- Lifecycle Methods ---
        
        /// Start the extension host process
        /// Returns: HRESULT indicating startup status
        HRESULT Startup();

        /// Gracefully shutdown the process
        /// Returns: HRESULT indicating shutdown success
        HRESULT Shutdown(DWORD timeoutMs = 5000);

        /// Force terminate the process (fail-closed)
        void ForceTerminate();

        // --- State Query Methods ---
        
        /// Get current process state
        HostProcessState GetState() const { return m_processState; }

        /// Get process ID if running
        DWORD GetProcessId() const { return m_processId; }

        /// Check if process is alive
        bool IsAlive() const;

        /// Get crash count
        DWORD GetCrashCount() const { return m_crashCount; }

        // --- IPC Boundary Setup ---
        
        /// Establish IPC channel with the extension process
        /// Returns: Pointer to established IPC_Channel or nullptr on failure
        IPC_Channel* EstablishIPCChannel();

        /// Terminate IPC channel
        void CloseIPCChannel();

        // --- Crash Handling ---
        
        /// Handle process crash with automatic restart logic
        void OnProcessCrashed(int exitCode);

        /// Setup crash monitoring (internal use)
        void SetupCrashMonitoring();

        // --- Resource Monitoring ---
        
        /// Enforce resource quotas
        /// Returns: true if resources are within limits
        bool EnforceResourceQuotas();

        /// Get current resource usage
        /// Returns: Memory size in bytes, or 0 if failed
        SIZE_T GetCurrentMemoryUsage() const;

        /// Get process CPU time
        FILETIME GetProcessCPUTime() const;

        // --- Callbacks Registration ---
        
        void SetOnStartedCallback(OnProcessStartedCallback callback) {
            m_onStartedCallback = callback;
        }

        void SetOnCrashedCallback(OnProcessCrashedCallback callback) {
            m_onCrashedCallback = callback;
        }

        void SetOnShutdownCallback(OnProcessShutdownCallback callback) {
            m_onShutdownCallback = callback;
        }

    private:
        // --- Internal Methods ---
        
        /// Create process in isolated environment
        HRESULT CreateIsolatedProcess();

        /// Setup job object for resource limitation
        HRESULT SetupJobObject();

        /// Monitor process health in background thread
        static DWORD WINAPI MonitoringThreadProc(LPVOID param);
        void MonitoringThreadMain();

        /// Cleanup resources on process termination
        void CleanupResources();

        /// Log process event
        void LogEvent(const std::string& eventType, const std::string& details);

        // --- Member Variables ---
        
        ExtensionMetadata m_metadata;
        ResourceQuota m_quota;
        
        // Process handles
        HANDLE m_processHandle      = nullptr;
        HANDLE m_threadHandle       = nullptr;
        HANDLE m_jobObject          = nullptr;
        DWORD m_processId           = 0;
        
        // State management
        std::atomic<HostProcessState> m_processState{HostProcessState::Uninitialized};
        std::atomic<DWORD> m_crashCount{0};
        std::atomic<DWORD> m_consecutiveCrashes{0};
        DWORD m_lastCrashTime       = 0;
        
        // Monitoring
        HANDLE m_monitoringThread   = nullptr;
        std::atomic<bool> m_monitoring{false};
        
        // IPC channel
        std::unique_ptr<IPC_Channel> m_ipcChannel;
        
        // Callbacks
        OnProcessStartedCallback m_onStartedCallback;
        OnProcessCrashedCallback m_onCrashedCallback;
        OnProcessShutdownCallback m_onShutdownCallback;
        
        // Telemetry
        FILETIME m_startTime{0, 0};
        FILETIME m_shutdownTime{0, 0};
    };

    // ============================================================================
    // Extension Process Broker Factory
    // ============================================================================
    class ExtensionProcessFactory {
    public:
        static std::shared_ptr<ExtensionHostProcess> CreateHostProcess(
            const ExtensionMetadata& metadata,
            const ResourceQuota& quota = ResourceQuota());
    };

} // namespace RawrXD::Extensions
