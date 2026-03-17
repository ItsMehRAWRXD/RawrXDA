// ============================================================================
// license_generator.hpp — RawrXD Enterprise License Generator
// ============================================================================
// Creates .rawrlic license files with RSA-4096 signatures.
// Supports all license tiers: Community, Trial, Pro, Enterprise, OEM.
//
// Build: Part of RawrXD_LicenseCreator.exe
// Platform: Windows (CryptoAPI) / Linux (OpenSSL)
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <ctime>

namespace RawrXD {
namespace LicenseCreator {

// ============================================================================
// License Tier Enum
// ============================================================================
enum class LicenseTier : uint32_t {
    Community   = 0,    // Free tier (70B, 32K context, no enterprise features)
    Trial       = 1,    // 30-day trial (180B, 128K context, limited features)
    Professional = 7,   // Paid professional (400B, 128K context, subset of features)
    Enterprise  = 2,    // Full enterprise (800B, 200K context, all features)
    OEM         = 3,    // OEM partner (800B, 200K context, all features)
};

// ============================================================================
// License Header Structure (must match RawrXD_EnterpriseLicense.asm)
// ============================================================================
#pragma pack(push, 1)
struct LicenseHeader {
    uint32_t Magic;              // "RXEL" (0x4C455852)
    uint16_t Version;            // License format version (1)
    uint16_t Flags;              // Reserved flags
    uint64_t FeatureMask;        // Feature bitmask (0xFF = all)
    uint64_t IssueTimestamp;     // Unix timestamp (seconds since epoch)
    uint64_t ExpiryTimestamp;    // 0 = perpetual
    uint64_t HardwareHash;       // 0 = floating (any machine)
    uint16_t SeatCount;          // Concurrent seat limit
    uint8_t  PubKeyId;           // Which signing key (key rotation)
    uint8_t  Reserved[21];       // Padding to reach 64 bytes (4+2+2+8+8+8+8+2+1+21=64)
};
static_assert(sizeof(LicenseHeader) == 64, "LicenseHeader must be 64 bytes");
#pragma pack(pop)

// ============================================================================
// License Configuration
// ============================================================================
struct LicenseConfig {
    LicenseTier tier = LicenseTier::Community;
    uint64_t featureMask = 0;           // Auto-computed from tier if 0
    uint64_t expiryDays = 0;            // 0 = perpetual, else days from now
    uint64_t hardwareHash = 0;          // 0 = floating, else specific HWID
    uint16_t seatCount = 1;             // Concurrent seats
    uint8_t pubKeyId = 0;               // RSA public key ID (for rotation)
    std::string customerName;           // For reference (not in blob)
    std::string customerEmail;          // For reference
};

// ============================================================================
// RSA Key Pair
// ============================================================================
struct RSAKeyPair {
    std::vector<uint8_t> publicKeyBlob;   // CryptoAPI PUBLICKEYBLOB format
    std::vector<uint8_t> privateKeyBlob;  // CryptoAPI PRIVATEKEYBLOB format
    uint32_t keySize = 4096;              // RSA-4096
};

// ============================================================================
// License Generator Class
// ============================================================================
class LicenseGenerator {
public:
    LicenseGenerator() = default;
    ~LicenseGenerator() = default;

    // ========================================================================
    // RSA Key Management
    // ========================================================================
    
    /// Generate a new RSA-4096 keypair (expensive — cache the result)
    /// @param keySize  RSA key size in bits (2048, 3072, 4096)
    /// @return true on success
    bool GenerateKeyPair(uint32_t keySize = 4096);

    /// Load RSA keypair from PEM files (OpenSSL format)
    /// @param publicKeyPath   Path to public key PEM
    /// @param privateKeyPath  Path to private key PEM (optional for verify-only)
    /// @return true on success
    bool LoadKeyPair(const std::string& publicKeyPath,
                     const std::string& privateKeyPath = "");

    /// Save RSA keypair to PEM files
    /// @param publicKeyPath   Output path for public key
    /// @param privateKeyPath  Output path for private key
    /// @return true on success
    bool SaveKeyPair(const std::string& publicKeyPath,
                     const std::string& privateKeyPath) const;

    /// Get public key blob for embedding in RawrXD_EnterpriseLicense.asm
    /// @return Public key in CryptoAPI PUBLICKEYBLOB format
    std::vector<uint8_t> GetPublicKeyBlob() const { return m_keyPair.publicKeyBlob; }

    /// Get current HWID for this machine (for binding licenses)
    /// @return 64-bit hardware fingerprint
    static uint64_t GetCurrentHardwareHash();

    // ========================================================================
    // License Generation
    // ========================================================================

    /// Generate a license file from configuration
    /// @param config      License configuration (tier, expiry, hwid, etc.)
    /// @param outputPath  Output .rawrlic file path
    /// @return true on success
    bool GenerateLicense(const LicenseConfig& config, const std::string& outputPath);

    /// Generate license blob (no file write — for testing)
    /// @param config  License configuration
    /// @param outBlob Output license blob (header + optional payload)
    /// @param outSig  Output RSA signature (512 bytes)
    /// @return true on success
    bool GenerateLicenseBlob(const LicenseConfig& config,
                             std::vector<uint8_t>& outBlob,
                             std::vector<uint8_t>& outSig);

    /// Verify a license file (signature + expiry + magic)
    /// @param licensePath  Path to .rawrlic file
    /// @param hwid         Expected HWID (0 = skip hwid check)
    /// @return true if valid
    bool VerifyLicense(const std::string& licensePath, uint64_t hwid = 0);

    // ========================================================================
    // Utility
    // ========================================================================

    /// Get feature mask for a given tier
    static uint64_t GetFeatureMaskForTier(LicenseTier tier);

    /// Get human-readable tier name
    static const char* GetTierName(LicenseTier tier);

    /// Get error message from last operation
    const std::string& GetLastError() const { return m_lastError; }

private:
    RSAKeyPair m_keyPair;
    std::string m_lastError;

    void setError(const std::string& msg) { m_lastError = msg; }
    uint64_t getCurrentUnixTime() const;
};

} // namespace LicenseCreator
} // namespace RawrXD
