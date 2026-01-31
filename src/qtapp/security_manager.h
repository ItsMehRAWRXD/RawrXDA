#pragma once


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
class SecurityManager : public void
{

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

    struct CredentialInfo {
        std::string username;
        std::string email;
        std::string tokenType;      // "bearer", "basic", "api_key"
        std::string token;          // Encrypted
        int64_t issuedAt;        // Unix timestamp
        int64_t expiresAt;       // Unix timestamp
        bool isRefreshable;
        std::string refreshToken;   // For OAuth2
    };

    struct SecurityAuditEntry {
        int64_t timestamp;
        std::string eventType;      // "login", "key_rotation", "decryption", "auth_failed"
        std::string actor;          // User or service
        std::string resource;
        bool success;
        std::string details;
    };

    // Singleton access
    static SecurityManager* getInstance();

    // Initialize security (load keys, initialize crypto)
    bool initialize(const std::string& masterPassword = "");

    // ===== Encryption/Decryption =====
    /**
     * @brief Encrypt data using AES-256-GCM
     * @param plaintext Data to encrypt
     * @param algorithm Algorithm to use (default: AES256_GCM)
     * @return Encrypted data (base64 encoded with IV + tag)
     */
    std::string encryptData(const std::vector<uint8_t>& plaintext, 
                       EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256_GCM);

    /**
     * @brief Decrypt data
     * @param ciphertext Encrypted data (base64 encoded)
     * @return Decrypted plaintext or empty if failed
     */
    std::vector<uint8_t> decryptData(const std::string& ciphertext);

    // ===== HMAC & Integrity =====
    /**
     * @brief Generate HMAC-SHA256 for data integrity
     * @param data Data to hash
     * @return HMAC-SHA256 (hex encoded)
     */
    std::string generateHMAC(const std::vector<uint8_t>& data);

    /**
     * @brief Verify HMAC
     * @param data Original data
     * @param hmac HMAC to verify (hex encoded)
     * @return true if HMAC is valid
     */
    bool verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac);

    // ===== Key Management =====
    /**
     * @brief Generate and store new encryption key
     * @param keyId Unique identifier for the key
     * @param algorithm Algorithm for key derivation
     * @return true if successful
     */
    bool generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm);

    /**
     * @brief Rotate current encryption key
     * @return true if successful; emits keyRotationCompleted signal
     */
    bool rotateEncryptionKey();

    /**
     * @brief Get key expiration date
     * @return Unix timestamp when key expires
     */
    int64_t getKeyExpirationTime() const;

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
    bool storeCredential(const std::string& username, const std::string& token,
                        const std::string& tokenType = "bearer", int64_t expiresAt = 0,
                        const std::string& refreshToken = "");

    /**
     * @brief Retrieve credential
     * @param username User identifier
     * @return Credential info or empty if not found/expired
     */
    CredentialInfo getCredential(const std::string& username) const;

    /**
     * @brief Remove credential
     * @param username User identifier
     * @return true if successful
     */
    bool removeCredential(const std::string& username);

    /**
     * @brief Check if token is expired
     * @param username User identifier
     * @return true if token has expired
     */
    bool isTokenExpired(const std::string& username) const;

    /**
     * @brief Refresh OAuth2 token
     * @param username User identifier
     * @param refreshToken Refresh token from original auth
     * @return New token or empty if refresh failed
     */
    std::string refreshToken(const std::string& username, const std::string& refreshToken = "");

    // ===== Access Control =====
    /**
     * @brief Set access level for resource
     * @param username User identifier
     * @param resource Resource path (e.g., "models/training/advanced")
     * @param level Access level (Read, Write, Execute, Admin)
     * @return true if successful
     */
    bool setAccessControl(const std::string& username, const std::string& resource,
                         AccessLevel level);

    /**
     * @brief Check if user has access to resource
     * @param username User identifier
     * @param resource Resource path
     * @param requiredLevel Required access level
     * @return true if user has sufficient access
     */
    bool checkAccess(const std::string& username, const std::string& resource,
                    AccessLevel requiredLevel) const;

    /**
     * @brief Get all users with access to resource
     * @param resource Resource path
     * @return List of (username, accessLevel) pairs
     */
    std::vector<std::pair<std::string, AccessLevel>> getResourceACL(const std::string& resource) const;

    // ===== Certificate Pinning =====
    /**
     * @brief Pin certificate for domain
     * @param domain Domain name (e.g., "api.example.com")
     * @param certificatePEM Certificate in PEM format
     * @return true if successful
     */
    bool pinCertificate(const std::string& domain, const std::string& certificatePEM);

    /**
     * @brief Verify certificate against pinned certificate
     * @param domain Domain name
     * @param certificatePEM Certificate to verify
     * @return true if matches pinned certificate
     */
    bool verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const;

    // ===== Audit Logging =====
    /**
     * @brief Log security event
     * @param eventType Type of event
     * @param actor User or service performing action
     * @param resource Resource involved
     * @param success Whether action succeeded
     * @param details Additional details
     */
    void logSecurityEvent(const std::string& eventType, const std::string& actor,
                         const std::string& resource, bool success, const std::string& details = "");

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
    bool exportAuditLog(const std::string& filePath) const;

    // ===== Configuration =====
    /**
     * @brief Load security configuration from JSON
     * @param config Configuration object
     * @return true if successful
     */
    bool loadConfiguration(const void*& config);

    /**
     * @brief Get current security configuration
     * @return Configuration as JSON
     */
    void* getConfiguration() const;

    /**
     * @brief Validate security setup
     * @return true if all security components are properly initialized
     */
    bool validateSetup() const;


    void keyRotationCompleted(const std::string& newKeyId);
    void credentialStored(const std::string& username);
    void credentialExpired(const std::string& username);
    void tokenRefreshFailed(const std::string& username);
    void accessDenied(const std::string& username, const std::string& resource);
    void securityEventLogged(const SecurityAuditEntry& entry);

private:
    // Private constructor for singleton
    explicit SecurityManager(void* parent = nullptr);
    
    // ===== Encryption Implementation =====
    std::vector<uint8_t> deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations);
    std::vector<uint8_t> encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256GCM(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);

    // Static singleton instance
    static std::unique_ptr<SecurityManager> s_instance;

    // ===== Internal State =====
    std::vector<uint8_t> m_masterKey;                           // Primary encryption key
    std::string m_currentKeyId;                           // ID of active key
    int64_t m_keyRotationInterval;                     // Seconds between key rotations
    int64_t m_lastKeyRotation;                         // Last rotation timestamp
    
    // Credentials storage (encrypted)
    std::map<std::string, CredentialInfo> m_credentials;  // username -> CredentialInfo
    
    // Access control (ACL)
    std::map<std::string, std::map<std::string, AccessLevel>> m_acl; // username -> (resource -> level)
    
    // Certificate pinning
    std::map<std::string, std::string> m_pinnedCertificates;  // domain -> certificate hash
    
    // Audit trail
    std::vector<SecurityAuditEntry> m_auditLog;       // Security events (max 10000 entries)
    
    bool m_initialized;
    bool m_debugMode;                                 // Log detailed security events
};


