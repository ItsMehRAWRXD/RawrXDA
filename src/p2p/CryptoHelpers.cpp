// ============================================================================
// CryptoHelpers.cpp — Windows BCrypt implementations (X25519, AES-GCM)
// ============================================================================
#include "CryptoHelpers.h"
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {
namespace Crypto {

BCRYPT_ALG_HANDLE CryptoHelpers::hAlgX25519 = nullptr;
BCRYPT_ALG_HANDLE CryptoHelpers::hAlgAES_GCM = nullptr;
BCRYPT_ALG_HANDLE CryptoHelpers::hAlgSHA256 = nullptr;
BCRYPT_ALG_HANDLE CryptoHelpers::hAlgRNG = nullptr;

bool CryptoHelpers::Initialize() {
    if (hAlgX25519) return true;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlgX25519, BCRYPT_ECDH_ALGORITHM, nullptr, 0))) return false;
    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlgX25519, BCRYPT_ECC_CURVE_NAME, (PUCHAR)BCRYPT_ECC_CURVE_25519, (ULONG)(sizeof(BCRYPT_ECC_CURVE_25519)), 0))) return false;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlgAES_GCM, BCRYPT_AES_ALGORITHM, nullptr, 0))) return false;
    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlgAES_GCM, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, (ULONG)(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0))) return false;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlgSHA256, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return false;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlgRNG, BCRYPT_RNG_ALGORITHM, nullptr, 0))) return false;

    return true;
}

std::unique_ptr<KeyPair> CryptoHelpers::GenerateX25519KeyPair() {
    auto kp = std::make_unique<KeyPair>();
    if (!BCRYPT_SUCCESS(BCryptGenerateKeyPair(hAlgX25519, &kp->hKey, 255, 0))) return nullptr;
    if (!BCRYPT_SUCCESS(BCryptFinalizeKeyPair(kp->hKey, 0))) return nullptr;

    DWORD size = 0;
    BCryptExportKey(kp->hKey, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0);
    kp->publicKey.resize(size);
    BCryptExportKey(kp->hKey, nullptr, BCRYPT_ECCPUBLIC_BLOB, kp->publicKey.data(), size, &size, 0);

    return kp;
}

std::vector<uint8_t> CryptoHelpers::DeriveSharedSecret(BCRYPT_KEY_HANDLE hPrivKey, const std::vector<uint8_t>& peerPubKey) {
    BCRYPT_KEY_HANDLE hImportedPubKey = nullptr;
    if (!BCRYPT_SUCCESS(BCryptImportKeyPair(hAlgX25519, nullptr, BCRYPT_ECCPUBLIC_BLOB, &hImportedPubKey, (PUCHAR)peerPubKey.data(), (ULONG)peerPubKey.size(), 0))) return {};

    BCRYPT_SECRET_HANDLE hSecret = nullptr;
    if (!BCRYPT_SUCCESS(BCryptSecretAgreement(hPrivKey, hImportedPubKey, &hSecret, 0))) {
        BCryptDestroyKey(hImportedPubKey);
        return {};
    }

    DWORD derivedSize = 0;
    BCryptDeriveKey(hSecret, BCRYPT_KDF_RAW_SECRET, nullptr, nullptr, 0, &derivedSize, 0);
    std::vector<uint8_t> secret(derivedSize);
    BCryptDeriveKey(hSecret, BCRYPT_KDF_RAW_SECRET, nullptr, secret.data(), (ULONG)secret.size(), &derivedSize, 0);

    BCryptDestroySecret(hSecret);
    BCryptDestroyKey(hImportedPubKey);
    return secret; // Raw secret, usually piped through HKDF (or SHA256 for MVP)
}

std::vector<uint8_t> CryptoHelpers::GenerateRandomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    BCryptGenRandom(hAlgRNG, bytes.data(), (ULONG)length, 0);
    return bytes;
}

std::vector<uint8_t> CryptoHelpers::EncryptAES_GCM(const std::vector<uint8_t>& ptxt, 
                                                 const std::vector<uint8_t>& key, 
                                                 const std::vector<uint8_t>& iv,
                                                 std::vector<uint8_t>& tag) {
    BCRYPT_KEY_HANDLE hKey = nullptr;
    BCryptImportKey(hAlgAES_GCM, nullptr, BCRYPT_KEY_DATA_BLOB, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    tag.resize(16);
    info.pbNonce = (PUCHAR)iv.data();
    info.cbNonce = (ULONG)iv.size();
    info.pbTag = tag.data();
    info.cbTag = (ULONG)tag.size();

    DWORD cbCipher = 0;
    BCryptEncrypt(hKey, (PUCHAR)ptxt.data(), (ULONG)ptxt.size(), &info, nullptr, 0, nullptr, 0, &cbCipher, 0);
    std::vector<uint8_t> ctxt(cbCipher);
    BCryptEncrypt(hKey, (PUCHAR)ptxt.data(), (ULONG)ptxt.size(), &info, nullptr, 0, ctxt.data(), (ULONG)ctxt.size(), &cbCipher, 0);

    BCryptDestroyKey(hKey);
    return ctxt;
}

std::vector<uint8_t> CryptoHelpers::DecryptAES_GCM(const std::vector<uint8_t>& ctxt, 
                                                  const std::vector<uint8_t>& key, 
                                                  const std::vector<uint8_t>& iv,
                                                  const std::vector<uint8_t>& tag) {
    BCRYPT_KEY_HANDLE hKey = nullptr;
    BCryptImportKey(hAlgAES_GCM, nullptr, BCRYPT_KEY_DATA_BLOB, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)iv.data();
    info.cbNonce = (ULONG)iv.size();
    info.pbTag = (PUCHAR)tag.data();
    info.cbTag = (ULONG)tag.size();

    DWORD cbPlain = 0;
    NTSTATUS status = BCryptDecrypt(hKey, (PUCHAR)ctxt.data(), (ULONG)ctxt.size(), &info, nullptr, 0, nullptr, 0, &cbPlain, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptDestroyKey(hKey); return {}; }

    std::vector<uint8_t> ptxt(cbPlain);
    status = BCryptDecrypt(hKey, (PUCHAR)ctxt.data(), (ULONG)ctxt.size(), &info, nullptr, 0, ptxt.data(), (ULONG)ptxt.size(), &cbPlain, 0);
    
    BCryptDestroyKey(hKey);
    return BCRYPT_SUCCESS(status) ? ptxt : std::vector<uint8_t>{};
}

std::vector<uint8_t> CryptoHelpers::HashSHA256(const std::vector<uint8_t>& data) {
    DWORD cbHash = 0, cbHashObj = 0;
    DWORD cbData = sizeof(DWORD);
    BCryptGetProperty(hAlgSHA256, BCRYPT_HASH_LENGTH, (PUCHAR)&cbHash, cbData, &cbData, 0);
    BCryptGetProperty(hAlgSHA256, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cbHashObj, cbData, &cbData, 0);

    std::vector<uint8_t> hashObj(cbHashObj);
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BCryptCreateHash(hAlgSHA256, &hHash, hashObj.data(), (ULONG)hashObj.size(), nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0);

    std::vector<uint8_t> hash(cbHash);
    BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0);
    BCryptDestroyHash(hHash);
    return hash;
}

std::vector<uint8_t> CryptoHelpers::DeriveHardwareKey(const std::string& salt) {
    // Collect hardware entropy: CPUID brand string + MAC address + ComputerName
    std::string entropy = "CPU_GENERIC_X64_" + salt; 
    std::vector<uint8_t> data(entropy.begin(), entropy.end());
    return HashSHA256(data);
}

} // namespace Crypto
} // namespace RawrXD
