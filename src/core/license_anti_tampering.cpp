// ============================================================================
// license_anti_tampering.cpp — Enterprise License Anti-Tampering Implementation
// ============================================================================

#include "../include/license_anti_tampering.h"
#include <cstring>
#include <ctime>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>

// SCAFFOLD_205: License anti-tampering

#pragma comment(lib, "bcrypt.lib")
#endif

namespace RawrXD::License::AntiTampering {

// ============================================================================
// CRC32 Lookup Table
// ============================================================================
static uint32_t g_crc32_table[256];
static bool g_crc32_initialized = false;

static void initialize_crc32_table() {
    if (g_crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (uint32_t j = 8; j > 0; --j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        g_crc32_table[i] = crc;
    }
    g_crc32_initialized = true;
}

// ============================================================================
// CRC32 Implementation
// ============================================================================
uint32_t computeCRC32(const uint8_t* data, size_t size) {
    if (!g_crc32_initialized) initialize_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc = (crc >> 8) ^ g_crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// HMAC-SHA256 Implementation
// ============================================================================

// Simple SHA256 implementation (for license verification)
namespace SHA256 {
    static constexpr uint32_t K[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static constexpr uint32_t INITIAL_HASH[] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    #define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
    #define CH(x, y, z)    (((x) & (y)) ^ (~(x) & (z)))
    #define MAJ(x, y, z)   (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
    #define SIGMA0(x)      (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
    #define SIGMA1(x)      (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
    #define GAMMA0(x)      (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
    #define GAMMA1(x)      (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

    static void sha256_process(uint32_t state[8], const uint8_t block[64]) {
        uint32_t W[64];
        for (int i = 0; i < 16; ++i) {
            W[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
        }
        for (int i = 16; i < 64; ++i) {
            W[i] = GAMMA1(W[i-2]) + W[i-7] + GAMMA0(W[i-15]) + W[i-16];
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t T1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i];
            uint32_t T2 = SIGMA0(a) + MAJ(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    static void sha256(const uint8_t* data, size_t size, uint8_t hash[32]) {
        uint32_t state[8];
        std::memcpy(state, INITIAL_HASH, sizeof(state));

        uint64_t bit_length = size * 8;
        size_t offset = 0;
        uint8_t buffer[64];

        // Process complete 64-byte blocks
        while (offset + 64 <= size) {
            sha256_process(state, data + offset);
            offset += 64;
        }

        // Handle remaining data
        size_t remain = size - offset;
        std::memcpy(buffer, data + offset, remain);
        buffer[remain] = 0x80;
        std::memset(buffer + remain + 1, 0, 63 - remain);

        // If remaining data + length doesn't fit in 64 bytes, process current block first
        if (remain >= 56) {
            sha256_process(state, buffer);
            std::memset(buffer, 0, 56);
        }

        // Append length
        for (int i = 0; i < 8; ++i) {
            buffer[63 - i] = (bit_length >> (i * 8)) & 0xFF;
        }
        sha256_process(state, buffer);

        // Write hash
        for (int i = 0; i < 8; ++i) {
            hash[i*4]     = (state[i] >> 24) & 0xFF;
            hash[i*4 + 1] = (state[i] >> 16) & 0xFF;
            hash[i*4 + 2] = (state[i] >> 8) & 0xFF;
            hash[i*4 + 3] = state[i] & 0xFF;
        }
    }
} // namespace SHA256

// Public SHA256 wrapper for testing
void sha256(const uint8_t* data, size_t size, uint8_t hash[32]) {
    SHA256::sha256(data, size, hash);
}

bool computeHMAC_SHA256(const uint8_t* data, size_t dataSize,
                        const uint8_t* key, size_t keySize,
                        uint8_t outSignature[32]) {
    if (!data || !key || !outSignature) return false;

    uint8_t ipad[64], opad[64];
    std::memset(ipad, 0x36, 64);
    std::memset(opad, 0x5C, 64);

    // XOR key with pads
    size_t key_use = keySize < 64 ? keySize : 64;
    for (size_t i = 0; i < key_use; ++i) {
        ipad[i] ^= key[i];
        opad[i] ^= key[i];
    }

    // Compute inner hash: H(ipad || data). Need 64 + dataSize bytes for input.
    // LicenseKeyV2 signing uses dataSize=64, so inner must be >= 128 bytes.
    const size_t innerLen = 64 + dataSize;
    if (innerLen > 256) return false;  // Sanity: reject huge inputs
    uint8_t inner[64 + 128];  // 192 bytes: supports dataSize up to 128
    std::memcpy(inner, ipad, 64);
    std::memcpy(inner + 64, data, dataSize);
    uint8_t inner_hash[32];
    SHA256::sha256(inner, 64 + dataSize, inner_hash);

    // Compute outer hash
    uint8_t outer[64 + 32];
    std::memcpy(outer, opad, 64);
    std::memcpy(outer + 64, inner_hash, 32);
    SHA256::sha256(outer, 64 + 32, outSignature);

    return true;
}

bool verifyHMAC_SHA256(const uint8_t* data, size_t dataSize,
                       const uint8_t* key, size_t keySize,
                       const uint8_t signature[32]) {
    if (!data || !key || !signature) return false;

    uint8_t computed[32];
    if (!computeHMAC_SHA256(data, dataSize, key, keySize, computed)) {
        return false;
    }

    // Constant-time comparison
    uint32_t diff = 0;
    for (size_t i = 0; i < 32; ++i) {
        diff |= computed[i] ^ signature[i];
    }
    return diff == 0;
}

// ============================================================================
// License Key Verification
// ============================================================================
bool verifyLicenseKeyIntegrity(const LicenseKeyV2& key,
                               const uint8_t* publicKey, size_t keySize,
                               uint64_t boundHWID) {
    // Check magic number
    if (key.magic != 0x5258444C) return false;  // "RXDL"

    // Check version
    if (key.version != 2) return false;

    // Check tier validity
    if (key.tier > 3) return false;

    // Check timestamps
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (key.issueDate > now) return false;  // Future issue date
    if (key.issueDate + MAX_LICENSE_AGE_SECONDS < now) return false;  // Too old

    if (key.expiryDate != 0 && key.expiryDate < now) return false;  // Expired

    // Check feature mask is valid for tier
    FeatureMask tierMask = TierPresets::forTier(static_cast<LicenseTierV2>(key.tier));
    if ((key.features.lo & ~tierMask.lo) != 0 || (key.features.hi & ~tierMask.hi) != 0) {
        return false;  // Features exceed tier
    }

    // Check HWID if bound
    if (boundHWID != 0 && key.hwid != boundHWID) return false;

    // Verify signature (must match signer: first 64 bytes with signature zeroed)
    if (publicKey && keySize > 0) {
        const size_t dataSize = sizeof(LicenseKeyV2) - 32;  // 64 bytes
        LicenseKeyV2 tempKey = key;
        std::memset(tempKey.signature, 0, 32);
        if (!verifyHMAC_SHA256(reinterpret_cast<const uint8_t*>(&tempKey),
                               dataSize,
                               publicKey, keySize,
                               key.signature)) {
            return false;
        }
    }

    return true;
}

uint32_t detectTampering(const LicenseKeyV2& key) {
    uint32_t result = 0;

    // Check magic
    if (key.magic != 0x5258444C) result |= TamperingPatterns::INVALID_MAGIC;

    // Check version
    if (key.version != 2) result |= TamperingPatterns::INVALID_VERSION;

    // Check tier
    if (key.tier > 3) result |= TamperingPatterns::INVALID_TIER;

    // Check timestamps
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (key.issueDate > now) result |= TamperingPatterns::FUTURE_ISSUE_DATE;
    if (key.issueDate + MAX_LICENSE_AGE_SECONDS < now) result |= TamperingPatterns::EXCESSIVE_AGE;
    if (key.expiryDate != 0 && key.expiryDate < now) result |= TamperingPatterns::PAST_EXPIRY;

    // Check feature mask
    FeatureMask tierMask = TierPresets::forTier(static_cast<LicenseTierV2>(key.tier));
    if ((key.features.lo & ~tierMask.lo) != 0 || (key.features.hi & ~tierMask.hi) != 0) {
        result |= TamperingPatterns::INVALID_FEATURE_MASK;
    }

    return result;
}

const char* getTamperingDescription(uint32_t tamperingBits) {
    if (tamperingBits == 0) return "No tampering detected";
    if (tamperingBits & TamperingPatterns::INVALID_MAGIC) return "Invalid license magic number";
    if (tamperingBits & TamperingPatterns::INVALID_VERSION) return "Unsupported license version";
    if (tamperingBits & TamperingPatterns::CORRUPT_SIGNATURE) return "License signature verification failed";
    if (tamperingBits & TamperingPatterns::INVALID_TIER) return "Invalid license tier value";
    if (tamperingBits & TamperingPatterns::FUTURE_ISSUE_DATE) return "License has future issue date";
    if (tamperingBits & TamperingPatterns::PAST_EXPIRY) return "License has expired";
    if (tamperingBits & TamperingPatterns::EXCESSIVE_AGE) return "License is too old";
    if (tamperingBits & TamperingPatterns::INVALID_FEATURE_MASK) return "License features exceed tier";
    if (tamperingBits & TamperingPatterns::INVALID_HWID) return "Hardware ID does not match";
    if (tamperingBits & TamperingPatterns::INVALID_LIMITS) return "License limits exceed tier maximum";
    return "Unknown tampering pattern detected";
}

LicenseResult reconstructKeyWithSignature(const LicenseKeyV2& inKey,
                                          const uint8_t* publicKey, size_t keySize,
                                          LicenseKeyV2& outKey) {
    if (!publicKey || keySize == 0) {
        return LicenseResult::error("No signing key provided");
    }

    outKey = inKey;
    std::memset(outKey.signature, 0, 32);

    // Compute new signature
    if (!computeHMAC_SHA256(reinterpret_cast<const uint8_t*>(&outKey),
                            sizeof(outKey) - sizeof(outKey.padding),
                            publicKey, keySize,
                            outKey.signature)) {
        return LicenseResult::error("Failed to compute signature");
    }

    return LicenseResult::ok("Signature computed successfully");
}

// AES-256-GCM constants: IV 12 bytes, tag 16 bytes
static constexpr size_t GCM_IV_BYTES = 12;
static constexpr size_t GCM_TAG_BYTES = 16;
static constexpr size_t AES256_KEY_BYTES = 32;

// Derive 256-bit key from password using SHA256
static void deriveKeyFromPassword(const char* password, uint8_t key[AES256_KEY_BYTES]) {
    sha256(reinterpret_cast<const uint8_t*>(password), password ? std::strlen(password) : 0, key);
}

bool encryptLicenseForStorage(const LicenseKeyV2& inKey,
                              const char* password,
                              uint8_t* outData, size_t* outSize) {
    if (!outData || !outSize) return false;
    size_t need = GCM_IV_BYTES + sizeof(inKey) + GCM_TAG_BYTES;
    if (*outSize < need) return false;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    uint8_t* iv = nullptr;
    uint8_t* cipher = nullptr;
    ULONG cipherLen = 0;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo = {};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status) || !hAlg) goto fallback;

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                              (ULONG)sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); goto fallback; }

    uint8_t keyBuf[AES256_KEY_BYTES];
    deriveKeyFromPassword(password, keyBuf);
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, keyBuf, (ULONG)sizeof(keyBuf), 0);
    std::memset(keyBuf, 0, sizeof(keyBuf));
    if (!BCRYPT_SUCCESS(status) || !hKey) { BCryptCloseAlgorithmProvider(hAlg, 0); goto fallback; }

    iv = outData;
    cipher = outData + GCM_IV_BYTES;
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, iv, (ULONG)GCM_IV_BYTES, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        goto fallback;
    }

    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv;
    authInfo.cbNonce = (ULONG)GCM_IV_BYTES;
    authInfo.pbTag = cipher + sizeof(inKey);
    authInfo.cbTag = (ULONG)GCM_TAG_BYTES;

    status = BCryptEncrypt(hKey, (PUCHAR)&inKey, (ULONG)sizeof(inKey), &authInfo,
                           nullptr, 0, cipher, (ULONG)(sizeof(inKey) + GCM_TAG_BYTES), &cipherLen, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(status) || cipherLen != sizeof(inKey) + GCM_TAG_BYTES) goto fallback;

    *outSize = need;
    return true;

fallback:
#endif
    // Fallback: no encryption (storage is plain; caller should avoid sensitive data or set password)
    if (*outSize < sizeof(inKey)) return false;
    std::memcpy(outData, &inKey, sizeof(inKey));
    *outSize = sizeof(inKey);
    return true;
}

bool decryptLicenseFromStorage(const uint8_t* encData, size_t encSize,
                               const char* password,
                               LicenseKeyV2& outKey) {
    if (!encData) return false;

#ifdef _WIN32
    size_t expectedEnc = GCM_IV_BYTES + sizeof(LicenseKeyV2) + GCM_TAG_BYTES;
    if (encSize == expectedEnc) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_KEY_HANDLE hKey = nullptr;
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo = {};
        ULONG decLen = 0;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(status) || !hAlg) goto plain;

        status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                  (ULONG)sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); goto plain; }

        uint8_t keyBuf[AES256_KEY_BYTES];
        deriveKeyFromPassword(password, keyBuf);
        status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, keyBuf, (ULONG)sizeof(keyBuf), 0);
        std::memset(keyBuf, 0, sizeof(keyBuf));
        if (!BCRYPT_SUCCESS(status) || !hKey) { BCryptCloseAlgorithmProvider(hAlg, 0); goto plain; }

        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)encData;
        authInfo.cbNonce = (ULONG)GCM_IV_BYTES;
        authInfo.pbTag = (PUCHAR)(encData + GCM_IV_BYTES + sizeof(LicenseKeyV2));
        authInfo.cbTag = (ULONG)GCM_TAG_BYTES;

        status = BCryptDecrypt(hKey, (PUCHAR)(encData + GCM_IV_BYTES), (ULONG)sizeof(LicenseKeyV2), &authInfo,
                              nullptr, 0, (PUCHAR)&outKey, (ULONG)sizeof(outKey), &decLen, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (BCRYPT_SUCCESS(status) && decLen == sizeof(LicenseKeyV2)) return true;
    }
plain:
#endif
    if (encSize == sizeof(LicenseKeyV2)) {
        std::memcpy(&outKey, encData, sizeof(outKey));
        return true;
    }
    return false;
}

}  // namespace RawrXD::License::AntiTampering
