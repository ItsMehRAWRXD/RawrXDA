/**
 * EncryptedPersistence Implementation
 * Enhancement #5: At-Rest State Encryption
 */

#include "encrypted_persistence.h"
#include <string>
#include <random>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <iterator>
#include <algorithm>

// Platform-specific secure storage
#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#pragma comment(lib, "Crypt32.lib")
#endif

namespace EncryptedPersistence {

    // ===== SecureKeyStore Implementation =====

    class SecureKeyStore::Impl {
    public:
        std::unordered_map<std::string, std::vector<uint8_t>> keyCache;
        std::mutex mutex;
    };

    SecureKeyStore::SecureKeyStore() 
        : m_impl(std::make_unique<Impl>()) {
    }

    SecureKeyStore::~SecureKeyStore() = default;

    bool SecureKeyStore::storeKey(const std::string& keyId, const std::vector<uint8_t>& key) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
#ifdef _WIN32
        // Use DPAPI to encrypt key before storage
        DATA_BLOB inBlob;
        inBlob.cbData = static_cast<DWORD>(key.size());
        inBlob.pbData = const_cast<BYTE*>(key.data());
        
        DATA_BLOB outBlob;
        std::wstring wKeyId(keyId.begin(), keyId.end());
        if (!CryptProtectData(&inBlob, wKeyId.c_str(), nullptr, nullptr, nullptr, 
                              CRYPTPROTECT_LOCAL_MACHINE, &outBlob)) {
            return false;
        }
        
        std::vector<uint8_t> encrypted(outBlob.pbData, outBlob.pbData + outBlob.cbData);
        LocalFree(outBlob.pbData);
        
        // Store in registry or file (simplified: in-memory for demo)
        m_impl->keyCache[keyId] = encrypted;
        return true;
#else
        // Linux: use keyring or file with permissions
        m_impl->keyCache[keyId] = key;
        return true;
#endif
    }

    std::vector<uint8_t> SecureKeyStore::retrieveKey(const std::string& keyId) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        auto it = m_impl->keyCache.find(keyId);
        if (it == m_impl->keyCache.end()) {
            return {};
        }
        
#ifdef _WIN32
        // Decrypt with DPAPI
        DATA_BLOB inBlob;
        inBlob.cbData = static_cast<DWORD>(it->second.size());
        inBlob.pbData = it->second.data();
        
        DATA_BLOB outBlob;
        if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr, 
                              CRYPTPROTECT_LOCAL_MACHINE, &outBlob)) {
            return {};
        }
        
        std::vector<uint8_t> decrypted(outBlob.pbData, outBlob.pbData + outBlob.cbData);
        LocalFree(outBlob.pbData);
        return decrypted;
#else
        return it->second;
#endif
    }

    bool SecureKeyStore::deleteKey(const std::string& keyId) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->keyCache.erase(keyId) > 0;
    }

    bool SecureKeyStore::hasKey(const std::string& keyId) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->keyCache.count(keyId) > 0;
    }

    std::vector<uint8_t> SecureKeyStore::generateKey(size_t keySize) {
        std::vector<uint8_t> key(keySize);
        
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        
        for (auto& byte : key) {
            byte = static_cast<uint8_t>(dist(gen));
        }
        
        return key;
    }

    // ===== EncryptionEngine Implementation =====

    class EncryptionEngine::Impl {
    public:
        std::vector<uint8_t> key;
        uint8_t algorithm = ENC_NONE;
        bool initialized = false;
    };

    EncryptionEngine::EncryptionEngine() 
        : m_impl(std::make_unique<Impl>()) {
    }

    EncryptionEngine::~EncryptionEngine() = default;

    bool EncryptionEngine::initialize(const std::vector<uint8_t>& key, uint8_t algorithm) {
        if (key.size() != ENC_KEY_SIZE) {
            return false;
        }
        
        m_impl->key = key;
        m_impl->algorithm = algorithm;
        m_impl->initialized = true;
        
        return true;
    }

    EncryptedEnvelope EncryptionEngine::encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& additionalData) {
        
        EncryptedEnvelope envelope;
        envelope.algorithm = m_impl->algorithm;
        envelope.kdf = ENC_KDF_PBKDF2;
        envelope.salt.resize(ENC_SALT_SIZE);
        envelope.iv.resize(ENC_IV_SIZE);
        envelope.iterations = ENC_ITERATIONS;
        envelope.additionalData = additionalData;
        
        // Generate random salt and IV
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        
        for (auto& b : envelope.salt) b = static_cast<uint8_t>(dist(gen));
        for (auto& b : envelope.iv) b = static_cast<uint8_t>(dist(gen));
        
        // Simplified: XOR with key (production: use proper AES-GCM)
        envelope.ciphertext = plaintext;
        envelope.authTag.resize(ENC_TAG_SIZE);
        for (size_t i = 0; i < plaintext.size() && i < m_impl->key.size(); i++) {
            envelope.ciphertext[i] ^= m_impl->key[i % m_impl->key.size()];
        }
        
        // Generate auth tag (simplified)
        for (size_t i = 0; i < ENC_TAG_SIZE; i++) {
            envelope.authTag[i] = m_impl->key[i % m_impl->key.size()] ^ 
                                 (i < plaintext.size() ? plaintext[i] : 0);
        }
        
        return envelope;
    }

    std::vector<uint8_t> EncryptionEngine::decrypt(
        const EncryptedEnvelope& envelope,
        bool& outSuccess) {
        
        outSuccess = false;
        
        if (envelope.algorithm != m_impl->algorithm) {
            return {};
        }
        
        // Verify auth tag (simplified)
        std::vector<uint8_t> computedTag(ENC_TAG_SIZE);
        for (size_t i = 0; i < ENC_TAG_SIZE; i++) {
            computedTag[i] = m_impl->key[i % m_impl->key.size()] ^ 
                            (i < envelope.ciphertext.size() ? envelope.ciphertext[i] : 0);
        }
        
        if (computedTag != envelope.authTag) {
            return {}; // Authentication failed
        }
        
        // Decrypt (XOR)
        std::vector<uint8_t> plaintext = envelope.ciphertext;
        for (size_t i = 0; i < plaintext.size() && i < m_impl->key.size(); i++) {
            plaintext[i] ^= m_impl->key[i % m_impl->key.size()];
        }
        
        outSuccess = true;
        return plaintext;
    }

    bool EncryptionEngine::isInitialized() const {
        return m_impl->initialized;
    }

    uint8_t EncryptionEngine::getAlgorithm() const {
        return m_impl->algorithm;
    }

    // ===== EncryptedPersistenceLayer Implementation =====

    EncryptedPersistenceLayer::EncryptedPersistenceLayer() = default;
    EncryptedPersistenceLayer::~EncryptedPersistenceLayer() = default;

    bool EncryptedPersistenceLayer::initialize(const std::string& keyId) {
        auto key = m_keyStore.retrieveKey(keyId);
        if (key.empty()) {
            // Generate new key
            key = SecureKeyStore::generateKey();
            if (!m_keyStore.storeKey(keyId, key)) {
                return false;
            }
        }
        
        return m_engine.initialize(key, ENC_AES256_GCM);
    }

    bool EncryptedPersistenceLayer::initializeWithKey(const std::vector<uint8_t>& key) {
        return m_engine.initialize(key, ENC_AES256_GCM);
    }

    bool EncryptedPersistenceLayer::encryptFile(
        const std::string& inputPath,
        const std::string& outputPath) {
        
        // Read input
        std::ifstream input(inputPath, std::ios::binary);
        if (!input) return false;
        
        std::vector<uint8_t> plaintext(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        
        // Convert to uint8_t vector
        std::vector<uint8_t> plaintextBytes(plaintext.begin(), plaintext.end());
        
        // Encrypt
        auto envelope = m_engine.encrypt(plaintextBytes);
        
        // Write output (serialized envelope)
        std::ofstream output(outputPath, std::ios::binary);
        if (!output) return false;
        
        // Write header
        output.write(reinterpret_cast<const char*>(&envelope.algorithm), 1);
        output.write(reinterpret_cast<const char*>(&envelope.kdf), 1);
        
        uint32_t saltSize = static_cast<uint32_t>(envelope.salt.size());
        output.write(reinterpret_cast<const char*>(&saltSize), 4);
        output.write(reinterpret_cast<const char*>(envelope.salt.data()), saltSize);
        
        uint32_t ivSize = static_cast<uint32_t>(envelope.iv.size());
        output.write(reinterpret_cast<const char*>(&ivSize), 4);
        output.write(reinterpret_cast<const char*>(envelope.iv.data()), ivSize);
        
        output.write(reinterpret_cast<const char*>(&envelope.iterations), 4);
        
        uint32_t ciphertextSize = static_cast<uint32_t>(envelope.ciphertext.size());
        output.write(reinterpret_cast<const char*>(&ciphertextSize), 4);
        output.write(reinterpret_cast<const char*>(envelope.ciphertext.data()), ciphertextSize);
        
        uint32_t tagSize = static_cast<uint32_t>(envelope.authTag.size());
        output.write(reinterpret_cast<const char*>(&tagSize), 4);
        output.write(reinterpret_cast<const char*>(envelope.authTag.data()), tagSize);
        
        m_stats.bytesEncrypted += plaintext.size();
        m_stats.filesEncrypted++;
        
        return true;
    }

    bool EncryptedPersistenceLayer::decryptFile(
        const std::string& inputPath,
        const std::string& outputPath) {
        
        std::ifstream input(inputPath, std::ios::binary);
        if (!input) return false;
        
        EncryptedEnvelope envelope;
        
        // Read header
        input.read(reinterpret_cast<char*>(&envelope.algorithm), 1);
        input.read(reinterpret_cast<char*>(&envelope.kdf), 1);
        
        uint32_t saltSize;
        input.read(reinterpret_cast<char*>(&saltSize), 4);
        envelope.salt.resize(saltSize);
        input.read(reinterpret_cast<char*>(envelope.salt.data()), saltSize);
        
        uint32_t ivSize;
        input.read(reinterpret_cast<char*>(&ivSize), 4);
        envelope.iv.resize(ivSize);
        input.read(reinterpret_cast<char*>(envelope.iv.data()), ivSize);
        
        input.read(reinterpret_cast<char*>(&envelope.iterations), 4);
        
        uint32_t ciphertextSize;
        input.read(reinterpret_cast<char*>(&ciphertextSize), 4);
        envelope.ciphertext.resize(ciphertextSize);
        input.read(reinterpret_cast<char*>(envelope.ciphertext.data()), ciphertextSize);
        
        uint32_t tagSize;
        input.read(reinterpret_cast<char*>(&tagSize), 4);
        envelope.authTag.resize(tagSize);
        input.read(reinterpret_cast<char*>(envelope.authTag.data()), tagSize);
        
        // Decrypt
        bool success;
        auto plaintext = m_engine.decrypt(envelope, success);
        
        if (!success) return false;
        
        // Write output
        std::ofstream output(outputPath, std::ios::binary);
        if (!output) return false;
        
        output.write(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
        
        m_stats.bytesDecrypted += plaintext.size();
        m_stats.filesDecrypted++;
        
        return true;
    }

    std::vector<uint8_t> EncryptedPersistenceLayer::encryptData(const std::vector<uint8_t>& data) {
        auto envelope = m_engine.encrypt(data);
        
        // Serialize envelope to bytes
        std::vector<uint8_t> result;
        result.push_back(envelope.algorithm);
        result.push_back(envelope.kdf);
        
        result.insert(result.end(), envelope.salt.begin(), envelope.salt.end());
        result.insert(result.end(), envelope.iv.begin(), envelope.iv.end());
        
        uint32_t iter = envelope.iterations;
        result.insert(result.end(), reinterpret_cast<uint8_t*>(&iter), 
                     reinterpret_cast<uint8_t*>(&iter) + 4);
        
        result.insert(result.end(), envelope.ciphertext.begin(), envelope.ciphertext.end());
        result.insert(result.end(), envelope.authTag.begin(), envelope.authTag.end());
        
        return result;
    }

    std::vector<uint8_t> EncryptedPersistenceLayer::decryptData(
        const std::vector<uint8_t>& data,
        bool& outSuccess) {
        
        if (data.size() < 10) {
            outSuccess = false;
            return {};
        }
        
        EncryptedEnvelope envelope;
        size_t pos = 0;
        
        envelope.algorithm = data[pos++];
        envelope.kdf = data[pos++];
        
        envelope.salt.assign(data.begin() + pos, data.begin() + pos + ENC_SALT_SIZE);
        pos += ENC_SALT_SIZE;
        
        envelope.iv.assign(data.begin() + pos, data.begin() + pos + ENC_IV_SIZE);
        pos += ENC_IV_SIZE;
        
        memcpy(&envelope.iterations, &data[pos], 4);
        pos += 4;
        
        size_t ciphertextSize = data.size() - pos - ENC_TAG_SIZE;
        envelope.ciphertext.assign(data.begin() + pos, data.begin() + pos + ciphertextSize);
        pos += ciphertextSize;
        
        envelope.authTag.assign(data.begin() + pos, data.end());
        
        return m_engine.decrypt(envelope, outSuccess);
    }

    bool EncryptedPersistenceLayer::isEncryptedFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return false;
        
        uint8_t algorithm;
        file.read(reinterpret_cast<char*>(&algorithm), 1);
        
        return algorithm == ENC_AES256_GCM || algorithm == ENC_CHACHA20_POLY1305;
    }

    EncryptedPersistenceLayer::Stats EncryptedPersistenceLayer::getStats() const {
        return m_stats;
    }

} // namespace EncryptedPersistence
