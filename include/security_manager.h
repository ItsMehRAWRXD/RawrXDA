#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QByteArray>
#include <QHash>
#include <QDateTime>
#include <vector>
#include <memory>
#include <map>

/**
 * @class SecurityManager
 * @brief Production-grade security management for the IDE
 *
 * Features:
 * - AES-256-GCM encryption for sensitive data (API keys, credentials)
 * - HMAC-SHA256 for data integrity verification
 * - Secure key derivation (PBKDF2)
 * - OAuth2 token management and refresh
 * - Certificate pinning for HTTPS connections
 * - Secure credential storage (with Windows DPAPI encryption)
 * - API key rotation tracking
 * - Access control lists (ACL)
 * - Audit logging of security events
 *
 * Thread-safe singleton pattern (use getInstance())
 */
class SecurityManager : public QObject
{
    Q_OBJECT

public:
    enum class EncryptionAlgorithm {
        AES256_GCM,     // Default: AES-256 with GCM mode
        AES256_CBC,     // AES-256 with CBC mode
        ChaCha20Poly1305 // ChaCha20-Poly1305
    };

    enum class AccessLevel {
        None = 0,
        Read = 1,
        Write = 2,
        Execute = 4,
        Admin = 7  // Full access
    };
    Q_ENUM(AccessLevel)

    struct CredentialInfo {
        QString username;
        QString email;
        QString tokenType;      // "bearer", "basic", "api_key"
        QString token;          // Encrypted
        qint64 issuedAt;        // Unix timestamp
        qint64 expiresAt;       // Unix timestamp
        bool isRefreshable;
        QString refreshToken;   // For OAuth2
    };

    struct SecurityAuditEntry {
        qint64 timestamp;
        QString eventType;      // "login", "key_rotation", "decryption", "auth_failed"
        QString actor;          // User or service
        QString resource;
        bool success;
        QString details;
    };

    // Singleton access
    static SecurityManager* getInstance();

    // Initialize security (load keys, initialize crypto)
    bool initialize(const QString& masterPassword = "");

    // ===== Encryption/Decryption =====
    /**
     * @brief Encrypt data using AES-256-GCM
     * @param plaintext Data to encrypt
     * @param algorithm Algorithm to use (default: AES256_GCM)
     * @return Encrypted data (base64 encoded with IV + tag)
     */
    QString encryptData(const QByteArray& plaintext, 
                       EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256_GCM);

    /**
     * @brief Decrypt data
     * @param ciphertext Encrypted data (base64 encoded)
     * @return Decrypted plaintext or empty if failed
     */
    QByteArray decryptData(const QString& ciphertext);

    // ===== HMAC & Integrity =====
    /**
     * @brief Generate HMAC-SHA256 for data integrity
     * @param data Data to hash
     * @return HMAC-SHA256 (hex encoded)
     */
    QString generateHMAC(const QByteArray& data);

    /**
     * @brief Verify HMAC
     * @param data Original data
     * @param hmac HMAC to verify (hex encoded)
     * @return true if HMAC is valid
     */
    bool verifyHMAC(const QByteArray& data, const QString& hmac);

    // ===== Key Management =====
    /**
     * @brief Generate and store new encryption key
     * @param keyId Unique identifier for the key
     * @param algorithm Algorithm for key derivation
     * @return true if successful
     */
    bool generateNewKey(const QString& keyId, EncryptionAlgorithm algorithm);

    /**
     * @brief Rotate current encryption key
     * @return true if successful; emits keyRotationCompleted signal
     */
    bool rotateEncryptionKey();

    /**
     * @brief Get key expiration date
     * @return Unix timestamp when key expires
     */
    qint64 getKeyExpirationTime() const;

    // ===== Credential Management =====
    /**
     * @brief Store OAuth2 or API credentials securely
     * @param username User identifier
     * @param token Auth token or API key
     * @param tokenType "bearer", "basic", or "api_key"
     * @param expiresAt Expiration time (unix timestamp)
     * @param refreshToken For OAuth2 refresh flow
     * @return true if successful
     */
    bool storeCredential(const QString& username, const QString& token,
                        const QString& tokenType = "bearer", qint64 expiresAt = 0,
                        const QString& refreshToken = "");

    /**
     * @brief Retrieve credential
     * @param username User identifier
     * @return Credential info or empty if not found/expired
     */
    CredentialInfo getCredential(const QString& username) const;

    /**
     * @brief Remove credential
     * @param username User identifier
     * @return true if successful
     */
    bool removeCredential(const QString& username);

    /**
     * @brief Check if token is expired
     * @param username User identifier
     * @return true if token has expired
     */
    bool isTokenExpired(const QString& username) const;

    /**
     * @brief Refresh OAuth2 token
     * @param username User identifier
     * @param refreshToken Refresh token from original auth
     * @return New token or empty if refresh failed
     */
    QString refreshToken(const QString& username, const QString& refreshToken = "");

    // ===== Access Control =====
    /**
     * @brief Set access level for resource
     * @param username User identifier
     * @param resource Resource path (e.g., "models/training/advanced")
     * @param level Access level (Read, Write, Execute, Admin)
     * @return true if successful
     */
    bool setAccessControl(const QString& username, const QString& resource,
                         AccessLevel level);

    /**
     * @brief Check if user has access to resource
     * @param username User identifier
     * @param resource Resource path
     * @param requiredLevel Required access level
     * @return true if user has sufficient access
     */
    bool checkAccess(const QString& username, const QString& resource,
                    AccessLevel requiredLevel) const;

    /**
     * @brief Get all users with access to resource
     * @param resource Resource path
     * @return List of (username, accessLevel) pairs
     */
    std::vector<std::pair<QString, AccessLevel>> getResourceACL(const QString& resource) const;

    // ===== Certificate Pinning =====
    /**
     * @brief Pin certificate for domain
     * @param domain Domain name (e.g., "api.example.com")
     * @param certificatePEM Certificate in PEM format
     * @return true if successful
     */
    bool pinCertificate(const QString& domain, const QString& certificatePEM);

    /**
     * @brief Verify certificate against pinned certificate
     * @param domain Domain name
     * @param certificatePEM Certificate to verify
     * @return true if matches pinned certificate
     */
    bool verifyCertificatePin(const QString& domain, const QString& certificatePEM) const;

    // ===== Audit Logging =====
    /**
     * @brief Log security event
     * @param eventType Type of event
     * @param actor User or service performing action
     * @param resource Resource involved
     * @param success Whether action succeeded
     * @param details Additional details
     */
    void logSecurityEvent(const QString& eventType, const QString& actor,
                         const QString& resource, bool success, const QString& details = "");

    /**
     * @brief Get audit log entries
     * @param limit Maximum number of entries to return
     * @return List of audit entries (most recent first)
     */
    std::vector<SecurityAuditEntry> getAuditLog(int limit = 100) const;

    /**
     * @brief Export audit log to file
     * @param filePath Path to export to
     * @return true if successful
     */
    bool exportAuditLog(const QString& filePath) const;

    // ===== Configuration =====
    /**
     * @brief Load security configuration from JSON
     * @param config Configuration object
     * @return true if successful
     */
    bool loadConfiguration(const QJsonObject& config);

    /**
     * @brief Get current security configuration
     * @return Configuration as JSON
     */
    QJsonObject getConfiguration() const;

    /**
     * @brief Validate security setup
     * @return true if all security components are properly initialized
     */
    bool validateSetup() const;

signals:
    void keyRotationCompleted(const QString& newKeyId);
    void credentialStored(const QString& username);
    void credentialExpired(const QString& username);
    void tokenRefreshFailed(const QString& username);
    void accessDenied(const QString& username, const QString& resource);
    void securityEventLogged(const SecurityAuditEntry& entry);

private:
    // Private constructor for singleton
    explicit SecurityManager(QObject* parent = nullptr);
    
    // ===== Encryption Implementation =====
    QByteArray deriveKeyPBKDF2(const QString& password, const QByteArray& salt, int iterations);
    QByteArray encryptAES256GCM(const QByteArray& plaintext, const QByteArray& key);
    QByteArray decryptAES256GCM(const QByteArray& ciphertext, const QByteArray& key);
    QByteArray encryptAES256CBC(const QByteArray& plaintext, const QByteArray& key);
    QByteArray decryptAES256CBC(const QByteArray& ciphertext, const QByteArray& key);

    // Static singleton instance
    static std::unique_ptr<SecurityManager> s_instance;

    // ===== Internal State =====
    QByteArray m_masterKey;                           // Primary encryption key
    QString m_currentKeyId;                           // ID of active key
    qint64 m_keyRotationInterval;                     // Seconds between key rotations
    qint64 m_lastKeyRotation;                         // Last rotation timestamp
    
    // Credentials storage (encrypted)
    std::map<QString, CredentialInfo> m_credentials;  // username -> CredentialInfo
    
    // Access control (ACL)
    std::map<QString, std::map<QString, AccessLevel>> m_acl; // username -> (resource -> level)
    
    // Certificate pinning
    std::map<QString, QString> m_pinnedCertificates;  // domain -> certificate hash
    
    // Audit trail
    std::vector<SecurityAuditEntry> m_auditLog;       // Security events (max 10000 entries)
    
    bool m_initialized;
    bool m_debugMode;                                 // Log detailed security events
};
