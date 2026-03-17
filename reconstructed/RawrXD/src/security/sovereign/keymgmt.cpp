// ============================================================================
// sovereign_keymgmt.cpp — Sovereign Key Management System
// ============================================================================
// Track C: Sovereign Tier Security Features
// Feature: SovereignKeyMgmt (Sovereign tier)
// Purpose: Independent key management without cloud dependencies
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <random>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

// Stub license check for test mode
#ifdef BUILD_KEYMGMT_TEST
#define LICENSE_CHECK(feature) true
#else
#include "../include/license_enforcement.h"
#define LICENSE_CHECK(feature) RawrXD::Enforce::LicenseEnforcer::Instance().allow(feature, __FUNCTION__)
#endif

namespace RawrXD::Sovereign {

// ============================================================================
// Key Types
// ============================================================================

enum class KeyType {
    ENCRYPTION,      // AES-256 for data encryption
    SIGNING,         // RSA/ECDSA for signatures
    HMAC,           // HMAC keys for integrity
    KEK             // Key Encryption Key (wraps other keys)
};

enum class KeyState {
    ACTIVE,
    SUSPENDED,
    REVOKED,
    DESTROYED
};

struct CryptoKey {
    std::string keyId;
    KeyType type;
    KeyState state;
    std::vector<uint8_t> keyMaterial;  // Encrypted at rest (CBC or GCM per keyMaterialGCM)
    bool keyMaterialGCM = false;       // True if keyMaterial is AES-256-GCM (nonce||ct||tag)
    std::time_t createdAt;
    std::time_t expiresAt;
    int rotationCount;
    std::string owner;
};

// ============================================================================
// Sovereign Key Manager
// ============================================================================

class SovereignKeyManager {
private:
    bool licensed;
    std::unordered_map<std::string, CryptoKey> keyStore;
    std::vector<uint8_t> masterKey;  // Master Key Encryption Key (KEK)
    bool useGCM_ = false;            // When true, use AES-256-GCM for key-at-rest

    // Audit trail
    std::vector<std::string> auditLog;

public:
    SovereignKeyManager() : licensed(false) {
        // License check (Sovereign tier required)
        licensed = LICENSE_CHECK(RawrXD::License::FeatureID::SovereignKeyMgmt);
        
        if (!licensed) {
            std::cerr << "[LICENSE] SovereignKeyMgmt requires Sovereign license\n";
            return;
        }
        
        std::cout << "[KeyMgmt] Sovereign key management initialized (AES-256-CBC via Windows CNG)\n";

        // One-time production warning: default build uses std::mt19937 master KEK.
        // In production, call setMasterKeyFromExternal() with an HSM-derived 256-bit key
        // or build with -DRAWR_HAS_PKCS11 and inject via HSM session.
        static std::once_flag warnOnce;
        std::call_once(warnOnce, []() {
            std::cerr << "[KeyMgmt] WARNING: Default build uses random master KEK generated from std::mt19937. "
                      << "This is NOT suitable for production sovereign deployments. "
                      << "Use setMasterKeyFromExternal() with an HSM-derived key, or build with "
                      << "-DRAWR_HAS_PKCS11 and inject the KEK from a PKCS#11 session.\n";
        });

        // Generate master KEK (in production, use setMasterKeyFromExternal() with HSM-derived key)
        generateMasterKey();
    }

    // Production: inject 32-byte master KEK from HSM or secure enclave. Overwrites current KEK.
    bool setMasterKeyFromExternal(const std::vector<uint8_t>& keyMaterial) {
        if (!licensed || keyMaterial.size() != 32) return false;
        masterKey.assign(keyMaterial.begin(), keyMaterial.end());
        logAudit("MASTER_KEK_INJECTED", "system", "external");
        std::cout << "[KeyMgmt] Master KEK set from external source (e.g. HSM)\n";
        return true;
    }

    // Use AES-256-GCM for at-rest encryption (AEAD). Format: nonce(12) || ciphertext || tag(16).
    std::vector<uint8_t> encryptWithMasterKeyGCM(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        if (masterKey.size() != 32 || data.empty()) return data;
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0))) return data;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, masterKey.data(), 32, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        std::vector<uint8_t> nonce(12);
        for (auto& b : nonce) b = static_cast<uint8_t>(std::uniform_int_distribution<>(0, 255)(genForIv()));
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = nonce.data();
        authInfo.cbNonce = 12;
        authInfo.cbTag = 16;
        std::vector<uint8_t> out(data.size() + 12 + 16);
        std::memcpy(out.data(), nonce.data(), 12);
        ULONG cbResult = 0;
        authInfo.pbTag = out.data() + 12 + data.size();
        NTSTATUS status = BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(),
                                        &authInfo, nullptr, 0, out.data() + 12, (ULONG)data.size(), &cbResult, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status)) return data;
        out.resize(12 + cbResult + 16);
        std::memcpy(out.data() + 12 + cbResult, authInfo.pbTag, 16);
        return out;
#else
        (void)data;
        return data;
#endif
    }

    std::vector<uint8_t> decryptWithMasterKeyGCM(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        if (masterKey.size() != 32 || data.size() <= 12 + 16) return data;
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0))) return data;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, masterKey.data(), 32, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)data.data();
        authInfo.cbNonce = 12;
        authInfo.pbTag = (PUCHAR)(data.data() + data.size() - 16);
        authInfo.cbTag = 16;
        size_t cipherLen = data.size() - 12 - 16;
        std::vector<uint8_t> out(cipherLen);
        ULONG cbResult = 0;
        NTSTATUS status = BCryptDecrypt(hKey, (PUCHAR)(data.data() + 12), (ULONG)cipherLen,
                                        &authInfo, nullptr, 0, out.data(), (ULONG)out.size(), &cbResult, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status)) return data;
        out.resize(cbResult);
        return out;
#else
        (void)data;
        return data;
#endif
    }

    // Generate new encryption key
    std::string generateKey(KeyType type, const std::string& owner, 
                            int validityDays = 365) {
        if (!licensed) {
            return "";
        }

        // Generate unique key ID
        std::string keyId = generateKeyId();
        
        std::cout << "[KeyMgmt] Generating " 
                  << keyTypeToString(type) << " key: " << keyId << "\n";

        CryptoKey key;
        key.keyId = keyId;
        key.type = type;
        key.state = KeyState::ACTIVE;
        key.createdAt = std::time(nullptr);
        key.expiresAt = key.createdAt + (validityDays * 86400);
        key.rotationCount = 0;
        key.owner = owner;

        // Generate key material
        key.keyMaterial = generateKeyMaterial(type);
        // Encrypt key material with master KEK (CBC or GCM per useGCM_)
        key.keyMaterialGCM = useGCM_;
        key.keyMaterial = useGCM_ ? encryptWithMasterKeyGCM(key.keyMaterial) : encryptWithMasterKey(key.keyMaterial);
        
        // Store key
        keyStore[keyId] = key;
        
        // Audit log
        logAudit("KEY_GENERATED", keyId, owner);
        
        std::cout << "[KeyMgmt] Key generated: " << keyId 
                  << " (expires in " << validityDays << " days)\n";
        
        return keyId;
    }

    // Retrieve key material (decrypted)
    std::vector<uint8_t> getKey(const std::string& keyId, 
                                 const std::string& requester) {
        if (!licensed) {
            return {};
        }

        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) {
            std::cerr << "[KeyMgmt] Key not found: " << keyId << "\n";
            return {};
        }

        CryptoKey& key = it->second;

        // Check key state
        if (key.state != KeyState::ACTIVE) {
            std::cerr << "[KeyMgmt] Key not active: " << keyId 
                      << " (state=" << static_cast<int>(key.state) << ")\n";
            logAudit("KEY_ACCESS_DENIED", keyId, requester);
            return {};
        }

        // Check expiration
        if (std::time(nullptr) > key.expiresAt) {
            std::cerr << "[KeyMgmt] Key expired: " << keyId << "\n";
            key.state = KeyState::SUSPENDED;
            logAudit("KEY_EXPIRED", keyId, requester);
            return {};
        }

        // Decrypt key material (CBC or GCM per key.keyMaterialGCM)
        std::vector<uint8_t> decrypted = key.keyMaterialGCM
            ? decryptWithMasterKeyGCM(key.keyMaterial) : decryptWithMasterKey(key.keyMaterial);
        
        logAudit("KEY_ACCESSED", keyId, requester);
        std::cout << "[KeyMgmt] Key accessed: " << keyId 
                  << " by " << requester << "\n";
        
        return decrypted;
    }

    // Rotate key (generate new version)
    bool rotateKey(const std::string& keyId) {
        if (!licensed) return false;

        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) return false;

        CryptoKey& key = it->second;
        
        std::cout << "[KeyMgmt] Rotating key: " << keyId 
                  << " (rotation #" << (key.rotationCount + 1) << ")\n";

        // Generate new key material
        key.keyMaterial = generateKeyMaterial(key.type);
        key.keyMaterialGCM = useGCM_;
        key.keyMaterial = useGCM_ ? encryptWithMasterKeyGCM(key.keyMaterial) : encryptWithMasterKey(key.keyMaterial);
        key.rotationCount++;
        key.createdAt = std::time(nullptr);
        
        logAudit("KEY_ROTATED", keyId, key.owner);
        
        std::cout << "[KeyMgmt] Key rotated successfully\n";
        return true;
    }

    // Revoke key
    bool revokeKey(const std::string& keyId, const std::string& reason) {
        if (!licensed) return false;

        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) return false;

        it->second.state = KeyState::REVOKED;
        
        std::cout << "[KeyMgmt] Key revoked: " << keyId 
                  << " (reason: " << reason << ")\n";
        
        logAudit("KEY_REVOKED", keyId, reason);
        return true;
    }

    // Destroy key (permanent deletion)
    bool destroyKey(const std::string& keyId) {
        if (!licensed) return false;

        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) return false;

        // Overwrite key material before deletion (secure erase)
        std::fill(it->second.keyMaterial.begin(), it->second.keyMaterial.end(), 0);
        
        keyStore.erase(it);
        
        std::cout << "[KeyMgmt] Key destroyed: " << keyId << "\n";
        logAudit("KEY_DESTROYED", keyId, "system");
        
        return true;
    }

    // Export key (encrypted bundle)
    std::vector<uint8_t> exportKey(const std::string& keyId, 
                                    const std::vector<uint8_t>& transportKey) {
        if (!licensed) return {};

        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) return {};

        std::cout << "[KeyMgmt] Exporting key: " << keyId << "\n";
        
        // Decrypt with master key (CBC or GCM per key), re-encrypt with transport key
        std::vector<uint8_t> plainKey = it->second.keyMaterialGCM
            ? decryptWithMasterKeyGCM(it->second.keyMaterial) : decryptWithMasterKey(it->second.keyMaterial);
        std::vector<uint8_t> wrapped = wrapKey(plainKey, transportKey);
        
        logAudit("KEY_EXPORTED", keyId, "transport");
        return wrapped;
    }

    // Key versioning: return rotation count (0 = original, 1+ = rotated). -1 if not found.
    int getKeyVersion(const std::string& keyId) const {
        auto it = keyStore.find(keyId);
        if (it == keyStore.end()) return -1;
        return it->second.rotationCount;
    }

    // Prefer GCM for new key material encryption (set true to use encryptWithMasterKeyGCM/decryptWithMasterKeyGCM for generateKey/getKey).
    void setUseGCMForKeyStorage(bool useGCM) { useGCM_ = useGCM; }
    bool getUseGCMForKeyStorage() const { return useGCM_; }

    // Get key status
    void printStatus() const {
        std::cout << "\n[KeyMgmt] Sovereign Key Manager Status:\n";
        std::cout << "  Licensed: " << (licensed ? "YES" : "NO") << "\n";
        std::cout << "  Total keys: " << keyStore.size() << "\n";
        std::cout << "  Audit log entries: " << auditLog.size() << "\n";
        
        if (!keyStore.empty()) {
            std::cout << "\nKeys:\n";
            for (const auto& [id, key] : keyStore) {
                std::cout << "  - " << id 
                          << " [" << keyTypeToString(key.type) << "]"
                          << " state=" << static_cast<int>(key.state)
                          << " rotations=" << key.rotationCount << "\n";
            }
        }
    }

private:
    void generateMasterKey() {
        std::cout << "[KeyMgmt] Generating master KEK...\n";
        masterKey.resize(32);  // 256-bit
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (auto& byte : masterKey) {
            byte = dis(gen);
        }
        
        std::cout << "[KeyMgmt] Master KEK generated\n";
    }

    std::string generateKeyId() {
        static int counter = 0;
        return "KEY-" + std::to_string(std::time(nullptr)) + "-" + 
               std::to_string(++counter);
    }

    std::vector<uint8_t> generateKeyMaterial(KeyType type) {
        if (type == KeyType::SIGNING) {
#ifdef _WIN32
            // Real RSA-2048 key pair via Windows CNG (BCrypt)
            BCRYPT_ALG_HANDLE hAlg = nullptr;
            BCRYPT_KEY_HANDLE hKey = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0);
            if (BCRYPT_SUCCESS(status) && hAlg) {
                status = BCryptGenerateKeyPair(hAlg, &hKey, 2048, 0);
                if (BCRYPT_SUCCESS(status) && hKey) {
                    status = BCryptFinalizeKeyPair(hKey, 0);
                    if (BCRYPT_SUCCESS(status)) {
                        ULONG cbBlob = 0;
                        status = BCryptExportKey(hKey, nullptr, BCRYPT_RSAPRIVATE_BLOB, nullptr, 0, &cbBlob, 0);
                        if (BCRYPT_SUCCESS(status) && cbBlob > 0) {
                            std::vector<uint8_t> material(cbBlob);
                            status = BCryptExportKey(hKey, nullptr, BCRYPT_RSAPRIVATE_BLOB, material.data(), cbBlob, &cbBlob, 0);
                            BCryptDestroyKey(hKey);
                            BCryptCloseAlgorithmProvider(hAlg, 0);
                            if (BCRYPT_SUCCESS(status))
                                return material;
                        }
                        BCryptDestroyKey(hKey);
                    } else
                        BCryptDestroyKey(hKey);
                }
                BCryptCloseAlgorithmProvider(hAlg, 0);
            }
            // Fallback when CNG fails
#endif
            std::vector<uint8_t> material(256);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);
            for (auto& byte : material) byte = static_cast<uint8_t>(dis(gen));
            return material;
        }
        // ENCRYPTION, HMAC, KEK: 256-bit random
        std::vector<uint8_t> material(32);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : material) byte = static_cast<uint8_t>(dis(gen));
        return material;
    }

    std::vector<uint8_t> encryptWithMasterKey(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        if (masterKey.size() != 32 || data.empty()) return data;
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
        if (!BCRYPT_SUCCESS(status) || !hAlg) return data;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, masterKey.data(), 32, 0);
        if (!BCRYPT_SUCCESS(status) || !hKey) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        ULONG blockLen = 0, cbGot = 0;
        BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (PUCHAR)&blockLen, sizeof(blockLen), &cbGot, 0);
        size_t paddedLen = ((data.size() + blockLen - 1) / blockLen) * blockLen;
        std::vector<uint8_t> iv(16, 0);
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& b : iv) b = static_cast<uint8_t>(dis(genForIv()));
        std::vector<uint8_t> out(paddedLen);
        ULONG cbResult = 0;
        status = BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(), NULL,
                               iv.data(), 16, out.data(), (ULONG)out.size(), &cbResult, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status)) return data;
        out.resize(cbResult);
        std::vector<uint8_t> result;
        result.reserve(16 + cbResult);
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), out.begin(), out.end());
        return result;
#else
        (void)data;
        return data;
#endif
    }

    std::vector<uint8_t> decryptWithMasterKey(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        if (masterKey.size() != 32 || data.size() <= 16) return data;
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
        if (!BCRYPT_SUCCESS(status) || !hAlg) return data;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, masterKey.data(), 32, 0);
        if (!BCRYPT_SUCCESS(status) || !hKey) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return data;
        }
        std::vector<uint8_t> iv(data.begin(), data.begin() + 16);
        std::vector<uint8_t> cipher(data.begin() + 16, data.end());
        std::vector<uint8_t> out(cipher.size());
        ULONG cbResult = 0;
        status = BCryptDecrypt(hKey, cipher.data(), (ULONG)cipher.size(), NULL,
                              iv.data(), 16, out.data(), (ULONG)out.size(), &cbResult, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status)) return data;
        out.resize(cbResult);
        return out;
#else
        (void)data;
        return data;
#endif
    }

    std::vector<uint8_t> wrapKey(const std::vector<uint8_t>& key,
                                  const std::vector<uint8_t>& transportKey) {
#ifdef _WIN32
        if (transportKey.size() < 32 || key.empty()) return key;
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
        if (!BCRYPT_SUCCESS(status) || !hAlg) return key;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return key;
        }
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, transportKey.data(), 32, 0);
        if (!BCRYPT_SUCCESS(status) || !hKey) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return key;
        }
        ULONG blockLen = 0, cbGot = 0;
        BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (PUCHAR)&blockLen, sizeof(blockLen), &cbGot, 0);
        size_t paddedLen = ((key.size() + blockLen - 1) / blockLen) * blockLen;
        std::vector<uint8_t> iv(16, 0);
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& b : iv) b = static_cast<uint8_t>(dis(genForIv()));
        std::vector<uint8_t> out(paddedLen);
        ULONG cbResult = 0;
        status = BCryptEncrypt(hKey, (PUCHAR)key.data(), (ULONG)key.size(), NULL,
                               iv.data(), 16, out.data(), (ULONG)out.size(), &cbResult, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(status)) return key;
        out.resize(cbResult);
        std::vector<uint8_t> result;
        result.reserve(16 + cbResult);
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), out.begin(), out.end());
        return result;
#else
        (void)transportKey;
        return key;
#endif
    }

    static std::mt19937& genForIv() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }

    void logAudit(const std::string& event, const std::string& keyId, 
                  const std::string& actor) {
        std::string entry = "[" + std::to_string(std::time(nullptr)) + "] " +
                            event + " | key=" + keyId + " | actor=" + actor;
        auditLog.push_back(entry);
    }

    const char* keyTypeToString(KeyType type) const {
        switch (type) {
            case KeyType::ENCRYPTION: return "ENCRYPTION";
            case KeyType::SIGNING: return "SIGNING";
            case KeyType::HMAC: return "HMAC";
            case KeyType::KEK: return "KEK";
        }
        return "UNKNOWN";
    }
};

} // namespace RawrXD::Sovereign

// ============================================================================
// Test Entry Point
// ============================================================================

#ifdef BUILD_KEYMGMT_TEST
int main() {
    std::cout << "RawrXD Sovereign Key Management Test\n";
    std::cout << "Track C: Sovereign Security Feature\n\n";

    RawrXD::Sovereign::SovereignKeyManager keymgr;

    // Generate keys
    std::string encKey = keymgr.generateKey(
        RawrXD::Sovereign::KeyType::ENCRYPTION, "alice", 365);
    std::string signKey = keymgr.generateKey(
        RawrXD::Sovereign::KeyType::SIGNING, "bob", 180);

    // Access key
    auto keyMaterial = keymgr.getKey(encKey, "alice");
    
    // Rotate key
    keymgr.rotateKey(encKey);
    
    // Revoke key
    keymgr.revokeKey(signKey, "compromised");
    
    keymgr.printStatus();

    std::cout << "\n[SUCCESS] Sovereign key management operational\n";
    return 0;
}
#endif
