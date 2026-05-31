// ============================================================================
// workspace_trust_integration.h — Workspace Trust Security Boundary
// ============================================================================
// PURPOSE:
//   Implements VS Code workspace trust model for extension security:
//   - Classify workspaces as trusted/untrusted
//   - Restrict extension capabilities in untrusted workspaces
//   - Ask user for trust before allowing privileged operations
//   - Workspace trust badge and UI indicators
//   - Trust policy enforcement per extension
//
// Architecture: C++20 | Win32 | No exceptions | Qt-free | Security-first
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <cstdint>
#include <mutex>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Workspace Trust State
// ============================================================================

enum class WorkspaceTrustState {
    Unknown,                // Not yet determined
    Trusted,                // Explicitly trusted by user
    Untrusted,              // Explicitly marked untrusted
    RestrictedMode,         // Running in restricted safety mode
};

// ============================================================================
// Trust-Guarded Capabilities
// ============================================================================

enum class GuardedCapability {
    ExecuteCommand,         // Run arbitrary commands
    TerminalAccess,         // Access to integrated terminal
    ProcessSpawn,           // Spawn child processes
    FileWrite,              // Write files to workspace
    NetworkRequest,         // Make network requests
    ExtensionScripting,     // Run arbitrary extension code
    TaskExecution,          // Run build tasks
    DebuggerAccess,         // Access debugger protocol
};

// ============================================================================
// Trust Decision
// ============================================================================

struct TrustDecision {
    std::string workspacePath;
    WorkspaceTrustState state = WorkspaceTrustState::Unknown;
    uint64_t decisionTimeMs = 0;
    std::string decidingUser;           // User who made the decision
    bool rememberDecision = false;      // Persist decision
};

// ============================================================================
// Trust Verification Result
// ============================================================================

struct TrustVerificationResult {
    bool allowed = false;
    std::string reason;
    WorkspaceTrustState workspaceState = WorkspaceTrustState::Unknown;
    GuardedCapability capability;
};

// ============================================================================
// Workspace Trust Manager
// ============================================================================

class WorkspaceTrustManager {
public:
    explicit WorkspaceTrustManager();
    ~WorkspaceTrustManager();

    // ── Trust State Queries ────────────────────────────────────────

    // Get trust state for workspace path
    WorkspaceTrustState GetWorkspaceTrustState(const std::string& workspacePath);

    // Check if workspace is trusted
    bool IsWorkspaceTrusted(const std::string& workspacePath) const;

    // Check if in restricted mode
    bool IsRestrictedMode() const;

    // ── Trust State Management ─────────────────────────────────────

    // Mark workspace as trusted
    bool TrustWorkspace(const std::string& workspacePath);

    // Mark workspace as untrusted
    bool UntrustWorkspace(const std::string& workspacePath);

    // Reset trust decision
    bool ResetWorkspaceTrust(const std::string& workspacePath);

    // ── Capability Guarding ────────────────────────────────────────

    // Check if capability is allowed for workspace
    TrustVerificationResult VerifyCapability(const std::string& workspacePath,
                                             GuardedCapability capability,
                                             const std::string& extensionId = "");

    // Pre-check if capability might be allowed (for UI hints)
    bool CanCapabilityBeAllowed(const std::string& workspacePath,
                                GuardedCapability capability) const;

    // ── User Prompts ───────────────────────────────────────────────

    // Request user to trust workspace
    void RequestWorkspaceTrust(const std::string& workspacePath,
                               std::function<void(bool trusted)> callback);

    // Request permission for specific capability
    void RequestCapabilityPermission(const std::string& workspacePath,
                                     GuardedCapability capability,
                                     const std::string& extensionId,
                                     std::function<void(bool allowed)> callback);

    // ── Extension Trust Policies ───────────────────────────────────

    // Set extension-specific trust policy
    void SetExtensionTrustPolicy(const std::string& extensionId,
                                 bool requiresWorkspaceTrust);

    bool ExtensionRequiresWorkspaceTrust(const std::string& extensionId) const;

    // Blacklist extension from accessing untrusted workspaces
    void BlacklistExtensionInUntrustedWorkspaces(const std::string& extensionId);

    // ── Persistence ────────────────────────────────────────────────

    // Load trust decisions from storage
    bool LoadTrustDecisions(const std::string& storagePath);

    // Save trust decisions to storage
    bool SaveTrustDecisions(const std::string& storagePath);

    // ── Statistics ─────────────────────────────────────────────────

    std::vector<std::string> GetTrustedWorkspaces() const;
    std::vector<std::string> GetUntrustedWorkspaces() const;

    // ── Restricted Mode Management ─────────────────────────────────

    // Enter restricted mode (minimal extension capabilities)
    void EnterRestrictedMode();

    // Exit restricted mode
    void ExitRestrictedMode();

private:
    mutable std::mutex m_lock;

    // Trust state storage
    std::unordered_map<std::string, TrustDecision> m_trustDecisions;

    // Guarded capabilities per trust state
    std::unordered_map<WorkspaceTrustState, std::unordered_set<GuardedCapability>>
        m_allowedCapabilities;

    // Extension policies
    std::unordered_map<std::string, bool> m_extensionTrustRequirements;  // extId -> requires trust
    std::unordered_set<std::string> m_blacklistedExtensions;

    // Restricted mode state
    std::atomic<bool> m_restrictedMode{false};

    // Callbacks for user prompts
    std::function<void(const std::string&, std::function<void(bool)>)> m_trustPromptCallback;
    std::function<void(const std::string&, GuardedCapability, const std::string&,
                       std::function<void(bool)>)> m_capabilityPermissionCallback;

    // Internal helpers
    void InitializeCapabilityPolices();
    std::string NormalizePath(const std::string& path) const;
    bool MatchesPath(const std::string& pattern, const std::string& path) const;
};

// ============================================================================
// Workspace Trust Context (RAII guard for trust-protected blocks)
// ============================================================================

class WorkspaceTrustGuard {
public:
    explicit WorkspaceTrustGuard(const std::string& workspacePath,
                                 GuardedCapability capability,
                                 const std::string& extensionId = "");

    ~WorkspaceTrustGuard();

    // Check if the operation is allowed
    bool IsAllowed() const;

    // Get reason if not allowed
    std::string GetReason() const;

private:
    std::string m_workspacePath;
    GuardedCapability m_capability;
    std::string m_extensionId;
    bool m_allowed = false;
    std::string m_reason;
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton trust manager instance
WorkspaceTrustManager& GetWorkspaceTrustManager();

// Convenience functions
bool VerifyWorkspaceTrustForCapability(const std::string& workspacePath,
                                       GuardedCapability capability);

WorkspaceTrustState GetCurrentWorkspaceTrustState(const std::string& workspacePath);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // WORKSPACE_TRUST_INTEGRATION_H
