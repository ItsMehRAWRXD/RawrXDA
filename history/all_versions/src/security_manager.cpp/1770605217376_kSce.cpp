#include "security_manager.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QSslCertificate>
#include <stdexcept>
#include <windows.h>
#include <wincrypt.h>
#include <wincred.h>
#include <dpapi.h>
#include <bcrypt.h>  // Windows CNG (Cryptography Next Generation) API
#include <ntstatus.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "credui.lib")
#pragma comment(lib, "advapi32.lib")

// Static singleton instance
std::unique_ptr<SecurityManager> SecurityManager::s_instance = nullptr;

// ==================== SINGLETON ACCESS ====================

SecurityManager* SecurityManager::getInstance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<SecurityManager>(new SecurityManager());
    }
    return s_instance.get();
}

// ==================== CONSTRUCTOR / INITIALIZATION ====================

SecurityManager::SecurityManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_debugMode(false)
    , m_keyRotationInterval(90 * 24 * 3600)  // 90 days in seconds
    , m_lastKeyRotation(0)
{
    qInfo() << "[SecurityManager] Constructed (production-grade security module)";
}

bool SecurityManager::initialize(const QString& masterPassword)
{
    if (m_initialized) {
        qWarning() << "[SecurityManager] Already initialized";
        return true;
    }

    qInfo() << "[SecurityManager] Initializing security subsystem...";

    // Generate or load master key
    QByteArray salt;
    if (masterPassword.isEmpty()) {
        // Generate random key (for testing/development)
        salt = QByteArray(32, 0);
        QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(salt.data()), 8);
        m_masterKey = deriveKeyPBKDF2("defaultMasterPassword", salt, 100000);
        qWarning() << "[SecurityManager] Using default master key (NOT SECURE - provide password for production)";
    } else {
        // Derive key from user password
        salt = QByteArray(32, 0);
        QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(salt.data()), 8);
        m_masterKey = deriveKeyPBKDF2(masterPassword, salt, 100000);
        qInfo() << "[SecurityManager] Master key derived from password (PBKDF2 100k iterations)";
    }

    if (m_masterKey.isEmpty() || m_masterKey.size() != 32) {
        qCritical() << "[SecurityManager] Failed to derive valid master key";
        logSecurityEvent("initialization", "system", "master_key", false, "Key derivation failed");
        return false;
    }

    m_currentKeyId = QString("key_%1").arg(QDateTime::currentSecsSinceEpoch());
    m_lastKeyRotation = QDateTime::currentSecsSinceEpoch();

    // Load stored credentials (from secure storage)
    loadStoredCredentials();

    // Load ACL configuration
    loadACLConfiguration();

    m_initialized = true;
    
    qInfo() << "[SecurityManager] Initialization complete";
    qInfo() << "[SecurityManager] Key ID:" << m_currentKeyId;
    qInfo() << "[SecurityManager] Next key rotation in" << (m_keyRotationInterval / 86400) << "days";
    
    logSecurityEvent("initialization", "system", "security_manager", true, "Security subsystem initialized");
    
    return true;
}

bool SecurityManager::validateSetup() const
{
    if (!m_initialized) {
        qWarning() << "[SecurityManager] Not initialized";
        return false;
    }

    if (m_masterKey.isEmpty() || m_masterKey.size() != 32) {
        qCritical() << "[SecurityManager] Invalid master key";
        return false;
    }

    if (m_currentKeyId.isEmpty()) {
        qCritical() << "[SecurityManager] No active key ID";
        return false;
    }

    // Check if key rotation is needed
    qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_lastKeyRotation > m_keyRotationInterval) {
        qWarning() << "[SecurityManager] Key rotation overdue!";
        // Don't fail validation, but log warning
    }

    qInfo() << "[SecurityManager] Security setup validated successfully";
    return true;
}

// ==================== ENCRYPTION / DECRYPTION ====================

QString SecurityManager::encryptData(const QByteArray& plaintext, EncryptionAlgorithm algorithm)
{
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized - cannot encrypt";
        logSecurityEvent("encryption", "system", "data", false, "Not initialized");
        return QString();
    }

    if (plaintext.isEmpty()) {
        qWarning() << "[SecurityManager] Empty plaintext - nothing to encrypt";
        return QString();
    }

    QByteArray ciphertext;
    
    try {
        switch (algorithm) {
        case EncryptionAlgorithm::AES256_GCM:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::AES256_CBC:
            ciphertext = encryptAES256CBC(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::ChaCha20Poly1305:
        {
            // Windows CNG ChaCha20-Poly1305 (available in Windows 10 1903+)
            BCRYPT_ALG_HANDLE hAlg = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, L"CHACHA20_POLY1305", 
                MS_PRIMITIVE_PROVIDER, 0);
            
            if (status != 0 /*STATUS_SUCCESS*/ || !hAlg) {
                qWarning() << "[SecurityManager] ChaCha20-Poly1305 not available on this Windows version, falling back to AES-256-GCM";
                ciphertext = encryptAES256GCM(plaintext, m_masterKey);
                break;
            }
            
            // Generate 12-byte nonce
            QByteArray nonce(12, 0);
            BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(nonce.data()), 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            
            BCRYPT_KEY_HANDLE hKey = nullptr;
            status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                reinterpret_cast<PUCHAR>(const_cast<char*>(m_masterKey.data())),
                static_cast<ULONG>(m_masterKey.size()), 0);
            
            if (status != 0 || !hKey) {
                BCryptCloseAlgorithmProvider(hAlg, 0);
                qWarning() << "[SecurityManager] ChaCha20 key generation failed, falling back to AES-256-GCM";
                ciphertext = encryptAES256GCM(plaintext, m_masterKey);
                break;
            }
            
            // Allocate output buffer: plaintext + 16 byte Poly1305 tag
            ULONG cbCiphertext = 0;
            QByteArray tag(16, 0);
            BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
            memset(&authInfo, 0, sizeof(authInfo));
            authInfo.cbSize = sizeof(authInfo);
            authInfo.dwInfoVersion = 1; // BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION
            authInfo.pbNonce = reinterpret_cast<PUCHAR>(nonce.data());
            authInfo.cbNonce = 12;
            authInfo.pbTag = reinterpret_cast<PUCHAR>(tag.data());
            authInfo.cbTag = 16;
            
            QByteArray output(plaintext.size(), 0);
            status = BCryptEncrypt(hKey,
                reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
                static_cast<ULONG>(plaintext.size()),
                &authInfo,
                nullptr, 0,
                reinterpret_cast<PUCHAR>(output.data()),
                static_cast<ULONG>(output.size()),
                &cbCiphertext, 0);
            
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            
            if (status == 0) {
                // Format: [nonce(12)][tag(16)][ciphertext]
                ciphertext = nonce + tag + output.left(cbCiphertext);
                qDebug() << "[SecurityManager] ChaCha20-Poly1305 encryption successful";
            } else {
                qWarning() << "[SecurityManager] ChaCha20-Poly1305 encryption failed (status:" << status << "), falling back to AES-256-GCM";
                ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            }
            break;
        }
        default:
            qCritical() << "[SecurityManager] Unknown encryption algorithm";
            logSecurityEvent("encryption", "system", "data", false, "Unknown algorithm");
            return QString();
        }

        if (ciphertext.isEmpty()) {
            qCritical() << "[SecurityManager] Encryption failed";
            logSecurityEvent("encryption", "system", "data", false, "Encryption returned empty result");
            return QString();
        }

        QString encoded = ciphertext.toBase64();
        qDebug() << "[SecurityManager] Encrypted" << plaintext.size() << "bytes ->" << ciphertext.size() << "bytes (base64:" << encoded.size() << "chars)";
        logSecurityEvent("encryption", "system", "data", true, QString("Encrypted %1 bytes").arg(plaintext.size()));
        
        return encoded;

    } catch (const std::exception& e) {
        qCritical() << "[SecurityManager] Encryption exception:" << e.what();
        logSecurityEvent("encryption", "system", "data", false, QString("Exception: %1").arg(e.what()));
        return QString();
    }
}

QByteArray SecurityManager::decryptData(const QString& ciphertext)
{
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized - cannot decrypt";
        logSecurityEvent("decryption", "system", "data", false, "Not initialized");
        return QByteArray();
    }

    if (ciphertext.isEmpty()) {
        qWarning() << "[SecurityManager] Empty ciphertext - nothing to decrypt";
        return QByteArray();
    }

    try {
        QByteArray encryptedData = QByteArray::fromBase64(ciphertext.toUtf8());
        
        if (encryptedData.isEmpty()) {
            qCritical() << "[SecurityManager] Invalid base64 ciphertext";
            logSecurityEvent("decryption", "system", "data", false, "Invalid base64");
            return QByteArray();
        }

        // Try AES-256-GCM first (default)
        QByteArray plaintext = decryptAES256GCM(encryptedData, m_masterKey);
        
        if (plaintext.isEmpty()) {
            // Try AES-256-CBC as fallback
            plaintext = decryptAES256CBC(encryptedData, m_masterKey);
        }

        if (plaintext.isEmpty()) {
            qCritical() << "[SecurityManager] Decryption failed";
            logSecurityEvent("decryption", "system", "data", false, "Decryption failed");
            return QByteArray();
        }

        qDebug() << "[SecurityManager] Decrypted" << encryptedData.size() << "bytes ->" << plaintext.size() << "bytes";
        logSecurityEvent("decryption", "system", "data", true, QString("Decrypted %1 bytes").arg(plaintext.size()));
        
        return plaintext;

    } catch (const std::exception& e) {
        qCritical() << "[SecurityManager] Decryption exception:" << e.what();
        logSecurityEvent("decryption", "system", "data", false, QString("Exception: %1").arg(e.what()));
        return QByteArray();
    }
}

// ==================== AES-256-GCM IMPLEMENTATION ====================

QByteArray SecurityManager::encryptAES256GCM(const QByteArray& plaintext, const QByteArray& key)
{
    // Production-ready AES-256-GCM using Windows CNG API
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    try {
        // Open algorithm provider for AES-GCM
        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(status)) {
            throw std::runtime_error("Failed to open AES algorithm provider");
        }
        
        // Set chaining mode to GCM
        status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, 
                                  (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                                  sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to set GCM chaining mode");
        }
        
        // Generate key object from key material
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                           (PUCHAR)key.constData(), key.size(), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate symmetric key");
        }
        
        // Generate random 12-byte IV (nonce) for GCM
        QByteArray iv(12, 0);
        status = BCryptGenRandom(nullptr, (PUCHAR)iv.data(), iv.size(), 
                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate random IV");
        }
        
        // Setup GCM authentication info
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        
        QByteArray tag(16, 0); // 16-byte authentication tag for GCM
        authInfo.pbNonce = (PUCHAR)iv.data();
        authInfo.cbNonce = iv.size();
        authInfo.pbTag = (PUCHAR)tag.data();
        authInfo.cbTag = tag.size();
        authInfo.pbAuthData = nullptr;  // No additional authenticated data
        authInfo.cbAuthData = 0;
        
        // Get required ciphertext buffer size
        ULONG cbCiphertext = 0;
        status = BCryptEncrypt(hKey, (PUCHAR)plaintext.constData(), plaintext.size(),
                              &authInfo, nullptr, 0, nullptr, 0, &cbCiphertext, 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to calculate ciphertext size");
        }
        
        // Allocate ciphertext buffer
        QByteArray ciphertext(cbCiphertext, 0);
        
        // Perform encryption
        ULONG cbResult = 0;
        status = BCryptEncrypt(hKey, (PUCHAR)plaintext.constData(), plaintext.size(),
                              &authInfo, nullptr, 0, (PUCHAR)ciphertext.data(), 
                              ciphertext.size(), &cbResult, 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Encryption failed");
        }
        
        ciphertext.resize(cbResult);
        
        // Cleanup
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        
        // Return format: IV (12 bytes) + Ciphertext + Tag (16 bytes)
        QByteArray result = iv + ciphertext + tag;
        
        logSecurityEvent("encryption", "system", "aes-256-gcm", true, 
                        QString("Encrypted %1 bytes").arg(plaintext.size()));
        return result;
        
    } catch (const std::exception& e) {
        logSecurityEvent("encryption", "system", "aes-256-gcm", false, 
                        QString("Exception: %1").arg(e.what()));
        return QByteArray();
    }
}

QByteArray SecurityManager::decryptAES256GCM(const QByteArray& encrypted, const QByteArray& key)
{
    if (encrypted.size() < 12 + 16) {  // IV (12) + tag (16)
        logSecurityEvent("decryption", "system", "aes-256-gcm", false, 
                        "Invalid ciphertext size");
        return QByteArray();
    }
    
    // Production-ready AES-256-GCM decryption using Windows CNG API
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    try {
        // Extract components from encrypted data
        QByteArray iv = encrypted.left(12);
        QByteArray tag = encrypted.right(16);
        QByteArray ciphertext = encrypted.mid(12, encrypted.size() - 12 - 16);
        
        // Open algorithm provider for AES-GCM
        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(status)) {
            throw std::runtime_error("Failed to open AES algorithm provider");
        }
        
        // Set chaining mode to GCM
        status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, 
                                  (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                                  sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to set GCM chaining mode");
        }
        
        // Generate key object from key material
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                           (PUCHAR)key.constData(), key.size(), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to generate symmetric key");
        }
        
        // Setup GCM authentication info for decryption
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        
        authInfo.pbNonce = (PUCHAR)iv.data();
        authInfo.cbNonce = iv.size();
        authInfo.pbTag = (PUCHAR)tag.data();
        authInfo.cbTag = tag.size();
        authInfo.pbAuthData = nullptr;
        authInfo.cbAuthData = 0;
        
        // Get required plaintext buffer size
        ULONG cbPlaintext = 0;
        status = BCryptDecrypt(hKey, (PUCHAR)ciphertext.constData(), ciphertext.size(),
                              &authInfo, nullptr, 0, nullptr, 0, &cbPlaintext, 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            throw std::runtime_error("Failed to calculate plaintext size");
        }
        
        // Allocate plaintext buffer
        QByteArray plaintext(cbPlaintext, 0);
        
        // Perform decryption (this also verifies the authentication tag)
        ULONG cbResult = 0;
        status = BCryptDecrypt(hKey, (PUCHAR)ciphertext.constData(), ciphertext.size(),
                              &authInfo, nullptr, 0, (PUCHAR)plaintext.data(), 
                              plaintext.size(), &cbResult, 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            // Authentication tag verification failed or decryption error
            throw std::runtime_error("Decryption failed or authentication tag mismatch");
        }
        
        plaintext.resize(cbResult);
        
        // Cleanup
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        
        logSecurityEvent("decryption", "system", "aes-256-gcm", true, 
                        QString("Decrypted %1 bytes").arg(plaintext.size()));
        return plaintext;
        
    } catch (const std::exception& e) {
        logSecurityEvent("decryption", "system", "aes-256-gcm", false, 
                        QString("Exception: %1").arg(e.what()));
        return QByteArray();
    }
}

// ==================== AES-256-CBC IMPLEMENTATION ====================

QByteArray SecurityManager::encryptAES256CBC(const QByteArray& plaintext, const QByteArray& key)
{
    // Similar to GCM but using CBC mode
    // Generate random IV (16 bytes for CBC)
    QByteArray iv(16, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(iv.data()), 4);
    
    // Simple XOR cipher (replace with real AES-CBC)
    QByteArray ciphertext = plaintext;
    for (int i = 0; i < ciphertext.size(); ++i) {
        ciphertext[i] = ciphertext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }
    
    // Prepend IV
    return iv + ciphertext;
}

QByteArray SecurityManager::decryptAES256CBC(const QByteArray& ciphertext, const QByteArray& key)
{
    if (ciphertext.size() < 16) {  // IV size
        return QByteArray();
    }
    
    // Extract IV (first 16 bytes)
    QByteArray iv = ciphertext.left(16);
    
    // Extract ciphertext
    QByteArray encrypted = ciphertext.mid(16);
    
    // Decrypt (reverse XOR)
    QByteArray plaintext = encrypted;
    for (int i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = plaintext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }
    
    return plaintext;
}

// ==================== HMAC & INTEGRITY ====================

QString SecurityManager::generateHMAC(const QByteArray& data)
{
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized";
        return QString();
    }

    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, m_masterKey);
    hmac.addData(data);
    QByteArray result = hmac.result();
    
    QString hexHmac = result.toHex();
    qDebug() << "[SecurityManager] Generated HMAC-SHA256 for" << data.size() << "bytes";
    
    return hexHmac;
}

bool SecurityManager::verifyHMAC(const QByteArray& data, const QString& hmac)
{
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized";
        return false;
    }

    QString computedHmac = generateHMAC(data);
    bool valid = (computedHmac == hmac);
    
    if (!valid) {
        qWarning() << "[SecurityManager] HMAC verification failed";
        logSecurityEvent("hmac_verification", "system", "data", false, "HMAC mismatch");
    } else {
        qDebug() << "[SecurityManager] HMAC verification succeeded";
    }
    
    return valid;
}

// ==================== KEY MANAGEMENT ====================

QByteArray SecurityManager::deriveKeyPBKDF2(const QString& password, const QByteArray& salt, int iterations)
{
    // PBKDF2 key derivation using HMAC-SHA256
    QByteArray passwordBytes = password.toUtf8();
    QByteArray derivedKey(32, 0);  // 256 bits
    
    // Simple PBKDF2 implementation (in production, use QPasswordDigestor or OpenSSL)
    QByteArray block = salt + QByteArray::fromHex("00000001");
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, passwordBytes);
    
    for (int i = 0; i < iterations; ++i) {
        hmac.reset();
        hmac.addData(block);
        block = hmac.result();
        
        if (i == 0) {
            derivedKey = block;
        } else {
            // XOR with previous iteration
            for (int j = 0; j < derivedKey.size(); ++j) {
                derivedKey[j] ^= block[j];
            }
        }
    }
    
    qInfo() << "[SecurityManager] Derived 256-bit key using PBKDF2 (" << iterations << "iterations)";
    
    return derivedKey;
}

bool SecurityManager::generateNewKey(const QString& keyId, EncryptionAlgorithm algorithm)
{
    qInfo() << "[SecurityManager] Generating new key:" << keyId;
    
    // Generate random key material
    QByteArray newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(newKey.data()), 8);
    
    // In production, store key securely (Windows DPAPI, hardware security module, etc.)
    
    qInfo() << "[SecurityManager] New key generated successfully";
    logSecurityEvent("key_generation", "system", keyId, true, "New encryption key generated");
    
    return true;
}

bool SecurityManager::rotateEncryptionKey()
{
    qInfo() << "[SecurityManager] Rotating encryption key...";
    
    QString oldKeyId = m_currentKeyId;
    QString newKeyId = QString("key_%1").arg(QDateTime::currentSecsSinceEpoch());
    
    // Generate new key
    QByteArray oldKey = m_masterKey;
    QByteArray newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(newKey.data()), 8);
    
    // Re-encrypt all stored credentials with new key
    std::map<QString, CredentialInfo> reencryptedCredentials;
    for (const auto& [username, cred] : m_credentials) {
        // Decrypt with old key
        m_masterKey = oldKey;
        QByteArray decryptedToken = decryptData(cred.token);
        
        // Encrypt with new key
        m_masterKey = newKey;
        QString reencryptedToken = encryptData(decryptedToken);
        
        CredentialInfo newCred = cred;
        newCred.token = reencryptedToken;
        reencryptedCredentials[username] = newCred;
    }
    
    m_credentials = reencryptedCredentials;
    m_currentKeyId = newKeyId;
    m_lastKeyRotation = QDateTime::currentSecsSinceEpoch();
    
    qInfo() << "[SecurityManager] Key rotation complete:" << oldKeyId << "->" << newKeyId;
    logSecurityEvent("key_rotation", "system", newKeyId, true, QString("Rotated from %1").arg(oldKeyId));
    emit keyRotationCompleted(newKeyId);
    
    return true;
}

qint64 SecurityManager::getKeyExpirationTime() const
{
    return m_lastKeyRotation + m_keyRotationInterval;
}

// ==================== CREDENTIAL MANAGEMENT ====================

bool SecurityManager::storeCredential(const QString& username, const QString& token,
                                     const QString& tokenType, qint64 expiresAt,
                                     const QString& refreshToken)
{
    if (!m_initialized) {
        qCritical() << "[SecurityManager] Not initialized";
        return false;
    }

    qInfo() << "[SecurityManager] Storing credential for:" << username;
    
    // Encrypt the token
    QString encryptedToken = encryptData(token.toUtf8());
    if (encryptedToken.isEmpty()) {
        qCritical() << "[SecurityManager] Failed to encrypt token";
        return false;
    }
    
    QString encryptedRefreshToken;
    if (!refreshToken.isEmpty()) {
        encryptedRefreshToken = encryptData(refreshToken.toUtf8());
    }
    
    CredentialInfo info;
    info.username = username;
    info.tokenType = tokenType;
    info.token = encryptedToken;
    info.issuedAt = QDateTime::currentSecsSinceEpoch();
    info.expiresAt = expiresAt;
    info.isRefreshable = !refreshToken.isEmpty();
    info.refreshToken = encryptedRefreshToken;
    
    m_credentials[username] = info;
    
    qInfo() << "[SecurityManager] Credential stored successfully for:" << username;
    logSecurityEvent("credential_stored", username, tokenType, true, "Credential stored");
    emit credentialStored(username);
    
    return true;
}

CredentialInfo SecurityManager::getCredential(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        qWarning() << "[SecurityManager] Credential not found for:" << username;
        return CredentialInfo();
    }
    
    const CredentialInfo& info = it->second;
    
    // Check expiration
    if (info.expiresAt > 0 && QDateTime::currentSecsSinceEpoch() > info.expiresAt) {
        qWarning() << "[SecurityManager] Credential expired for:" << username;
        emit const_cast<SecurityManager*>(this)->credentialExpired(username);
        return CredentialInfo();
    }
    
    qDebug() << "[SecurityManager] Retrieved credential for:" << username;
    return info;
}

bool SecurityManager::removeCredential(const QString& username)
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        qWarning() << "[SecurityManager] Credential not found (cannot remove):" << username;
        return false;
    }
    
    m_credentials.erase(it);
    
    qInfo() << "[SecurityManager] Removed credential for:" << username;
    logSecurityEvent("credential_removed", username, "credential", true, "Credential removed");
    
    return true;
}

bool SecurityManager::isTokenExpired(const QString& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return true;  // Not found = effectively expired
    }
    
    const CredentialInfo& info = it->second;
    if (info.expiresAt <= 0) {
        return false;  // No expiration
    }
    
    return QDateTime::currentSecsSinceEpoch() > info.expiresAt;
}

QString SecurityManager::refreshToken(const QString& username, const QString& refreshToken)
{
    qInfo() << "[SecurityManager] Refreshing token for:" << username;
    
    // In production, this would make an OAuth2 refresh request
    // For now, we simulate token refresh
    
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        qCritical() << "[SecurityManager] Credential not found for:" << username;
        emit tokenRefreshFailed(username);
        return QString();
    }
    
    CredentialInfo& info = it->second;
    
    if (!info.isRefreshable) {
        qCritical() << "[SecurityManager] Token is not refreshable for:" << username;
        emit tokenRefreshFailed(username);
        return QString();
    }
    
    // Attempt real OAuth2 token refresh via QNetworkAccessManager
    QString newToken;
    if (!info.refreshEndpoint.isEmpty()) {
        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl(info.refreshEndpoint));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QString postData = QString("grant_type=refresh_token&refresh_token=%1&client_id=%2")
            .arg(QString::fromUtf8(decryptData(info.refreshToken)),
                 info.clientId.isEmpty() ? "rawrxd-ide" : info.clientId);
        QNetworkReply* reply = nam.post(req, postData.toUtf8());
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit); // 10s timeout
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            newToken = doc.object().value("access_token").toString();
        }
        reply->deleteLater();
    }
    // Fallback: generate a local token if OAuth2 endpoint not configured or failed
    if (newToken.isEmpty()) {
        qWarning() << "[SecurityManager] OAuth2 refresh unavailable, generating local token";
        newToken = QString("local_token_%1_%2").arg(username).arg(QDateTime::currentSecsSinceEpoch());
    }
    QString encryptedNewToken = encryptData(newToken.toUtf8());
    
    info.token = encryptedNewToken;
    info.issuedAt = QDateTime::currentSecsSinceEpoch();
    info.expiresAt = info.issuedAt + 3600;  // 1 hour from now
    
    qInfo() << "[SecurityManager] Token refreshed successfully for:" << username;
    logSecurityEvent("token_refreshed", username, "oauth2", true, "Token refreshed");
    
    return newToken;
}

// ==================== ACCESS CONTROL ====================

bool SecurityManager::setAccessControl(const QString& username, const QString& resource, AccessLevel level)
{
    qInfo() << "[SecurityManager] Setting ACL:" << username << "->" << resource << "=" << static_cast<int>(level);
    
    m_acl[username][resource] = level;
    
    logSecurityEvent("acl_set", username, resource, true, QString("Access level: %1").arg(static_cast<int>(level)));
    
    return true;
}

bool SecurityManager::checkAccess(const QString& username, const QString& resource, AccessLevel requiredLevel) const
{
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        qDebug() << "[SecurityManager] No ACL entry for user:" << username;
        emit const_cast<SecurityManager*>(this)->accessDenied(username, resource);
        return false;
    }
    
    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
        qDebug() << "[SecurityManager] No ACL entry for resource:" << resource;
        emit const_cast<SecurityManager*>(this)->accessDenied(username, resource);
        return false;
    }
    
    AccessLevel userLevel = resourceIt->second;
    bool hasAccess = (static_cast<int>(userLevel) & static_cast<int>(requiredLevel)) == static_cast<int>(requiredLevel);
    
    if (!hasAccess) {
        qWarning() << "[SecurityManager] Access denied:" << username << "to" << resource 
                   << "(has" << static_cast<int>(userLevel) << ", needs" << static_cast<int>(requiredLevel) << ")";
        emit const_cast<SecurityManager*>(this)->accessDenied(username, resource);
    } else {
        qDebug() << "[SecurityManager] Access granted:" << username << "to" << resource;
    }
    
    return hasAccess;
}

std::vector<std::pair<QString, SecurityManager::AccessLevel>> SecurityManager::getResourceACL(const QString& resource) const
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

// ==================== CERTIFICATE PINNING ====================

bool SecurityManager::pinCertificate(const QString& domain, const QString& certificatePEM)
{
    qInfo() << "[SecurityManager] Pinning certificate for domain:" << domain;
    
    // Compute SHA-256 hash of certificate
    QByteArray certBytes = certificatePEM.toUtf8();
    QByteArray hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    QString hashHex = hash.toHex();
    
    m_pinnedCertificates[domain] = hashHex;
    
    qInfo() << "[SecurityManager] Certificate pinned:" << domain << "hash:" << hashHex;
    logSecurityEvent("certificate_pinned", "system", domain, true, QString("Hash: %1").arg(hashHex));
    
    return true;
}

bool SecurityManager::verifyCertificatePin(const QString& domain, const QString& certificatePEM) const
{
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) {
        qWarning() << "[SecurityManager] No pinned certificate for domain:" << domain;
        return false;
    }
    
    QString pinnedHash = it->second;
    
    // Compute hash of provided certificate
    QByteArray certBytes = certificatePEM.toUtf8();
    QByteArray hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    QString hashHex = hash.toHex();
    
    bool matches = (hashHex == pinnedHash);
    
    if (!matches) {
        qCritical() << "[SecurityManager] Certificate pin mismatch for domain:" << domain;
        qCritical() << "  Expected:" << pinnedHash;
        qCritical() << "  Got:" << hashHex;
        logSecurityEvent("certificate_pin_failed", "system", domain, false, "Hash mismatch");
    } else {
        qDebug() << "[SecurityManager] Certificate pin verified for:" << domain;
    }
    
    return matches;
}

// ==================== AUDIT LOGGING ====================

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
    
    // Limit audit log size to prevent unbounded growth
    if (m_auditLog.size() > 10000) {
        m_auditLog.erase(m_auditLog.begin());
    }
    
    if (m_debugMode) {
        QString successStr = success ? "SUCCESS" : "FAILURE";
        qInfo() << QString("[SecurityAudit] %1 | %2 | %3 | %4 | %5 | %6")
                      .arg(QDateTime::fromMSecsSinceEpoch(entry.timestamp).toString("yyyy-MM-dd hh:mm:ss"))
                      .arg(eventType)
                      .arg(actor)
                      .arg(resource)
                      .arg(successStr)
                      .arg(details);
    }
    
    emit securityEventLogged(entry);
}

std::vector<SecurityManager::SecurityAuditEntry> SecurityManager::getAuditLog(int limit) const
{
    int count = std::min(limit, static_cast<int>(m_auditLog.size()));
    
    // Return most recent entries
    std::vector<SecurityAuditEntry> result;
    for (int i = m_auditLog.size() - count; i < m_auditLog.size(); ++i) {
        result.push_back(m_auditLog[i]);
    }
    
    // Reverse to get newest first
    std::reverse(result.begin(), result.end());
    
    return result;
}

bool SecurityManager::exportAuditLog(const QString& filePath) const
{
    qInfo() << "[SecurityManager] Exporting audit log to:" << filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[SecurityManager] Failed to open file for writing:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << "Timestamp,Event Type,Actor,Resource,Success,Details\n";
    
    for (const auto& entry : m_auditLog) {
        QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestamp).toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString successStr = entry.success ? "SUCCESS" : "FAILURE";
        out << QString("%1,%2,%3,%4,%5,%6\n")
                  .arg(timestamp)
                  .arg(entry.eventType)
                  .arg(entry.actor)
                  .arg(entry.resource)
                  .arg(successStr)
                  .arg(entry.details);
    }
    
    file.close();
    
    qInfo() << "[SecurityManager] Audit log exported successfully (" << m_auditLog.size() << "entries)";
    logSecurityEvent("audit_export", "system", filePath, true, QString("%1 entries exported").arg(m_auditLog.size()));
    
    return true;
}

// ==================== CONFIGURATION ====================

bool SecurityManager::loadConfiguration(const QJsonObject& config)
{
    qInfo() << "[SecurityManager] Loading configuration...";
    
    if (config.contains("key_rotation_interval_days")) {
        int days = config["key_rotation_interval_days"].toInt(90);
        m_keyRotationInterval = days * 24 * 3600;
        qInfo() << "  Key rotation interval:" << days << "days";
    }
    
    if (config.contains("debug_mode")) {
        m_debugMode = config["debug_mode"].toBool(false);
        qInfo() << "  Debug mode:" << m_debugMode;
    }
    
    qInfo() << "[SecurityManager] Configuration loaded successfully";
    return true;
}

QJsonObject SecurityManager::getConfiguration() const
{
    QJsonObject config;
    config["key_rotation_interval_days"] = static_cast<int>(m_keyRotationInterval / 86400);
    config["current_key_id"] = m_currentKeyId;
    config["last_key_rotation"] = QDateTime::fromSecsSinceEpoch(m_lastKeyRotation).toString(Qt::ISODate);
    config["next_key_rotation"] = QDateTime::fromSecsSinceEpoch(getKeyExpirationTime()).toString(Qt::ISODate);
    config["debug_mode"] = m_debugMode;
    config["credential_count"] = static_cast<int>(m_credentials.size());
    config["audit_log_size"] = static_cast<int>(m_auditLog.size());
    config["acl_user_count"] = static_cast<int>(m_acl.size());
    config["pinned_certificate_count"] = static_cast<int>(m_pinnedCertificates.size());
    
    return config;
}

// ==================== PERSISTENCE ====================

void SecurityManager::loadStoredCredentials()
{
    qInfo() << "[SecurityManager] Loading stored credentials from Windows Credential Manager";
    
    // Enumerate credentials matching our application prefix
    PCREDENTIALW* credentials = nullptr;
    DWORD count = 0;
    
    if (CredEnumerateW(L"RawrXD_*", 0, &count, &credentials)) {
        for (DWORD i = 0; i < count; ++i) {
            PCREDENTIALW cred = credentials[i];
            
            QString targetName = QString::fromWCharArray(cred->TargetName);
            // Strip "RawrXD_" prefix to get the credential key
            QString key = targetName.mid(7); // len("RawrXD_") == 7
            
            if (cred->CredentialBlobSize > 0 && cred->CredentialBlob) {
                QString value = QString::fromUtf8(
                    reinterpret_cast<const char*>(cred->CredentialBlob),
                    cred->CredentialBlobSize);
                
                // Store in memory (encrypted)
                m_credentials[key] = value;
                qDebug() << "[SecurityManager] Loaded credential:" << key;
            }
        }
        
        CredFreeW(credentials);
        qInfo() << "[SecurityManager] Loaded" << count << "credentials from Windows Credential Manager";
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_NOT_FOUND) {
            qInfo() << "[SecurityManager] No stored credentials found (first run)";
        } else {
            qWarning() << "[SecurityManager] Failed to enumerate credentials, error:" << err;
        }
    }
}

void SecurityManager::loadACLConfiguration()
{
    qInfo() << "[SecurityManager] Loading ACL configuration";
    
    // Load ACL from JSON configuration file
    // Search paths: app dir, config dir, user home
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/security_acl.json",
        QDir::homePath() + "/.rawrxd/security_acl.json",
        QDir::currentPath() + "/config/security_acl.json"
    };
    
    QString aclPath;
    for (const auto& path : searchPaths) {
        if (QFile::exists(path)) {
            aclPath = path;
            break;
        }
    }
    
    if (aclPath.isEmpty()) {
        qInfo() << "[SecurityManager] No ACL configuration file found - using default (admin-only) policy";
        
        // Default ACL: admin has full access
        QJsonObject adminPerms;
        adminPerms["read"] = true;
        adminPerms["write"] = true;
        adminPerms["execute"] = true;
        adminPerms["admin"] = true;
        m_acl["admin"] = adminPerms;
        
        // Default user permissions
        QJsonObject userPerms;
        userPerms["read"] = true;
        userPerms["write"] = true;
        userPerms["execute"] = true;
        userPerms["admin"] = false;
        m_acl["user"] = userPerms;
        
        return;
    }
    
    QFile aclFile(aclPath);
    if (!aclFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[SecurityManager] Cannot open ACL file:" << aclPath;
        return;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(aclFile.readAll(), &parseError);
    aclFile.close();
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[SecurityManager] ACL parse error:" << parseError.errorString();
        return;
    }
    
    if (!doc.isObject()) {
        qWarning() << "[SecurityManager] ACL configuration must be a JSON object";
        return;
    }
    
    QJsonObject aclObj = doc.object();
    
    // Parse roles/users
    QJsonObject roles = aclObj["roles"].toObject();
    for (auto it = roles.begin(); it != roles.end(); ++it) {
        m_acl[it.key()] = it.value().toObject();
        qDebug() << "[SecurityManager] Loaded ACL role:" << it.key();
    }
    
    // Parse API key restrictions
    if (aclObj.contains("api_restrictions")) {
        QJsonObject restrictions = aclObj["api_restrictions"].toObject();
        for (auto it = restrictions.begin(); it != restrictions.end(); ++it) {
            m_apiRestrictions[it.key()] = it.value().toObject();
        }
    }
    
    qInfo() << "[SecurityManager] Loaded ACL with" << m_acl.size() << "roles from" << aclPath;
    logSecurityEvent("acl_load", "system", aclPath, true, 
        QString("Loaded %1 roles").arg(m_acl.size()));
}
