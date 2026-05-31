// ============================================================================
// extension_permissions.h — Extension Permissions & Security Policy System
// ============================================================================
// PURPOSE:
//   Enforces permission-based security for extensions. Extensions must declare
//   required permissions in package.json and user approval is required for:
//   - File system access (read/write paths)
//   - Network access (http/https/sockets)
//   - Workspace trust (can modify files, run commands)
//   - System integration (terminal, clipboard, process spawning)
//
// Architecture: C++20 | Win32 | No exceptions | Qt-free | Function pointers
// Security Model: Deny-by-default, require explicit approval
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <functional>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Permission Types (Scopes)
// ============================================================================

enum class PermissionScope : uint32_t {
    // File System
    FileSystemRead         = 0x0001,   // Read files from specific paths
    FileSystemWrite        = 0x0002,   // Write/modify files
    FileSystemDelete       = 0x0004,   // Delete/remove files
    FileSystemExecute      = 0x0008,   // Execute files (dangerous!)
    
    // Network
    NetworkHttp            = 0x0010,   // HTTP/HTTPS requests
    NetworkWebsocket       = 0x0020,   // WebSocket connections
    NetworkTcp             = 0x0040,   // Raw TCP sockets
    NetworkUdp             = 0x0080,   // UDP sockets
    
    // Workspace
    WorkspaceRead          = 0x0100,   // Read workspace configuration
    WorkspaceWrite         = 0x0200,   // Modify workspace settings/files
    WorkspaceCommand       = 0x0400,   // Execute commands in workspace
    WorkspaceTrust         = 0x0800,   // Trusted workspace operations
    
    // System
    SystemTerminal         = 0x1000,   // Access to VS Code terminal
    SystemClipboard        = 0x2000,   // Read/write system clipboard
    SystemProcessSpawn     = 0x4000,   // Spawn child processes
    SystemEnvironment      = 0x8000,   // Read environment variables
    
    // VSCode Internal
    VscodeSecrets          = 0x10000,  // Access credential manager
    VscodeLanguageServer   = 0x20000,  // Access LSP infrastructure
    VscodeDebugger         = 0x40000,  // Access debugger protocol
    
    // Special
    DevTools               = 0x80000,  // Development tools (telemetry, logging)
    
    // Default safe set
    DefaultSafe            = FileSystemRead | NetworkHttp | VscodeLanguageServer,
};

// ============================================================================
// Permission Decision
// ============================================================================

enum class PermissionDecision {
    Denied,                 // Explicitly denied
    Approved,               // Explicitly approved (persisted)
    PromptRequired,         // Need user prompt
    TemporaryApproval,      // Approved for this session only
};

// ============================================================================
// Permission Request / Approval
// ============================================================================

struct PermissionRequest {
    std::string                extensionId;
    PermissionScope            scope;
    std::string                reason;         // Why the extension needs this permission
    std::string                detail;         // Optional path, hostname, etc.
    
    explicit PermissionRequest(const std::string& id, PermissionScope s, const std::string& r = "")
        : extensionId(id), scope(s), reason(r) {}
};

struct PermissionApproval {
    std::string                extensionId;
    PermissionScope            scope;
    PermissionDecision         decision = PermissionDecision::PromptRequired;
    bool                       rememberChoice = false;  // Persist to disk
    uint64_t                   approvalTimeMs = 0;      // Approval timestamp
    uint64_t                   expiresMs = 0;           // 0 = never expires
};

// ============================================================================
// Permission Query Result
// ============================================================================

struct PermissionCheckResult {
    bool                       allowed = false;
    std::string                reason;
    PermissionScope            requestedScope;
    PermissionScope            grantedScope;
    PermissionDecision         decision = PermissionDecision::Denied;
    
    static PermissionCheckResult Allow(PermissionScope granted) {
        PermissionCheckResult r;
        r.allowed = true;
        r.grantedScope = granted;
        r.decision = PermissionDecision::Approved;
        return r;
    }
    
    static PermissionCheckResult Deny(const std::string& reason) {
        PermissionCheckResult r;
        r.allowed = false;
        r.reason = reason;
        r.decision = PermissionDecision::Denied;
        return r;
    }
};

// ============================================================================
// Extension Permissions Manager
// ============================================================================

class ExtensionPermissionsManager {
public:
    explicit ExtensionPermissionsManager();
    ~ExtensionPermissionsManager();

    // ── Registration ───────────────────────────────────────────────────

    // Register extension with required permissions  
    bool RegisterExtension(const std::string& extensionId,
                           PermissionScope requestedScopes,
                           std::function<void()> onNeedsApproval = nullptr);

    // ── Permission Checks ──────────────────────────────────────────────

    // Check if extension has permission (may prompt user)
    PermissionCheckResult CheckPermission(const std::string& extensionId,
                                          PermissionScope scope,
                                          const std::string& detail = "");

    // Batch check multiple permissions
    bool CheckAllPermissions(const std::string& extensionId,
                             PermissionScope scope);

    // ── Approval Workflow ──────────────────────────────────────────────

    // Prompt user for permission (UI callback)
    void RequestUserApproval(const PermissionRequest& request,
                             std::function<void(const PermissionApproval&)> callback);

    // Record user approval decision
    bool RecordApproval(const PermissionApproval& approval);

    // Revoke previously granted permission
    bool RevokePermission(const std::string& extensionId, PermissionScope scope);

    // ── Queries ────────────────────────────────────────────────────────

    PermissionScope GetGrantedScopes(const std::string& extensionId) const;
    PermissionScope GetRequestedScopes(const std::string& extensionId) const;
    
    std::vector<std::string> GetApprovedExtensions() const;
    std::vector<std::string> GetDeniedExtensions() const;
    std::vector<std::string> GetPendingApprovals() const;

    // ── Configuration ──────────────────────────────────────────────────

    // Set global permission policy (default permissions for all extensions)
    void SetDefaultPolicy(PermissionScope defaultScopes);
    PermissionScope GetDefaultPolicy() const;

    // Set per-extension policy override
    void SetExtensionPolicy(const std::string& extensionId, PermissionScope scopes);

    // Workspace trust boundary
    void SetWorkspaceTrusted(bool trusted);
    bool IsWorkspaceTrusted() const;

    // ── Persistence ────────────────────────────────────────────────────

    // Load permissions from storage
    bool LoadPermissions(const std::string& storagePath);

    // Save permissions to storage
    bool SavePermissions(const std::string& storagePath);

private:
    mutable std::mutex m_lock;

    struct ExtensionPermissionRecord {
        PermissionScope requestedScopes = static_cast<PermissionScope>(0);
        PermissionScope grantedScopes = static_cast<PermissionScope>(0);
        PermissionDecision lastDecision = PermissionDecision::PromptRequired;
        uint64_t lastPromptTimeMs = 0;
        bool isBlacklisted = false;
    };

    std::unordered_map<std::string, ExtensionPermissionRecord> m_permissions;
    PermissionScope m_defaultPolicy = PermissionScope::DefaultSafe;
    bool m_workspaceTrusted = false;

    // Approval handling
    std::unordered_map<std::string, std::function<void()>> m_approvalCallbacks;

    // Internal helpers
    bool ScopeContains(PermissionScope granted, PermissionScope requested) const;
    std::string ScopeToString(PermissionScope scope) const;
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton manager instance  
ExtensionPermissionsManager& GetPermissionsManager();

// Convenience functions
PermissionCheckResult CheckExtensionPermission(const std::string& extensionId,
                                               PermissionScope scope);

bool RequestExtensionPermission(const std::string& extensionId,
                                PermissionScope scope,
                                const std::string& reason);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_PERMISSIONS_H
