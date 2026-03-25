#include "security_manager.h"

#include <ctime>
#include <sstream>

std::unique_ptr<SecurityManager> SecurityManager::s_instance = nullptr;

SecurityManager::SecurityManager() = default;

SecurityManager* SecurityManager::getInstance() {
    if (!s_instance) {
        s_instance = std::unique_ptr<SecurityManager>(new SecurityManager());
    }
    return s_instance.get();
}

bool SecurityManager::initialize(const std::string& masterPassword) {
    m_initialized = !masterPassword.empty();
    if (m_initialized) {
        m_masterKey.assign(masterPassword.begin(), masterPassword.end());
        m_currentKeyId = "test-key";
    }
    return m_initialized;
}

std::string SecurityManager::encryptData(const std::vector<uint8_t>& plaintext,
                                         EncryptionAlgorithm algorithm) {
    (void)algorithm;
    return std::string(plaintext.begin(), plaintext.end());
}

std::vector<uint8_t> SecurityManager::decryptData(const std::string& ciphertext) {
    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
}

std::string SecurityManager::generateHMAC(const std::vector<uint8_t>& data) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(std::string(data.begin(), data.end())));
}

bool SecurityManager::verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac) {
    return generateHMAC(data) == hmac;
}

bool SecurityManager::generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm) {
    (void)algorithm;
    m_currentKeyId = keyId;
    return true;
}

bool SecurityManager::rotateEncryptionKey() {
    return generateNewKey("rotated-key", EncryptionAlgorithm::AES256_GCM);
}

int64_t SecurityManager::getKeyExpirationTime() const {
    return 0;
}

bool SecurityManager::storeCredential(const std::string& username, const std::string& token,
                                      const std::string& tokenType, int64_t expiresAt,
                                      const std::string& refreshToken) {
    CredentialInfo info;
    info.username = username;
    info.email = username;
    info.tokenType = tokenType;
    info.token = token;
    info.issuedAt = static_cast<int64_t>(std::time(nullptr));
    info.expiresAt = expiresAt;
    info.isRefreshable = !refreshToken.empty();
    info.refreshToken = refreshToken;
    m_credentials[username] = info;
    return true;
}

SecurityManager::CredentialInfo SecurityManager::getCredential(const std::string& username) const {
    auto it = m_credentials.find(username);
    if (it != m_credentials.end()) {
        return it->second;
    }
    return CredentialInfo{};
}

bool SecurityManager::removeCredential(const std::string& username) {
    return m_credentials.erase(username) > 0;
}

bool SecurityManager::isTokenExpired(const std::string& username) const {
    auto it = m_credentials.find(username);
    return it == m_credentials.end() || (it->second.expiresAt != 0 && it->second.expiresAt < static_cast<int64_t>(std::time(nullptr)));
}

std::string SecurityManager::refreshToken(const std::string& username, const std::string& refreshToken) {
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return {};
    }
    if (!refreshToken.empty()) {
        it->second.refreshToken = refreshToken;
    }
    return it->second.refreshToken;
}

bool SecurityManager::setAccessControl(const std::string& username, const std::string& resource, AccessLevel level) {
    m_acl[resource][username] = level;
    return true;
}

bool SecurityManager::checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const {
    auto resourceIt = m_acl.find(resource);
    if (resourceIt == m_acl.end()) {
        return false;
    }
    auto userIt = resourceIt->second.find(username);
    if (userIt == resourceIt->second.end()) {
        return false;
    }
    return static_cast<int>(userIt->second) >= static_cast<int>(requiredLevel);
}

std::vector<std::pair<std::string, SecurityManager::AccessLevel>> SecurityManager::getResourceACL(const std::string& resource) const {
    std::vector<std::pair<std::string, AccessLevel>> acl;
    auto it = m_acl.find(resource);
    if (it != m_acl.end()) {
        for (const auto& entry : it->second) {
            acl.emplace_back(entry.first, entry.second);
        }
    }
    return acl;
}

bool SecurityManager::pinCertificate(const std::string& domain, const std::string& certificatePEM) {
    m_pinnedCertificates[domain] = certificatePEM;
    return true;
}

bool SecurityManager::verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const {
    auto it = m_pinnedCertificates.find(domain);
    return it != m_pinnedCertificates.end() && it->second == certificatePEM;
}

void SecurityManager::logSecurityEvent(const std::string& eventType, const std::string& actor,
                                       const std::string& resource, bool success,
                                       const std::string& details) {
    SecurityAuditEntry entry;
    entry.timestamp = static_cast<int64_t>(std::time(nullptr));
    entry.eventType = eventType;
    entry.actor = actor;
    entry.resource = resource;
    entry.success = success;
    entry.details = details;
    m_auditLog.push_back(entry);
}

std::vector<SecurityManager::SecurityAuditEntry> SecurityManager::getAuditLog(int limit) const {
    if (limit <= 0 || static_cast<size_t>(limit) >= m_auditLog.size()) {
        return m_auditLog;
    }
    return std::vector<SecurityAuditEntry>(m_auditLog.end() - limit, m_auditLog.end());
}

bool SecurityManager::exportAuditLog(const std::string& filePath) const {
    (void)filePath;
    return true;
}

bool SecurityManager::loadConfiguration(const std::string& configJson) {
    m_debugMode = configJson.find("debug") != std::string::npos;
    return true;
}

std::string SecurityManager::getConfiguration() const {
    std::ostringstream out;
    out << "{\"initialized\":" << (m_initialized ? "true" : "false")
        << ",\"keyId\":\"" << m_currentKeyId << "\"}";
    return out.str();
}

bool SecurityManager::validateSetup() const {
    return m_initialized;
}
