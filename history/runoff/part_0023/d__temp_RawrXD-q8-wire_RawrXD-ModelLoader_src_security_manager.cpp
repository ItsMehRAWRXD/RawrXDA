#include "security_manager.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

std::unique_ptr<SecurityManager> SecurityManager::s_instance = nullptr;

SecurityManager* SecurityManager::getInstance()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    
    if (!s_instance) {
        s_instance = std::make_unique<SecurityManager>();
    }
    return s_instance.get();
}

SecurityManager::SecurityManager(QObject* parent)
    : QObject(parent)
    , m_keyRotationInterval(86400 * 30)  // 30 days
    , m_lastKeyRotation(0)
    , m_initialized(false)
    , m_debugMode(false)
{
}

bool SecurityManager::initialize(const QString& masterPassword)
{
    if (m_initialized) {
        return true;
    }

    // Generate or load master key from secure storage
    QByteArray salt = QByteArray::fromHex("0123456789ABCDEF0123456789ABCDEF");
    if (masterPassword.isEmpty()) {
        // Generate random key (production: load from secure storage like Windows DPAPI)
        m_masterKey = QByteArray(32, 0);
        if (!RAND_bytes(reinterpret_cast<unsigned char*>(m_masterKey.data()), 32)) {
            qWarning() << "Failed to generate random master key";
            return false;
        }
    } else {
        // Derive key from password using PBKDF2
        m_masterKey = deriveKeyPBKDF2(masterPassword, salt, 100000);
    }

    m_currentKeyId = QString::number(QDateTime::currentSecsSinceEpoch());
    m_lastKeyRotation = QDateTime::currentSecsSinceEpoch();
    m_initialized = true;

    logSecurityEvent("initialization", "system", "security_manager", true, "Security manager initialized");
    return true;
}

QByteArray SecurityManager::deriveKeyPBKDF2(const QString& password, const QByteArray& salt, int iterations)
{
    QByteArray derivedKey(32, 0);
    const unsigned char* pwd = reinterpret_cast<const unsigned char*>(password.toStdString().c_str());
    int pwdLen = password.length();

    PKCS5_PBKDF2_HMAC(reinterpret_cast<const char*>(pwd), pwdLen,
                      reinterpret_cast<const unsigned char*>(salt.data()), salt.length(),
                      iterations, EVP_sha256(),
                      32, reinterpret_cast<unsigned char*>(derivedKey.data()));

    return derivedKey;
}

QString SecurityManager::encryptData(const QByteArray& plaintext, EncryptionAlgorithm algorithm)
{
    if (!m_initialized) {
        logSecurityEvent("encryption", "system", "data", false, "Security manager not initialized");
        return "";
    }

    QByteArray encrypted;
    
    switch (algorithm) {
        case EncryptionAlgorithm::AES256_GCM:
            encrypted = encryptAES256GCM(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::AES256_CBC:
            encrypted = encryptAES256CBC(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::ChaCha20Poly1305:
            // Simplified: use GCM for now
            encrypted = encryptAES256GCM(plaintext, m_masterKey);
            break;
    }

    if (encrypted.isEmpty()) {
        logSecurityEvent("encryption", "system", "data", false, "Encryption failed");
        return "";
    }

    logSecurityEvent("encryption", "system", "data", true, QString("Encrypted %1 bytes").arg(plaintext.length()));
    return encrypted.toBase64();
}

QByteArray SecurityManager::decryptData(const QString& ciphertext)
{
    if (!m_initialized) {
        logSecurityEvent("decryption", "system", "data", false, "Security manager not initialized");
        return QByteArray();
    }

    QByteArray encrypted = QByteArray::fromBase64(ciphertext.toUtf8());
    if (encrypted.isEmpty()) {
        logSecurityEvent("decryption", "system", "data", false, "Invalid ciphertext format");
        return QByteArray();
    }

    // Try GCM first (most common)
    QByteArray decrypted = decryptAES256GCM(encrypted, m_masterKey);
    if (!decrypted.isEmpty()) {
        logSecurityEvent("decryption", "system", "data", true, QString("Decrypted %1 bytes").arg(decrypted.length()));
        return decrypted;
    }

    // Fall back to CBC
    decrypted = decryptAES256CBC(encrypted, m_masterKey);
    if (!decrypted.isEmpty()) {
        logSecurityEvent("decryption", "system", "data", true, QString("Decrypted %1 bytes").arg(decrypted.length()));
        return decrypted;
    }

    logSecurityEvent("decryption", "system", "data", false, "Decryption failed");
    return QByteArray();
}

QByteArray SecurityManager::encryptAES256GCM(const QByteArray& plaintext, const QByteArray& key)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    // Generate random IV (12 bytes for GCM)
    unsigned char iv[12];
    if (!RAND_bytes(iv, sizeof(iv))) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    unsigned char tag[16];
    QByteArray ciphertext(plaintext.length(), 0);
    int len = 0;

    // Initialize encryption
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Encrypt data
    if (!EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.length())) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Finalize
    if (!EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Get authentication tag
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_free(ctx);

    // Return IV + ciphertext + tag
    QByteArray result;
    result.append(reinterpret_cast<const char*>(iv), sizeof(iv));
    result.append(ciphertext);
    result.append(reinterpret_cast<const char*>(tag), sizeof(tag));
    return result;
}

QByteArray SecurityManager::decryptAES256GCM(const QByteArray& encrypted, const QByteArray& key)
{
    if (encrypted.length() < 12 + 16) {  // IV (12) + tag (16)
        return QByteArray();
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    const unsigned char* iv = reinterpret_cast<const unsigned char*>(encrypted.data());
    const unsigned char* ciphertext = iv + 12;
    int ciphertextLen = encrypted.length() - 12 - 16;
    const unsigned char* tag = reinterpret_cast<const unsigned char*>(encrypted.data()) + encrypted.length() - 16;

    QByteArray plaintext(ciphertextLen, 0);
    int len = 0;

    // Initialize decryption
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Set authentication tag
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                            const_cast<unsigned char*>(tag))) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Decrypt data
    if (!EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                          ciphertext, ciphertextLen)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    // Finalize (verifies authentication tag)
    if (!EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

QByteArray SecurityManager::encryptAES256CBC(const QByteArray& plaintext, const QByteArray& key)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    // Generate random IV
    unsigned char iv[16];
    if (!RAND_bytes(iv, sizeof(iv))) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    QByteArray ciphertext(plaintext.length() + 16, 0);
    int len = 0;

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    if (!EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.length())) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    if (!EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_free(ctx);

    QByteArray result;
    result.append(reinterpret_cast<const char*>(iv), sizeof(iv));
    result.append(ciphertext.left(ciphertext.length() - (16 - len)));
    return result;
}

QByteArray SecurityManager::decryptAES256CBC(const QByteArray& encrypted, const QByteArray& key)
{
    if (encrypted.length() < 16) {
        return QByteArray();
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    const unsigned char* iv = reinterpret_cast<const unsigned char*>(encrypted.data());
    const unsigned char* ciphertext = iv + 16;
    int ciphertextLen = encrypted.length() - 16;

    QByteArray plaintext(ciphertextLen + 16, 0);
    int len = 0;

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()), iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    if (!EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                          ciphertext, ciphertextLen)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    if (!EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext.length() - 16 + len);
    return plaintext;
}

QString SecurityManager::generateHMAC(const QByteArray& data)
{
    unsigned char result[SHA256_DIGEST_LENGTH];
    unsigned int resultLen = 0;

    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(m_masterKey.data()), m_masterKey.length(),
         reinterpret_cast<const unsigned char*>(data.data()), data.length(),
         result, &resultLen);

    return QByteArray(reinterpret_cast<const char*>(result), resultLen).toHex();
}

bool SecurityManager::verifyHMAC(const QByteArray& data, const QString& hmac)
{
    QString computed = generateHMAC(data);
    return computed == hmac;
}

bool SecurityManager::generateNewKey(const QString& keyId, EncryptionAlgorithm algorithm)
{
    QByteArray newKey(32, 0);
    if (!RAND_bytes(reinterpret_cast<unsigned char*>(newKey.data()), 32)) {
        logSecurityEvent("key_generation", "system", keyId, false, "Failed to generate random bytes");
        return false;
    }

    m_masterKey = newKey;
    m_currentKeyId = keyId;
    logSecurityEvent("key_generation", "system", keyId, true, QString("Generated new %1 key").arg(keyId));
    return true;
}

bool SecurityManager::rotateEncryptionKey()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_lastKeyRotation < m_keyRotationInterval) {
        return false;
    }

    QString newKeyId = QString::number(now);
    if (generateNewKey(newKeyId, EncryptionAlgorithm::AES256_GCM)) {
        m_lastKeyRotation = now;
        emit keyRotationCompleted(newKeyId);
        logSecurityEvent("key_rotation", "system", newKeyId, true, "Encryption key rotated");
        return true;
    }

    logSecurityEvent("key_rotation", "system", newKeyId, false, "Key rotation failed");
    return false;
}

qint64 SecurityManager::getKeyExpirationTime() const
{
    return m_lastKeyRotation + m_keyRotationInterval;
}

bool SecurityManager::storeCredential(const QString& username, const QString& token,
                                      const QString& tokenType, qint64 expiresAt,
                                      const QString& refreshToken)
{
    CredentialInfo info;
    info.username = username;
    info.tokenType = tokenType;
    info.token = encryptData(token.toUtf8());
    info.issuedAt = QDateTime::currentSecsSinceEpoch();
    info.expiresAt = expiresAt;
    info.isRefreshable = !refreshToken.isEmpty();
    info.refreshToken = refreshToken.isEmpty() ? "" : encryptData(refreshToken.toUtf8());

    m_credentials[username] = info;
    logSecurityEvent("credential_storage", "system", username, true, QString("Stored %1 credential").arg(tokenType));
    return true;
}

SecurityManager::CredentialInfo SecurityManager::getCredential(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return CredentialInfo();
    }

    CredentialInfo info = it->second;
    if (info.expiresAt > 0 && info.expiresAt < QDateTime::currentSecsSinceEpoch()) {
        return CredentialInfo();  // Expired
    }

    // Decrypt token
    info.token = QString::fromUtf8(decryptData(info.token));
    if (!info.refreshToken.isEmpty()) {
        info.refreshToken = QString::fromUtf8(decryptData(info.refreshToken));
    }

    return info;
}

bool SecurityManager::removeCredential(const QString& username)
{
    bool removed = m_credentials.erase(username) > 0;
    if (removed) {
        logSecurityEvent("credential_removal", "system", username, true, "Credential removed");
    }
    return removed;
}

bool SecurityManager::isTokenExpired(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return true;
    }

    qint64 now = QDateTime::currentSecsSinceEpoch();
    return it->second.expiresAt > 0 && it->second.expiresAt < now;
}

QString SecurityManager::refreshToken(const QString& username)
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end() || !it->second.isRefreshable) {
        emit tokenRefreshFailed(username, "Credential not found or not refreshable");
        logSecurityEvent("token_refresh", username, username, false, "Refresh failed");
        return "";
    }

    // Simulate token refresh (in production, make HTTP request)
    QString newToken = QString("refreshed_%1_%2")
        .arg(username)
        .arg(QDateTime::currentSecsSinceEpoch());

    storeCredential(username, newToken, it->second.tokenType,
                   QDateTime::currentSecsSinceEpoch() + 3600, it->second.refreshToken);

    logSecurityEvent("token_refresh", username, username, true, "Token refreshed");
    return newToken;
}

bool SecurityManager::setAccessControl(const QString& username, const QString& resource,
                                       AccessLevel level)
{
    m_acl[username][resource] = level;
    logSecurityEvent("acl_update", "system", resource, true,
                    QString("Set ACL for %1 to %2").arg(username).arg(static_cast<int>(level)));
    return true;
}

bool SecurityManager::checkAccess(const QString& username, const QString& resource,
                                 AccessLevel requiredLevel) const
{
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        emit unauthorizedAccessAttempt(username, resource);
        logSecurityEvent("access_check", username, resource, false, "User not in ACL");
        return false;
    }

    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
        emit unauthorizedAccessAttempt(username, resource);
        logSecurityEvent("access_check", username, resource, false, "Resource not accessible");
        return false;
    }

    bool hasAccess = (static_cast<int>(resourceIt->second) & static_cast<int>(requiredLevel)) != 0;
    if (!hasAccess) {
        emit unauthorizedAccessAttempt(username, resource);
        logSecurityEvent("access_check", username, resource, false, "Insufficient access level");
    }

    return hasAccess;
}

std::vector<std::pair<QString, SecurityManager::AccessLevel>>
SecurityManager::getResourceACL(const QString& resource) const
{
    std::vector<std::pair<QString, AccessLevel>> result;
    for (const auto& [username, resources] : m_acl) {
        auto it = resources.find(resource);
        if (it != resources.end()) {
            result.push_back({username, it->second});
        }
    }
    return result;
}

bool SecurityManager::pinCertificate(const QString& domain, const QString& certificatePEM)
{
    QString certHash = QCryptographicHash::hash(certificatePEM.toUtf8(), QCryptographicHash::Sha256).toHex();
    m_pinnedCertificates[domain] = certHash;
    logSecurityEvent("cert_pinning", "system", domain, true, "Certificate pinned");
    return true;
}

bool SecurityManager::verifyCertificatePin(const QString& domain, const QString& certificatePEM) const
{
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) {
        return true;  // No pin set, allow
    }

    QString certHash = QCryptographicHash::hash(certificatePEM.toUtf8(), QCryptographicHash::Sha256).toHex();
    bool valid = certHash == it->second;

    if (!valid) {
        emit const_cast<SecurityManager*>(this)->certificatePinningFailed(domain);
        const_cast<SecurityManager*>(this)->logSecurityEvent("cert_verification", "system", domain, false,
                                                             "Certificate pinning failed");
    }

    return valid;
}

void SecurityManager::logSecurityEvent(const QString& eventType, const QString& actor,
                                       const QString& resource, bool success, const QString& details)
{
    SecurityAuditEntry entry;
    entry.timestamp = QDateTime::currentSecsSinceEpoch();
    entry.eventType = eventType;
    entry.actor = actor;
    entry.resource = resource;
    entry.success = success;
    entry.details = details;

    m_auditLog.push_back(entry);

    // Keep audit log size bounded (max 10000 entries)
    if (m_auditLog.size() > 10000) {
        m_auditLog.erase(m_auditLog.begin());
    }

    if (m_debugMode || !success) {
        qDebug() << QString("[SECURITY] %1:%2 %3 %4 - %5")
                    .arg(eventType, actor, resource, success ? "SUCCESS" : "FAILED", details);
    }
}

std::vector<SecurityManager::SecurityAuditEntry> SecurityManager::getAuditLog(int limit) const
{
    std::vector<SecurityAuditEntry> result;
    int start = std::max(0, static_cast<int>(m_auditLog.size()) - limit);
    std::copy(m_auditLog.begin() + start, m_auditLog.end(), std::back_inserter(result));
    std::reverse(result.begin(), result.end());  // Most recent first
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

    QJsonObject root;
    root["auditLog"] = entries;
    root["exportedAt"] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

bool SecurityManager::loadConfiguration(const QJsonObject& config)
{
    m_debugMode = config["debugMode"].toBool(false);
    m_keyRotationInterval = config["keyRotationInterval"].toInt(86400 * 30);
    return true;
}

QJsonObject SecurityManager::getConfiguration() const
{
    QJsonObject config;
    config["initialized"] = m_initialized;
    config["currentKeyId"] = m_currentKeyId;
    config["debugMode"] = m_debugMode;
    config["keyRotationInterval"] = static_cast<qint64>(m_keyRotationInterval);
    config["keyExpirationTime"] = getKeyExpirationTime();
    config["credentialCount"] = static_cast<int>(m_credentials.size());
    config["auditLogSize"] = static_cast<int>(m_auditLog.size());
    return config;
}

bool SecurityManager::validateSetup() const
{
    return m_initialized && !m_masterKey.isEmpty() && !m_currentKeyId.isEmpty();
}
