<<<<<<< HEAD
#pragma once

/**
 * SecurityManager — C++20, no Qt. AES-256-GCM, HMAC, credentials, ACL, audit.
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

class SecurityManager
{
public:
    enum class EncryptionAlgorithm {
        AES256_GCM,
        AES256_CBC,
        ChaCha20Poly1305
    };

    enum class AccessLevel {
        None = 0,
        Read = 1,
        Write = 2,
        Execute = 4,
        Admin = 7
    };

    struct CredentialInfo {
        std::string username;
        std::string email;
        std::string tokenType;
        std::string token;
        int64_t issuedAt = 0;
        int64_t expiresAt = 0;
        bool isRefreshable = false;
        std::string refreshToken;
    };

    struct SecurityAuditEntry {
        int64_t timestamp = 0;
        std::string eventType;
        std::string actor;
        std::string resource;
        bool success = false;
        std::string details;
    };

    static SecurityManager* getInstance();

    bool initialize(const std::string& masterPassword = "");

    std::string encryptData(const std::vector<uint8_t>& plaintext,
                            EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256_GCM);
    std::vector<uint8_t> decryptData(const std::string& ciphertext);

    std::string generateHMAC(const std::vector<uint8_t>& data);
    bool verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac);

    bool generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm);
    bool rotateEncryptionKey();
    int64_t getKeyExpirationTime() const;

    bool storeCredential(const std::string& username, const std::string& token,
                        const std::string& tokenType = "bearer", int64_t expiresAt = 0,
                        const std::string& refreshToken = "");
    CredentialInfo getCredential(const std::string& username) const;
    bool removeCredential(const std::string& username);
    bool isTokenExpired(const std::string& username) const;
    std::string refreshToken(const std::string& username, const std::string& refreshToken = "");

    bool setAccessControl(const std::string& username, const std::string& resource, AccessLevel level);
    bool checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const;
    std::vector<std::pair<std::string, AccessLevel>> getResourceACL(const std::string& resource) const;

    bool pinCertificate(const std::string& domain, const std::string& certificatePEM);
    bool verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const;

    void logSecurityEvent(const std::string& eventType, const std::string& actor,
                          const std::string& resource, bool success, const std::string& details = "");
    std::vector<SecurityAuditEntry> getAuditLog(int limit = 100) const;
    bool exportAuditLog(const std::string& filePath) const;

    bool loadConfiguration(const std::string& configJson);
    std::string getConfiguration() const;
    bool validateSetup() const;

    using KeyRotationCompletedFn = std::function<void(const std::string& newKeyId)>;
    using CredentialStoredFn    = std::function<void(const std::string& username)>;
    using CredentialExpiredFn   = std::function<void(const std::string& username)>;
    using TokenRefreshFailedFn  = std::function<void(const std::string& username)>;
    using AccessDeniedFn       = std::function<void(const std::string& username, const std::string& resource)>;
    using SecurityEventLoggedFn = std::function<void(const SecurityAuditEntry& entry)>;

    void setOnKeyRotationCompleted(KeyRotationCompletedFn f)   { m_onKeyRotationCompleted = std::move(f); }
    void setOnCredentialStored(CredentialStoredFn f)            { m_onCredentialStored = std::move(f); }
    void setOnCredentialExpired(CredentialExpiredFn f)         { m_onCredentialExpired = std::move(f); }
    void setOnTokenRefreshFailed(TokenRefreshFailedFn f)       { m_onTokenRefreshFailed = std::move(f); }
    void setOnAccessDenied(AccessDeniedFn f)                   { m_onAccessDenied = std::move(f); }
    void setOnSecurityEventLogged(SecurityEventLoggedFn f)    { m_onSecurityEventLogged = std::move(f); }

private:
    explicit SecurityManager();

    std::vector<uint8_t> deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations);
    std::vector<uint8_t> encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256GCM(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);

    static std::unique_ptr<SecurityManager> s_instance;

    std::vector<uint8_t> m_masterKey;
    std::string m_currentKeyId;
    int64_t m_keyRotationInterval = 0;
    int64_t m_lastKeyRotation = 0;
    std::map<std::string, CredentialInfo> m_credentials;
    std::map<std::string, std::map<std::string, AccessLevel>> m_acl;
    std::map<std::string, std::string> m_pinnedCertificates;
    std::vector<SecurityAuditEntry> m_auditLog;
    bool m_initialized = false;
    bool m_debugMode = false;

    KeyRotationCompletedFn   m_onKeyRotationCompleted;
    CredentialStoredFn       m_onCredentialStored;
    CredentialExpiredFn      m_onCredentialExpired;
    TokenRefreshFailedFn     m_onTokenRefreshFailed;
    AccessDeniedFn           m_onAccessDenied;
    SecurityEventLoggedFn    m_onSecurityEventLogged;
};
=======
#pragma once

/**
 * SecurityManager — C++20, no Qt. AES-256-GCM, HMAC, credentials, ACL, audit.
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

class SecurityManager
{
public:
    enum class EncryptionAlgorithm {
        AES256_GCM,
        AES256_CBC,
        ChaCha20Poly1305
    };

    enum class AccessLevel {
        None = 0,
        Read = 1,
        Write = 2,
        Execute = 4,
        Admin = 7
    };

    struct CredentialInfo {
        std::string username;
        std::string email;
        std::string tokenType;
        std::string token;
        int64_t issuedAt = 0;
        int64_t expiresAt = 0;
        bool isRefreshable = false;
        std::string refreshToken;
    };

    struct SecurityAuditEntry {
        int64_t timestamp = 0;
        std::string eventType;
        std::string actor;
        std::string resource;
        bool success = false;
        std::string details;
    };

    static SecurityManager* getInstance();

    bool initialize(const std::string& masterPassword = "");

    std::string encryptData(const std::vector<uint8_t>& plaintext,
                            EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256_GCM);
    std::vector<uint8_t> decryptData(const std::string& ciphertext);

    std::string generateHMAC(const std::vector<uint8_t>& data);
    bool verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac);

    bool generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm);
    bool rotateEncryptionKey();
    int64_t getKeyExpirationTime() const;

    bool storeCredential(const std::string& username, const std::string& token,
                        const std::string& tokenType = "bearer", int64_t expiresAt = 0,
                        const std::string& refreshToken = "");
    CredentialInfo getCredential(const std::string& username) const;
    bool removeCredential(const std::string& username);
    bool isTokenExpired(const std::string& username) const;
    std::string refreshToken(const std::string& username, const std::string& refreshToken = "");

    bool setAccessControl(const std::string& username, const std::string& resource, AccessLevel level);
    bool checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const;
    std::vector<std::pair<std::string, AccessLevel>> getResourceACL(const std::string& resource) const;

    bool pinCertificate(const std::string& domain, const std::string& certificatePEM);
    bool verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const;

    void logSecurityEvent(const std::string& eventType, const std::string& actor,
                          const std::string& resource, bool success, const std::string& details = "");
    std::vector<SecurityAuditEntry> getAuditLog(int limit = 100) const;
    bool exportAuditLog(const std::string& filePath) const;

    bool loadConfiguration(const std::string& configJson);
    std::string getConfiguration() const;
    bool validateSetup() const;

    using KeyRotationCompletedFn = std::function<void(const std::string& newKeyId)>;
    using CredentialStoredFn    = std::function<void(const std::string& username)>;
    using CredentialExpiredFn   = std::function<void(const std::string& username)>;
    using TokenRefreshFailedFn  = std::function<void(const std::string& username)>;
    using AccessDeniedFn       = std::function<void(const std::string& username, const std::string& resource)>;
    using SecurityEventLoggedFn = std::function<void(const SecurityAuditEntry& entry)>;

    void setOnKeyRotationCompleted(KeyRotationCompletedFn f)   { m_onKeyRotationCompleted = std::move(f); }
    void setOnCredentialStored(CredentialStoredFn f)            { m_onCredentialStored = std::move(f); }
    void setOnCredentialExpired(CredentialExpiredFn f)         { m_onCredentialExpired = std::move(f); }
    void setOnTokenRefreshFailed(TokenRefreshFailedFn f)       { m_onTokenRefreshFailed = std::move(f); }
    void setOnAccessDenied(AccessDeniedFn f)                   { m_onAccessDenied = std::move(f); }
    void setOnSecurityEventLogged(SecurityEventLoggedFn f)    { m_onSecurityEventLogged = std::move(f); }

private:
    explicit SecurityManager();

    std::vector<uint8_t> deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations);
    std::vector<uint8_t> encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256GCM(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);

    static std::unique_ptr<SecurityManager> s_instance;

    std::vector<uint8_t> m_masterKey;
    std::string m_currentKeyId;
    int64_t m_keyRotationInterval = 0;
    int64_t m_lastKeyRotation = 0;
    std::map<std::string, CredentialInfo> m_credentials;
    std::map<std::string, std::map<std::string, AccessLevel>> m_acl;
    std::map<std::string, std::string> m_pinnedCertificates;
    std::vector<SecurityAuditEntry> m_auditLog;
    bool m_initialized = false;
    bool m_debugMode = false;

    KeyRotationCompletedFn   m_onKeyRotationCompleted;
    CredentialStoredFn       m_onCredentialStored;
    CredentialExpiredFn      m_onCredentialExpired;
    TokenRefreshFailedFn     m_onTokenRefreshFailed;
    AccessDeniedFn           m_onAccessDenied;
    SecurityEventLoggedFn    m_onSecurityEventLogged;
};
>>>>>>> origin/main
