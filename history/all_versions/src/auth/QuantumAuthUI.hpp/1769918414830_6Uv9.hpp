/**
 * @file QuantumAuthUI.hpp
 * @brief User-Facing Quantum Authentication Key Generation UI
 * 
 * Provides a wizard-style interface for users to generate, manage,
 * and enroll quantum-resistant cryptographic keys for system authentication.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <optional>

// Forward declarations


namespace rawrxd::auth {

// ═══════════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Supported key algorithms
 */
enum class KeyAlgorithm {
    RDRAND_AES256,              // RDRAND + AES-256 (fast, hardware-accelerated)
    RDRAND_ChaCha20,            // RDRAND + ChaCha20 (fast, side-channel resistant)
    RDSEED_AES256,              // RDSEED + AES-256 (higher entropy, slower)
    Hybrid_Quantum_Classical,   // Combined quantum-resistant approach
    Custom                      // User-defined parameters
};

/**
 * @brief Key purpose/usage flags
 */
enum class KeyPurpose {
    SystemAuthentication = 0x01,
    ThermalDataSigning = 0x02,
    ConfigEncryption = 0x04,
    IPC_Authentication = 0x08,
    DriveBinding = 0x10,
    All = 0xFF
};

Q_DECLARE_FLAGS(KeyPurposes, KeyPurpose)

/**
 * @brief Key strength level
 */
enum class KeyStrength {
    Standard,       // 128-bit effective security
    High,           // 192-bit effective security  
    Maximum,        // 256-bit effective security
    Paranoid        // 512-bit with additional hardening
};

/**
 * @brief Generated key metadata
 */
struct KeyMetadata {
    std::string keyId;
    std::string keyName;
    KeyAlgorithm algorithm;
    KeyStrength strength;
    KeyPurposes purposes;
    
    // DateTime created;
    // DateTime expires;
    // DateTime lastUsed;
    
    std::string hardwareFingerprint;
    std::string systemFingerprint;
    bool isBoundToHardware;
    
    int usageCount;
    int maxUsages;  // 0 = unlimited
    
    bool isRevoked;
    std::string revocationReason;
    // DateTime revocationDate;
    
    std::anyMap customMetadata;
    
    void* toJson() const;
    static KeyMetadata fromJson(const void*& obj);
};

/**
 * @brief Key generation result
 */
struct KeyGenerationResult {
    bool success;
    std::string errorMessage;
    
    KeyMetadata metadata;
    std::vector<uint8_t> publicKey;       // For sharing/verification
    std::vector<uint8_t> privateKeyHash;  // Hash only, never store actual private key
    
    // Entropy quality metrics
    double entropyBitsPerByte;
    int rdrandCycles;
    int rdseedCycles;
    bool hardwareEntropyVerified;
    
    // Timing
    int64_t generationTimeMs;
};

/**
 * @brief Key enrollment status
 */
struct EnrollmentStatus {
    bool isEnrolled;
    std::string keyId;
    std::string deviceId;
    // DateTime enrollmentDate;
    std::string enrollmentServer;
    
    bool isPendingSync;
    // DateTime lastSyncAttempt;
    std::string lastSyncError;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

using KeyGeneratedCallback = std::function<void(const KeyGenerationResult& result)>;
using EnrollmentCallback = std::function<void(const EnrollmentStatus& status)>;

// ═══════════════════════════════════════════════════════════════════════════════
// Quantum Auth Manager
// ═══════════════════════════════════════════════════════════════════════════════

class QuantumAuthManager {
public:
    QuantumAuthManager();
    ~QuantumAuthManager();

    // Key Generation Pipeline
    KeyGenerationResult generateKey(const std::string& name, KeyAlgorithm algo, KeyStrength strength);
    
    // Management
    bool revokeKey(const std::string& keyId, const std::string& reason);
    std::vector<KeyMetadata> listKeys() const;
    std::optional<KeyMetadata> getKey(const std::string& keyId) const;

    // Simulation of entropy collection (real implementation to come)
    double measureEntropy();

private:
    std::string m_keystorePath;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Storage Backend
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyStorage
 * @brief Secure key storage and retrieval
 */
class KeyStorage 
{public:
    static KeyStorage& instance();
    
    // Storage operations
    bool storeKey(const KeyMetadata& metadata, const std::vector<uint8_t>& encryptedKey);
    std::optional<KeyMetadata> getKeyMetadata(const std::string& keyId);
    std::vector<uint8_t> getEncryptedKey(const std::string& keyId);
    bool deleteKey(const std::string& keyId);
    
    // Queries
    std::vector<KeyMetadata> getAllKeys();
    std::vector<KeyMetadata> getKeysByPurpose(KeyPurpose purpose);
    std::optional<KeyMetadata> getActiveKeyForPurpose(KeyPurpose purpose);
    
    // Lifecycle
    bool revokeKey(const std::string& keyId, const std::string& reason);
    bool renewKey(const std::string& keyId, const // DateTime& newExpiration);
    
    // Verification
    bool verifyKeyIntegrity(const std::string& keyId);
    bool verifyHardwareBinding(const std::string& keyId);

\npublic:\n    void keyStored(const std::string& keyId);
    void keyDeleted(const std::string& keyId);
    void keyRevoked(const std::string& keyId);

private:
    KeyStorage();
    ~KeyStorage() override;
    
    void loadFromDisk();
    void saveToDisk();
    std::string getStoragePath() const;
    std::vector<uint8_t> encryptMetadata(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decryptMetadata(const std::vector<uint8_t>& data);
    
    std::map<std::string, KeyMetadata> m_keys;
    std::map<std::string, std::vector<uint8_t>> m_encryptedKeys;
    std::string m_storagePath;
    std::vector<uint8_t> m_masterKey;
};

} // namespace rawrxd::auth

Q_DECLARE_OPERATORS_FOR_FLAGS(rawrxd::auth::KeyPurposes)

