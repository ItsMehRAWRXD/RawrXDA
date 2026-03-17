// ============================================================================
// RawrXD_KeyGen.cpp — RSA-4096 Key Authority & License Signer
// ============================================================================
// Standalone CLI tool for generating RSA-4096 key pairs and signing
// RawrXD enterprise license blobs.
//
// Usage:
//   RawrXD_KeyGen.exe --genkey                    Generate RSA-4096 key pair
//   RawrXD_KeyGen.exe --export-pub <file.inc>     Export public key as MASM .inc
//   RawrXD_KeyGen.exe --sign <license.bin>        Sign a license blob
//   RawrXD_KeyGen.exe --issue <args>              Generate + sign a full license
//   RawrXD_KeyGen.exe --hwid                      Print this machine's HWID
//
// Build:
//   cl /EHsc /O2 /Fe:RawrXD_KeyGen.exe RawrXD_KeyGen.cpp advapi32.lib crypt32.lib
//
// Architecture: Win32 CryptoAPI (advapi32.lib) — no third-party dependencies
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

// ============================================================================
// Constants
// ============================================================================
static constexpr uint32_t RSA_KEY_BITS     = 4096;
static constexpr uint32_t RSA_SIG_BYTES    = 512;   // 4096 / 8
static constexpr uint32_t LICENSE_MAGIC    = 0x5258444C;  // "RXDL" little-endian
static constexpr uint16_t LICENSE_VERSION  = 0x0200;      // v2.0
static constexpr char     CONTAINER_NAME[] = "RawrXD_Enterprise_KeyAuthority";
static constexpr char     PROVIDER_TYPE[]  = "";

// License feature bitmasks (must match RawrXD_Common.inc)
static constexpr uint64_t FEAT_DUAL_ENGINE    = 0x01;
static constexpr uint64_t FEAT_AVX512         = 0x02;
static constexpr uint64_t FEAT_DISTRIBUTED    = 0x04;
static constexpr uint64_t FEAT_GPU_QUANT      = 0x08;
static constexpr uint64_t FEAT_ENTERPRISE_SUP = 0x10;
static constexpr uint64_t FEAT_UNLIMITED_CTX  = 0x20;
static constexpr uint64_t FEAT_FLASH_ATTN     = 0x40;
static constexpr uint64_t FEAT_MULTI_GPU      = 0x80;
static constexpr uint64_t FEAT_ALL            = 0xFF;

// ============================================================================
// License Blob Structure (must match ASM LICENSE_HEADER)
// ============================================================================
#pragma pack(push, 1)
struct LicenseHeader {
    uint32_t magic;           // "RXDL"
    uint16_t version;         // 0x0200
    uint16_t licenseType;     // 1=Trial, 2=Enterprise, 3=OEM
    uint64_t featureMask;     // Bitmask of enabled features
    uint64_t hwid;            // Target hardware ID (0 = floating)
    uint64_t issuedTimestamp;  // Unix timestamp of issue
    uint64_t expiryTimestamp;  // Unix timestamp of expiry (0 = perpetual)
    uint32_t maxModelGB;      // Max model size in GB
    uint32_t maxContextK;     // Max context length in K tokens
    uint8_t  reserved[8];     // Reserved for future use
};
#pragma pack(pop)

static_assert(sizeof(LicenseHeader) == 64, "LicenseHeader must be 64 bytes");

// ============================================================================
// Utility Functions
// ============================================================================

static void print_usage() {
    printf("RawrXD Key Authority — RSA-4096 License Signer\n");
    printf("================================================\n\n");
    printf("Usage:\n");
    printf("  RawrXD_KeyGen --genkey                    Generate RSA-4096 key pair\n");
    printf("  RawrXD_KeyGen --export-pub <file.inc>     Export public key as MASM .inc\n");
    printf("  RawrXD_KeyGen --export-pub-bin <file.bin> Export public key as raw binary\n");
    printf("  RawrXD_KeyGen --sign <license.bin>        Sign a raw license blob\n");
    printf("  RawrXD_KeyGen --issue [options]            Generate + sign a full license\n");
    printf("  RawrXD_KeyGen --hwid                      Print this machine's HWID\n");
    printf("\n");
    printf("Issue options:\n");
    printf("  --type <trial|enterprise|oem>  License type (default: enterprise)\n");
    printf("  --features <hex_mask>          Feature bitmask (default: 0xFF = all)\n");
    printf("  --hwid <hex64>                 Target HWID (default: 0 = floating)\n");
    printf("  --days <N>                     Validity in days (default: 365)\n");
    printf("  --model-gb <N>                 Max model size GB (default: 800)\n");
    printf("  --context-k <N>                Max context K tokens (default: 200)\n");
    printf("  --output <file.rawrlic>        Output file (default: license.rawrlic)\n");
    printf("\n");
}

static bool file_write(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

static std::vector<uint8_t> file_read(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(sz);
    fread(data.data(), 1, sz, f);
    fclose(f);
    return data;
}

// ============================================================================
// CryptoAPI Wrapper
// ============================================================================
class CryptoContext {
public:
    CryptoContext() : m_hProv(0), m_hKey(0) {}
    
    ~CryptoContext() {
        if (m_hKey) CryptDestroyKey(m_hKey);
        if (m_hProv) CryptReleaseContext(m_hProv, 0);
    }

    // Acquire existing container or create new one
    bool acquire(bool createNew = false) {
        if (m_hProv) return true;

        // Try to open existing container
        if (!createNew) {
            if (CryptAcquireContextA(&m_hProv, CONTAINER_NAME, NULL,
                                      PROV_RSA_AES, 0)) {
                return true;
            }
        }

        // Create new container
        if (CryptAcquireContextA(&m_hProv, CONTAINER_NAME, NULL,
                                  PROV_RSA_AES, CRYPT_NEWKEYSET)) {
            return true;
        }

        printf("[ERROR] CryptAcquireContext failed: 0x%08lX\n", GetLastError());
        return false;
    }

    bool generateKeyPair() {
        if (!m_hProv) return false;

        // Generate RSA-4096 key pair (AT_SIGNATURE)
        DWORD flags = (RSA_KEY_BITS << 16) | CRYPT_EXPORTABLE;
        if (!CryptGenKey(m_hProv, AT_SIGNATURE, flags, &m_hKey)) {
            printf("[ERROR] CryptGenKey failed: 0x%08lX\n", GetLastError());
            return false;
        }

        printf("[OK] RSA-%u key pair generated in container '%s'\n",
               RSA_KEY_BITS, CONTAINER_NAME);
        return true;
    }

    bool loadExistingKey() {
        if (!m_hProv) return false;
        if (m_hKey) return true;

        if (!CryptGetUserKey(m_hProv, AT_SIGNATURE, &m_hKey)) {
            printf("[ERROR] No signing key found. Run --genkey first.\n");
            return false;
        }
        return true;
    }

    // Export public key as PUBLICKEYBLOB
    std::vector<uint8_t> exportPublicKey() {
        if (!loadExistingKey()) return {};

        DWORD cbPubKey = 0;
        CryptExportKey(m_hKey, 0, PUBLICKEYBLOB, 0, NULL, &cbPubKey);
        
        std::vector<uint8_t> pubKey(cbPubKey);
        if (!CryptExportKey(m_hKey, 0, PUBLICKEYBLOB, 0, pubKey.data(), &cbPubKey)) {
            printf("[ERROR] CryptExportKey failed: 0x%08lX\n", GetLastError());
            return {};
        }
        pubKey.resize(cbPubKey);
        return pubKey;
    }

    // Sign data with SHA-256 + RSA-4096
    std::vector<uint8_t> signData(const void* data, size_t dataSize) {
        if (!loadExistingKey()) return {};

        // Create SHA-256 hash
        HCRYPTHASH hHash = 0;
        if (!CryptCreateHash(m_hProv, CALG_SHA_256, 0, 0, &hHash)) {
            printf("[ERROR] CryptCreateHash failed: 0x%08lX\n", GetLastError());
            return {};
        }

        if (!CryptHashData(hHash, (const BYTE*)data, (DWORD)dataSize, 0)) {
            printf("[ERROR] CryptHashData failed: 0x%08lX\n", GetLastError());
            CryptDestroyHash(hHash);
            return {};
        }

        // Sign the hash
        DWORD cbSig = 0;
        CryptSignHashA(hHash, AT_SIGNATURE, NULL, 0, NULL, &cbSig);

        std::vector<uint8_t> signature(cbSig);
        if (!CryptSignHashA(hHash, AT_SIGNATURE, NULL, 0, signature.data(), &cbSig)) {
            printf("[ERROR] CryptSignHash failed: 0x%08lX\n", GetLastError());
            CryptDestroyHash(hHash);
            return {};
        }

        CryptDestroyHash(hHash);
        signature.resize(cbSig);

        // CryptoAPI returns signature in little-endian; we keep it as-is
        // since our ASM verifier expects CryptoAPI format
        return signature;
    }

    HCRYPTPROV handle() const { return m_hProv; }

private:
    HCRYPTPROV m_hProv;
    HCRYPTKEY  m_hKey;
};

// ============================================================================
// Commands
// ============================================================================

static int cmd_genkey() {
    CryptoContext ctx;
    
    // Delete existing container first
    HCRYPTPROV hProv;
    if (CryptAcquireContextA(&hProv, CONTAINER_NAME, NULL, PROV_RSA_AES,
                              CRYPT_DELETEKEYSET)) {
        printf("[INFO] Deleted existing key container '%s'\n", CONTAINER_NAME);
    }

    if (!ctx.acquire(true)) return 1;
    if (!ctx.generateKeyPair()) return 1;

    // Export and save public key
    auto pubKey = ctx.exportPublicKey();
    if (pubKey.empty()) return 1;

    if (file_write("rawrxd_pub.bin", pubKey.data(), pubKey.size())) {
        printf("[OK] Public key saved to rawrxd_pub.bin (%zu bytes)\n", pubKey.size());
    }

    printf("[OK] Key pair is stored in Windows crypto container '%s'\n", CONTAINER_NAME);
    printf("[OK] Use --export-pub to generate MASM include file\n");
    return 0;
}

static int cmd_export_pub_inc(const char* outPath) {
    CryptoContext ctx;
    if (!ctx.acquire()) return 1;

    auto pubKey = ctx.exportPublicKey();
    if (pubKey.empty()) return 1;

    FILE* f = fopen(outPath, "w");
    if (!f) {
        printf("[ERROR] Cannot create %s\n", outPath);
        return 1;
    }

    fprintf(f, "; =============================================================================\n");
    fprintf(f, "; RawrXD RSA-4096 Public Key Blob (auto-generated by RawrXD_KeyGen)\n");
    time_t now = time(NULL);
    fprintf(f, "; Generated: %s", ctime(&now));
    fprintf(f, "; Size: %zu bytes\n", pubKey.size());
    fprintf(f, "; =============================================================================\n\n");
    fprintf(f, "RSA_PUBLIC_KEY_SIZE EQU %zu\n\n", pubKey.size());
    fprintf(f, "RSA_PUBLIC_KEY_BLOB LABEL BYTE\n");

    for (size_t i = 0; i < pubKey.size(); i += 16) {
        fprintf(f, "    DB ");
        size_t lineEnd = (i + 16 < pubKey.size()) ? i + 16 : pubKey.size();
        for (size_t j = i; j < lineEnd; j++) {
            fprintf(f, "0%02Xh", pubKey[j]);
            if (j + 1 < lineEnd) fprintf(f, ",");
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\n; End of RSA public key blob\n");
    fclose(f);

    printf("[OK] MASM public key include written to %s (%zu bytes)\n",
           outPath, pubKey.size());
    return 0;
}

static int cmd_export_pub_bin(const char* outPath) {
    CryptoContext ctx;
    if (!ctx.acquire()) return 1;

    auto pubKey = ctx.exportPublicKey();
    if (pubKey.empty()) return 1;

    if (!file_write(outPath, pubKey.data(), pubKey.size())) {
        printf("[ERROR] Cannot write %s\n", outPath);
        return 1;
    }

    printf("[OK] Raw public key blob written to %s (%zu bytes)\n",
           outPath, pubKey.size());
    return 0;
}

static int cmd_sign(const char* blobPath) {
    CryptoContext ctx;
    if (!ctx.acquire()) return 1;

    auto blob = file_read(blobPath);
    if (blob.empty()) {
        printf("[ERROR] Cannot read %s\n", blobPath);
        return 1;
    }

    auto sig = ctx.signData(blob.data(), blob.size());
    if (sig.empty()) return 1;

    // Write signature alongside blob
    std::string sigPath = std::string(blobPath) + ".sig";
    if (!file_write(sigPath.c_str(), sig.data(), sig.size())) {
        printf("[ERROR] Cannot write %s\n", sigPath.c_str());
        return 1;
    }

    printf("[OK] Signed %s (%zu bytes) → %s (%zu byte RSA-%u signature)\n",
           blobPath, blob.size(), sigPath.c_str(), sig.size(), RSA_KEY_BITS);

    // Also write combined .rawrlic (blob + sig)
    std::string licPath = std::string(blobPath);
    auto dot = licPath.rfind('.');
    if (dot != std::string::npos) licPath = licPath.substr(0, dot);
    licPath += ".rawrlic";

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), blob.begin(), blob.end());
    combined.insert(combined.end(), sig.begin(), sig.end());

    if (file_write(licPath.c_str(), combined.data(), combined.size())) {
        printf("[OK] Combined license written to %s (%zu bytes)\n",
               licPath.c_str(), combined.size());
    }

    return 0;
}

static int cmd_issue(int argc, char* argv[], int startIdx) {
    // Defaults
    uint16_t licenseType = 2;   // Enterprise
    uint64_t features    = FEAT_ALL;
    uint64_t hwid        = 0;   // Floating
    int      days        = 365;
    uint32_t maxModelGB  = 800;
    uint32_t maxContextK = 200;
    const char* output   = "license.rawrlic";

    // Parse options
    for (int i = startIdx; i < argc; i++) {
        if (strcmp(argv[i], "--type") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "trial") == 0)       licenseType = 1;
            else if (strcmp(argv[i], "enterprise") == 0) licenseType = 2;
            else if (strcmp(argv[i], "oem") == 0)    licenseType = 3;
        } else if (strcmp(argv[i], "--features") == 0 && i + 1 < argc) {
            features = strtoull(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--hwid") == 0 && i + 1 < argc) {
            hwid = strtoull(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) {
            days = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--model-gb") == 0 && i + 1 < argc) {
            maxModelGB = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--context-k") == 0 && i + 1 < argc) {
            maxContextK = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        }
    }

    // Build license header
    LicenseHeader hdr = {};
    hdr.magic           = LICENSE_MAGIC;
    hdr.version         = LICENSE_VERSION;
    hdr.licenseType     = licenseType;
    hdr.featureMask     = features;
    hdr.hwid            = hwid;
    hdr.issuedTimestamp  = (uint64_t)time(NULL);
    hdr.expiryTimestamp  = (days > 0) ? hdr.issuedTimestamp + (uint64_t)(days * 86400) : 0;
    hdr.maxModelGB      = maxModelGB;
    hdr.maxContextK     = maxContextK;
    memset(hdr.reserved, 0, sizeof(hdr.reserved));

    printf("[INFO] License details:\n");
    printf("  Type:      %s\n", 
           licenseType == 1 ? "Trial" : licenseType == 2 ? "Enterprise" : "OEM");
    printf("  Features:  0x%016llX\n", (unsigned long long)features);
    printf("  HWID:      0x%016llX%s\n", (unsigned long long)hwid,
           hwid == 0 ? " (floating)" : " (locked)");
    printf("  Issued:    %s", ctime((time_t*)&hdr.issuedTimestamp));
    if (hdr.expiryTimestamp) {
        printf("  Expires:   %s", ctime((time_t*)&hdr.expiryTimestamp));
    } else {
        printf("  Expires:   NEVER (perpetual)\n");
    }
    printf("  Max Model: %u GB\n", maxModelGB);
    printf("  Max Ctx:   %u K tokens\n", maxContextK);

    // Sign the license blob
    CryptoContext ctx;
    if (!ctx.acquire()) {
        printf("[ERROR] No key container. Run --genkey first.\n");
        return 1;
    }

    auto sig = ctx.signData(&hdr, sizeof(hdr));
    if (sig.empty()) return 1;

    // Pad/truncate signature to exactly RSA_SIG_BYTES
    sig.resize(RSA_SIG_BYTES, 0);

    // Write combined .rawrlic file (header + signature)
    std::vector<uint8_t> combined;
    combined.resize(sizeof(hdr));
    memcpy(combined.data(), &hdr, sizeof(hdr));
    combined.insert(combined.end(), sig.begin(), sig.end());

    if (!file_write(output, combined.data(), combined.size())) {
        printf("[ERROR] Cannot write %s\n", output);
        return 1;
    }

    printf("[OK] License written to %s (%zu bytes = %zu blob + %zu sig)\n",
           output, combined.size(), sizeof(hdr), sig.size());

    // Also write a .reg file for direct registry import
    std::string regPath = std::string(output);
    auto dot = regPath.rfind('.');
    if (dot != std::string::npos) regPath = regPath.substr(0, dot);
    regPath += ".reg";

    FILE* regFile = fopen(regPath.c_str(), "w");
    if (regFile) {
        fprintf(regFile, "Windows Registry Editor Version 5.00\n\n");
        fprintf(regFile, "[HKEY_CURRENT_USER\\SOFTWARE\\RawrXD\\Enterprise]\n");
        
        // Write blob as hex
        fprintf(regFile, "\"LicenseBlob\"=hex:");
        for (size_t i = 0; i < sizeof(hdr); i++) {
            fprintf(regFile, "%02x", ((uint8_t*)&hdr)[i]);
            if (i + 1 < sizeof(hdr)) fprintf(regFile, ",");
            if ((i + 1) % 20 == 0 && i + 1 < sizeof(hdr)) fprintf(regFile, "\\\n  ");
        }
        fprintf(regFile, "\n");

        // Write signature as hex
        fprintf(regFile, "\"LicenseSignature\"=hex:");
        for (size_t i = 0; i < sig.size(); i++) {
            fprintf(regFile, "%02x", sig[i]);
            if (i + 1 < sig.size()) fprintf(regFile, ",");
            if ((i + 1) % 20 == 0 && i + 1 < sig.size()) fprintf(regFile, "\\\n  ");
        }
        fprintf(regFile, "\n");

        fprintf(regFile, "\"LicenseType\"=dword:%08x\n", licenseType);
        fprintf(regFile, "\"Version\"=dword:%08x\n", LICENSE_VERSION);

        fclose(regFile);
        printf("[OK] Registry file written to %s\n", regPath.c_str());
    }

    return 0;
}

static int cmd_hwid() {
    // Use CPUID + volume serial (simplified — matches Shield_GenerateHWID logic)
    uint32_t cpuid_ebx, cpuid_ecx, cpuid_edx;
    uint32_t cpuid1_eax, cpuid1_ebx;

    // CPUID leaf 0
    int regs[4];
    __cpuid(regs, 0);
    cpuid_ebx = regs[1];
    cpuid_ecx = regs[2];
    cpuid_edx = regs[3];

    // CPUID leaf 1
    __cpuid(regs, 1);
    cpuid1_eax = regs[0];
    cpuid1_ebx = regs[1];

    uint64_t hwid = 0;
    hwid ^= (uint64_t)cpuid_ebx;
    hwid = _rotl64(hwid, 13);
    hwid ^= (uint64_t)cpuid_ecx;
    hwid = _rotl64(hwid, 13);
    hwid ^= (uint64_t)cpuid_edx;
    hwid = _rotl64(hwid, 7);
    hwid ^= (uint64_t)cpuid1_eax;
    hwid = _rotl64(hwid, 11);
    hwid ^= (uint64_t)cpuid1_ebx;

    // Volume serial
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) {
        hwid ^= (uint64_t)serial;
        hwid = _rotl64(hwid, 17);
    }

    // Murmur3-style avalanche
    hwid ^= hwid >> 33;
    hwid *= 0xFF51AFD7ED558CCDull;
    hwid ^= hwid >> 33;
    hwid *= 0xC4CEB9FE1A85EC53ull;
    hwid ^= hwid >> 33;

    printf("Hardware ID (HWID): 0x%016llX\n", (unsigned long long)hwid);
    printf("\nUse with: RawrXD_KeyGen --issue --hwid %016llX\n",
           (unsigned long long)hwid);
    return 0;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "--genkey") == 0) {
        return cmd_genkey();
    }
    else if (strcmp(cmd, "--export-pub") == 0) {
        if (argc < 3) {
            printf("[ERROR] --export-pub requires output path\n");
            return 1;
        }
        return cmd_export_pub_inc(argv[2]);
    }
    else if (strcmp(cmd, "--export-pub-bin") == 0) {
        if (argc < 3) {
            printf("[ERROR] --export-pub-bin requires output path\n");
            return 1;
        }
        return cmd_export_pub_bin(argv[2]);
    }
    else if (strcmp(cmd, "--sign") == 0) {
        if (argc < 3) {
            printf("[ERROR] --sign requires input file path\n");
            return 1;
        }
        return cmd_sign(argv[2]);
    }
    else if (strcmp(cmd, "--issue") == 0) {
        return cmd_issue(argc, argv, 2);
    }
    else if (strcmp(cmd, "--hwid") == 0) {
        return cmd_hwid();
    }
    else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }
    else {
        printf("[ERROR] Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
