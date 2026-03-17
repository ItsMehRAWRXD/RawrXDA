// ============================================================================
// license_anti_tampering.h — Enterprise License Anti-Tampering Layer
// ============================================================================
// Implements tamper detection and verification for LicenseKeyV2 structures.
// Uses CRC32 checksums and HMAC-SHA256 signatures for integrity verification.
//
// Pattern: No exceptions. Returns bool/status codes.
// Threading: Thread-safe with internal critical sections.
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include <cstdint>
#include <cstddef>

namespace RawrXD::License {

// ============================================================================
// Anti-Tampering Constants
// ============================================================================
namespace AntiTampering {

    // CRC32 polynomial for license key validation
    static constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320;

    // HMAC-SHA256 output size
    static constexpr size_t HMAC_SIZE = 32;

    // Anti-tampering magic bytes (embedded in protected region)
    static constexpr uint32_t PROTECTION_MAGIC = 0x5254414D;  // "RTAM"

    // Maximum allowed age of a license key (in seconds) - 1 year
    static constexpr uint64_t MAX_LICENSE_AGE_SECONDS = 365 * 24 * 3600;

    // ========================================================================
    // CRC32 Functions
    // ========================================================================

    /// Compute CRC32 checksum of data
    /// @param data Pointer to data to checksum
    /// @param size Size of data in bytes
    /// @return CRC32 checksum value
    uint32_t computeCRC32(const uint8_t* data, size_t size);

    /// Verify CRC32 checksum
    /// @param data Pointer to data
    /// @param size Size of data
    /// @param expectedCRC Expected CRC32 value
    /// @return true if CRC32 matches
    inline bool verifyCRC32(const uint8_t* data, size_t size, uint32_t expectedCRC) {
        return computeCRC32(data, size) == expectedCRC;
    }

    // ========================================================================
    // HMAC-SHA256 Functions
    // ========================================================================

    /// Compute SHA256 hash (for testing and internal use)
    /// @param data Data to hash
    /// @param size Size of data
    /// @param hash Output buffer (32 bytes)
    void sha256(const uint8_t* data, size_t size, uint8_t hash[32]);

    /// Compute HMAC-SHA256 signature
    /// @param data Data to sign
    /// @param dataSize Size of data
    /// @param key Secret key
    /// @param keySize Size of key
    /// @param outSignature Output buffer (32 bytes)
    /// @return true if successful
    bool computeHMAC_SHA256(const uint8_t* data, size_t dataSize,
                            const uint8_t* key, size_t keySize,
                            uint8_t outSignature[32]);

    /// Verify HMAC-SHA256 signature (constant-time comparison)
    /// @param data Data to verify
    /// @param dataSize Size of data
    /// @param key Secret key
    /// @param keySize Size of key
    /// @param signature Expected signature (32 bytes)
    /// @return true if signature is valid
    bool verifyHMAC_SHA256(const uint8_t* data, size_t dataSize,
                           const uint8_t* key, size_t keySize,
                           const uint8_t signature[32]);

    // ========================================================================
    // License Key Verification
    // ========================================================================

    /// Comprehensive license key verification
    /// Checks:
    /// - Magic number validity
    /// - Structural integrity (CRC32)
    /// - Signature validity (HMAC-SHA256)
    /// - Timestamp sanity checks
    /// - Feature bitmask validity
    /// - HWID binding (if machine-locked)
    /// @param key License key to verify
    /// @param publicKey Public HMAC key (for signature verification)
    /// @param keySize Size of public key
    /// @param boundHWID Expected HWID (0 = skip check)
    /// @return true if key passes all checks
    bool verifyLicenseKeyIntegrity(const LicenseKeyV2& key,
                                   const uint8_t* publicKey, size_t keySize,
                                   uint64_t boundHWID = 0);

    /// Detect tampering patterns in license key
    /// @param key License key to analyze
    /// @return Bitmask of detected tampering patterns (0 = no tampering)
    uint32_t detectTampering(const LicenseKeyV2& key);

    /// Human-readable tampering detection result
    const char* getTamperingDescription(uint32_t tamperingBits);

    // ========================================================================
    // Tampering Detection Patterns
    // ========================================================================
    namespace TamperingPatterns {
        static constexpr uint32_t INVALID_MAGIC          = 0x0001;   // Magic not 0x5258444C
        static constexpr uint32_t INVALID_VERSION        = 0x0002;   // Version not 2
        static constexpr uint32_t CORRUPT_SIGNATURE      = 0x0004;   // Signature mismatch
        static constexpr uint32_t INVALID_TIER           = 0x0008;   // Tier > 3
        static constexpr uint32_t FUTURE_ISSUE_DATE      = 0x0010;   // Issue > now
        static constexpr uint32_t PAST_EXPIRY            = 0x0020;   // Expiry < now
        static constexpr uint32_t EXCESSIVE_AGE          = 0x0040;   // Issue > 1 year ago
        static constexpr uint32_t INVALID_FEATURE_MASK   = 0x0080;   // Features > max tier allows
        static constexpr uint32_t INVALID_HWID           = 0x0100;   // HWID mismatch
        static constexpr uint32_t FEATURE_DOWNGRADE      = 0x0200;   // Features less than tier allows
        static constexpr uint32_t ZERO_DURATION          = 0x0400;   // Issue == Expiry (non-perpetual)
        static constexpr uint32_t INVALID_LIMITS         = 0x0800;   // Limits exceed tier max
    }

    // ========================================================================
    // License Key Reconstruction (Anti-Forgery)
    // ========================================================================

    /// Reconstruct a license key with verified integrity
    /// Used to create tamper-proof keys
    /// @param inKey Source key data
    /// @param publicKey HMAC key
    /// @param keySize Key size
    /// @param outKey Output key with signature
    /// @return true if reconstruction successful
    LicenseResult reconstructKeyWithSignature(const LicenseKeyV2& inKey,
                                              const uint8_t* publicKey, size_t keySize,
                                              LicenseKeyV2& outKey);

    // ========================================================================
    // License Storage Protection
    // ========================================================================

    /// Encrypt license key for storage (optional)
    /// @param inKey Key to encrypt
    /// @param password Encryption password
    /// @param outData Encrypted data
    /// @param outSize Size of encrypted data
    /// @return true if successful
    bool encryptLicenseForStorage(const LicenseKeyV2& inKey,
                                  const char* password,
                                  uint8_t* outData, size_t* outSize);

    /// Decrypt license key from storage
    /// @param encData Encrypted data
    /// @param encSize Size of encrypted data
    /// @param password Decryption password
    /// @param outKey Decrypted key
    /// @return true if successful
    bool decryptLicenseFromStorage(const uint8_t* encData, size_t encSize,
                                   const char* password,
                                   LicenseKeyV2& outKey);

}  // namespace AntiTampering

}  // namespace RawrXD::License
