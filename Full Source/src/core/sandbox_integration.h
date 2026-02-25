// ============================================================================
// sandbox_integration.h — Windows Sandbox Integration for Isolated Model Exec
// ============================================================================
// Provides isolated execution environments for untrusted GGUF models using
// Windows Sandbox (AppContainer / Job Objects / Restricted Tokens).
// Prevents untrusted model files from accessing host filesystem or network.
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef SANDBOX_INTEGRATION_H
#define SANDBOX_INTEGRATION_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// ============================================================================
// Sandbox Enums
// ============================================================================

enum class SandboxType : uint8_t {
    None        = 0,   // No sandbox (trusted models)
    JobObject   = 1,   // Job object with resource limits
    AppContainer = 2,  // Full AppContainer isolation
    Restricted  = 3,   // Restricted token (no network, limited FS)
    Full        = 4    // Job + AppContainer + Restricted Token
};

enum class SandboxState : uint8_t {
    Inactive    = 0,
    Creating    = 1,
    Ready       = 2,
    Running     = 3,
    Suspended   = 4,
    Terminating = 5,
    Failed      = 6
};

enum class SandboxPolicy : uint8_t {
    AllowGPU        = 0x01,  // Allow GPU device access
    AllowNetLocal   = 0x02,  // Allow localhost networking
    AllowNetLAN     = 0x04,  // Allow LAN networking
    AllowTempWrite  = 0x08,  // Allow writes to temp directory
    AllowModelRead  = 0x10,  // Allow reading model files
    DenyRegistry    = 0x20,  // Deny registry access
    DenyProcessCreate = 0x40,// Deny creating child processes
    LimitMemory     = 0x80   // Enforce memory limits
};

// ============================================================================
// Sandbox Structures
// ============================================================================

struct SandboxResult {
    bool success;
    const char* detail;
    int errorCode;

    static SandboxResult ok(const char* msg) {
        SandboxResult r; r.success = true; r.detail = msg; r.errorCode = 0; return r;
    }
    static SandboxResult error(const char* msg, int code = -1) {
        SandboxResult r; r.success = false; r.detail = msg; r.errorCode = code; return r;
    }
};

struct SandboxConfig {
    SandboxType type;
    uint8_t policyFlags;        // Bitfield of SandboxPolicy
    uint64_t memoryLimitBytes;  // Max memory for sandbox process
    uint64_t cpuRateLimit;      // CPU rate in 1/10000 units (e.g. 5000 = 50%)
    uint32_t timeoutMs;         // Max execution time
    uint32_t maxThreads;        // Max thread count
    std::string allowedReadPath;  // Model file read-only path
    std::string tempWritePath;    // Temp directory for scratch
    std::string sandboxName;      // Unique sandbox identifier

    SandboxConfig()
        : type(SandboxType::JobObject)
        , policyFlags(static_cast<uint8_t>(SandboxPolicy::AllowGPU) |
                      static_cast<uint8_t>(SandboxPolicy::AllowModelRead) |
                      static_cast<uint8_t>(SandboxPolicy::LimitMemory))
        , memoryLimitBytes(8ULL * 1024 * 1024 * 1024) // 8 GB
        , cpuRateLimit(8000)    // 80%
        , timeoutMs(300000)     // 5 minutes
        , maxThreads(64)
    {}
};

struct SandboxInstance {
    std::string sandboxId;
    SandboxConfig config;
    SandboxState state;
    HANDLE hProcess;
    HANDLE hJob;
    HANDLE hToken;
    DWORD processId;
    uint64_t createdAtMs;
    uint64_t cpuTimeMs;
    uint64_t peakMemoryBytes;
    std::string lastError;

    SandboxInstance()
        : state(SandboxState::Inactive)
        , hProcess(nullptr), hJob(nullptr), hToken(nullptr)
        , processId(0), createdAtMs(0), cpuTimeMs(0), peakMemoryBytes(0)
    {}
};

struct SandboxStats {
    std::atomic<uint64_t> sandboxesCreated{0};
    std::atomic<uint64_t> sandboxesDestroyed{0};
    std::atomic<uint64_t> sandboxesFailed{0};
    std::atomic<uint64_t> policyViolations{0};
    std::atomic<uint64_t> memoryLimitHits{0};
    std::atomic<uint64_t> timeoutKills{0};
    std::atomic<uint64_t> totalExecTimeMs{0};
    std::atomic<uint64_t> gpuAccessDenied{0};
};

// ============================================================================
// Callback Types
// ============================================================================

typedef void (*SandboxEventCallback)(const char* sandboxId, SandboxState state, void* userData);
typedef void (*SandboxViolationCallback)(const char* sandboxId, const char* violation, void* userData);

// ============================================================================
// SandboxManager — Singleton
// ============================================================================

class SandboxManager {
public:
    static SandboxManager& instance();

    // ----- Lifecycle -----
    SandboxResult initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Sandbox CRUD -----
    SandboxResult createSandbox(const SandboxConfig& config, std::string& outSandboxId);
    SandboxResult destroySandbox(const std::string& sandboxId);
    SandboxResult getSandboxInfo(const std::string& sandboxId, SandboxInstance& outInfo) const;

    // ----- Execution -----
    SandboxResult launchInSandbox(const std::string& sandboxId,
                                   const std::string& exePath,
                                   const std::string& args);
    SandboxResult suspendSandbox(const std::string& sandboxId);
    SandboxResult resumeSandbox(const std::string& sandboxId);
    SandboxResult terminateSandbox(const std::string& sandboxId);

    // ----- Model-Specific -----
    SandboxResult loadModelInSandbox(const std::string& sandboxId,
                                      const std::string& modelPath);
    SandboxResult runInferenceInSandbox(const std::string& sandboxId,
                                         const std::string& prompt,
                                         std::string& outResponse);

    // ----- Policy -----
    SandboxResult setPolicy(const std::string& sandboxId, uint8_t policyFlags);
    SandboxResult grantGPUAccess(const std::string& sandboxId, bool allow);
    SandboxResult setMemoryLimit(const std::string& sandboxId, uint64_t bytes);
    SandboxResult setCPURate(const std::string& sandboxId, uint64_t rate);

    // ----- Monitoring -----
    bool isSandboxRunning(const std::string& sandboxId) const;
    uint64_t getSandboxMemoryUsage(const std::string& sandboxId) const;
    uint64_t getSandboxCPUTime(const std::string& sandboxId) const;

    // ----- Callbacks -----
    void setEventCallback(SandboxEventCallback cb, void* userData);
    void setViolationCallback(SandboxViolationCallback cb, void* userData);

    // ----- Enumeration -----
    std::vector<std::string> listSandboxes() const;
    uint32_t getActiveSandboxCount() const;

    // ----- Stats & JSON -----
    const SandboxStats& getStats() const { return m_stats; }
    void resetStats();
    std::string toJson() const;
    std::string sandboxToJson(const std::string& sandboxId) const;

private:
    SandboxManager();
    ~SandboxManager();
    SandboxManager(const SandboxManager&) = delete;
    SandboxManager& operator=(const SandboxManager&) = delete;

    // ----- Internal -----
    SandboxResult createJobObject(SandboxInstance& inst);
    SandboxResult createRestrictedToken(SandboxInstance& inst);
    SandboxResult configureAppContainer(SandboxInstance& inst);
    SandboxResult applyPolicies(SandboxInstance& inst);

    void monitorThread();
    static DWORD WINAPI monitorThreadProc(LPVOID param);

    std::string generateSandboxId() const;

    // ----- Members -----
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdownRequested{false};
    mutable std::mutex m_mutex;

    std::map<std::string, SandboxInstance> m_sandboxes;
    HANDLE m_hMonitorThread;

    SandboxEventCallback m_eventCb;
    void* m_eventData;
    SandboxViolationCallback m_violationCb;
    void* m_violationData;

    SandboxStats m_stats;
    uint32_t m_nextId;
};

#endif // SANDBOX_INTEGRATION_H
