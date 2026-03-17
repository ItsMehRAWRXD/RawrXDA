/**
 * RawrXD Cryptographic Library
 * 
 * A production-ready, from-scratch implementation of cryptographic primitives
 * to replace OpenSSL dependency. Fully implements JWT verification, secure
 * hashing, encryption, and signature verification.
 * 
 * SECURITY NOTE: This is a production implementation with constant-time
 * operations where applicable to prevent timing attacks.
 */

#ifndef RAWRXD_CRYPTO_H
#define RAWRXD_CRYPTO_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <array>

namespace RawrXD {
namespace Crypto {

// ============================================================
// BASE64 URL-SAFE ENCODING (RFC 4648)
// ============================================================

class Base64Url {
public:
    /**
     * Encode binary data to base64url format (no padding)
     * RFC 4648 Section 5
     */
    static std::string encode(const uint8_t* data, size_t len);
    static std::string encode(const std::vector<uint8_t>& data);
    static std::string encode(const std::string& data);
    
    /**
     * Decode base64url format to binary data
     * Handles both padded and unpadded input
     */
    static std::vector<uint8_t> decode(const std::string& encoded);
    static bool decode(const std::string& encoded, std::vector<uint8_t>& output);
};

// ============================================================
// SECURE RANDOM NUMBER GENERATOR
// ============================================================

class SecureRandom {
public:
    /**
     * Generate cryptographically secure random bytes
     * Uses Windows BCrypt API for CSPRNG
     */
    static bool generate(uint8_t* buffer, size_t size);
    static std::vector<uint8_t> generate(size_t size);
    
    /**
     * Generate random uint32_t/uint64_t
     */
    static uint32_t generateUInt32();
    static uint64_t generateUInt64();
};

// ============================================================
// SHA-2 FAMILY (SHA-256, SHA-384, SHA-512)
// ============================================================

class SHA256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;
    static constexpr size_t BLOCK_SIZE = 64;
    
    SHA256();
    void update(const uint8_t* data, size_t len);
    void update(const std::vector<uint8_t>& data);
    void update(const std::string& data);
    void finalize(uint8_t digest[DIGEST_SIZE]);
    std::vector<uint8_t> finalize();
    
    // One-shot hash
    static void hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]);
    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hash(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hash(const std::string& data);
    
private:
    uint32_t state_[8];
    uint64_t bitlen_;
    uint8_t buffer_[BLOCK_SIZE];
    size_t buflen_;
    
    void transform();
    static constexpr uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
};

class SHA384 {
public:
    static constexpr size_t DIGEST_SIZE = 48;
    static constexpr size_t BLOCK_SIZE = 128;
    
    SHA384();
    void update(const uint8_t* data, size_t len);
    void update(const std::vector<uint8_t>& data);
    void update(const std::string& data);
    void finalize(uint8_t digest[DIGEST_SIZE]);
    std::vector<uint8_t> finalize();
    
    static void hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]);
    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hash(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hash(const std::string& data);
    
private:
    uint64_t state_[8];
    uint64_t bitlen_[2]; // 128-bit counter
    uint8_t buffer_[BLOCK_SIZE];
    size_t buflen_;
    
    void transform();
};

class SHA512 {
public:
    static constexpr size_t DIGEST_SIZE = 64;
    static constexpr size_t BLOCK_SIZE = 128;
    
    SHA512();
    void update(const uint8_t* data, size_t len);
    void update(const std::vector<uint8_t>& data);
    void update(const std::string& data);
    void finalize(uint8_t digest[DIGEST_SIZE]);
    std::vector<uint8_t> finalize();
    
    static void hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]);
    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hash(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hash(const std::string& data);
    
    // Friend declarations for SHA384 which shares implementation
    friend class SHA384;
    
private:
    uint64_t state_[8];
    uint64_t bitlen_[2]; // 128-bit counter
    uint8_t buffer_[BLOCK_SIZE];
    size_t buflen_;
    
    void transform();
};

// ============================================================
// HMAC (HASH-BASED MESSAGE AUTHENTICATION CODE)
// RFC 2104
// ============================================================

class HMAC_SHA256 {
public:
    static constexpr size_t DIGEST_SIZE = SHA256::DIGEST_SIZE;
    
    /**
     * Compute HMAC-SHA256
     * Constant-time comparison for security
     */
    static void compute(const uint8_t* key, size_t keylen,
                       const uint8_t* message, size_t msglen,
                       uint8_t mac[DIGEST_SIZE]);
    
    static std::vector<uint8_t> compute(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& message);
    
    static std::vector<uint8_t> compute(const std::string& key,
                                       const std::string& message);
    
    /**
     * Constant-time comparison to prevent timing attacks
     */
    static bool verify(const uint8_t* mac1, const uint8_t* mac2, size_t len);
};

class HMAC_SHA384 {
public:
    static constexpr size_t DIGEST_SIZE = SHA384::DIGEST_SIZE;
    
    static void compute(const uint8_t* key, size_t keylen,
                       const uint8_t* message, size_t msglen,
                       uint8_t mac[DIGEST_SIZE]);
    
    static std::vector<uint8_t> compute(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& message);
    
    static bool verify(const uint8_t* mac1, const uint8_t* mac2, size_t len);
};

class HMAC_SHA512 {
public:
    static constexpr size_t DIGEST_SIZE = SHA512::DIGEST_SIZE;
    
    static void compute(const uint8_t* key, size_t keylen,
                       const uint8_t* message, size_t msglen,
                       uint8_t mac[DIGEST_SIZE]);
    
    static std::vector<uint8_t> compute(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& message);
    
    static bool verify(const uint8_t* mac1, const uint8_t* mac2, size_t len);
};

// ============================================================
// ARBITRARY PRECISION INTEGER (BIG INTEGER)
// For RSA and ECC operations
// ============================================================

class BigInt {
public:
    BigInt();
    BigInt(uint64_t value);
    BigInt(const std::vector<uint8_t>& bytes); // Big-endian
    BigInt(const uint8_t* bytes, size_t len);
    
    // Arithmetic operations
    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;
    
    BigInt& operator+=(const BigInt& other);
    BigInt& operator-=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    
    // Comparison
    bool operator==(const BigInt& other) const;
    bool operator!=(const BigInt& other) const;
    bool operator<(const BigInt& other) const;
    bool operator>(const BigInt& other) const;
    bool operator<=(const BigInt& other) const;
    bool operator>=(const BigInt& other) const;
    
    // Bit operations
    BigInt operator<<(size_t shift) const;
    BigInt operator>>(size_t shift) const;
    BigInt operator&(const BigInt& other) const;
    BigInt operator|(const BigInt& other) const;
    BigInt operator^(const BigInt& other) const;
    
    // Modular arithmetic
    BigInt modPow(const BigInt& exponent, const BigInt& modulus) const;
    BigInt modInverse(const BigInt& modulus) const;
    
    // Utility
    bool isZero() const;
    bool isOne() const;
    size_t bitLength() const;
    std::vector<uint8_t> toBytes() const; // Big-endian
    std::string toHex() const;
    
    static BigInt fromHex(const std::string& hex);
    
private:
    std::vector<uint32_t> digits_; // Little-endian, base 2^32
    bool negative_;
    
    void normalize();
    static void addMagnitude(const std::vector<uint32_t>& a,
                            const std::vector<uint32_t>& b,
                            std::vector<uint32_t>& result);
    static void subMagnitude(const std::vector<uint32_t>& a,
                            const std::vector<uint32_t>& b,
                            std::vector<uint32_t>& result);
    static void mulMagnitude(const std::vector<uint32_t>& a,
                            const std::vector<uint32_t>& b,
                            std::vector<uint32_t>& result);
    static void divMagnitude(const std::vector<uint32_t>& dividend,
                            const std::vector<uint32_t>& divisor,
                            std::vector<uint32_t>& quotient,
                            std::vector<uint32_t>& remainder);
    static int compareMagnitude(const std::vector<uint32_t>& a,
                               const std::vector<uint32_t>& b);
};

// ============================================================
// RSA PUBLIC KEY OPERATIONS
// For JWT RS256/RS384/RS512 signature verification
// ============================================================

class RSAPublicKey {
public:
    RSAPublicKey();
    RSAPublicKey(const BigInt& n, const BigInt& e);
    
    /**
     * Load from JWK format (JSON Web Key)
     */
    bool loadFromJWK(const std::string& n_base64url, const std::string& e_base64url);
    
    /**
     * Verify PKCS#1 v1.5 signature (RS256/RS384/RS512)
     */
    bool verifyPKCS1(const std::vector<uint8_t>& message,
                     const std::vector<uint8_t>& signature,
                     const std::string& hashAlg); // "SHA-256", "SHA-384", "SHA-512"
    
    /**
     * Verify PSS signature (PS256/PS384/PS512)
     */
    bool verifyPSS(const std::vector<uint8_t>& message,
                   const std::vector<uint8_t>& signature,
                   const std::string& hashAlg);
    
    /**
     * Raw RSA operation: c = m^e mod n
     */
    BigInt encrypt(const BigInt& plaintext) const;
    
    size_t getModulusBits() const;
    
private:
    BigInt n_; // Modulus
    BigInt e_; // Public exponent
    
    bool verifyPKCS1Padding(const std::vector<uint8_t>& em,
                           const std::vector<uint8_t>& hash,
                           const std::string& hashAlg);
    
    bool verifyPSSPadding(const std::vector<uint8_t>& em,
                         const std::vector<uint8_t>& mHash,
                         size_t sLen,
                         const std::string& hashAlg);
};

// ============================================================
// ELLIPTIC CURVE (NIST P-256, P-384, P-521)
// For JWT ES256/ES384/ES512 signature verification
// ============================================================

struct ECPoint {
    BigInt x;
    BigInt y;
    bool infinity;
    
    ECPoint() : infinity(true) {}
    ECPoint(const BigInt& x_, const BigInt& y_) : x(x_), y(y_), infinity(false) {}
};

class ECCurve {
public:
    enum class CurveType {
        P256,  // secp256r1 / prime256v1
        P384,  // secp384r1
        P521   // secp521r1
    };
    
    explicit ECCurve(CurveType type);
    
    // Point operations
    ECPoint add(const ECPoint& p, const ECPoint& q) const;
    ECPoint double_(const ECPoint& p) const;
    ECPoint multiply(const ECPoint& p, const BigInt& k) const;
    
    bool isOnCurve(const ECPoint& p) const;
    
    const BigInt& getP() const { return p_; }
    const BigInt& getN() const { return n_; }
    const ECPoint& getG() const { return g_; }
    
private:
    BigInt p_;  // Prime modulus
    BigInt a_;  // Curve coefficient
    BigInt b_;  // Curve coefficient
    BigInt n_;  // Order of base point
    ECPoint g_; // Base point (generator)
    
    void initP256();
    void initP384();
    void initP521();
};

class ECDSAPublicKey {
public:
    ECDSAPublicKey(ECCurve::CurveType curve);
    
    /**
     * Load from JWK format
     */
    bool loadFromJWK(const std::string& x_base64url,
                     const std::string& y_base64url,
                     const std::string& crv);
    
    /**
     * Verify ECDSA signature (ES256/ES384/ES512)
     * Signature format: r || s (concatenated, fixed length)
     */
    bool verify(const std::vector<uint8_t>& message,
                const std::vector<uint8_t>& signature,
                const std::string& hashAlg);
    
private:
    ECCurve curve_;
    ECPoint publicKey_;
};

// ============================================================
// AES-256-GCM (AUTHENTICATED ENCRYPTION)
// ============================================================

class AES256GCM {
public:
    static constexpr size_t KEY_SIZE = 32;
    static constexpr size_t IV_SIZE = 12;
    static constexpr size_t TAG_SIZE = 16;
    
    /**
     * Encrypt with authentication
     * Returns ciphertext || tag
     */
    static bool encrypt(const uint8_t* key,
                       const uint8_t* iv,
                       const uint8_t* plaintext, size_t ptlen,
                       const uint8_t* aad, size_t aadlen,
                       std::vector<uint8_t>& output);
    
    /**
     * Decrypt and verify authentication tag
     */
    static bool decrypt(const uint8_t* key,
                       const uint8_t* iv,
                       const uint8_t* ciphertext, size_t ctlen,
                       const uint8_t* tag,
                       const uint8_t* aad, size_t aadlen,
                       std::vector<uint8_t>& output);
    
private:
    // AES-256 core
    static void aesEncryptBlock(const uint8_t* key,
                               const uint8_t* in,
                               uint8_t* out);
    
    // GCM mode operations
    static void gcmGhash(const uint8_t* h,
                        const uint8_t* data, size_t len,
                        uint8_t* result);
};

// ============================================================
// MEMORY UTILITIES
// Secure memory operations
// ============================================================

class SecureMemory {
public:
    /**
     * Constant-time memory comparison
     * Returns 0 if equal, non-zero otherwise
     */
    static int constantTimeCompare(const void* a, const void* b, size_t len);
    
    /**
     * Securely zero memory (prevents compiler optimization)
     */
    static void secureZero(void* ptr, size_t len);
    
    /**
     * Constant-time conditional copy
     */
    static void constantTimeCopy(uint8_t* dest, const uint8_t* src,
                                size_t len, int condition);
};

} // namespace Crypto
} // namespace RawrXD

#endif // RAWRXD_CRYPTO_H
