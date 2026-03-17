// ============================================================================
// license_creator.cpp — Enterprise License V2 Key Generator CLI
// ============================================================================
// Standalone CLI tool for creating, validating, and inspecting V2 license keys.
//
// Usage:
//   RawrXD-LicenseCreatorV2 --create --tier <community|professional|enterprise|sovereign>
//                           [--days N] [--secret S] [--output file] [--bind-machine]
//   RawrXD-LicenseCreatorV2 --validate <keyfile>
//   RawrXD-LicenseCreatorV2 --inspect <keyfile>
//   RawrXD-LicenseCreatorV2 --hwid
//   RawrXD-LicenseCreatorV2 --dev-unlock
//   RawrXD-LicenseCreatorV2 --status
//   RawrXD-LicenseCreatorV2 --list
//
// PATTERN:   No exceptions. Exit codes: 0=ok, 1=error, 2=license denied.
// THREADING: Single CLI thread.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "enterprise_license.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <iomanip>

using namespace RawrXD::License;

// ============================================================================
// Helpers
// ============================================================================

static void printUsage() {
    std::cout <<
        "RawrXD-LicenseCreatorV2 — Enterprise License V2 Key Generator\n"
        "\nUsage:\n"
        "  --create   --tier <T> [--days N] [--secret S] [--output F] [--bind-machine]\n"
        "  --validate <keyfile>    Validate a .rawrlic key file\n"
        "  --inspect  <keyfile>    Display key fields\n"
        "  --hwid                  Print hardware ID\n"
        "  --dev-unlock            Dev unlock (set RAWRXD_ENTERPRISE_DEV=1)\n"
        "  --status                Show license system status\n"
        "  --list                  List all features by tier\n"
        "\nTiers: community, professional, enterprise, sovereign\n";
}

static LicenseTierV2 parseTier(const char* s) {
    if (!s) return LicenseTierV2::Community;
    std::string t(s);
    for (auto& c : t) c = (char)tolower(c);
    if (t == "professional" || t == "pro")    return LicenseTierV2::Professional;
    if (t == "enterprise"   || t == "ent")    return LicenseTierV2::Enterprise;
    if (t == "sovereign"    || t == "sov")    return LicenseTierV2::Sovereign;
    return LicenseTierV2::Community;
}

static void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i)
        printf("%02x", data[i]);
}

// ============================================================================
// Commands
// ============================================================================

static int cmdCreate(LicenseTierV2 tier, uint32_t days, const char* secret,
                     const char* output, bool bindMachine) {
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    if (!secret) secret = "RawrXD-Dev-Secret-2026";

    LicenseKeyV2 key{};
    LicenseResult r = lic.createKey(tier, days, secret, &key);
    if (!r.success) {
        std::cerr << "Error: " << r.detail << "\n";
        return 1;
    }

    if (bindMachine) {
        key.hwid = lic.getHardwareID();
    }

    // Write key to file
    const char* outPath = output ? output : "license.rawrlic";
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "Error: Cannot write to " << outPath << "\n";
        return 1;
    }
    ofs.write(reinterpret_cast<const char*>(&key), sizeof(key));
    ofs.close();

    std::cout << "License key created successfully:\n"
              << "  Tier:    " << tierName(tier) << "\n"
              << "  Days:    " << days << "\n"
              << "  File:    " << outPath << "\n"
              << "  HWID:    ";
    char hwidBuf[20]{};
    snprintf(hwidBuf, sizeof(hwidBuf), "%016llX", (unsigned long long)key.hwid);
    std::cout << hwidBuf << "\n"
              << "  Features: " << key.features.popcount() << "/" << TOTAL_FEATURES << "\n"
              << "  Sig:     ";
    printHex(key.signature, 8);
    std::cout << "...\n";

    return 0;
}

static int cmdValidate(const char* path) {
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    LicenseResult r = lic.loadKeyFromFile(path);
    if (r.success) {
        std::cout << "License VALID\n"
                  << "  Tier: " << tierName(lic.currentTier()) << "\n"
                  << "  Features: " << lic.enabledFeatureCount() << "/" << TOTAL_FEATURES << "\n";
        return 0;
    } else {
        std::cerr << "License INVALID: " << r.detail << "\n";
        return 2;
    }
}

static int cmdInspect(const char* path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    LicenseKeyV2 key{};
    ifs.read(reinterpret_cast<char*>(&key), sizeof(key));
    if (ifs.gcount() != sizeof(key)) {
        std::cerr << "Invalid key file size (expected " << sizeof(key) << " bytes)\n";
        return 1;
    }

    printf("=== License Key Inspection ===\n");
    printf("  Magic:       0x%08X %s\n", key.magic, key.magic == 0x5258444C ? "(RXDL)" : "(INVALID)");
    printf("  Version:     %u\n", key.version);
    printf("  Tier:        %s (%u)\n", tierName(static_cast<LicenseTierV2>(key.tier)), key.tier);
    printf("  HWID:        %016llX\n", (unsigned long long)key.hwid);
    printf("  Features:    lo=0x%016llX hi=0x%016llX (%u enabled)\n",
           (unsigned long long)key.features.lo,
           (unsigned long long)key.features.hi,
           key.features.popcount());
    printf("  Issue Date:  %u\n", key.issueDate);
    printf("  Expiry Date: %u %s\n", key.expiryDate, key.expiryDate == 0 ? "(perpetual)" : "");
    printf("  Max Model:   %u GB\n", key.maxModelGB);
    printf("  Max Context: %u tokens\n", key.maxContextTokens);
    printf("  Signature:   ");
    printHex(key.signature, 32);
    printf("\n");

    return 0;
}

static int cmdHWID() {
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    char buf[20]{};
    lic.getHardwareIDHex(buf, sizeof(buf));
    std::cout << "Hardware ID: " << buf << "\n";
    return 0;
}

static int cmdDevUnlock() {
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    LicenseResult r = lic.devUnlock();
    if (r.success) {
        std::cout << "Dev unlock: SUCCESS — tier upgraded to "
                  << tierName(lic.currentTier()) << "\n"
                  << "Features: " << lic.enabledFeatureCount() << "/" << TOTAL_FEATURES << "\n";
        return 0;
    } else {
        std::cerr << "Dev unlock: " << r.detail << "\n"
                  << "Set RAWRXD_ENTERPRISE_DEV=1 to enable\n";
        return 1;
    }
}

static int cmdStatus() {
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    std::cout << "=== License System Status ===\n"
              << "  Initialized: " << (lic.isInitialized() ? "Yes" : "No") << "\n"
              << "  Tier:        " << tierName(lic.currentTier()) << "\n"
              << "  Features:    " << lic.enabledFeatureCount() << "/" << TOTAL_FEATURES << "\n"
              << "  Implemented: " << lic.countImplemented() << "/" << TOTAL_FEATURES << "\n"
              << "  UI Wired:    " << lic.countWiredToUI() << "/" << TOTAL_FEATURES << "\n"
              << "  Tested:      " << lic.countTested() << "/" << TOTAL_FEATURES << "\n"
              << "  Audit:       " << lic.getAuditEntryCount() << "/" << lic.MAX_AUDIT_ENTRIES << " entries\n";
    return 0;
}

static int cmdList() {
    std::cout << "=== Feature List by Tier ===\n\n";

    const char* tierNames[] = { "Community", "Professional", "Enterprise", "Sovereign" };
    for (uint32_t t = 0; t < 4; ++t) {
        LicenseTierV2 tier = static_cast<LicenseTierV2>(t);
        std::cout << "── " << tierNames[t] << " ──\n";

        for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
            const FeatureDefV2& def = g_FeatureManifest[i];
            if (def.minTier != tier) continue;
            printf("  [%2u] %-28s  %s  %s  %s\n",
                   i,
                   def.name,
                   def.implemented ? "[IMPL]" : "[----]",
                   def.wiredToUI   ? "[UI]"   : "[--]",
                   def.tested      ? "[TEST]" : "[----]");
        }
        std::cout << "\n";
    }
    return 0;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    LicenseTierV2 tier = LicenseTierV2::Community;
    uint32_t days = 365;
    const char* secret = nullptr;
    const char* output = nullptr;
    const char* keyfile = nullptr;
    bool bindMachine = false;

    enum class Cmd { None, Create, Validate, Inspect, HWID, DevUnlock, Status, List };
    Cmd cmd = Cmd::None;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--create") == 0)       cmd = Cmd::Create;
        else if (strcmp(argv[i], "--validate") == 0) { cmd = Cmd::Validate; if (i + 1 < argc) keyfile = argv[++i]; }
        else if (strcmp(argv[i], "--inspect") == 0)  { cmd = Cmd::Inspect;  if (i + 1 < argc) keyfile = argv[++i]; }
        else if (strcmp(argv[i], "--hwid") == 0)     cmd = Cmd::HWID;
        else if (strcmp(argv[i], "--dev-unlock") == 0) cmd = Cmd::DevUnlock;
        else if (strcmp(argv[i], "--status") == 0)   cmd = Cmd::Status;
        else if (strcmp(argv[i], "--list") == 0)     cmd = Cmd::List;
        else if (strcmp(argv[i], "--tier") == 0 && i + 1 < argc)   tier = parseTier(argv[++i]);
        else if (strcmp(argv[i], "--days") == 0 && i + 1 < argc)   days = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--secret") == 0 && i + 1 < argc) secret = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--bind-machine") == 0) bindMachine = true;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage();
            return 0;
        }
    }

    switch (cmd) {
        case Cmd::Create:    return cmdCreate(tier, days, secret, output, bindMachine);
        case Cmd::Validate:  return keyfile ? cmdValidate(keyfile) : (std::cerr << "Missing keyfile\n", 1);
        case Cmd::Inspect:   return keyfile ? cmdInspect(keyfile)  : (std::cerr << "Missing keyfile\n", 1);
        case Cmd::HWID:      return cmdHWID();
        case Cmd::DevUnlock: return cmdDevUnlock();
        case Cmd::Status:    return cmdStatus();
        case Cmd::List:      return cmdList();
        default:             printUsage(); return 1;
    }
}
