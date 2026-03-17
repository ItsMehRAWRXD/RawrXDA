#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>

// Encryption algorithm constants
enum class EncryptionAlgorithm {
    AES256_GCM,
    AES256_CBC,
    ChaCha20Poly1305
};

// Access control levels (bitflags)
enum class AccessLevel : int {
    NONE = 0,
    READ = 1,
    WRITE = 2,
    EXECUTE = 4,
    ADMIN = 8,
    FULL = 15
};

// Credential information structure
struct CredentialInfo {
    std::string username;
    std::string tokenType;      // "Bearer", "Basic", etc.
    std::string token;          // Encrypted
    int64_t issuedAt;
    int64_t expiresAt;
    bool isRefreshable;
    std::string refreshToken;   // Encrypted
};

// Audit log entry
struct SecurityAuditEntry {
    int64_t timestamp;      // milliseconds since epoch
    std::string eventType;  // "LOGIN", "FILE_ACCESS", "KEY_ROTATION", etc.
    std::string actor;      // Username or system component
    std::string resource;   // Path, API, resource identifier
    bool success;
    std::string details;
};

class SecurityManager {
public:
    // Singleton access
    static SecurityManager& getInstance();
    
    // Initialization
    bool initialize(const std::string& masterPassword = "");
    bool validateSetup() const;
    
    // Encryption / Decryption
    std::string encryptData(const std::vector<uint8_t>& plaintext, EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256_GCM);
    std::vector<uint8_t> decryptData(const std::string& ciphertext);
    
    // Specialized encryption methods
    std::vector<uint8_t> encryptAES256GCM(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256GCM(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key);
    std::vector<uint8_t> encryptAES256CBC(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key);
    std::vector<uint8_t> decryptAES256CBC(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key);
    
    // HMAC and integrity
    std::string generateHMAC(const std::vector<uint8_t>& data);
    bool verifyHMAC(const std::vector<uint8_t>& data, const std::string& hmac);
    
    // Key management
    std::vector<uint8_t> deriveKeyPBKDF2(const std::string& password, const std::vector<uint8_t>& salt, int iterations);
    bool generateNewKey(const std::string& keyId, EncryptionAlgorithm algorithm);
    bool rotateEncryptionKey();
    int64_t getKeyExpirationTime() const;
    
    // Credential management
    bool storeCredential(const std::string& username, const std::string& token,
                       const std::string& tokenType = "Bearer", int64_t expiresAt = 0,
                       const std::string& refreshToken = "");
    CredentialInfo getCredential(const std::string& username) const;
    bool removeCredential(const std::string& username);
    bool isTokenExpired(const std::string& username) const;
    std::string refreshToken(const std::string& username, const std::string& refreshToken);
    
    // Access control
    bool setAccessControl(const std::string& username, const std::string& resource, AccessLevel level);
    bool checkAccess(const std::string& username, const std::string& resource, AccessLevel requiredLevel) const;
    std::vector<std::pair<std::string, AccessLevel>> getResourceACL(const std::string& resource) const;
    
    // Certificate pinning
    bool pinCertificate(const std::string& domain, const std::string& certificatePEM);
    bool verifyCertificatePin(const std::string& domain, const std::string& certificatePEM) const;
    
    // Audit logging
    void logSecurityEvent(const std::string& eventType, const std::string& actor,
                         const std::string& resource, bool success, const std::string& details = "");
    std::vector<SecurityAuditEntry> getAuditLog(int limit = 100) const;
    bool exportAuditLog(const std::string& filePath) const;
    
    // Configuration
    bool loadConfiguration(const std::map<std::string, std::string>& config);
    std::map<std::string, std::string> getConfiguration() const;
    
private:
    SecurityManager();
    ~SecurityManager();
    
    // Singleton instance
    static std::unique_ptr<SecurityManager> s_instance;
    
    // Configuration
    void loadStoredCredentials();
    void loadACLConfiguration();
    
    // State
    bool m_initialized;
    bool m_debugMode;
    std::vector<uint8_t> m_masterKey;
    std::string m_currentKeyId;
    int64_t m_lastKeyRotation;
    int64_t m_keyRotationInterval;      // in seconds
    
    // Credentials storage (encrypted)
    std::map<std::string, CredentialInfo> m_credentials;
    
    // Access control lists
    std::map<std::string, std::map<std::string, AccessLevel>> m_acl;
    
    // Certificate pinning database
    std::map<std::string, std::string> m_pinnedCertificates;
    
    // Audit log
    std::vector<SecurityAuditEntry> m_auditLog;
};
