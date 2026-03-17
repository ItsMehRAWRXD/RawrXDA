// ============================================================================
// rbac_engine.hpp — Enterprise Role-Based Access Control + SSO/SAML Bridge
// ============================================================================
// Gap #5: Enterprise Auth (RBAC, SSO/SAML, audit trail, compliance)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
//
// Architecture:
//   - Role hierarchy with permission inheritance
//   - Fine-grained permission checks on all hotpatch layers
//   - SSO/SAML token validation bridge (pluggable provider)
//   - Audit event log with tamper-detection (SHA-256 chain)
//   - Session management with lease-based expiry
//   - Compliance policy enforcement (SOC2, HIPAA flags)
//   - No exceptions. PatchResult-style returns.
//   - No std::function. Function pointers only.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

namespace RawrXD {
namespace Auth {

// ============================================================================
// Result type — mirrors PatchResult convention
// ============================================================================
struct AuthResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static AuthResult ok(const char* msg) {
        return {true, msg, 0};
    }
    static AuthResult denied(const char* msg, int code = 403) {
        return {false, msg, code};
    }
    static AuthResult error(const char* msg, int code = 500) {
        return {false, msg, code};
    }
};

// ============================================================================
// Permission — Fine-grained action identifiers
// ============================================================================
enum class Permission : uint32_t {
    // Hotpatch layers
    HOTPATCH_MEMORY_READ        = 0x0001,
    HOTPATCH_MEMORY_WRITE       = 0x0002,
    HOTPATCH_MEMORY_REVERT      = 0x0004,
    HOTPATCH_BYTE_READ          = 0x0008,
    HOTPATCH_BYTE_WRITE         = 0x0010,
    HOTPATCH_SERVER_ADD         = 0x0020,
    HOTPATCH_SERVER_REMOVE      = 0x0040,
    HOTPATCH_LIVE_BINARY        = 0x0080,
    HOTPATCH_PT_DRIVER          = 0x0100,

    // Model / inference
    MODEL_LOAD                  = 0x0200,
    MODEL_UNLOAD                = 0x0400,
    MODEL_QUANTIZE              = 0x0800,
    INFERENCE_EXECUTE           = 0x1000,
    INFERENCE_CONFIG            = 0x2000,

    // Extension marketplace
    EXTENSION_INSTALL           = 0x4000,
    EXTENSION_UNINSTALL         = 0x8000,
    EXTENSION_ACTIVATE          = 0x00010000,
    MARKETPLACE_BROWSE          = 0x00020000,

    // Workspace / files
    WORKSPACE_READ              = 0x00040000,
    WORKSPACE_WRITE             = 0x00080000,
    WORKSPACE_ADMIN             = 0x00100000,

    // Admin
    ADMIN_USER_MANAGE           = 0x00200000,
    ADMIN_ROLE_MANAGE           = 0x00400000,
    ADMIN_AUDIT_VIEW            = 0x00800000,
    ADMIN_POLICY_MANAGE         = 0x01000000,
    ADMIN_SESSION_MANAGE        = 0x02000000,

    // Agent
    AGENT_EXECUTE               = 0x04000000,
    AGENT_CONFIGURE             = 0x08000000,

    // Superuser
    ALL_PERMISSIONS             = 0xFFFFFFFF
};

// Bitwise ops for Permission
inline Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline Permission operator&(Permission a, Permission b) {
    return static_cast<Permission>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasPermission(Permission mask, Permission check) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(check)) ==
           static_cast<uint32_t>(check);
}

// ============================================================================
// Role — Named collection of permissions with hierarchy
// ============================================================================
struct Role {
    std::string         name;
    std::string         description;
    Permission          permissions;       // Bitfield of granted permissions
    std::string         parentRole;        // Inherits from this role (empty = none)
    bool                isBuiltin;         // System-defined, cannot be deleted
    uint64_t            createdAt;
    uint64_t            modifiedAt;
};

// ============================================================================
// User — Authenticated identity
// ============================================================================
struct User {
    std::string         id;                // Unique user ID (UUID or SSO sub)
    std::string         username;
    std::string         email;
    std::string         displayName;
    std::vector<std::string> roles;        // Assigned role names
    bool                isActive;
    bool                isLocked;
    uint64_t            createdAt;
    uint64_t            lastLoginAt;
    uint32_t            failedLoginCount;
    std::string         ssoProvider;       // "local", "saml", "oidc", "github"
    std::string         ssoSubject;        // External SSO subject claim

    // Merged permission mask (computed from all roles)
    Permission          effectivePermissions;
};

// ============================================================================
// Session — Authenticated session with lease
// ============================================================================
struct Session {
    char                token[128];        // Session token (opaque)
    std::string         userId;
    uint64_t            createdAt;
    uint64_t            expiresAt;
    uint64_t            lastActivityAt;
    bool                isValid;
    std::string         sourceIp;
    std::string         userAgent;

    // Cached effective permissions for fast checks
    Permission          cachedPermissions;
};

// ============================================================================
// AuditEntry — Tamper-evident log entry
// ============================================================================
struct AuditEntry {
    uint64_t            sequenceId;
    uint64_t            timestamp;
    std::string         userId;
    std::string         action;            // "hotpatch.memory.write", etc.
    std::string         resource;          // Target of the action
    std::string         detail;            // Additional context
    AuthResult          result;            // Was action permitted?
    uint8_t             prevHash[32];      // SHA-256 of previous entry
    uint8_t             entryHash[32];     // SHA-256 of this entry
};

// ============================================================================
// SSOProviderConfig — SSO/SAML/OIDC provider configuration
// ============================================================================
struct SSOProviderConfig {
    enum ProviderType : uint8_t {
        LOCAL       = 0,
        SAML_2_0    = 1,
        OIDC        = 2,
        GITHUB_APP  = 3,
        AZURE_AD    = 4,
        CUSTOM      = 5
    };

    ProviderType        type;
    std::string         name;              // Provider display name
    std::string         entityId;          // SAML Entity ID / OIDC Issuer
    std::string         metadataUrl;       // SAML metadata / OIDC discovery
    std::string         loginUrl;          // SSO login endpoint
    std::string         callbackUrl;       // Post-auth redirect
    std::string         clientId;          // OAuth client ID
    std::string         clientSecret;      // OAuth client secret (encrypted)
    std::string         certificate;       // SAML X.509 cert (PEM)
    bool                enabled;
    bool                autoCreateUsers;   // Create user on first login
    std::string         defaultRole;       // Role for auto-created users
    uint32_t            sessionTimeoutSec; // Session TTL
};

// ============================================================================
// ComplianceConfig — SOC2/HIPAA/GDP compliance flags
// ============================================================================
struct ComplianceConfig {
    bool        enforceAuditLog;           // All actions must be logged
    bool        enforceSessionTimeout;     // Sessions must expire
    uint32_t    maxSessionDurationSec;     // Max session lifetime
    uint32_t    maxIdleTimeoutSec;         // Max idle before auto-logout
    uint32_t    maxFailedLogins;           // Lockout threshold
    uint32_t    lockoutDurationSec;        // Lockout duration
    bool        requireMFA;                // Multi-factor auth required
    bool        enforcePasswordComplexity; // Password rules
    uint32_t    passwordMinLength;
    bool        auditHashChain;            // Tamper-evident audit entries
    bool        encryptSessionTokens;      // Encrypt tokens at rest
    bool        maskSensitiveData;         // Redact PII in logs

    ComplianceConfig()
        : enforceAuditLog(true),
          enforceSessionTimeout(true),
          maxSessionDurationSec(28800),      // 8 hours
          maxIdleTimeoutSec(1800),           // 30 min
          maxFailedLogins(5),
          lockoutDurationSec(900),           // 15 min
          requireMFA(false),
          enforcePasswordComplexity(true),
          passwordMinLength(12),
          auditHashChain(true),
          encryptSessionTokens(false),
          maskSensitiveData(true) {}
};

// ============================================================================
// Callback types — function pointers only (no std::function)
// ============================================================================
typedef void (*AuditEventCallback)(const AuditEntry* entry, void* userData);
typedef void (*SessionEventCallback)(const Session* session, bool created, void* userData);
typedef AuthResult (*ExternalAuthProvider)(const char* token, User* outUser, void* ctx);

// ============================================================================
// RBACEngine — Singleton
// ============================================================================
class RBACEngine {
public:
    static RBACEngine& instance();

    // ---- Initialization ----
    AuthResult initialize();
    AuthResult shutdown();
    AuthResult loadFromFile(const char* configPath);
    AuthResult saveToFile(const char* configPath) const;

    // ---- Role Management ----
    AuthResult createRole(const Role& role);
    AuthResult deleteRole(const std::string& roleName);
    AuthResult updateRole(const Role& role);
    AuthResult getRole(const std::string& roleName, Role& outRole) const;
    std::vector<Role> listRoles() const;
    Permission resolvePermissions(const std::string& roleName) const;

    // ---- User Management ----
    AuthResult createUser(const User& user);
    AuthResult deleteUser(const std::string& userId);
    AuthResult updateUser(const User& user);
    AuthResult getUser(const std::string& userId, User& outUser) const;
    AuthResult getUserByUsername(const std::string& username, User& outUser) const;
    std::vector<User> listUsers() const;
    AuthResult assignRole(const std::string& userId, const std::string& roleName);
    AuthResult revokeRole(const std::string& userId, const std::string& roleName);
    AuthResult lockUser(const std::string& userId);
    AuthResult unlockUser(const std::string& userId);

    // ---- Authentication ----
    AuthResult authenticateLocal(const std::string& username,
                                  const std::string& passwordHash,
                                  Session& outSession);
    AuthResult authenticateSSO(const std::string& providerName,
                                const std::string& assertion,
                                Session& outSession);
    AuthResult validateSession(const char* token, Session& outSession) const;
    AuthResult invalidateSession(const char* token);
    AuthResult invalidateAllSessions(const std::string& userId);
    AuthResult refreshSession(const char* token, Session& outSession);

    // ---- Authorization (Core Permission Check) ----
    AuthResult checkPermission(const char* sessionToken,
                                Permission required) const;
    AuthResult checkPermission(const Session& session,
                                Permission required) const;
    AuthResult checkPermission(const User& user,
                                Permission required) const;

    // Convenience: check + audit in one call
    AuthResult authorize(const char* sessionToken,
                         Permission required,
                         const char* action,
                         const char* resource);

    // ---- SSO Provider Config ----
    AuthResult addSSOProvider(const SSOProviderConfig& config);
    AuthResult removeSSOProvider(const std::string& name);
    AuthResult getSSOProvider(const std::string& name,
                               SSOProviderConfig& outConfig) const;
    std::vector<SSOProviderConfig> listSSOProviders() const;

    // ---- External Auth Hook ----
    void setExternalAuthProvider(ExternalAuthProvider provider, void* ctx);

    // ---- Compliance ----
    AuthResult applyComplianceConfig(const ComplianceConfig& config);
    ComplianceConfig getComplianceConfig() const;

    // ---- Audit Log ----
    void logAudit(const std::string& userId,
                  const std::string& action,
                  const std::string& resource,
                  const std::string& detail,
                  const AuthResult& result);
    std::vector<AuditEntry> getAuditLog(uint64_t fromSeq,
                                         uint32_t maxEntries) const;
    AuthResult verifyAuditChain() const;        // Verify hash chain integrity
    AuthResult exportAuditLog(const char* path) const;
    uint64_t getAuditEntryCount() const;

    // ---- Event Listeners ----
    void addAuditListener(AuditEventCallback cb, void* userData);
    void removeAuditListener(AuditEventCallback cb);
    void addSessionListener(SessionEventCallback cb, void* userData);
    void removeSessionListener(SessionEventCallback cb);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalAuthAttempts{0};
        std::atomic<uint64_t> successfulAuths{0};
        std::atomic<uint64_t> failedAuths{0};
        std::atomic<uint64_t> permissionChecks{0};
        std::atomic<uint64_t> permissionDenials{0};
        std::atomic<uint64_t> activeSessions{0};
        std::atomic<uint64_t> auditEntries{0};
    };

    const Stats& getStats() const;
    void resetStats();

private:
    RBACEngine();
    ~RBACEngine();
    RBACEngine(const RBACEngine&) = delete;
    RBACEngine& operator=(const RBACEngine&) = delete;

    // Internal helpers
    void computeEffectivePermissions(User& user) const;
    void generateSessionToken(char* outToken, size_t tokenLen) const;
    void hashAuditEntry(AuditEntry& entry, const uint8_t* prevHash) const;
    void createBuiltinRoles();
    uint64_t now() const;

    // State
    mutable std::mutex                                  mutex_;
    std::atomic<bool>                                   initialized_{false};
    std::unordered_map<std::string, Role>               roles_;       // name→Role
    std::unordered_map<std::string, User>               users_;       // id→User
    std::unordered_map<std::string, std::string>        usernameIndex_; // username→id
    std::unordered_map<std::string, std::string>        passwordHashes_; // userId→hash
    std::unordered_map<std::string, Session>            sessions_;    // token→Session
    std::unordered_map<std::string, SSOProviderConfig>  ssoProviders_;
    ComplianceConfig                                    compliance_;
    Stats                                               stats_;

    // Audit log (ring buffer for memory, flush to disk)
    static constexpr size_t MAX_AUDIT_ENTRIES = 16384;
    std::vector<AuditEntry>                             auditLog_;
    std::atomic<uint64_t>                               auditSequence_{0};
    uint8_t                                             lastAuditHash_[32];

    // External auth provider hook
    ExternalAuthProvider                                externalAuthProvider_{nullptr};
    void*                                               externalAuthCtx_{nullptr};

    // Event listeners
    static constexpr size_t MAX_LISTENERS = 16;
    struct AuditListenerEntry { AuditEventCallback callback; void* userData; };
    struct SessionListenerEntry { SessionEventCallback callback; void* userData; };
    AuditListenerEntry                                  auditListeners_[MAX_LISTENERS];
    std::atomic<uint32_t>                               auditListenerCount_{0};
    SessionListenerEntry                                sessionListeners_[MAX_LISTENERS];
    std::atomic<uint32_t>                               sessionListenerCount_{0};
};

} // namespace Auth
} // namespace RawrXD
