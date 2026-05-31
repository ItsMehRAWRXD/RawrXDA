// ============================================================================
// ExtensionPermissions.hpp — Extension Security & Permission System
// ============================================================================
//
// Phase 29A: Extension Permissions & Security Framework
//
// Purpose:
//   Provides fine-grained permission control for extension API access.
//   Implements security boundaries for VS Code Extension API compatibility.
//   Enforces workspace trust, API proposals, and resource access limits.
//
// Features:
//   - API Proposal Permission Enforcement (vscode enabledApiProposals)
//   - File System Access Control (workspace-scoped, read/write permissions)
//   - Network Access Control (HTTP/HTTPS outbound)
//   - Command Execution Limits (subprocess restrictions)
//   - Resource Usage Limits (memory, CPU, file handles)
//   - Workspace Trust Integration
//   - Permission Manifest Validation
//
// Design:
//   - Fail-closed gates (deny by default)
//   - Per-extension permission context
//   - Runtime permission checking
//   - Audit logging for security events
//   - Compatible with existing VSIXLoader + VSCodeExtensionAPI
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>

// ============================================================================
// Permission Types & API Scopes
// ============================================================================

enum class ExtensionPermission : uint32_t {
    // File System Access
    FileSystemRead          = 0x00000001,  // Read workspace files
    FileSystemWrite         = 0x00000002,  // Write workspace files
    FileSystemGlobal        = 0x00000004,  // Access outside workspace
    
    // Network Access
    NetworkHTTP             = 0x00000010,  // HTTP/HTTPS requests
    NetworkLocalhost        = 0x00000020,  // Localhost connections
    NetworkPrivate          = 0x00000040,  // Private network ranges
    
    // Process & Command Execution
    ProcessSpawn            = 0x00000100,  // Spawn child processes
    ProcessShell            = 0x00000200,  // Shell command execution
    ProcessDebugger         = 0x00000400,  // Debugger attachment
    
    // VS Code API Access Levels
    APIStable               = 0x00001000,  // Stable VS Code APIs
    APIProposed             = 0x00002000,  // Proposed API access
    APIInternal             = 0x00004000,  // Internal/private APIs
    
    // Workspace & Environment
    WorkspaceSettings       = 0x00010000,  // Modify workspace settings
    GlobalSettings          = 0x00020000,  // Modify global settings
    EnvironmentVariables    = 0x00040000,  // Read/write env vars
    
    // Security Sensitive
    KeychainAccess          = 0x00100000,  // OS keychain/credential store
    ClipboardAccess         = 0x00200000,  // System clipboard
    ScreenCapture           = 0x00400000,  // Screen recording/capture
    
    // Extension Management
    ExtensionInstall        = 0x01000000,  // Install other extensions
    ExtensionManage         = 0x02000000,  // Enable/disable extensions
    ExtensionData           = 0x04000000,  // Access other extension data
    
    // Administrative
    SystemAdmin             = 0x10000000,  // Administrative privileges
    TrustedWorkspaceOnly    = 0x20000000,  // Only in trusted workspaces
    DeveloperMode           = 0x40000000,  // Development/debug mode only
};

// Permission combinations for common extension types
static constexpr uint32_t PERMISSION_READ_ONLY = 
    static_cast<uint32_t>(ExtensionPermission::FileSystemRead) |
    static_cast<uint32_t>(ExtensionPermission::APIStable);

static constexpr uint32_t PERMISSION_EDITOR_BASIC = 
    static_cast<uint32_t>(ExtensionPermission::FileSystemRead) |
    static_cast<uint32_t>(ExtensionPermission::FileSystemWrite) |
    static_cast<uint32_t>(ExtensionPermission::APIStable) |
    static_cast<uint32_t>(ExtensionPermission::WorkspaceSettings);

static constexpr uint32_t PERMISSION_LANGUAGE_SERVER = 
    PERMISSION_EDITOR_BASIC |
    static_cast<uint32_t>(ExtensionPermission::ProcessSpawn) |
    static_cast<uint32_t>(ExtensionPermission::NetworkLocalhost);

static constexpr uint32_t PERMISSION_DEBUGGER = 
    PERMISSION_LANGUAGE_SERVER |
    static_cast<uint32_t>(ExtensionPermission::ProcessDebugger) |
    static_cast<uint32_t>(ExtensionPermission::APIProposed);

static constexpr uint32_t PERMISSION_FULL_TRUST = 0xFFFFFFFF;

// ============================================================================
// Resource Limits
// ============================================================================

struct ExtensionResourceLimits {
    size_t      maxMemoryBytes;         // Maximum memory usage
    uint32_t    maxOpenFiles;           // Maximum open file handles
    uint32_t    maxProcesses;           // Maximum child processes
    uint32_t    maxNetworkConnections;  // Maximum network connections
    uint32_t    maxAPICallsPerSecond;   // Rate limiting
    std::chrono::milliseconds maxExecutionTime; // Max command execution time
    
    // Default limits for untrusted extensions
    static ExtensionResourceLimits getDefault() {
        return {
            .maxMemoryBytes = 256 * 1024 * 1024,   // 256 MB
            .maxOpenFiles = 64,
            .maxProcesses = 4,
            .maxNetworkConnections = 16,
            .maxAPICallsPerSecond = 100,
            .maxExecutionTime = std::chrono::seconds(30)
        };
    }
    
    // Limits for trusted workspace extensions
    static ExtensionResourceLimits getTrusted() {
        return {
            .maxMemoryBytes = 1024 * 1024 * 1024,  // 1 GB
            .maxOpenFiles = 256,
            .maxProcesses = 16,
            .maxNetworkConnections = 64,
            .maxAPICallsPerSecond = 1000,
            .maxExecutionTime = std::chrono::minutes(5)
        };
    }
};

// ============================================================================
// Extension Manifest Permission Declaration
// ============================================================================

struct ExtensionPermissionManifest {
    std::vector<std::string>            requestedPermissions;       // Human-readable permissions
    std::unordered_set<std::string>     enabledApiProposals;        // VS Code API proposals
    std::vector<std::string>            fileSystemPaths;            // Specific path access
    std::vector<std::string>            networkHosts;               // Allowed network hosts
    std::vector<std::string>            commandPatterns;            // Allowed command patterns
    bool                                requireTrustedWorkspace;    // Trust requirement
    bool                                requireDeveloperMode;       // Development-only
    ExtensionResourceLimits             resourceLimits;             // Custom limits
    
    // Parse from VS Code package.json manifest
    static ExtensionPermissionManifest fromVSCodeManifest(const std::string& manifestJson);
    
    // Validate manifest against security policies
    bool validate(std::string& errorMessage) const;
};

// ============================================================================
// Runtime Permission Context
// ============================================================================

struct ExtensionRuntimeContext {
    std::string                 extensionId;
    uint32_t                    grantedPermissions;     // Bitmask of granted permissions
    ExtensionResourceLimits     resourceLimits;
    bool                        isTrustedWorkspace;
    bool                        isDeveloperMode;
    std::atomic<size_t>         currentMemoryUsage;
    std::atomic<uint32_t>       currentOpenFiles;
    std::atomic<uint32_t>       currentProcesses;
    std::atomic<uint32_t>       currentNetworkConnections;
    std::chrono::steady_clock::time_point lastAPICall;
    std::atomic<uint32_t>       apiCallCount;
    std::mutex                  contextMutex;
};

// ============================================================================
// Permission Checker & Enforcer
// ============================================================================

class ExtensionPermissionManager {
public:
    ExtensionPermissionManager();
    ~ExtensionPermissionManager();
    
    // Extension registration & lifecycle
    bool registerExtension(
        const std::string& extensionId,
        const ExtensionPermissionManifest& manifest,
        bool isTrustedWorkspace,
        bool isDeveloperMode,
        std::string& errorMessage
    );
    
    bool unregisterExtension(const std::string& extensionId);
    
    // Permission checking
    bool hasPermission(const std::string& extensionId, ExtensionPermission permission) const;
    bool canAccessFile(const std::string& extensionId, const std::string& filePath, bool write = false) const;
    bool canConnectTo(const std::string& extensionId, const std::string& host, uint16_t port) const;
    bool canSpawnProcess(const std::string& extensionId, const std::string& command) const;
    bool canUseAPIProposal(const std::string& extensionId, const std::string& proposalName) const;
    
    // Resource management
    bool requestResource(const std::string& extensionId, const std::string& resourceType, size_t amount = 1);
    void releaseResource(const std::string& extensionId, const std::string& resourceType, size_t amount = 1);
    bool checkResourceLimits(const std::string& extensionId, std::string& limitReached) const;
    
    // API rate limiting
    bool checkAPIRateLimit(const std::string& extensionId);
    
    // Audit & logging
    void logSecurityEvent(const std::string& extensionId, const std::string& event, const std::string& details);
    std::vector<std::string> getSecurityLogs(const std::string& extensionId = "") const;
    
    // Configuration
    void setWorkspaceTrusted(bool trusted);
    void setDeveloperMode(bool enabled);
    bool isWorkspaceTrusted() const { return m_workspaceTrusted; }
    bool isDeveloperModeEnabled() const { return m_developerMode; }
    
    // Built-in extension permissions (for RawrXD system extensions)
    void grantBuiltinPermissions(const std::string& extensionId);
    
private:
    std::unordered_map<std::string, ExtensionRuntimeContext> m_extensions;
    mutable std::mutex m_extensionsMutex;
    
    bool m_workspaceTrusted;
    bool m_developerMode;
    
    // Security audit log
    struct SecurityEvent {
        std::chrono::steady_clock::time_point timestamp;
        std::string extensionId;
        std::string event;
        std::string details;
    };
    std::vector<SecurityEvent> m_auditLog;
    mutable std::mutex m_auditMutex;
    
    // Helper methods
    bool validateManifest(const ExtensionPermissionManifest& manifest, std::string& error) const;
    uint32_t parsePermissions(const std::vector<std::string>& permissions) const;
    std::string getWorkspaceRoot() const;
    bool isPathInWorkspace(const std::string& path) const;
    bool isNetworkHostAllowed(const std::string& host, const std::vector<std::string>& allowedHosts) const;
    bool isCommandAllowed(const std::string& command, const std::vector<std::string>& patterns) const;
};

// ============================================================================
// Global Permission Manager Instance
// ============================================================================

// Singleton instance for global access from VSCodeExtensionAPI
ExtensionPermissionManager& GetExtensionPermissionManager();

// ============================================================================
// Permission Check Macros (for VSCodeExtensionAPI)
// ============================================================================

#define EXTENSION_PERMISSION_CHECK(extensionId, permission) \
    do { \
        if (!GetExtensionPermissionManager().hasPermission(extensionId, permission)) { \
            GetExtensionPermissionManager().logSecurityEvent( \
                extensionId, "Permission Denied", #permission \
            ); \
            return VSCodeAPIResult::error("Permission denied: " #permission, -403); \
        } \
    } while(0)

#define EXTENSION_FILE_ACCESS_CHECK(extensionId, filePath, write) \
    do { \
        if (!GetExtensionPermissionManager().canAccessFile(extensionId, filePath, write)) { \
            GetExtensionPermissionManager().logSecurityEvent( \
                extensionId, "File Access Denied", filePath \
            ); \
            return VSCodeAPIResult::error("File access denied", -403); \
        } \
    } while(0)

#define EXTENSION_API_RATE_LIMIT_CHECK(extensionId) \
    do { \
        if (!GetExtensionPermissionManager().checkAPIRateLimit(extensionId)) { \
            GetExtensionPermissionManager().logSecurityEvent( \
                extensionId, "Rate Limit Exceeded", "API calls per second" \
            ); \
            return VSCodeAPIResult::error("Rate limit exceeded", -429); \
        } \
    } while(0)

#define EXTENSION_RESOURCE_CHECK(extensionId) \
    do { \
        std::string limitReached; \
        if (!GetExtensionPermissionManager().checkResourceLimits(extensionId, limitReached)) { \
            GetExtensionPermissionManager().logSecurityEvent( \
                extensionId, "Resource Limit Exceeded", limitReached \
            ); \
            return VSCodeAPIResult::error(("Resource limit exceeded: " + limitReached).c_str(), -507); \
        } \
    } while(0)

// ============================================================================
// VS Code API Integration Helpers
// ============================================================================

// Get extension ID from current execution context (implemented in VSCodeExtensionAPI)
std::string GetCurrentExtensionId();

// Mark extension as built-in system component (bypass all permission checks)
void MarkExtensionAsSystemBuiltin(const std::string& extensionId);

// Register extension from VSIX manifest parsing
bool RegisterExtensionFromVSIX(
    const std::string& extensionId,
    const std::string& manifestJson,
    bool isTrustedWorkspace,
    std::string& errorMessage
);
