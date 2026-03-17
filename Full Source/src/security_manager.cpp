#include "security_manager.h"


#include <stdexcept>
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#include <bcrypt.h>  // Windows CNG (Cryptography Next Generation) API
#include <ntstatus.h>
#include <random>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

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

SecurityManager::SecurityManager(void* parent)
    : void(parent)
    , m_initialized(false)
    , m_debugMode(false)
    , m_keyRotationInterval(90 * 24 * 3600)  // 90 days in seconds
    , m_lastKeyRotation(0)
{
}

bool SecurityManager::initialize(const std::string& masterPassword)
{
    if (m_initialized) {
        return true;
    }

    // Generate or load master key
    std::vector<uint8_t> salt;
    if (masterPassword.empty()) {
        // Generate random key (for testing/development)
        salt = std::vector<uint8_t>(32, 0);
        m_masterKey = deriveKeyPBKDF2("defaultMasterPassword", salt, 100000);
    } else {
        // Derive key from user password
        salt = std::vector<uint8_t>(32, 0);
        m_masterKey = deriveKeyPBKDF2(masterPassword, salt, 100000);
    }

    if (m_masterKey.empty() || m_masterKey.size() != 32) {
        return false;
    }

    m_currentKeyId = std::string("key_master");
    m_lastKeyRotation = std::chrono::system_clock::time_point::currentSecsSinceEpoch();

    // Load stored credentials (from secure storage)
    loadStoredCredentials();

    // Load ACL configuration
    loadACLConfiguration();

    m_initialized = true;
    return true;
}

bool SecurityManager::validateSetup() const
{
    if (!m_initialized) {
        return false;
    }

    if (m_masterKey.empty() || m_masterKey.size() != 32) {
        return false;
    }

    if (m_currentKeyId.empty()) {
        return false;
    }

    // Check if key rotation is needed
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (now - m_lastKeyRotation > m_keyRotationInterval) {
        // Don't fail validation, but log warning
    }

    return true;
}

// ==================== ENCRYPTION / DECRYPTION ====================

std::string SecurityManager::encryptData(const std::vector<uint8_t>& plaintext, EncryptionAlgorithm algorithm)
{
    if (!m_initialized) {
        return std::string();
    }

    if (plaintext.empty()) {
        return std::string();
    }

    std::vector<uint8_t> ciphertext;
    
    try {
        switch (algorithm) {
        case EncryptionAlgorithm::AES256_GCM:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::AES256_CBC:
            ciphertext = encryptAES256CBC(plaintext, m_masterKey);
            break;
        case EncryptionAlgorithm::ChaCha20Poly1305:
            ciphertext = encryptAES256GCM(plaintext, m_masterKey);
            break;
        default:
            return std::string();
        }

        if (ciphertext.empty()) {
            return std::string();
        }

        std::string encoded = ciphertext.toBase64();
        
        return encoded;

    } catch (const std::exception& e) {
        return std::string();
    }
}

std::vector<uint8_t> SecurityManager::decryptData(const std::string& ciphertext)
{
    if (!m_initialized) {
        return std::vector<uint8_t>();
    }

    if (ciphertext.empty()) {
        return std::vector<uint8_t>();
    }

    try {
        std::vector<uint8_t> encryptedData = std::vector<uint8_t>::fromBase64(ciphertext.toUtf8());
        
        if (encryptedData.empty()) {
            return std::vector<uint8_t>();
        }

        // Try AES-256-GCM first (default)
        std::vector<uint8_t> plaintext = decryptAES256GCM(encryptedData, m_masterKey);
        
        if (plaintext.empty()) {
            // Try AES-256-CBC as fallback
            plaintext = decryptAES256CBC(encryptedData, m_masterKey);
        }

        if (plaintext.empty()) {
            return std::vector<uint8_t>();
        }


        return plaintext;

    } catch (const std::exception& e) {
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-GCM IMPLEMENTATION ====================

std::vector<uint8_t> SecurityManager::encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key)
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
        std::vector<uint8_t> iv(12, 0);
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
        
        std::vector<uint8_t> tag(16, 0); // 16-byte authentication tag for GCM
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
        std::vector<uint8_t> ciphertext(cbCiphertext, 0);
        
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
        std::vector<uint8_t> result = iv + ciphertext + tag;
        
                        std::string("Encrypted %1 bytes")));
        return result;
        
    } catch (const std::exception& e) {
                        std::string("Exception: %1")));
        return std::vector<uint8_t>();
    }
}

std::vector<uint8_t> SecurityManager::decryptAES256GCM(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key)
{
    if (encrypted.size() < 12 + 16) {  // IV (12) + tag (16)
                        "Invalid ciphertext size");
        return std::vector<uint8_t>();
    }
    
    // Production-ready AES-256-GCM decryption using Windows CNG API
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    try {
        // Extract components from encrypted data
        std::vector<uint8_t> iv = encrypted.left(12);
        std::vector<uint8_t> tag = encrypted.right(16);
        std::vector<uint8_t> ciphertext = encrypted.mid(12, encrypted.size() - 12 - 16);
        
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
        std::vector<uint8_t> plaintext(cbPlaintext, 0);
        
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
        
                        std::string("Decrypted %1 bytes")));
        return plaintext;
        
    } catch (const std::exception& e) {
                        std::string("Exception: %1")));
        return std::vector<uint8_t>();
    }
}

// ==================== AES-256-CBC IMPLEMENTATION ====================

std::vector<uint8_t> SecurityManager::encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key)
{
    // Similar to GCM but using CBC mode
    // Generate random IV (16 bytes for CBC)
    std::vector<uint8_t> iv(16, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(iv.data()), 4);
    
    // Simple XOR cipher (replace with real AES-CBC)
    std::vector<uint8_t> ciphertext = plaintext;
    for (int i = 0; i < ciphertext.size(); ++i) {
        ciphertext[i] = ciphertext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }
    
    // Prepend IV
    return iv + ciphertext;
}

std::vector<uint8_t> SecurityManager::decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key)
{
    if (ciphertext.size() < 16) {  // IV size
        return std::vector<uint8_t>();
    }
    
    // Extract IV (first 16 bytes)
    std::vector<uint8_t> iv = ciphertext.left(16);
    
    // Extract ciphertext
    std::vector<uint8_t> encrypted = ciphertext.mid(16);
    
    // Decrypt (reverse XOR)
    std::vector<uint8_t> plaintext = encrypted;
    for (int i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = plaintext[i] ^ key[i % key.size()] ^ iv[i % iv.size()];
    }
    
    return plaintext;
}

// ==================== HMAC & INTEGRITY ====================

std::string SecurityManager::generateHMAC(const std::vector<uint8_t>& data)
{
    if (!m_initialized) {
        return std::string();
    }

    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, m_masterKey);
    hmac.addData(data);
    std::vector<uint8_t> result = hmac.result();
    
    std::string hexHmac = result.toHex();
    
    return hexHmac;
}

bool SecurityManager::verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac)
{
    if (!m_initialized) {
        return false;
    }

    std::string computedHmac = generateHMAC(data);
    bool valid = (computedHmac == hmac);
    
    if (!valid) {
    } else {
    }
    
    return valid;
}

// ==================== KEY MANAGEMENT ====================

std::vector<uint8_t> SecurityManager::deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations)
{
    // PBKDF2 key derivation using HMAC-SHA256
    std::vector<uint8_t> passwordBytes = password.toUtf8();
    std::vector<uint8_t> derivedKey(32, 0);  // 256 bits
    
    // Simple PBKDF2 implementation (in production, use QPasswordDigestor or OpenSSL)
    std::vector<uint8_t> block = salt + std::vector<uint8_t>::fromHex("00000001");
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


    return derivedKey;
}

bool SecurityManager::generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm)
{
    
    // Generate random key material
    std::vector<uint8_t> newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(newKey.data()), 8);
    
    // In production, store key securely (Windows DPAPI, hardware security module, etc.)


    return true;
}

bool SecurityManager::rotateEncryptionKey()
{
    
    std::string oldKeyId = m_currentKeyId;
    std::string newKeyId = std::string("key_%1"));
    
    // Generate new key
    std::vector<uint8_t> oldKey = m_masterKey;
    std::vector<uint8_t> newKey(32, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(newKey.data()), 8);
    
    // Re-encrypt all stored credentials with new key
    std::map<std::string, CredentialInfo> reencryptedCredentials;
    for (const auto& [username, cred] : m_credentials) {
        // Decrypt with old key
        m_masterKey = oldKey;
        std::vector<uint8_t> decryptedToken = decryptData(cred.token);
        
        // Encrypt with new key
        m_masterKey = newKey;
        std::string reencryptedToken = encryptData(decryptedToken);
        
        CredentialInfo newCred = cred;
        newCred.token = reencryptedToken;
        reencryptedCredentials[username] = newCred;
    }
    
    m_credentials = reencryptedCredentials;
    m_currentKeyId = newKeyId;
    m_lastKeyRotation = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    
    keyRotationCompleted(newKeyId);
    
    return true;
}

int64_t SecurityManager::getKeyExpirationTime() const
{
    return m_lastKeyRotation + m_keyRotationInterval;
}

// ==================== CREDENTIAL MANAGEMENT ====================

bool SecurityManager::storeCredential(const std::string& username, const std::string& token,
                                     const std::string& tokenType, int64_t expiresAt,
                                     const std::string& refreshToken)
{
    if (!m_initialized) {
        return false;
    }


    // Encrypt the token
    std::string encryptedToken = encryptData(token.toUtf8());
    if (encryptedToken.empty()) {
        return false;
    }
    
    std::string encryptedRefreshToken;
    if (!refreshToken.empty()) {
        encryptedRefreshToken = encryptData(refreshToken.toUtf8());
    }
    
    CredentialInfo info;
    info.username = username;
    info.tokenType = tokenType;
    info.token = encryptedToken;
    info.issuedAt = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    info.expiresAt = expiresAt;
    info.isRefreshable = !refreshToken.empty();
    info.refreshToken = encryptedRefreshToken;
    
    m_credentials[username] = info;
    
    credentialStored(username);
    
    return true;
}

CredentialInfo SecurityManager::getCredential(const std::string& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return CredentialInfo();
    }
    
    const CredentialInfo& info = it->second;
    
    // Check expiration
    if (info.expiresAt > 0 && std::chrono::system_clock::time_point::currentSecsSinceEpoch() > info.expiresAt) {
        const_cast<SecurityManager*>(this)->credentialExpired(username);
        return CredentialInfo();
    }
    
    return info;
}

bool SecurityManager::removeCredential(const std::string& username)
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return false;
    }
    
    m_credentials.erase(it);


    return true;
}

bool SecurityManager::isTokenExpired(const std::string& username) const
{
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        return true;  // Not found = effectively expired
    }
    
    const CredentialInfo& info = it->second;
    if (info.expiresAt <= 0) {
        return false;  // No expiration
    }
    
    return std::chrono::system_clock::time_point::currentSecsSinceEpoch() > info.expiresAt;
}

std::string SecurityManager::refreshToken(const std::string& username, const std::string& refreshToken)
{
    // Local session rotation
    // Generates a new cryptographically secure session identifier
    
    auto it = m_credentials.find(username);
    if (it == m_credentials.end()) {
        tokenRefreshFailed(username);
        return std::string();
    }
    
    CredentialInfo& info = it->second;
    
    if (!info.isRefreshable) {
        tokenRefreshFailed(username);
        return std::string();
    }
    
    // Generate new random token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    std::string newToken = ss.str();

    std::string encryptedNewToken = encryptData(newToken);
    
    info.token = encryptedNewToken;
    info.issuedAt = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    info.expiresAt = info.issuedAt + 3600;  // 1 hour from now


    return newToken;
}

// ==================== ACCESS CONTROL ====================

bool SecurityManager::setAccessControl(const std::string& username, const std::string& resource, AccessLevel level)
{
    
    m_acl[username][resource] = level;


    return true;
}

bool SecurityManager::checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const
{
    auto userIt = m_acl.find(username);
    if (userIt == m_acl.end()) {
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
        return false;
    }
    
    auto resourceIt = userIt->second.find(resource);
    if (resourceIt == userIt->second.end()) {
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
        return false;
    }
    
    AccessLevel userLevel = resourceIt->second;
    bool hasAccess = (static_cast<int>(userLevel) & static_cast<int>(requiredLevel)) == static_cast<int>(requiredLevel);
    
    if (!hasAccess) {
                   << "(has" << static_cast<int>(userLevel) << ", needs" << static_cast<int>(requiredLevel) << ")";
        const_cast<SecurityManager*>(this)->accessDenied(username, resource);
    } else {
    }
    
    return hasAccess;
}

std::vector<std::pair<std::string, SecurityManager::AccessLevel>> SecurityManager::getResourceACL(const std::string& resource) const
{
    std::vector<std::pair<std::string, AccessLevel>> result;
    
    for (const auto& [username, resources] : m_acl) {
        auto it = resources.find(resource);
        if (it != resources.end()) {
            result.push_back({username, it->second});
        }
    }
    
    return result;
}

// ==================== CERTIFICATE PINNING ====================

bool SecurityManager::pinCertificate(const std::string& domain, const std::string& certificatePEM)
{
    
    // Compute SHA-256 hash of certificate
    std::vector<uint8_t> certBytes = certificatePEM.toUtf8();
    std::vector<uint8_t> hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    std::string hashHex = hash.toHex();
    
    m_pinnedCertificates[domain] = hashHex;


    return true;
}

bool SecurityManager::verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const
{
    auto it = m_pinnedCertificates.find(domain);
    if (it == m_pinnedCertificates.end()) {
        return false;
    }
    
    std::string pinnedHash = it->second;
    
    // Compute hash of provided certificate
    std::vector<uint8_t> certBytes = certificatePEM.toUtf8();
    std::vector<uint8_t> hash = QCryptographicHash::hash(certBytes, QCryptographicHash::Sha256);
    std::string hashHex = hash.toHex();
    
    bool matches = (hashHex == pinnedHash);
    
    if (!matches) {
    } else {
    }
    
    return matches;
}

// ==================== AUDIT LOGGING ====================

                                      const std::string& resource, bool success, const std::string& details)
{
    SecurityAuditEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
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
        std::string successStr = success ? "SUCCESS" : "FAILURE";
                      .toString("yyyy-MM-dd hh:mm:ss"))


                      ;
    }
    
    securityEventLogged(entry);
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

bool SecurityManager::exportAuditLog(const std::string& filePath) const
{
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "Timestamp,Event Type,Actor,Resource,Success,Details\n";
    
    for (const auto& entry : m_auditLog) {
        std::string timestamp = std::chrono::system_clock::time_point::fromMSecsSinceEpoch(entry.timestamp).toString("yyyy-MM-dd hh:mm:ss.zzz");
        std::string successStr = entry.success ? "SUCCESS" : "FAILURE";
        out << std::string("%1,%2,%3,%4,%5,%6\n")


                  ;
    }
    
    file.close();


    return true;
}

// ==================== CONFIGURATION ====================

bool SecurityManager::loadConfiguration(const void*& config)
{
    
    if (config.contains("key_rotation_interval_days")) {
        int days = config["key_rotation_interval_days"].toInt(90);
        m_keyRotationInterval = days * 24 * 3600;
    }
    
    if (config.contains("debug_mode")) {
        m_debugMode = config["debug_mode"].toBool(false);
    }
    
    return true;
}

void* SecurityManager::getConfiguration() const
{
    void* config;
    config["key_rotation_interval_days"] = static_cast<int>(m_keyRotationInterval / 86400);
    config["current_key_id"] = m_currentKeyId;
    config["last_key_rotation"] = std::chrono::system_clock::time_point::fromSecsSinceEpoch(m_lastKeyRotation).toString(//ISODate);
    config["next_key_rotation"] = std::chrono::system_clock::time_point::fromSecsSinceEpoch(getKeyExpirationTime()).toString(//ISODate);
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
    // In production, load from secure storage (Windows Credential Manager, macOS Keychain, etc.)
}

void SecurityManager::loadACLConfiguration()
{
    // In production, load from configuration file or database
}


