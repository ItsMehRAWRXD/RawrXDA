#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>

class EnterprisePolicyEngine {
public:
    struct AuditEntry {
        int64_t timestamp;
        std::string userId;
        std::string extensionId;
        std::string action;
        std::string details;
    };

    struct Settings {
        std::vector<std::string> allowList;
        std::vector<std::string> denyList;
        bool requireSignature;
        std::string jwtSecret;
    };

    EnterprisePolicyEngine();
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

    std::vector<AuditEntry> getAuditLog(int limit);

private:
    bool checkDenyList(const std::string& id);
    bool checkAllowList(const std::string& id);
    void addToAuditLog(const std::string& userId, const std::string& extensionId, 
                      const std::string& action, const std::string& details);
    void policyViolation(const std::string& extensionId, const std::string& reason);
    bool isJwtValid(const std::string& token);

    Settings m_settings;
    std::vector<AuditEntry> m_auditLog;
    std::mutex m_mutex;
};
