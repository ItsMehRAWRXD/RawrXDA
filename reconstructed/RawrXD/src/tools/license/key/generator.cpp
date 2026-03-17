// ============================================================================
// src/tools/license_key_generator.cpp — License Key Generator
// ============================================================================
// Generates cryptographically-signed license keys for each tier
// Output: community.key, professional.key, enterprise.key, sovereign.key
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>

namespace RawrXD::Tools {

// License Tier Constants
enum class LicenseTierForGen {
    Community = 1,
    Professional = 2,
    Enterprise = 3,
    Sovereign = 4
};

// ============================================================================
// License Key Structure (matches enterprise_license.h)
// ============================================================================
struct LicenseKeyData {
    char magic[4];              // "RAWR"
    uint32_t version;           // 2
    uint8_t tier;               // LicenseTierForGen
    uint32_t featureMask[4];    // Feature flags (up to 128 features)
    uint64_t expirationTime;    // Unix timestamp
    char hardwareId[64];        // Optional hardware binding
    uint8_t reserved[128];      // For future use
    uint8_t signature[256];     // RSA-2048 signature
};

// ============================================================================
// License Key Generator Implementation
// ============================================================================

class LicenseKeyGenerator {
private:
    bool initialized;
    HCRYPTPROV hCryptProv;
    
public:
    LicenseKeyGenerator() : initialized(false), hCryptProv(nullptr) {
        // Initialize Windows Cryptography API
        if (CryptAcquireContextW(&hCryptProv, nullptr, nullptr,
                                 PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            initialized = true;
        }
    }
    
    ~LicenseKeyGenerator() {
        if (hCryptProv) {
            CryptReleaseContext(hCryptProv, 0);
        }
    }
    
    // Generate license key for given tier
    bool generateLicenseKey(LicenseTierForGen tier,
                           const std::string& outputPath,
                           uint64_t expirationDays = 365) {
        if (!initialized) {
            fprintf(stderr, "[LICENSE_GEN] Cryptography not initialized\n");
            return false;
        }
        
        LicenseKeyData key = {};
        
        // Fill in header
        std::memcpy(key.magic, "RAWR", 4);
        key.version = 2;
        key.tier = static_cast<uint8_t>(tier);
        
        // Set features based on tier
        setFeaturesForTier(tier, key.featureMask);
        
        // Set expiration (14 days for community test, 365 for others)
        time_t now = time(nullptr);
        key.expirationTime = now + (expirationDays * 24 * 3600);
        
        // Add hardware binding (optional)
        getHardwareId(key.hardwareId, sizeof(key.hardwareId));
        
        // Sign the key (mock signature for now - in production use real RSA)
        mockSignKey(key);
        
        // Write to file
        return writeKeyToFile(outputPath, key);
    }
    
    bool generateAllTierKeys(const std::string& outputDir) {
        printf("[LICENSE_GEN] Generating test license keys...\n");
        
        bool success = true;
        success &= generateLicenseKey(LicenseTierForGen::Community,
                                     outputDir + "\\community.key", 14);
        success &= generateLicenseKey(LicenseTierForGen::Professional,
                                     outputDir + "\\professional.key", 365);
        success &= generateLicenseKey(LicenseTierForGen::Enterprise,
                                     outputDir + "\\enterprise.key", 365);
        success &= generateLicenseKey(LicenseTierForGen::Sovereign,
                                     outputDir + "\\sovereign.key", 365);
        
        if (success) {
            printf("[LICENSE_GEN] ✓ All tier keys generated successfully\n");
        } else {
            printf("[LICENSE_GEN] ✗ Key generation failed\n");
        }
        
        return success;
    }
    
private:
    void setFeaturesForTier(LicenseTierForGen tier, uint32_t* featureMask) {
        // Clear all features
        for (int i = 0; i < 4; i++) {
            featureMask[i] = 0;
        }
        
        // Set bits according to tier (hierarchical - higher tier includes lower)
        if (tier >= LicenseTierForGen::Community) {
            // Community: BasicGGUFLoading, Q4Quantization, CPUInference, BasicChatUI
            featureMask[0] |= (1 << 0);  // BasicGGUFLoading
            featureMask[0] |= (1 << 1);  // Q4Quantization
            featureMask[0] |= (1 << 2);  // CPUInference
            featureMask[0] |= (1 << 3);  // BasicChatUI
        }
        
        if (tier >= LicenseTierForGen::Professional) {
            // Professional: All Community + 21 Professional features
            featureMask[0] |= (1 << 5);  // TokenStreaming
            featureMask[0] |= (1 << 6);  // InferenceStatistics
            featureMask[0] |= (1 << 7);  // KVCacheManagement
            featureMask[0] |= (1 << 8);  // CUDABackend
            featureMask[0] |= (1 << 9);  // GrammarConstrainedGen
            featureMask[0] |= (1 << 10); // LoRAAdapterSupport
            featureMask[0] |= (1 << 11); // ResponseCaching
            // ... more Professional features
        }
        
        if (tier >= LicenseTierForGen::Enterprise) {
            // Enterprise: All Professional + 26 Enterprise features
            featureMask[1] |= (1 << 0);  // AgenticFailureDetect
            featureMask[1] |= (1 << 1);  // ProxyHotpatching
            featureMask[1] |= (1 << 2);  // ServerSidePatching
            // ... more Enterprise features
        }
        
        if (tier >= LicenseTierForGen::Sovereign) {
            // Sovereign: All Enterprise + 8 Sovereign features
            featureMask[2] |= (1 << 0);  // AirGappedDeploy
            featureMask[2] |= (1 << 1);  // HSMIntegration
            featureMask[2] |= (1 << 2);  // FIPS140_2Compliance
            featureMask[2] |= (1 << 3);  // CustomSecurityPolicies
            featureMask[2] |= (1 << 4);  // SovereignKeyMgmt
            featureMask[2] |= (1 << 5);  // ClassifiedNetwork
            featureMask[2] |= (1 << 6);  // ImmutableAuditLogs
            featureMask[2] |= (1 << 7);  // KubernetesSupport
        }
    }
    
    void getHardwareId(char* buffer, size_t buflen) {
        // Mock hardware ID (in production: read from WMI/registry)
        const char* mockId = "MOCK-HW-001";
        strncpy_s(buffer, buflen, mockId, _TRUNCATE);
    }
    
    void mockSignKey(LicenseKeyData& key) {
        // Mock signature (in production: use real RSA signing)
        // This is deterministic for testing
        uint64_t checksum = 0;
        checksum ^= *reinterpret_cast<uint64_t*>(key.magic);
        checksum ^= key.version;
        checksum ^= key.tier;
        checksum ^= key.expirationTime;
        
        // Fill first 8 bytes of signature with checksum
        uint64_t* sig = reinterpret_cast<uint64_t*>(key.signature);
        sig[0] = checksum;
    }
    
    bool writeKeyToFile(const std::string& path, const LicenseKeyData& key) {
        HANDLE hFile = CreateFileA(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hFile == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "[LICENSE_GEN] Failed to create file: %s\n", path.c_str());
            return false;
        }
        
        DWORD bytesWritten = 0;
        bool success = WriteFile(hFile, &key, sizeof(key), &bytesWritten, nullptr);
        
        CloseHandle(hFile);
        
        if (success && bytesWritten == sizeof(key)) {
            printf("[LICENSE_GEN] ✓ Key written: %s (%zu bytes)\n", path.c_str(), bytesWritten);
            return true;
        }
        
        printf("[LICENSE_GEN] ✗ Failed to write key: %s\n", path.c_str());
        return false;
    }
};

// ============================================================================
// Public API
// ============================================================================

bool GenerateTestLicenseKeys(const std::string& outputDir) {
    LicenseKeyGenerator generator;
    return generator.generateAllTierKeys(outputDir);
}

} // namespace RawrXD::Tools

// ============================================================================
// Main Entry Point (for standalone key generator)
// ============================================================================

int main(int argc, char* argv[]) {
    printf("RawrXD License Key Generator v1.0\n");
    printf("==================================\n\n");
    
    std::string outputDir = ".";
    if (argc > 1) {
        outputDir = argv[1];
    }
    
    if (RawrXD::Tools::GenerateTestLicenseKeys(outputDir)) {
        printf("\nKeys generated successfully in: %s\n", outputDir.c_str());
        return 0;
    } else {
        printf("\nKey generation failed!\n");
        return 1;
    }
}
