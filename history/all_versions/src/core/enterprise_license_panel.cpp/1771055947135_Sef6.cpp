// ============================================================================
// enterprise_license_panel.cpp — Enterprise License Panel Implementation
// ============================================================================
// Console/REPL display functions for enterprise license status.
// Provides formatted output for dashboard, audit, and feature lists.
// ============================================================================

#include "enterprise_license_panel.hpp"
#include "enterprise_feature_manager.hpp"
#include "enterprise_license.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace RawrXD::UI {

// ============================================================================
// Status Bar Badge
// ============================================================================
const char* GetLicenseTierBadge() {
    auto tier = EnterpriseFeatureManager::Instance().GetCurrentTier();
    switch (tier) {
        case LicenseTier::Enterprise: return "ENT";
        case LicenseTier::OEM:        return "OEM";
        case LicenseTier::Pro:        return "PRO";
        case LicenseTier::Trial:      return "TRIAL";
        default:                       return "FREE";
    }
}

// ============================================================================
// Status Tooltip
// ============================================================================
std::string GetLicenseStatusTooltip() {
    auto& mgr = EnterpriseFeatureManager::Instance();
    auto statuses = mgr.GetFeatureStatuses();
    
    int active = 0;
    for (const auto& s : statuses) {
        if (s.active) active++;
    }

    std::ostringstream ss;
    ss << mgr.GetEditionName()
       << " | " << active << "/" << statuses.size() << " features"
       << " | " << mgr.GetMaxModelSizeGB() << "B models"
       << " | " << mgr.GetMaxContextLength() << " context";
    return ss.str();
}

// ============================================================================
// Console Dashboard
// ============================================================================
void PrintLicenseDashboard() {
    std::cout << EnterpriseFeatureManager::Instance().GenerateDashboard();
}

// ============================================================================
// Console Audit
// ============================================================================
void PrintLicenseAudit() {
    std::cout << EnterpriseFeatureManager::Instance().GenerateAuditReport();
}

// ============================================================================
// Feature List
// ============================================================================
void PrintFeatureList() {
    auto statuses = EnterpriseFeatureManager::Instance().GetFeatureStatuses();
    auto& mgr = EnterpriseFeatureManager::Instance();

    std::cout << "\nRawrXD Enterprise Features (" << mgr.GetEditionName() << "):\n\n";
    std::cout << "  Mask  | Feature                    | Status              | Licensed | Available\n";
    std::cout << " -------+----------------------------+---------------------+----------+----------\n";

    for (const auto& s : statuses) {
        std::cout << "  0x" << std::hex << std::setfill('0') << std::setw(2)
                  << std::uppercase << s.mask << std::dec
                  << "  | " << std::left << std::setw(27) << s.name
                  << "| " << std::left << std::setw(20) << s.statusText
                  << "| " << std::left << std::setw(9) << (s.licensed ? "YES" : "NO")
                  << "| " << (s.available ? "YES" : "NO") << "\n";
    }

    int active = 0, locked = 0;
    for (const auto& s : statuses) {
        if (s.active) active++; else locked++;
    }

    std::cout << "\n  Total: " << statuses.size() << " features | "
              << active << " active | " << locked << " locked\n";
    std::cout << "  Max Model: " << mgr.GetMaxModelSizeGB() << "GB"
              << " | Max Context: " << mgr.GetMaxContextLength() << " tokens\n\n";
}

// ============================================================================
// Startup Banner
// ============================================================================
void PrintEnterpriseBanner() {
    auto& lic = RawrXD::EnterpriseLicense::Instance();
    auto& mgr = EnterpriseFeatureManager::Instance();

    std::cout << "  License:       " << lic.GetEditionName() << "\n";
    std::cout << "  HWID:          " << mgr.GetHWIDString() << "\n";
    std::cout << "  Feature Mask:  " << mgr.GetFeatureMaskString() << "\n";
    std::cout << "  Model Limit:   " << lic.GetMaxModelSizeGB() << "GB\n";
    std::cout << "  Context Limit: " << lic.GetMaxContextLength() << " tokens\n";
    std::cout << "  800B Engine:   "
              << (lic.Is800BUnlocked() ? "UNLOCKED" : "locked (requires Enterprise license)")
              << "\n";

    // Show locked features as hints
    auto statuses = mgr.GetFeatureStatuses();
    bool hasLocked = false;
    for (const auto& s : statuses) {
        if (!s.active && s.available) {
            if (!hasLocked) {
                std::cout << "  Upgradeable:   ";
                hasLocked = true;
            } else {
                std::cout << "                 ";
            }
            std::cout << s.name << " (0x" << std::hex << s.mask << std::dec << ")\n";
        }
    }

    if (hasLocked) {
        std::cout << "  Upgrade:       RawrXD_KeyGen.exe --issue | Tools > License Creator\n";
    }
}

// ============================================================================
// Feature Denial Messages
// ============================================================================
std::string GetFeatureDenialMessage(uint64_t featureMask) {
    auto& mgr = EnterpriseFeatureManager::Instance();
    const auto& defs = mgr.GetFeatureDefinitions();

    const char* featName = "Unknown Feature";
    const char* phase = "Unknown";
    for (const auto& def : defs) {
        if (def.mask == featureMask) {
            featName = def.name;
            phase = def.phase;
            break;
        }
    }

    std::ostringstream ss;
    ss << featName << " requires Enterprise license. "
       << "Current edition: " << mgr.GetEditionName() << ". "
       << "Feature bitmask: 0x" << std::hex << featureMask << std::dec
       << " (" << phase << ")";
    return ss.str();
}

std::string GetUpgradePrompt(uint64_t featureMask) {
    auto& mgr = EnterpriseFeatureManager::Instance();
    const auto& defs = mgr.GetFeatureDefinitions();

    const char* featName = "Unknown Feature";
    for (const auto& def : defs) {
        if (def.mask == featureMask) {
            featName = def.name;
            break;
        }
    }

    std::ostringstream ss;
    ss << "To unlock " << featName << ":\n"
       << "  1. RawrXD_KeyGen.exe --genkey (generate key authority)\n"
       << "  2. RawrXD_KeyGen.exe --issue --features 0x"
       << std::hex << featureMask << std::dec << " --hwid " << mgr.GetHWIDString() << "\n"
       << "  3. Tools > License Creator > Install License\n"
       << "  Or: Set RAWRXD_ENTERPRISE_DEV=1 for dev unlock";
    return ss.str();
}

} // namespace RawrXD::UI
