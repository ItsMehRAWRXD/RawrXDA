#include "security_manager.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <memory>
#include <map>

// Static instance
std::unique_ptr<SecurityManager> SecurityManager::s_instance = nullptr;

SecurityManager::SecurityManager(QObject* parent)
    : QObject(parent),
      m_keyRotationInterval(86400),  // 24 hours
      m_lastKeyRotation(0),
      m_initialized(false),
      m_debugMode(false)
{
    qDebug() << "[SecurityManager] Constructing SecurityManager singleton";
}

SecurityManager* SecurityManager::getInstance()
{
    if (!s_instance) {
        s_instance = std::make_unique<SecurityManager>();
    }
    return s_instance.get();
}

bool SecurityManager::initialize(const QString& masterPassword)
{
    qDebug() << "[SecurityManager] Initializing security manager";
    
    if (masterPassword.isEmpty()) {
        qWarning() << "[SecurityManager] Master password not provided, using default";
        m_masterKey = QCryptographicHash::hash("default", QCryptographicHash::Sha256);
    } else {
        m_masterKey = QCryptographicHash::hash(masterPassword.toUtf8(), QCryptographicHash::Sha256);
    }
    
    m_currentKeyId = QString("key_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    m_lastKeyRotation = QDateTime::currentMSecsSinceEpoch();
    m_initialized = true;
    
    return true;
}

QString SecurityManager::encryptData(const QByteArray& plaintext, EncryptionAlgorithm algorithm)
{
    qDebug() << "[SecurityManager] Encrypting data with algorithm" << static_cast<int>(algorithm);
    
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized";
        return QString();
    }
    
    QByteArray ciphertext;
    
    switch (algorithm) {
        case EncryptionAlgorithm::AES256_GCM:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::AES256_CBC:
            ciphertext = encryptAES256CBC(plaintext, m_masterKey);
            break;
        default:
            ciphertext = plaintext;
    }
    
    // Return as base64
    return QString::fromUtf8(ciphertext.toBase64());
}

QByteArray SecurityManager::decryptData(const QString& ciphertext)
{
    qDebug() << "[SecurityManager] Decrypting data";
    
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized";
        return QByteArray();
    }
    
    QByteArray encrypted = QByteArray::fromBase64(ciphertext.toUtf8());
    return decryptAES256GCM(encrypted, m_masterKey);
}

QString SecurityManager::generateHMAC(const QByteArray& data)
{
    QByteArray hmac = QCryptographicHash::hash(data + m_masterKey, QCryptographicHash::Sha256);
    return QString::fromUtf8(hmac.toHex());
}

bool SecurityManager::verifyHMAC(const QByteArray& data, const QString& hmac)
{
    QString computed = generateHMAC(data);
    return computed == hmac;
}

bool SecurityManager::generateNewKey(const QString& keyId, EncryptionAlgorithm algorithm)
{
    qDebug() << "[SecurityManager] Generating new key:" << keyId;
    m_currentKeyId = keyId;
    return true;
}

bool SecurityManager::rotateEncryptionKey()
{
    qDebug() << "[SecurityManager] Rotating encryption key";
    
    QString newKeyId = QString("key_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    m_currentKeyId = newKeyId;
    m_lastKeyRotation = QDateTime::currentMSecsSinceEpoch();
    
    emit keyRotationCompleted(newKeyId);
    logSecurityEvent("key_rotation", "system", "encryption", true);
    
    return true;
}

qint64 SecurityManager::getKeyExpirationTime() const
{
    return m_lastKeyRotation + m_keyRotationInterval;
}

bool SecurityManager::storeCredential(const QString& username, const QString& token,
                                     const QString& tokenType, qint64 expiresAt,
                                     const QString& refreshToken)
{
    qDebug() << "[SecurityManager] Storing credential for user:" << username;
    
    CredentialInfo cred;
    cred.username = username;
    cred.token = encryptData(token.toUtf8());
    cred.tokenType = tokenType;
    cred.issuedAt = QDateTime::currentMSecsSinceEpoch();
    cred.expiresAt = expiresAt > 0 ? expiresAt : (cred.issuedAt + 3600 * 1000); // 1 hour default
    cred.isRefreshable = !refreshToken.isEmpty();
    cred.refreshToken = refreshToken;
    
    m_credentials[username] = cred;
    logSecurityEvent("credential_stored", "system", username, true);
    
    return true;
}

SecurityManager::CredentialInfo SecurityManager::getCredential(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it != m_credentials.end()) {
        if (QDateTime::currentMSecsSinceEpoch() < it->second.expiresAt) {
            return it->second;
        }
    }
    return CredentialInfo();
}

bool SecurityManager::removeCredential(const QString& username)
{
    qDebug() << "[SecurityManager] Removing credential for user:" << username;
    
    auto it = m_credentials.find(username);
    if (it != m_credentials.end()) {
        m_credentials.erase(it);
        logSecurityEvent("credential_removed", "system", username, true);
        return true;
    }
    
    return false;
}

bool SecurityManager::isTokenExpired(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return true;
    }
    
    return QDateTime::currentMSecsSinceEpoch() >= it->second.expiresAt;
}

QString SecurityManager::refreshToken(const QString& username)
{
    qDebug() << "[SecurityManager] Refreshing token for user:" << username;
    
    auto it = m_credentials.find(username);
    if (it == m_credentials.end() || !it->second.isRefreshable) {
        emit tokenRefreshFailed(username, "Token not refreshable");
        logSecurityEvent("token_refresh_failed", "system", username, false);
        return QString();
    }
    
    // Placeholder: would call auth server with refresh token
    QString newToken = "new_token_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    it->second.token = encryptData(newToken.toUtf8());
    it->second.issuedAt = QDateTime::currentMSecsSinceEpoch();
    it->second.expiresAt = it->second.issuedAt + 3600 * 1000;
    
    logSecurityEvent("token_refreshed", "system", username, true);
    return newToken;
}

bool SecurityManager::setAccessControl(const QString& username, const QString& resource,
                                      AccessLevel level)
{
    qDebug() << "[SecurityManager] Setting access control for" << username << "to" << resource;
    
    m_acl[username][resource] = level;
    logSecurityEvent("acl_updated", "system", resource, true, username);
    
    return true;
}

bool SecurityManager::checkAccess(const QString& username, const QString& resource,
                                 AccessLevel requiredLevel) const
{
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        logSecurityEvent("access_denied", username, resource, false);
        emit unauthorizedAccessAttempt(username, resource);
        return false;
    }
    
    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
        logSecurityEvent("access_denied", username, resource, false);
        emit unauthorizedAccessAttempt(username, resource);
        return false;
    }
    
    bool hasAccess = (resourceIt->second & requiredLevel) != AccessLevel::None;
    if (!hasAccess) {
        logSecurityEvent("access_denied", username, resource, false);
        emit unauthorizedAccessAttempt(username, resource);
    }
    
    return hasAccess;
}

std::vector<std::pair<QString, SecurityManager::AccessLevel>> SecurityManager::getResourceACL(const QString& resource) const
{
    std::vector<std::pair<QString, AccessLevel>> result;
    
    for (const auto& userPair : m_acl) {
        auto it = userPair.second.find(resource);
        if (it != userPair.second.end()) {
            result.push_back({userPair.first, it->second});
        }
    }
    
    return result;
}

bool SecurityManager::pinCertificate(const QString& domain, const QString& certificatePEM)
{
    qDebug() << "[SecurityManager] Pinning certificate for domain:" << domain;
    
    QString certHash = QString::fromUtf8(QCryptographicHash::hash(
        certificatePEM.toUtf8(), QCryptographicHash::Sha256).toHex());
    
    m_pinnedCertificates[domain] = certHash;
    logSecurityEvent("certificate_pinned", "system", domain, true);
    
    return true;
}

bool SecurityManager::verifyCertificatePin(const QString& domain, const QString& certificatePEM) const
{
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) {
        return false;
    }
    
    QString certHash = QString::fromUtf8(QCryptographicHash::hash(
        certificatePEM.toUtf8(), QCryptographicHash::Sha256).toHex());
    
    bool verified = (it->second == certHash);
    if (!verified) {
        emit certificatePinningFailed(domain);
    }
    
    return verified;
}

void SecurityManager::logSecurityEvent(const QString& eventType, const QString& actor,
                                      const QString& resource, bool success, const QString& details)
{
    SecurityAuditEntry entry;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.eventType = eventType;
    entry.actor = actor;
    entry.resource = resource;
    entry.success = success;
    entry.details = details;
    
    m_auditLog.push_back(entry);
    
    // Keep audit log bounded (max 10000 entries)
    if (m_auditLog.size() > 10000) {
        m_auditLog.erase(m_auditLog.begin());
    }
    
    if (m_debugMode || !success) {
        qDebug() << "[SecurityAudit]" << eventType << "by" << actor << "on" << resource << ":" << (success ? "OK" : "FAILED");
    }
}

std::vector<SecurityManager::SecurityAuditEntry> SecurityManager::getAuditLog(int limit) const
{
    std::vector<SecurityAuditEntry> result;
    
    int start = static_cast<int>(m_auditLog.size()) - limit;
    if (start < 0) start = 0;
    
    for (size_t i = start; i < m_auditLog.size(); ++i) {
        result.push_back(m_auditLog[i]);
    }
    
    return result;
}

bool SecurityManager::exportAuditLog(const QString& filePath) const
{
    QJsonArray entries;
    
    for (const auto& entry : m_auditLog) {
        QJsonObject obj;
        obj["timestamp"] = static_cast<qint64>(entry.timestamp);
        obj["eventType"] = entry.eventType;
        obj["actor"] = entry.actor;
        obj["resource"] = entry.resource;
        obj["success"] = entry.success;
        obj["details"] = entry.details;
        entries.append(obj);
    }
    
    QJsonDocument doc(entries);
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    return true;
}

bool SecurityManager::loadConfiguration(const QJsonObject& config)
{
    qDebug() << "[SecurityManager] Loading configuration";
    
    m_keyRotationInterval = static_cast<qint64>(config["keyRotationInterval"].toDouble(86400));
    m_debugMode = config["debugMode"].toBool(false);
    
    return true;
}

QJsonObject SecurityManager::getConfiguration() const
{
    QJsonObject config;
    config["currentKeyId"] = m_currentKeyId;
    config["keyRotationInterval"] = static_cast<double>(m_keyRotationInterval);
    config["lastKeyRotation"] = static_cast<double>(m_lastKeyRotation);
    config["credentialsCount"] = static_cast<int>(m_credentials.size());
    config["auditLogSize"] = static_cast<int>(m_auditLog.size());
    config["initialized"] = m_initialized;
    return config;
}

bool SecurityManager::validateSetup() const
{
    return m_initialized && !m_masterKey.isEmpty();
}

// Private encryption methods
QByteArray SecurityManager::deriveKeyPBKDF2(const QString& password, const QByteArray& salt, int iterations)
{
    // Placeholder: in production, use actual PBKDF2
    return QCryptographicHash::hash(password.toUtf8() + salt, QCryptographicHash::Sha256);
}

QByteArray SecurityManager::encryptAES256GCM(const QByteArray& plaintext, const QByteArray& key)
{
    // Placeholder: in production, use OpenSSL EVP or similar
    return plaintext;
}

QByteArray SecurityManager::decryptAES256GCM(const QByteArray& ciphertext, const QByteArray& key)
{
    // Placeholder: in production, use OpenSSL EVP or similar
    return ciphertext;
}

QByteArray SecurityManager::encryptAES256CBC(const QByteArray& plaintext, const QByteArray& key)
{
    // Placeholder: in production, use OpenSSL
    return plaintext;
}

QByteArray SecurityManager::decryptAES256CBC(const QByteArray& ciphertext, const QByteArray& key)
{
    // Placeholder: in production, use OpenSSL
    return ciphertext;
}
