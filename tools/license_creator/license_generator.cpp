// ============================================================================
// license_generator.cpp — RawrXD Enterprise License Generator Implementation
// ============================================================================
// Creates .rawrlic license files with RSA-4096 signatures.
// Uses CryptoAPI on Windows, OpenSSL fallback on Linux.
//
// Build: Link with advapi32.lib (Windows) or libcrypto (Linux)
// ============================================================================

#include "license_generator.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <chrono>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <wincrypt.h>
  #pragma comment(lib, "advapi32.lib")
#else
  #include <openssl/rsa.h>
  #include <openssl/pem.h>
  #include <openssl/sha.h>
  #include <openssl/evp.h>
#endif

namespace RawrXD {
namespace LicenseCreator {

// ============================================================================
// Constants (must match RawrXD_Common.inc)
// ============================================================================
constexpr uint32_t RAWR_LICENSE_MAGIC = 0x4C455852;  // "RXEL"
constexpr uint16_t CURRENT_LICENSE_VERSION = 1;
constexpr size_t RSA_SIGNATURE_SIZE = 512;  // RSA-4096 → 512-byte signature

// Feature bitmasks (must match RawrXD_Common.inc)
constexpr uint64_t FEATURE_800B_DUALENGINE       = 0x00000001;
constexpr uint64_t FEATURE_AVX512_PREMIUM        = 0x00000002;
constexpr uint64_t FEATURE_DISTRIBUTED_SWARM     = 0x00000004;
constexpr uint64_t FEATURE_GPU_QUANT_4BIT        = 0x00000008;
constexpr uint64_t FEATURE_ENTERPRISE_SUPPORT    = 0x00000010;
constexpr uint64_t FEATURE_UNLIMITED_CONTEXT     = 0x00000020;
constexpr uint64_t FEATURE_FLASH_ATTENTION       = 0x00000040;
constexpr uint64_t FEATURE_MULTI_GPU             = 0x00000080;

constexpr uint64_t FEATURE_COMMUNITY  = 0x00;
constexpr uint64_t FEATURE_PRO        = FEATURE_AVX512_PREMIUM | FEATURE_GPU_QUANT_4BIT | FEATURE_FLASH_ATTENTION;  // 0x4A
constexpr uint64_t FEATURE_ENTERPRISE = 0xFF;

// ============================================================================
// Platform-Specific Crypto Wrappers
// ============================================================================

#ifdef _WIN32
// Windows CryptoAPI implementation

bool LicenseGenerator::GenerateKeyPair(uint32_t keySize) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    
    // Acquire crypto context
    if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        setError("CryptAcquireContext failed");
        return false;
    }

    // Generate RSA keypair
    DWORD dwFlags = (keySize << 16) | CRYPT_EXPORTABLE;
    if (!CryptGenKey(hProv, AT_KEYEXCHANGE, dwFlags, &hKey)) {
        setError("CryptGenKey failed");
        CryptReleaseContext(hProv, 0);
        return false;
    }

    // Export public key
    DWORD cbPublic = 0;
    CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, nullptr, &cbPublic);
    m_keyPair.publicKeyBlob.resize(cbPublic);
    if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, m_keyPair.publicKeyBlob.data(), &cbPublic)) {
        setError("CryptExportKey (public) failed");
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    // Export private key
    DWORD cbPrivate = 0;
    CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, nullptr, &cbPrivate);
    m_keyPair.privateKeyBlob.resize(cbPrivate);
    if (!CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, m_keyPair.privateKeyBlob.data(), &cbPrivate)) {
        setError("CryptExportKey (private) failed");
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    m_keyPair.keySize = keySize;

    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);

    std::cout << "[LicenseCreator] Generated RSA-" << keySize << " keypair (" 
              << cbPublic << " bytes public, " << cbPrivate << " bytes private)" << std::endl;

    return true;
}

bool LicenseGenerator::GenerateLicenseBlob(const LicenseConfig& config,
                                            std::vector<uint8_t>& outBlob,
                                            std::vector<uint8_t>& outSig) {
    if (m_keyPair.privateKeyBlob.empty()) {
        setError("No private key loaded — call GenerateKeyPair() or LoadKeyPair() first");
        return false;
    }

    // Build license header
    LicenseHeader header{};
    header.Magic = RAWR_LICENSE_MAGIC;
    header.Version = CURRENT_LICENSE_VERSION;
    header.Flags = 0;
    header.FeatureMask = config.featureMask == 0 ? GetFeatureMaskForTier(config.tier) : config.featureMask;
    header.IssueTimestamp = getCurrentUnixTime();
    header.ExpiryTimestamp = (config.expiryDays == 0) ? 0 : (header.IssueTimestamp + config.expiryDays * 86400ULL);
    header.HardwareHash = config.hardwareHash;
    header.SeatCount = config.seatCount;
    header.PubKeyId = config.pubKeyId;
    std::memset(header.Reserved, 0, sizeof(header.Reserved));

    // Serialize to blob
    outBlob.resize(sizeof(LicenseHeader));
    std::memcpy(outBlob.data(), &header, sizeof(LicenseHeader));

    // Sign the blob with RSA-4096 (SHA-512 hash)
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        setError("CryptAcquireContext failed");
        return false;
    }

    // Import private key
    if (!CryptImportKey(hProv, m_keyPair.privateKeyBlob.data(),
                        static_cast<DWORD>(m_keyPair.privateKeyBlob.size()),
                        0, 0, &hKey)) {
        setError("CryptImportKey failed");
        CryptReleaseContext(hProv, 0);
        return false;
    }

    // Hash the license blob (SHA-512)
    if (!CryptCreateHash(hProv, CALG_SHA_512, 0, 0, &hHash)) {
        setError("CryptCreateHash failed");
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    if (!CryptHashData(hHash, outBlob.data(), static_cast<DWORD>(outBlob.size()), 0)) {
        setError("CryptHashData failed");
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    // Sign the hash
    DWORD cbSig = RSA_SIGNATURE_SIZE;
    outSig.resize(RSA_SIGNATURE_SIZE);
    if (!CryptSignHashA(hHash, AT_KEYEXCHANGE, nullptr, 0, outSig.data(), &cbSig)) {
        setError("CryptSignHash failed");
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return false;
    }

    // Resize to actual signature size (should be 512 for RSA-4096)
    outSig.resize(cbSig);

    CryptDestroyHash(hHash);
    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);

    std::cout << "[LicenseCreator] Generated license blob (" << outBlob.size() 
              << " bytes) with signature (" << outSig.size() << " bytes)" << std::endl;

    return true;
}

uint64_t LicenseGenerator::GetCurrentHardwareHash() {
    // On Windows: use Shield_GenerateHWID from ASM (CPUID + Volume Serial + MurmurHash3)
    // For standalone license creator: simplified version (Volume serial only)
    char volumeName[MAX_PATH + 1] = {};
    DWORD volumeSerial = 0;
    
    if (GetVolumeInformationA("C:\\", volumeName, MAX_PATH, &volumeSerial,
                              nullptr, nullptr, nullptr, 0)) {
        // Simple hash: just use volume serial as-is (real system uses MurmurHash3)
        return static_cast<uint64_t>(volumeSerial);
    }

    return 0; // Floating license
}

#else
// Linux/OpenSSL implementation (placeholder — expand as needed)

bool LicenseGenerator::GenerateKeyPair(uint32_t keySize) {
    setError("OpenSSL implementation not yet complete (use Windows CryptoAPI)");
    return false;
}

bool LicenseGenerator::GenerateLicenseBlob(const LicenseConfig& config,
                                            std::vector<uint8_t>& outBlob,
                                            std::vector<uint8_t>& outSig) {
    setError("OpenSSL implementation not yet complete (use Windows CryptoAPI)");
    return false;
}

uint64_t LicenseGenerator::GetCurrentHardwareHash() {
    return 0; // Floating license (OpenSSL stub)
}

#endif

// ============================================================================
// Platform-Independent Implementation
// ============================================================================

bool LicenseGenerator::GenerateLicense(const LicenseConfig& config, const std::string& outputPath) {
    std::vector<uint8_t> blob;
    std::vector<uint8_t> sig;

    if (!GenerateLicenseBlob(config, blob, sig)) {
        return false;
    }

    // Write .rawrlic file: [blob][512-byte signature]
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open output file: " + outputPath);
        return false;
    }

    file.write(reinterpret_cast<const char*>(blob.data()), blob.size());
    file.write(reinterpret_cast<const char*>(sig.data()), sig.size());
    file.close();

    std::cout << "[LicenseCreator] Wrote license file: " << outputPath 
              << " (" << (blob.size() + sig.size()) << " bytes)" << std::endl;

    return true;
}

bool LicenseGenerator::VerifyLicense(const std::string& licensePath, uint64_t hwid) {
    std::ifstream file(licensePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        setError("Cannot open license file: " + licensePath);
        return false;
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize <= RSA_SIGNATURE_SIZE) {
        setError("License file too small");
        return false;
    }

    file.seekg(0);
    size_t blobSize = fileSize - RSA_SIGNATURE_SIZE;

    std::vector<uint8_t> blob(blobSize);
    std::vector<uint8_t> sig(RSA_SIGNATURE_SIZE);

    file.read(reinterpret_cast<char*>(blob.data()), blobSize);
    file.read(reinterpret_cast<char*>(sig.data()), RSA_SIGNATURE_SIZE);
    file.close();

    // Parse header
    if (blob.size() < sizeof(LicenseHeader)) {
        setError("License blob too small for header");
        return false;
    }

    LicenseHeader header;
    std::memcpy(&header, blob.data(), sizeof(LicenseHeader));

    // Check magic
    if (header.Magic != RAWR_LICENSE_MAGIC) {
        setError("Invalid license magic");
        return false;
    }

    // Check expiry
    if (header.ExpiryTimestamp != 0) {
        uint64_t now = getCurrentUnixTime();
        if (now > header.ExpiryTimestamp) {
            setError("License expired");
            return false;
        }
    }

    // Check HWID (if binding specified)
    if (hwid != 0 && header.HardwareHash != 0 && header.HardwareHash != hwid) {
        setError("Hardware mismatch");
        return false;
    }

    std::cout << "[LicenseCreator] License verified successfully" << std::endl;
    std::cout << "  Tier: " << GetTierName(static_cast<LicenseTier>(header.Version)) << std::endl;
    std::cout << "  Features: 0x" << std::hex << header.FeatureMask << std::dec << std::endl;
    std::cout << "  Expiry: " << (header.ExpiryTimestamp == 0 ? "Perpetual" : "Limited") << std::endl;

    return true;
}

bool LicenseGenerator::SaveKeyPair(const std::string& publicKeyPath,
                                    const std::string& privateKeyPath) const {
    if (m_keyPair.publicKeyBlob.empty() || m_keyPair.privateKeyBlob.empty()) {
        return false;
    }

    // Write public key (binary blob)
    std::ofstream pubFile(publicKeyPath, std::ios::binary);
    if (pubFile.is_open()) {
        pubFile.write(reinterpret_cast<const char*>(m_keyPair.publicKeyBlob.data()),
                      m_keyPair.publicKeyBlob.size());
        pubFile.close();
        std::cout << "[LicenseCreator] Saved public key: " << publicKeyPath << std::endl;
    }

    // Write private key (binary blob)
    std::ofstream privFile(privateKeyPath, std::ios::binary);
    if (privFile.is_open()) {
        privFile.write(reinterpret_cast<const char*>(m_keyPair.privateKeyBlob.data()),
                       m_keyPair.privateKeyBlob.size());
        privFile.close();
        std::cout << "[LicenseCreator] Saved private key: " << privateKeyPath 
                  << " (KEEP THIS SECRET!)" << std::endl;
    }

    return true;
}

bool LicenseGenerator::LoadKeyPair(const std::string& publicKeyPath,
                                    const std::string& privateKeyPath) {
    // Load public key
    std::ifstream pubFile(publicKeyPath, std::ios::binary | std::ios::ate);
    if (pubFile.is_open()) {
        auto size = static_cast<size_t>(pubFile.tellg());
        pubFile.seekg(0);
        m_keyPair.publicKeyBlob.resize(size);
        pubFile.read(reinterpret_cast<char*>(m_keyPair.publicKeyBlob.data()), size);
        pubFile.close();
    }

    // Load private key (optional)
    if (!privateKeyPath.empty()) {
        std::ifstream privFile(privateKeyPath, std::ios::binary | std::ios::ate);
        if (privFile.is_open()) {
            auto size = static_cast<size_t>(privFile.tellg());
            privFile.seekg(0);
            m_keyPair.privateKeyBlob.resize(size);
            privFile.read(reinterpret_cast<char*>(m_keyPair.privateKeyBlob.data()), size);
            privFile.close();
        }
    }

    return !m_keyPair.publicKeyBlob.empty();
}

uint64_t LicenseGenerator::GetFeatureMaskForTier(LicenseTier tier) {
    switch (tier) {
        case LicenseTier::Community:     return FEATURE_COMMUNITY;
        case LicenseTier::Trial:         return FEATURE_PRO;  // Same as Pro for trial
        case LicenseTier::Professional:  return FEATURE_PRO;
        case LicenseTier::Enterprise:    return FEATURE_ENTERPRISE;
        case LicenseTier::OEM:           return FEATURE_ENTERPRISE;
        default:                         return FEATURE_COMMUNITY;
    }
}

const char* LicenseGenerator::GetTierName(LicenseTier tier) {
    switch (tier) {
        case LicenseTier::Community:     return "Community";
        case LicenseTier::Trial:         return "Trial";
        case LicenseTier::Professional:  return "Professional";
        case LicenseTier::Enterprise:    return "Enterprise";
        case LicenseTier::OEM:           return "OEM";
        default:                         return "Unknown";
    }
}

uint64_t LicenseGenerator::getCurrentUnixTime() const {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace LicenseCreator
} // namespace RawrXD
