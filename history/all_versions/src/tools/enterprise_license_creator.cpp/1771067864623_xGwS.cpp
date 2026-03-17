// ============================================================================
// enterprise_license_creator.cpp — Standalone Enterprise License Creator Tool
// ============================================================================
// CLI tool for creating, validating, and managing RawrXD Enterprise licenses.
// Works alongside RawrXD_KeyGen.exe and the Python license tools.
//
// Usage:
//   RawrXD_LicenseCreator --create --tier enterprise --days 365
//   RawrXD_LicenseCreator --create --tier trial --days 30
//   RawrXD_LicenseCreator --create --tier pro --days 180 --features 0x4A
//   RawrXD_LicenseCreator --validate license.rawrlic
//   RawrXD_LicenseCreator --status
//   RawrXD_LicenseCreator --hwid
//   RawrXD_LicenseCreator --dev-unlock
//   RawrXD_LicenseCreator --list-features
//   RawrXD_LicenseCreator --audit
//
// This tool can generate license files directly using the built-in
// MurmurHash3 validation system (no RSA keys required for dev/trial).
// For production RSA-4096 signed licenses, use RawrXD_KeyGen.exe.
// ============================================================================

#include "core/enterprise_license.h"
#include "enterprise_feature_manager.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// ============================================================================
// License Header Structure (must match RawrXD_KeyGen.cpp / .asm)
// ============================================================================
#pragma pack(push, 1)
struct LicenseHeader {
    uint32_t magic;         // 0x4C445852 = "RXDL"
    uint16_t version;       // 0x0200 = v2.0
    uint16_t headerSize;    // sizeof(LicenseHeader) = 64
    uint64_t hwid;          // Hardware fingerprint
    uint64_t features;      // Feature bitmask (0x00–0xFF)
    uint32_t issueDate;     // Unix timestamp
    uint32_t expiryDate;    // Unix timestamp (0 = perpetual)
    uint8_t  tier;          // 0=Community, 1=Trial, 2=Pro, 3=Enterprise
    uint8_t  reserved[23];  // Padding to 64 bytes
};
#pragma pack(pop)

static_assert(sizeof(LicenseHeader) == 64, "LicenseHeader must be 64 bytes");

// ============================================================================
// Feature table (matches enterprise_feature_manager.hpp)
// ============================================================================
struct FeatureInfo {
    uint64_t    mask;
    const char* name;
    const char* tier;
};

static const FeatureInfo g_features[] = {
    { 0x01, "800B Dual-Engine",     "Enterprise" },
    { 0x02, "AVX-512 Premium",      "Pro" },
    { 0x04, "Distributed Swarm",    "Enterprise" },
    { 0x08, "GPU Quant 4-bit",      "Pro" },
    { 0x10, "Enterprise Support",   "Enterprise" },
    { 0x20, "Unlimited Context",    "Enterprise" },
    { 0x40, "Flash Attention",      "Pro" },
    { 0x80, "Multi-GPU",            "Enterprise" },
};

// ============================================================================
// Tier Feature Presets
// ============================================================================
static uint64_t getTierFeatures(const std::string& tier) {
    if (tier == "community")  return 0x00;
    if (tier == "trial")      return 0xFF;   // All features for trial
    if (tier == "pro")        return 0x4A;   // AVX-512 + GPU Quant + Flash Attention
    if (tier == "enterprise") return 0xFF;   // All features
    if (tier == "oem")        return 0xFF;   // All features
    return 0x00;
}

static uint8_t getTierByte(const std::string& tier) {
    if (tier == "community")  return 0;
    if (tier == "trial")      return 1;
    if (tier == "pro")        return 2;
    if (tier == "enterprise") return 3;
    if (tier == "oem")        return 4;
    return 0;
}

// ============================================================================
// Create License (MurmurHash3-based for dev/trial use)
// ============================================================================
static bool createLicense(const std::string& tier, int days, uint64_t featureOverride,
                           const std::string& outputPath) {
    // Initialize license system to get HWID
    RawrXD::EnterpriseLicense::Instance().Initialize();

    LicenseHeader hdr{};
    hdr.magic      = 0x4C445852;  // "RXDL"
    hdr.version    = 0x0200;      // v2.0
    hdr.headerSize = sizeof(LicenseHeader);
    hdr.hwid       = RawrXD::EnterpriseLicense::Instance().GetHardwareHash();
    hdr.features   = (featureOverride != 0) ? featureOverride : getTierFeatures(tier);
    hdr.issueDate  = static_cast<uint32_t>(time(nullptr));
    hdr.expiryDate = (days == 0) ? 0 : hdr.issueDate + (days * 86400);
    hdr.tier       = getTierByte(tier);
    memset(hdr.reserved, 0, sizeof(hdr.reserved));

    // Write header as blob
    std::string outFile = outputPath.empty() ? "license.rawrlic" : outputPath;
    std::ofstream f(outFile, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot create output file: " << outFile << "\n";
        return false;
    }

    // Write header blob
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Write 512-byte dummy signature (for format compatibility)
    // Real RSA-4096 signing requires RawrXD_KeyGen.exe with --sign
    uint8_t dummySig[512] = {};
    // Fill with a deterministic pattern for dev licenses
    for (int i = 0; i < 512; i++) {
        dummySig[i] = static_cast<uint8_t>((hdr.hwid >> (i % 8 * 8)) ^ i);
    }
    f.write(reinterpret_cast<const char*>(dummySig), sizeof(dummySig));
    f.close();

    std::cout << "\n=== License Created ===\n";
    std::cout << "  File:     " << outFile << "\n";
    std::cout << "  Tier:     " << tier << "\n";
    std::cout << "  HWID:     0x" << std::hex << std::uppercase << hdr.hwid << std::dec << "\n";
    std::cout << "  Features: 0x" << std::hex << std::uppercase << hdr.features << std::dec << "\n";
    std::cout << "  Issued:   " << hdr.issueDate << "\n";
    std::cout << "  Expires:  " << (hdr.expiryDate == 0 ? "PERPETUAL" : std::to_string(hdr.expiryDate)) << "\n";
    std::cout << "  Size:     " << (sizeof(hdr) + 512) << " bytes\n";
    std::cout << "\nEnabled features:\n";
    for (const auto& feat : g_features) {
        bool enabled = (hdr.features & feat.mask) != 0;
        std::cout << "  " << (enabled ? "[+]" : "[ ]") << " " << feat.name
                  << " (0x" << std::hex << feat.mask << std::dec << ")\n";
    }
    std::cout << "\nNote: This is a dev/trial license (MurmurHash3 validation).\n"
              << "For production RSA-4096 signed licenses, use RawrXD_KeyGen.exe --issue\n";

    return true;
}

// ============================================================================
// Validate License File
// ============================================================================
static bool validateLicense(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open: " << path << "\n";
        return false;
    }

    auto size = f.tellg();
    if (size < static_cast<std::streamoff>(sizeof(LicenseHeader) + 512)) {
        std::cerr << "[ERROR] File too small (" << size << " bytes, need >= "
                  << (sizeof(LicenseHeader) + 512) << ")\n";
        return false;
    }

    f.seekg(0);
    LicenseHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    f.close();

    std::cout << "\n=== License Validation ===\n";
    std::cout << "  File:      " << path << "\n";
    std::cout << "  Size:      " << size << " bytes\n";
    std::cout << "  Magic:     0x" << std::hex << hdr.magic << std::dec;
    if (hdr.magic == 0x4C445852) std::cout << " (RXDL - valid)";
    else if (hdr.magic == 0x52415752) std::cout << " (RAWR - v1 format)";
    else std::cout << " (UNKNOWN - invalid)";
    std::cout << "\n";
    std::cout << "  Version:   0x" << std::hex << hdr.version << std::dec << "\n";
    std::cout << "  HWID:      0x" << std::hex << std::uppercase << hdr.hwid << std::dec << "\n";
    std::cout << "  Features:  0x" << std::hex << std::uppercase << hdr.features << std::dec << "\n";
    
    const char* tiers[] = { "Community", "Trial", "Pro", "Enterprise", "OEM" };
    std::cout << "  Tier:      " << (hdr.tier < 5 ? tiers[hdr.tier] : "Unknown") << "\n";

    time_t issue = hdr.issueDate;
    time_t expiry = hdr.expiryDate;
    std::cout << "  Issued:    " << ctime(&issue);
    if (expiry == 0) {
        std::cout << "  Expires:   PERPETUAL\n";
    } else {
        std::cout << "  Expires:   " << ctime(&expiry);
        if (expiry < time(nullptr)) {
            std::cout << "  ** LICENSE EXPIRED **\n";
        } else {
            int daysLeft = static_cast<int>((expiry - time(nullptr)) / 86400);
            std::cout << "  Days left: " << daysLeft << "\n";
        }
    }

    // Check HWID match
    RawrXD::EnterpriseLicense::Instance().Initialize();
    uint64_t localHwid = RawrXD::EnterpriseLicense::Instance().GetHardwareHash();
    std::cout << "  Local HWID: 0x" << std::hex << std::uppercase << localHwid << std::dec << "\n";
    if (hdr.hwid == localHwid) {
        std::cout << "  HWID Match: YES\n";
    } else {
        std::cout << "  HWID Match: NO (license was issued for different hardware)\n";
    }

    std::cout << "\nFeatures:\n";
    for (const auto& feat : g_features) {
        bool enabled = (hdr.features & feat.mask) != 0;
        std::cout << "  " << (enabled ? "[ENABLED] " : "[       ] ") << feat.name
                  << " (" << feat.tier << ")\n";
    }

    return (hdr.magic == 0x4C445852 || hdr.magic == 0x52415752);
}

// ============================================================================
// Print usage
// ============================================================================
static void printUsage() {
    std::cout << R"(
RawrXD Enterprise License Creator — v2.0

Usage:
  RawrXD_LicenseCreator <command> [options]

Commands:
  --create              Create a new license file
    --tier <tier>       License tier: community|trial|pro|enterprise|oem (default: trial)
    --days <n>          Validity period in days (0 = perpetual, default: 30)
    --features <mask>   Feature bitmask override (hex, e.g. 0xFF)
    --output <path>     Output file path (default: license.rawrlic)

  --validate <file>     Validate and inspect a .rawrlic file

  --status              Show current license status

  --hwid                Show hardware ID for this machine

  --dev-unlock          Dev unlock (requires RAWRXD_ENTERPRISE_DEV=1)

  --list-features       List all enterprise features with descriptions

  --audit               Run full enterprise feature audit

Feature Bitmask Reference:
  0x01  800B Dual-Engine       Multi-shard 800B model inference
  0x02  AVX-512 Premium        Hardware-accelerated quant kernels
  0x04  Distributed Swarm      Multi-node swarm inference
  0x08  GPU Quant 4-bit        GPU-accelerated Q4 quantization
  0x10  Enterprise Support     Priority support tier
  0x20  Unlimited Context      200K token context window
  0x40  Flash Attention        AVX-512 flash attention kernels
  0x80  Multi-GPU              Multi-GPU inference distribution

Tier Presets:
  community  0x00  (free)
  trial      0xFF  (all features, 30 days)
  pro        0x4A  (AVX-512 + GPU Quant + Flash Attention)
  enterprise 0xFF  (all features)
  oem        0xFF  (all features, custom branding)

Examples:
  RawrXD_LicenseCreator --create --tier enterprise --days 365
  RawrXD_LicenseCreator --create --tier trial --days 30 --output my_trial.rawrlic
  RawrXD_LicenseCreator --create --tier pro --features 0xFF --days 0
  RawrXD_LicenseCreator --validate license.rawrlic
  RawrXD_LicenseCreator --audit
)" << "\n";
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 0;
    }

    // Parse arguments
    std::string command;
    std::string tier = "trial";
    int days = 30;
    uint64_t features = 0;
    std::string output;
    std::string validatePath;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--create")         command = "create";
        else if (arg == "--validate")  { command = "validate"; if (i + 1 < argc) validatePath = argv[++i]; }
        else if (arg == "--status")    command = "status";
        else if (arg == "--hwid")      command = "hwid";
        else if (arg == "--dev-unlock") command = "devunlock";
        else if (arg == "--list-features") command = "features";
        else if (arg == "--audit")     command = "audit";
        else if (arg == "--help")      { printUsage(); return 0; }
        else if (arg == "--tier" && i + 1 < argc)     tier = argv[++i];
        else if (arg == "--days" && i + 1 < argc)     days = std::stoi(argv[++i]);
        else if (arg == "--features" && i + 1 < argc) features = std::stoull(argv[++i], nullptr, 0);
        else if (arg == "--output" && i + 1 < argc)   output = argv[++i];
    }

    if (command.empty()) {
        printUsage();
        return 1;
    }

    // Execute command
    if (command == "create") {
        return createLicense(tier, days, features, output) ? 0 : 1;
    }
    else if (command == "validate") {
        if (validatePath.empty()) {
            std::cerr << "[ERROR] --validate requires a file path\n";
            return 1;
        }
        return validateLicense(validatePath) ? 0 : 1;
    }
    else if (command == "status") {
        auto& lic = RawrXD::EnterpriseLicense::Instance();
        lic.Initialize();
        auto& mgr = EnterpriseFeatureManager::Instance();
        mgr.Initialize();

        std::cout << mgr.GenerateDashboard();
        return 0;
    }
    else if (command == "hwid") {
        auto& lic = RawrXD::EnterpriseLicense::Instance();
        lic.Initialize();
        std::cout << "HWID: 0x" << std::hex << std::uppercase
                  << lic.GetHardwareHash() << std::dec << "\n";
        return 0;
    }
    else if (command == "devunlock") {
        auto& lic = RawrXD::EnterpriseLicense::Instance();
        lic.Initialize();
        auto& mgr = EnterpriseFeatureManager::Instance();
        mgr.Initialize();

        if (mgr.DevUnlock()) {
            std::cout << "Dev unlock succeeded!\n";
            std::cout << mgr.GenerateDashboard();
        } else {
            std::cout << "Dev unlock failed. Set RAWRXD_ENTERPRISE_DEV=1\n";
        }
        return 0;
    }
    else if (command == "features") {
        std::cout << "\nRawrXD Enterprise Features:\n\n";
        std::cout << "  Mask  | Feature                  | Min Tier\n";
        std::cout << " -------+--------------------------+-----------\n";
        for (const auto& f : g_features) {
            std::cout << "  0x" << std::hex << std::setw(2) << std::setfill('0')
                      << std::uppercase << f.mask << std::dec << "  | "
                      << std::left << std::setw(25) << f.name
                      << "| " << f.tier << "\n";
        }
        std::cout << "\nTier presets:\n";
        std::cout << "  Community:   0x00 (free, 70B models, 32K context)\n";
        std::cout << "  Trial:       0xFF (all features, 30 days, 180B models)\n";
        std::cout << "  Pro:         0x4A (AVX-512 + GPU Quant + Flash Attn)\n";
        std::cout << "  Enterprise:  0xFF (all features, 800B models, 200K context)\n";
        return 0;
    }
    else if (command == "audit") {
        auto& lic = RawrXD::EnterpriseLicense::Instance();
        lic.Initialize();
        auto& mgr = EnterpriseFeatureManager::Instance();
        mgr.Initialize();

        std::cout << mgr.GenerateAuditReport();
        return 0;
    }

    printUsage();
    return 1;
}
