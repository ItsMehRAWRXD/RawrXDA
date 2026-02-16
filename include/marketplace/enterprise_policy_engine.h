#pragma once

// ============================================================================
// EnterprisePolicyEngine — C++20, no Qt. Enterprise extension policies.
// ============================================================================

#include <list>
#include <string>
#include <vector>

/**
 * Manages enterprise policies for extension installation:
 * JWT SSO, allow/deny lists, signature verification, compliance, audit logging.
 */
class EnterprisePolicyEngine {
public:
    EnterprisePolicyEngine() = default;
    ~EnterprisePolicyEngine();

    void setAllowList(const std::vector<std::string>& extensionIds);
    void setDenyList(const std::vector<std::string>& extensionIds);
    void setRequireSignature(bool require);
    void setJwtSecret(const std::string& secret);

    bool isExtensionAllowed(const std::string& extensionId);
    bool verifyExtensionSignature(const std::string& extensionId, const std::string& signature);
    bool validateUserAccess(const std::string& userId, const std::string& jwtToken);

    void logExtensionInstallation(const std::string& extensionId, const std::string& userId);
    void logExtensionUninstallation(const std::string& extensionId, const std::string& userId);
    /** Returns JSON-serialized audit entries (replaces QList<QJsonObject>) */
    std::vector<std::string> getAuditLog(int limit = 100);

    bool isJwtValid(const std::string& token);
    std::string generateJwtToken(const std::string& userId, const std::vector<std::string>& permissions);

    using PolicyViolationFn = void(*)(const std::string& extensionId, const std::string& reason);
    using AuditLogEntryFn = void(*)(const std::string& entry);
    using ComplianceChangedFn = void(*)(bool compliant);
    void setOnPolicyViolation(PolicyViolationFn fn) { m_onPolicyViolation = fn; }
    void setOnAuditLogEntry(AuditLogEntryFn fn) { m_onAuditLogEntry = fn; }
    void setOnComplianceStatusChanged(ComplianceChangedFn fn) { m_onComplianceStatusChanged = fn; }

private:
    struct PolicySettings {
        std::vector<std::string> allowList;
        std::vector<std::string> denyList;
        bool requireSignature = false;
        std::string jwtSecret;
    };

    struct AuditEntry {
        std::string timestamp;
        std::string userId;
        std::string extensionId;
        std::string action;
        std::string details;
    };

    bool checkAllowList(const std::string& extensionId);
    bool checkDenyList(const std::string& extensionId);
    void addToAuditLog(const std::string& userId, const std::string& extensionId, const std::string& action, const std::string& details);
    std::string getCurrentTimestamp();
    bool verifyJwtSignature(const std::string& token);

    PolicySettings m_settings;
    std::list<AuditEntry> m_auditLog;
    bool m_compliant = true;

    PolicyViolationFn m_onPolicyViolation = nullptr;
    AuditLogEntryFn m_onAuditLogEntry = nullptr;
    ComplianceChangedFn m_onComplianceStatusChanged = nullptr;
};
