#pragma once
/**
 * EncryptedPersistence - Enhancement #5: At-Rest State Encryption
 * 
 * Provides transparent encryption for sensitive workflow state.
 * Uses AES-256-GCM for authenticated encryption.
 * 
 * Symbols: ENC_AES256_GCM, ENC_CHACHA20_POLY1305, ENC_NONE
 */

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// Encryption algorithms
#define ENC_NONE                0x00
#define ENC_AES256_GCM          0x01
#define ENC_CHACHA20_POLY1305   0x02

// Key derivation
#define ENC_KDF_PBKDF2          0x01
#define ENC_KDF_ARGON2          0x02

// Default parameters
#define ENC_DEFAULT_ALGORITHM   ENC_AES256_GCM
#define ENC_DEFAULT_KDF         ENC_KDF_PBKDF2
#define ENC_KEY_SIZE            32   // 256 bits
#define ENC_IV_SIZE             12   // 96 bits for GCM
#define ENC_TAG_SIZE            16   // 128 bits auth tag
#define ENC_SALT_SIZE           32
#define ENC_ITERATIONS          100000

namespace EncryptedPersistence {

    /**
     * Secure key storage (platform-specific)
     */
    class SecureKeyStore {
    public:
        SecureKeyStore();
        ~SecureKeyStore();

        // Store key in platform secure storage (DPAPI/Keychain)
        bool storeKey(const std::string& keyId, const std::vector<uint8_t>& key);
        
        // Retrieve key
        std::vector<uint8_t> retrieveKey(const std::string& keyId);
        
        // Delete key
        bool deleteKey(const std::string& keyId);
        
        // Check if key exists
        bool hasKey(const std::string& keyId);
        
        // Generate new random key
        static std::vector<uint8_t> generateKey(size_t keySize = ENC_KEY_SIZE);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Encrypted data envelope
     */
    struct EncryptedEnvelope {
        uint8_t algorithm;
        uint8_t kdf;
        std::vector<uint8_t> salt;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> authTag;
        uint32_t iterations;
        std::vector<uint8_t> additionalData;
    };

    /**
     * Encryption/decryption interface
     */
    class EncryptionEngine {
    public:
        EncryptionEngine();
        ~EncryptionEngine();

        // Initialize with key
        bool initialize(const std::vector<uint8_t>& key, uint8_t algorithm = ENC_DEFAULT_ALGORITHM);
        
        // Encrypt data
        EncryptedEnvelope encrypt(
            const std::vector<uint8_t>& plaintext,
            const std::vector<uint8_t>& additionalData = {});
        
        // Decrypt data
        std::vector<uint8_t> decrypt(
            const EncryptedEnvelope& envelope,
            bool& outSuccess);
        
        // Check if initialized
        bool isInitialized() const;
        
        // Get current algorithm
        uint8_t getAlgorithm() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Transparent encryption layer for persistence
     */
    class EncryptedPersistenceLayer {
    public:
        EncryptedPersistenceLayer();
        ~EncryptedPersistenceLayer();

        // Initialize with key from secure store
        bool initialize(const std::string& keyId);
        
        // Initialize with explicit key (for testing)
        bool initializeWithKey(const std::vector<uint8_t>& key);

        // Encrypt file
        bool encryptFile(
            const std::string& inputPath,
            const std::string& outputPath);
        
        // Decrypt file
        bool decryptFile(
            const std::string& inputPath,
            const std::string& outputPath);
        
        // Encrypt/decrypt in memory
        std::vector<uint8_t> encryptData(const std::vector<uint8_t>& data);
        std::vector<uint8_t> decryptData(
            const std::vector<uint8_t>& data,
            bool& outSuccess);

        // Check if file is encrypted
        static bool isEncryptedFile(const std::string& filePath);

        // Get encryption stats
        struct Stats {
            size_t bytesEncrypted = 0;
            size_t bytesDecrypted = 0;
            size_t filesEncrypted = 0;
            size_t filesDecrypted = 0;
            double avgEncryptionTimeMs = 0;
            double avgDecryptionTimeMs = 0;
        };
        Stats getStats() const;

    private:
        EncryptionEngine m_engine;
        SecureKeyStore m_keyStore;
        Stats m_stats;
    };

    /**
     * Key rotation support
     */
    class KeyRotation {
    public:
        // Rotate key for existing encrypted file
        static bool rotateKey(
            const std::string& filePath,
            const std::vector<uint8_t>& oldKey,
            const std::vector<uint8_t>& newKey);
        
        // Batch rotate all files in directory
        static size_t rotateAllKeys(
            const std::string& directory,
            const std::vector<uint8_t>& oldKey,
            const std::vector<uint8_t>& newKey);
    };

} // namespace EncryptedPersistence
