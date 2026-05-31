// ============================================================================
// CryptoHelpers.h — Windows BCrypt wrappers for X25519 and AES-256-GCM
// ============================================================================
#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <memory>

namespace RawrXD {
namespace Crypto {

struct KeyPair {
    std::vector<uint8_t> publicKey;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    ~KeyPair() { if (hKey) BCryptDestroyKey(hKey); }
};

class CryptoHelpers {
public:
    static bool Initialize();
    
    // ECC Key Exchange (X25519)
    static std::unique_ptr<KeyPair> GenerateX25519KeyPair();
    static std::vector<uint8_t> DeriveSharedSecret(BCRYPT_KEY_HANDLE hPrivKey, const std::vector<uint8_t>& peerPubKey);

    // Authenticated Encryption (AES-GCM)
    static std::vector<uint8_t> EncryptAES_GCM(const std::vector<uint8_t>& plaintext, 
                                              const std::vector<uint8_t>& key, 
                                              const std::vector<uint8_t>& iv,
                                              std::vector<uint8_t>& outTag);
                                              
    static std::vector<uint8_t> DecryptAES_GCM(const std::vector<uint8_t>& ciphertext, 
                                              const std::vector<uint8_t>& key, 
                                              const std::vector<uint8_t>& iv,
                                              const std::vector<uint8_t>& tag);

    static std::vector<uint8_t> GenerateRandomBytes(size_t length);
    static std::vector<uint8_t> HashSHA256(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> DeriveHardwareKey(const std::string& salt);

private:
    static BCRYPT_ALG_HANDLE hAlgX25519;
    static BCRYPT_ALG_HANDLE hAlgAES_GCM;
    static BCRYPT_ALG_HANDLE hAlgSHA256;
    static BCRYPT_ALG_HANDLE hAlgRNG;
};

} // namespace Crypto
} // namespace RawrXD
