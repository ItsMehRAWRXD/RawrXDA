// ============================================================================
// quickjs_sandbox.h — Plugin Trust Boundary (QuickJS Sandbox Hardening)
// ============================================================================
//
// PURPOSE:
//   Restricts QuickJS extensions from executing arbitrary native code.
//   Enforces whitelist-based native function access, memory/CPU limits,
//   file system sandboxing, and network access control.
//
//   Wraps the JSExtensionHost to add security boundaries between
//   user-installed extensions and the core IDE/engine.
//
// DEPS:     js_extension_host.hpp (existing QuickJS integration)
// PATTERN:  PatchResult-compatible, no exceptions, no std::function
// THREADING: Per-extension sandbox context (single-threaded JS)
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Sandbox {

// ============================================================================
// Constants
// ============================================================================
static constexpr uint32_t SANDBOX_MAX_EXTENSIONS      = 64;
static constexpr uint64_t SANDBOX_DEFAULT_MEMORY_MB   = 64;     // 64MB per extension
static constexpr uint32_t SANDBOX_DEFAULT_CPU_MS       = 5000;   // 5s CPU time limit
static constexpr uint32_t SANDBOX_MAX_NATIVE_FUNCS     = 256;
static constexpr uint32_t SANDBOX_MAX_FS_PATHS         = 32;
static constexpr uint32_t SANDBOX_MAX_NET_HOSTS        = 16;

// ============================================================================
// Security Tier — determines what an extension is allowed to do
// ============================================================================
enum class SecurityTier : uint32_t {
    Untrusted    = 0,   // No native access, no FS, no network
    Sandboxed    = 1,   // Whitelist-only native funcs, read-only FS, no network
    Trusted      = 2,   // Extended native access, read-write FS, whitelisted network
    Privileged   = 3,   // Full native access (signed by RawrXD authority only)
};

// ============================================================================
// Sandbox Violation Types
// ============================================================================
enum class ViolationType : uint32_t {
    NativeFunctionBlocked   = 0x01,
    MemoryLimitExceeded     = 0x02,
    CPUTimeLimitExceeded    = 0x03,
    FileAccessDenied        = 0x04,
    NetworkAccessDenied     = 0x05,
    SystemCallBlocked       = 0x06,
    ModuleImportBlocked     = 0x07,
    PrivilegeEscalation     = 0x08,
};

// ============================================================================
// Sandbox Violation Record
// ============================================================================
struct SandboxViolation {
    ViolationType  type;
    char           extensionId[128];     // VSIX ID
    char           detail[512];          // What was attempted
    uint64_t       timestamp;            // GetTickCount64()
    uint32_t       consecutiveCount;     // Rapid-fire violation tracking

    static SandboxViolation create(ViolationType t, const char* extId, const char* msg) {
        SandboxViolation v{};
        v.type = t;
        v.timestamp = GetTickCount64();
        v.consecutiveCount = 1;
        if (extId) strncpy_s(v.extensionId, sizeof(v.extensionId), extId, _TRUNCATE);
        if (msg)   strncpy_s(v.detail, sizeof(v.detail), msg, _TRUNCATE);
        return v;
    }
};

// ============================================================================
// Sandbox Configuration for a single extension
// ============================================================================
struct SandboxConfig {
    SecurityTier  tier;

    // Memory limits
    uint64_t      maxMemoryBytes;        // Maximum heap allocation
    uint64_t      maxStackBytes;         // Maximum stack depth

    // CPU limits
    uint32_t      maxCpuMs;              // Maximum CPU time per API call
    uint32_t      maxTotalCpuMs;         // Maximum total CPU time (lifetime)

    // File system whitelist
    struct FSPath {
        wchar_t   path[260];
        bool      readOnly;              // true = read-only, false = read-write
    };
    FSPath        allowedPaths[SANDBOX_MAX_FS_PATHS];
    uint32_t      allowedPathCount;

    // Network whitelist
    struct NetHost {
        char      hostname[256];
        uint16_t  port;                  // 0 = any port
        bool      httpsOnly;
    };
    NetHost       allowedHosts[SANDBOX_MAX_NET_HOSTS];
    uint32_t      allowedHostCount;

    // Native function whitelist
    struct NativeFunc {
        char      name[128];             // Function name (e.g., "rawrxd.getVersion")
    };
    NativeFunc    allowedNativeFuncs[SANDBOX_MAX_NATIVE_FUNCS];
    uint32_t      allowedNativeFuncCount;

    // Behavior on violation
    bool          killOnViolation;       // Terminate extension on first violation
    bool          logViolations;         // Write violations to debug output
    uint32_t      maxViolationsBeforeKill; // Kill after N violations (if !killOnViolation)
};

// ============================================================================
// Sandbox Result
// ============================================================================
struct SandboxResult {
    bool        success;
    const char* detail;
    int         errorCode;
    uint32_t    violationCount;

    static SandboxResult ok(const char* msg = "OK") {
        SandboxResult r{};
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        r.violationCount = 0;
        return r;
    }

    static SandboxResult error(const char* msg, int code = -1) {
        SandboxResult r{};
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        r.violationCount = 0;
        return r;
    }
};

// ============================================================================
// Sandbox Statistics
// ============================================================================
struct SandboxStats {
    uint64_t totalExtensionsLoaded;
    uint64_t totalViolations;
    uint64_t extensionsKilled;
    uint64_t nativeCallsAllowed;
    uint64_t nativeCallsBlocked;
    uint64_t fsAccessAllowed;
    uint64_t fsAccessBlocked;
    uint64_t netAccessAllowed;
    uint64_t netAccessBlocked;
    uint64_t memoryLimitHits;
    uint64_t cpuLimitHits;
};

// ============================================================================
// Violation Callback — function pointer, NOT std::function
// ============================================================================
typedef void (*ViolationCallback)(const SandboxViolation* violation, void* userData);

// ============================================================================
// PluginSandbox — Singleton
// ============================================================================
class PluginSandbox {
public:
    static PluginSandbox& instance();

    // ======================================================================
    // Lifecycle
    // ======================================================================

    /// Initialize the sandbox system
    SandboxResult initialize();

    /// Shutdown and release all sandbox contexts
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // ======================================================================
    // Extension Registration
    // ======================================================================

    /// Register an extension with a specific security tier
    SandboxResult registerExtension(const char* extensionId, SecurityTier tier);

    /// Register with full custom config
    SandboxResult registerExtensionWithConfig(const char* extensionId,
                                               const SandboxConfig& config);

    /// Unregister (kill and cleanup)
    void unregisterExtension(const char* extensionId);

    /// Get the config for an extension
    const SandboxConfig* getConfig(const char* extensionId) const;

    // ======================================================================
    // Access Control — called by JSExtensionHost before executing
    // ======================================================================

    /// Check if a native function call is allowed
    bool isNativeFuncAllowed(const char* extensionId, const char* funcName);

    /// Check if a file access is allowed
    bool isFileAccessAllowed(const char* extensionId, const wchar_t* filePath,
                              bool isWrite);

    /// Check if a network access is allowed
    bool isNetworkAccessAllowed(const char* extensionId, const char* hostname,
                                 uint16_t port);

    /// Check if a module import is allowed
    bool isModuleImportAllowed(const char* extensionId, const char* moduleName);

    /// Report a memory allocation (returns false if would exceed limit)
    bool checkMemoryAllocation(const char* extensionId, uint64_t allocationSize);

    /// Report CPU time consumed (returns false if limit exceeded)
    bool checkCPUTime(const char* extensionId, uint32_t elapsedMs);

    // ======================================================================
    // Violation Handling
    // ======================================================================

    /// Set the violation callback
    void setViolationCallback(ViolationCallback callback, void* userData);

    /// Get recent violations for an extension
    uint32_t getViolations(const char* extensionId,
                            SandboxViolation* outViolations,
                            uint32_t maxCount) const;

    /// Get total violation count for an extension
    uint32_t getViolationCount(const char* extensionId) const;

    // ======================================================================
    // Default Configurations
    // ======================================================================

    /// Create default config for a given tier
    static SandboxConfig createDefaultConfig(SecurityTier tier);

    /// Add the standard RawrXD API functions to a whitelist (safe subset)
    static void addStandardAPIWhitelist(SandboxConfig& config);

    // ======================================================================
    // Statistics & Diagnostics
    // ======================================================================

    SandboxStats getStats() const;
    void resetStats();

    /// Dump sandbox state for all extensions to debug output
    void dumpState() const;

private:
    PluginSandbox();
    ~PluginSandbox();
    PluginSandbox(const PluginSandbox&) = delete;
    PluginSandbox& operator=(const PluginSandbox&) = delete;

    // Record a violation and optionally kill the extension
    void recordViolation(const char* extensionId, ViolationType type,
                          const char* detail);

    // Find extension index by ID (-1 if not found)
    int findExtension(const char* extensionId) const;

    struct ExtensionSlot {
        char            id[128];
        SandboxConfig   config;
        bool            active;
        uint64_t        memoryUsed;
        uint32_t        cpuTimeMs;
        uint32_t        violationCount;

        // Recent violations ring buffer
        SandboxViolation violations[32];
        uint32_t         violationHead;
    };

    bool                m_initialized;
    mutable std::mutex  m_mutex;
    ExtensionSlot       m_extensions[SANDBOX_MAX_EXTENSIONS];
    uint32_t            m_extensionCount;

    ViolationCallback   m_violationCallback;
    void*               m_violationUserData;

    SandboxStats        m_stats;
};

} // namespace Sandbox
} // namespace RawrXD
