// ============================================================================
// license_creator.cpp — CLI License Key Generator/Validator (V2)
// ============================================================================
// Standalone CLI tool for the 55+ feature Enterprise License V2 system.
// 
// Commands:
//   --create <tier> [--days N] [--secret S] [--output file]
//   --validate <file>
//   --status
//   --hwid
//   --dev-unlock
//   --list-features [--tier T]
//   --audit
//   --manifest
//
// PATTERN:   No exceptions. Returns exit codes.
// BUILD:     cmake --build . --target RawrXD-LicenseCreatorV2
// ============================================================================

#include "enterprise_license.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace RawrXD::License;

// ============================================================================
// Helper: Print formatted feature table
// ============================================================================
static void printFeatureTable(LicenseTierV2 filterTier, bool showAll) {
    printf("\n%-4s %-30s %-14s %-5s %-5s %-5s %s\n",
           "ID", "Feature Name", "Min Tier", "Impl", "UI", "Test", "Source");
    printf("%-4s %-30s %-14s %-5s %-5s %-5s %s\n",
           "----", "------------------------------", "--------------",
           "-----", "-----", "-----", "--------------------");

    auto& lic = EnterpriseLicenseV2::Instance();
    uint32_t shown = 0;

    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const FeatureDefV2& f = g_FeatureManifest[i];
        if (!showAll && f.minTier != filterTier) continue;

        bool licensed = lic.isFeatureLicensed(f.id);
        printf("%-4u %-30s %-14s %-5s %-5s %-5s %s %s\n",
               i, f.name, tierName(f.minTier),
               f.implemented ? "[Y]" : "[ ]",
               f.wiredToUI   ? "[Y]" : "[ ]",
               f.tested      ? "[Y]" : "[ ]",
               f.sourceFile,
               licensed ? "(LICENSED)" : "");
        shown++;
    }

    printf("\nTotal: %u features shown\n", shown);
}

// ============================================================================
// Helper: Print tier summary
// ============================================================================
static void printTierSummary() {
    auto& lic = EnterpriseLicenseV2::Instance();
    printf("\n=== Enterprise License V2 Summary ===\n");
    printf("Current Tier:     %s\n", tierName(lic.currentTier()));
    printf("Enabled Features: %u / %u\n", lic.enabledFeatureCount(), TOTAL_FEATURES);
    printf("Implemented:      %u / %u\n", lic.countImplemented(), TOTAL_FEATURES);
    printf("Wired to UI:      %u / %u\n", lic.countWiredToUI(), TOTAL_FEATURES);
    printf("Tested:           %u / %u\n", lic.countTested(), TOTAL_FEATURES);

    char hwidBuf[32];
    lic.getHardwareIDHex(hwidBuf, sizeof(hwidBuf));
    printf("Hardware ID:      %s\n", hwidBuf);

    const auto& limits = lic.currentLimits();
    printf("Max Model Size:   %u GB\n", limits.maxModelGB);
    printf("Max Context:      %u tokens\n", limits.maxContextTokens);
    printf("Max Concurrent:   %u models\n", limits.maxConcurrentModels);

    printf("\nTier Breakdown:\n");
    for (uint32_t t = 0; t < static_cast<uint32_t>(LicenseTierV2::COUNT); ++t) {
        LicenseTierV2 tier = static_cast<LicenseTierV2>(t);
        printf("  %-14s: %u features\n", tierName(tier), lic.countByTier(tier));
    }
}

// ============================================================================
// Helper: Parse tier from string
// ============================================================================
static LicenseTierV2 parseTier(const char* str) {
    if (!str) return LicenseTierV2::Community;
    if (_stricmp(str, "community")    == 0 || _stricmp(str, "free") == 0) return LicenseTierV2::Community;
    if (_stricmp(str, "professional") == 0 || _stricmp(str, "pro")  == 0) return LicenseTierV2::Professional;
    if (_stricmp(str, "enterprise")   == 0 || _stricmp(str, "ent")  == 0) return LicenseTierV2::Enterprise;
    if (_stricmp(str, "sovereign")    == 0 || _stricmp(str, "sov")  == 0) return LicenseTierV2::Sovereign;
    return LicenseTierV2::Community;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    printf("RawrXD Enterprise License Creator V2\n");
    printf("4-Tier System | 55+ Features\n");
    printf("========================================\n");

    auto& lic = EnterpriseLicenseV2::Instance();
    LicenseResult initResult = lic.initialize();
    if (!initResult.success) {
        fprintf(stderr, "ERROR: License init failed: %s\n", initResult.detail);
        return 1;
    }

    if (argc < 2) {
        printf("\nUsage:\n");
        printf("  %s --create <tier> [--days N] [--secret S] [--output file]\n", argv[0]);
        printf("  %s --validate <file>\n", argv[0]);
        printf("  %s --status\n", argv[0]);
        printf("  %s --hwid\n", argv[0]);
        printf("  %s --dev-unlock\n", argv[0]);
        printf("  %s --list-features [--tier community|pro|enterprise|sovereign]\n", argv[0]);
        printf("  %s --audit\n", argv[0]);
        printf("  %s --manifest\n", argv[0]);
        printf("\nTiers: community, professional (pro), enterprise (ent), sovereign (sov)\n");
        return 0;
    }

    // ── --create ──
    if (strcmp(argv[1], "--create") == 0) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: --create requires <tier>\n");
            return 1;
        }

        LicenseTierV2 tier = parseTier(argv[2]);
        uint32_t days = 365;
        const char* secret = "RawrXD-DefaultSecret-2026";
        const char* output = "rawrxd_license.key";

        for (int i = 3; i < argc - 1; ++i) {
            if (strcmp(argv[i], "--days") == 0)   days = static_cast<uint32_t>(atoi(argv[i+1]));
            if (strcmp(argv[i], "--secret") == 0) secret = argv[i+1];
            if (strcmp(argv[i], "--output") == 0) output = argv[i+1];
        }

        LicenseKeyV2 key{};
        LicenseResult r = lic.createKey(tier, days, secret, &key);
        if (!r.success) {
            fprintf(stderr, "ERROR: Key creation failed: %s\n", r.detail);
            return 1;
        }

        FILE* f = fopen(output, "wb");
        if (!f) {
            fprintf(stderr, "ERROR: Cannot write to %s\n", output);
            return 1;
        }
        fwrite(&key, sizeof(key), 1, f);
        fclose(f);

        printf("License key created:\n");
        printf("  Tier:    %s\n", tierName(tier));
        printf("  Days:    %u\n", days);
        printf("  Output:  %s\n", output);
        printf("  Size:    %zu bytes\n", sizeof(key));
        printf("  Features: %u enabled\n", TierPresets::forTier(tier).popcount());
        return 0;
    }

    // ── --validate ──
    if (strcmp(argv[1], "--validate") == 0) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: --validate requires <file>\n");
            return 1;
        }
        LicenseResult r = lic.loadKeyFromFile(argv[2]);
        printf("Validation: %s — %s\n", r.success ? "PASS" : "FAIL", r.detail);
        if (r.success) printTierSummary();
        return r.success ? 0 : 1;
    }

    // ── --status ──
    if (strcmp(argv[1], "--status") == 0) {
        printTierSummary();
        return 0;
    }

    // ── --hwid ──
    if (strcmp(argv[1], "--hwid") == 0) {
        char buf[32];
        lic.getHardwareIDHex(buf, sizeof(buf));
        printf("Hardware ID: %s\n", buf);
        return 0;
    }

    // ── --dev-unlock ──
    if (strcmp(argv[1], "--dev-unlock") == 0) {
        LicenseResult r = lic.devUnlock();
        printf("Dev Unlock: %s — %s\n", r.success ? "OK" : "FAIL", r.detail);
        if (r.success) printTierSummary();
        return r.success ? 0 : 1;
    }

    // ── --list-features ──
    if (strcmp(argv[1], "--list-features") == 0) {
        bool showAll = true;
        LicenseTierV2 filterTier = LicenseTierV2::Community;

        if (argc >= 4 && strcmp(argv[2], "--tier") == 0) {
            filterTier = parseTier(argv[3]);
            showAll = false;
        }

        printFeatureTable(filterTier, showAll);
        return 0;
    }

    // ── --audit ──
    if (strcmp(argv[1], "--audit") == 0) {
        size_t count = lic.getAuditEntryCount();
        printf("\nAudit Trail: %zu entries\n", count);
        const LicenseAuditEntry* entries = lic.getAuditEntries();
        for (size_t i = 0; i < count && i < 50; ++i) {
            printf("  [%llu] Feature=%u %s caller=%s: %s\n",
                   (unsigned long long)entries[i].timestamp,
                   static_cast<uint32_t>(entries[i].feature),
                   entries[i].granted ? "GRANTED" : "DENIED",
                   entries[i].caller, entries[i].detail);
        }
        return 0;
    }

    // ── --manifest ──
    if (strcmp(argv[1], "--manifest") == 0) {
        printf("\n=== Enterprise License V2 Feature Manifest ===\n");
        printf("Total Features: %u\n", TOTAL_FEATURES);
        printf("Implemented:    %u\n", lic.countImplemented());
        printf("Wired to UI:    %u\n", lic.countWiredToUI());
        printf("Tested:         %u\n", lic.countTested());
        printFeatureTable(LicenseTierV2::Community, true);
        return 0;
    }

    fprintf(stderr, "ERROR: Unknown command: %s\n", argv[1]);
    return 1;
}
