#include "marketplace/enterprise_policy_engine.h"
EnterprisePolicyEngine::EnterprisePolicyEngine()
    
    , m_compliant(true)
{
    m_settings.requireSignature = false;
    // // qDebug:  "[EnterprisePolicyEngine] Initialized";
}

EnterprisePolicyEngine::~EnterprisePolicyEngine() {
    // Save audit log if needed
}

void EnterprisePolicyEngine::setAllowList(const std::stringList& extensionIds) {
    m_settings.allowList = extensionIds;
}

void EnterprisePolicyEngine::setDenyList(const std::stringList& extensionIds) {
    m_settings.denyList = extensionIds;
}

void EnterprisePolicyEngine::setRequireSignature(bool require) {
    m_settings.requireSignature = require;
}

void EnterprisePolicyEngine::setJwtSecret(const std::string& secret) {
    m_settings.jwtSecret = secret;
}

bool EnterprisePolicyEngine::isExtensionAllowed(const std::string& extensionId) {
    // Check deny list first
    if (checkDenyList(extensionId)) {
        addToAuditLog("system", extensionId, "block", "Extension in deny list");
        policyViolation(extensionId, "Extension is in deny list");
        return false;
    }
    
    // Check allow list if it's not empty
    if (!m_settings.allowList.empty()) {
        if (!checkAllowList(extensionId)) {
            addToAuditLog("system", extensionId, "block", "Extension not in allow list");
            policyViolation(extensionId, "Extension not in allow list");
            return false;
        }
    }
    
    // If we get here, the extension is allowed
    return true;
}

bool EnterprisePolicyEngine::verifyExtensionSignature(const std::string& extensionId, const std::string& signature) {
    // In a real implementation, this would verify the digital signature of the extension
    // For now, we'll just return true if signature verification is not required
    if (!m_settings.requireSignature) {
        return true;
    }
    
    // Simulate signature verification
    // // qDebug:  "[EnterprisePolicyEngine] Verifying signature for:" << extensionId;
    return !signature.empty(); // Simple check for demo
}

bool EnterprisePolicyEngine::validateUserAccess(const std::string& userId, const std::string& jwtToken) {
    if (!isJwtValid(jwtToken)) {
        return false;
    }
    
    // In a real implementation, this would check the user's permissions
    // For now, we'll just assume valid JWT means access is granted
    return true;
}

void EnterprisePolicyEngine::logExtensionInstallation(const std::string& extensionId, const std::string& userId) {
    addToAuditLog(userId, extensionId, "install", "Extension installed");
}

void EnterprisePolicyEngine::logExtensionUninstallation(const std::string& extensionId, const std::string& userId) {
    addToAuditLog(userId, extensionId, "uninstall", "Extension uninstalled");
}

std::vector<void*> EnterprisePolicyEngine::getAuditLog(int limit) {
    std::vector<void*> result;
    
    int start = qMax(0, m_auditLog.size() - limit);
    for (int i = start; i < m_auditLog.size(); ++i) {
        const AuditEntry& entry = m_auditLog[i];
        void* obj;
        obj["timestamp"] = entry.timestamp;
        obj["userId"] = entry.userId;
        obj["extensionId"] = entry.extensionId;
        obj["action"] = entry.action;
        obj["details"] = entry.details;
        result.append(obj);
    }
    
    return result;
}

bool EnterprisePolicyEngine::isJwtValid(const std::string& token) {
    if (m_settings.jwtSecret.empty()) {
        return true; // No secret configured, assume valid
    }
    
    return verifyJwtSignature(token);
}

std::string EnterprisePolicyEngine::generateJwtToken(const std::string& userId, const std::stringList& permissions) {
    // In a real implementation, this would generate a proper JWT token
    // For now, we'll just return a placeholder
    return std::string("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.%1.%2")
            .arg(userId)
            .arg(QUuid::createUuid().toString());
}

bool EnterprisePolicyEngine::checkAllowList(const std::string& extensionId) {
    return m_settings.allowList.contains(extensionId);
}

bool EnterprisePolicyEngine::checkDenyList(const std::string& extensionId) {
    return m_settings.denyList.contains(extensionId);
}

void EnterprisePolicyEngine::addToAuditLog(const std::string& userId, const std::string& extensionId, const std::string& action, const std::string& details) {
    AuditEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.userId = userId;
    entry.extensionId = extensionId;
    entry.action = action;
    entry.details = details;
    
    m_auditLog.append(entry);
    
    // Emit audit log entry signal
    void* obj;
    obj["timestamp"] = entry.timestamp;
    obj["userId"] = entry.userId;
    obj["extensionId"] = entry.extensionId;
    obj["action"] = entry.action;
    obj["details"] = entry.details;
    
    auditLogEntry(obj);
}

std::string EnterprisePolicyEngine::getCurrentTimestamp() {
    return // DateTime::currentDateTime().toString(ISODate);
}

bool EnterprisePolicyEngine::verifyJwtSignature(const std::string& token) {
    // In a real implementation, this would verify the JWT signature
    // For now, we'll just check if the token has the basic structure
    std::stringList parts = token.split('.');
    return parts.size() == 3;
}





