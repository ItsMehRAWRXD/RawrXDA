// UEC-X Microkernel - Security Sandbox
// Capability-based security and isolation

#pragma once

#include "uec_core.h"
#include <unordered_set>
#include <unordered_map>

namespace uec {

// =============================================================================
// Security Policy
// =============================================================================

struct SecurityPolicy {
    CapabilityMask allowedCapabilities = 0;
    std::unordered_set<std::string> allowedPaths;
    std::unordered_set<std::string> allowedNetworkHosts;
    std::unordered_set<std::string> allowedEnvironmentVariables;
    uint32_t maxMemoryMB = 512;
    uint32_t maxThreads = 4;
    uint32_t maxFileDescriptors = 256;
    uint32_t maxIPCChannels = 16;
    bool allowDynamicCode = false;
    bool allowProcessSpawning = false;
    bool allowNetworkAccess = false;
    bool allowFileSystemAccess = false;
};

// =============================================================================
// Security Context
// =============================================================================

class UEC_API SecurityContext {
public:
    SecurityContext(ExtensionId owner, const SecurityPolicy& policy);
    ~SecurityContext();

    // Capability checking
    bool HasCapability(Capability cap) const;
    bool HasCapabilities(CapabilityMask caps) const;
    Result<void> RequireCapability(Capability cap) const;
    Result<void> RequireCapabilities(CapabilityMask caps) const;

    // Path validation
    bool IsPathAllowed(const std::string& path) const;
    Result<void> ValidatePath(const std::string& path) const;

    // Network validation
    bool IsHostAllowed(const std::string& host) const;
    Result<void> ValidateHost(const std::string& host) const;

    // Resource tracking
    void TrackMemoryAllocation(size_t bytes);
    void TrackMemoryDeallocation(size_t bytes);
    bool CheckMemoryLimit(size_t requestedBytes) const;
    
    void TrackThreadCreation();
    void TrackThreadDestruction();
    bool CheckThreadLimit() const;
    
    void TrackFileOpen();
    void TrackFileClose();
    bool CheckFileLimit() const;

    // Policy updates
    Result<void> GrantCapability(Capability cap);
    Result<void> RevokeCapability(Capability cap);
    Result<void> AddAllowedPath(const std::string& path);
    Result<void> RemoveAllowedPath(const std::string& path);

    // Audit
    struct AuditEntry {
        Timestamp timestamp;
        std::string operation;
        std::string resource;
        bool allowed;
        std::string reason;
    };
    std::vector<AuditEntry> GetAuditLog() const;
    void ClearAuditLog();

private:
    ExtensionId m_owner;
    SecurityPolicy m_policy;
    
    std::atomic<size_t> m_memoryUsed{0};
    std::atomic<uint32_t> m_threadCount{0};
    std::atomic<uint32_t> m_fileCount{0};
    
    mutable std::mutex m_auditMutex;
    std::vector<AuditEntry> m_auditLog;
    
    void LogAudit(const std::string& operation, const std::string& resource, 
                  bool allowed, const std::string& reason);
};

// =============================================================================
// Security Sandbox
// =============================================================================

class UEC_API SecuritySandbox {
public:
    SecuritySandbox();
    ~SecuritySandbox();

    // Non-copyable, non-movable
    SecuritySandbox(const SecuritySandbox&) = delete;
    SecuritySandbox& operator=(const SecuritySandbox&) = delete;
    SecuritySandbox(SecuritySandbox&&) = delete;
    SecuritySandbox& operator=(SecuritySandbox&&) = delete;

    // Lifecycle
    Result<void> Initialize();
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Context management
    Result<void> CreateContext(ExtensionId id, const SecurityPolicy& policy);
    Result<void> DestroyContext(ExtensionId id);
    std::shared_ptr<SecurityContext> GetContext(ExtensionId id) const;
    bool HasContext(ExtensionId id) const;

    // Default policies
    void SetDefaultPolicy(const SecurityPolicy& policy);
    SecurityPolicy GetDefaultPolicy() const;
    
    SecurityPolicy CreateMinimalPolicy();
    SecurityPolicy CreateStandardPolicy();
    SecurityPolicy CreateElevatedPolicy();
    SecurityPolicy CreateUnrestrictedPolicy();

    // System-wide enforcement
    Result<void> EnforceSystemPolicy();
    Result<void> RelaxSystemPolicy();

    // Audit
    std::vector<SecurityContext::AuditEntry> GetGlobalAuditLog() const;
    void ClearGlobalAuditLog();

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<ExtensionId, std::shared_ptr<SecurityContext>> m_contexts;
    SecurityPolicy m_defaultPolicy;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_systemPolicyEnforced{false};
};

} // namespace uec
