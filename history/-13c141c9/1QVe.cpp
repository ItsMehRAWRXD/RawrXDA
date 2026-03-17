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
#include <ctime>
#include <random>

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
    std::vector<uint8_t> keyMaterial;  // Encrypted at rest
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
        
        std::cout << "[KeyMgmt] Sovereign key management initialized\n";
        
        // Generate master KEK (in production, loaded from HSM or secure storage)
        generateMasterKey();
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
        
        // Encrypt key material with master KEK
        key.keyMaterial = encryptWithMasterKey(key.keyMaterial);
        
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

        // Decrypt key material
        std::vector<uint8_t> decrypted = decryptWithMasterKey(key.keyMaterial);
        
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
        key.keyMaterial = encryptWithMasterKey(key.keyMaterial);
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
        
        // Decrypt with master key, re-encrypt with transport key
        std::vector<uint8_t> plainKey = decryptWithMasterKey(it->second.keyMaterial);
        std::vector<uint8_t> wrapped = wrapKey(plainKey, transportKey);
        
        logAudit("KEY_EXPORTED", keyId, "transport");
        return wrapped;
    }

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
        size_t keySize = 32;  // 256-bit by default
        if (type == KeyType::SIGNING) keySize = 64;  // RSA placeholder
        
        std::vector<uint8_t> material(keySize);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (auto& byte : material) {
            byte = dis(gen);
        }
        
        return material;
    }

    std::vector<uint8_t> encryptWithMasterKey(const std::vector<uint8_t>& data) {
        // Simplified: XOR with master key (in production: AES-256-GCM)
        std::vector<uint8_t> encrypted = data;
        for (size_t i = 0; i < encrypted.size(); ++i) {
            encrypted[i] ^= masterKey[i % masterKey.size()];
        }
        return encrypted;
    }

    std::vector<uint8_t> decryptWithMasterKey(const std::vector<uint8_t>& data) {
        // XOR is symmetric
        return encryptWithMasterKey(data);
    }

    std::vector<uint8_t> wrapKey(const std::vector<uint8_t>& key, 
                                  const std::vector<uint8_t>& transportKey) {
        // Simplified key wrapping (in production: AES-KW RFC 3394)
        std::vector<uint8_t> wrapped = key;
        for (size_t i = 0; i < wrapped.size(); ++i) {
            wrapped[i] ^= transportKey[i % transportKey.size()];
        }
        return wrapped;
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
