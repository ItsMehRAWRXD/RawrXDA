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
         << " | " << mgr.GetMaxModelSizeGB() << " GB models"
         << " | " << mgr.GetMaxContextLength() << " tokens";
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
    std::cout << "  Mask  | Feature                      | Status               | Licensed | Available\n";
    std::cout << " -------+------------------------------+----------------------+----------+----------\n";

    for (const auto& s : statuses) {
        std::cout << "  0x" << std::hex << std::setfill('0') << std::setw(2)
                  << std::uppercase << s.mask << std::dec
                  << "  | " << std::left << std::setw(29) << s.name
                  << " | " << std::left << std::setw(21) << s.statusText
                  << " | " << std::left << std::setw(9) << (s.licensed ? "YES" : "NO")
                  << " | " << (s.available ? "YES" : "NO") << "\n";
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
    auto state = lic.GetState();

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    RawrXD Enterprise License System                    ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Edition:       " << std::left << std::setw(56) << lic.GetEditionName() << " ║\n";
    std::cout << "║ Feature Mask:  " << std::left << std::setw(56) << mgr.GetFeatureMaskString() << " ║\n";
    std::cout << "║ HWID:          " << std::left << std::setw(56) << mgr.GetHWIDString() << " ║\n";
    std::cout << "║ Features:      " << std::left << std::setw(56) << lic.GetFeatureString() << " ║\n";
    std::cout << "║ Max Model:     " << std::left << std::setw(56) 
              << (std::to_string(lic.GetMaxModelSizeGB()) + " GB") << " ║\n";
    std::cout << "║ Max Context:   " << std::left << std::setw(56) 
              << (std::to_string(lic.GetMaxContextLength()) + " tokens") << " ║\n";

    const char* dualEngineStatus = lic.Is800BUnlocked()
        ? "UNLOCKED"
        : "locked (requires Enterprise)";
    std::cout << "║ 800B Dual-Engine: " << std::left << std::setw(53)
              << dualEngineStatus << " ║\n";

    std::cout << "╚════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Show community mode message if not licensed
    if (state == LicenseState::Invalid) {
        std::cout << "Running in Community mode (limited to 70GB models, 32K context).\n";
        std::cout << "Install a license or set RAWRXD_ENTERPRISE_DEV=1 for dev unlock.\n\n";
    }
}

// ============================================================================
// GUI Panel Implementation (Windows API / Cross-Platform)
// ============================================================================

#ifdef _WIN32
#include <windows.h>

class LicensePanelWindow {
public:
    static void Show() {
        // TODO: Create native Windows dialog with license status
        // For now, print to console
        PrintLicenseDashboard();
    }
};

#else

class LicensePanelWindow {
public:
    static void Show() {
        // TODO: GTK/Qt dialog for Linux
        PrintLicenseDashboard();
    }
};

#endif

void ShowLicensePanel() {
    LicensePanelWindow::Show();
}

// ============================================================================
// License Import Dialog
// ============================================================================
bool ShowImportLicenseDialog() {
    std::cout << "Enter path to .rawrlic license file: ";
    std::string path;
    std::getline(std::cin, path);

    if (path.empty()) {
        std::cout << "Import cancelled.\n";
        return false;
    }

    auto& lic = RawrXD::EnterpriseLicense::Instance();
    std::cout << "Installing license from: " << path << "\n";

    if (lic.InstallLicenseFromFile(path)) {
        std::cout << "✓ License installed successfully!\n";
        std::cout << "  Edition: " << lic.GetEditionName() << "\n";
        std::cout << "  Features: " << lic.GetFeatureString() << "\n";
        return true;
    } else {
        std::cout << "✗ License installation failed.\n";
        return false;
    }
}

// ============================================================================
// Dev Unlock UI
// ============================================================================
void ShowDevUnlockDialog() {
#ifdef _WIN32
    // Check if dev env var is set
    char envValue[64] = {};
    DWORD result = GetEnvironmentVariableA("RAWRXD_ENTERPRISE_DEV", envValue, 64);
    
    if (result == 0 || envValue[0] != '1') {
        std::cout << "Dev unlock not available (RAWRXD_ENTERPRISE_DEV not set to 1)\n";
        std::cout << "\nTo enable dev unlock:\n";
        std::cout << "  Windows: set RAWRXD_ENTERPRISE_DEV=1\n";
        std::cout << "  Linux:   export RAWRXD_ENTERPRISE_DEV=1\n";
        return;
    }
#endif

    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout <<   "║            Developer / License Creator Unlock                  ║\n";
    std::cout <<   "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout <<   "║ This will unlock ALL enterprise features for local testing.   ║\n";
    std::cout <<   "║ Use this mode when creating licenses or developing features.  ║\n";
    std::cout <<   "╚════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Activate dev unlock? (y/n): ";
    std::string response;
    std::getline(std::cin, response);

    if (response == "y" || response == "Y" || response == "yes") {
        auto result = Enterprise_DevUnlock();
        if (result != 0) {
            std::cout << "\n✓ DEV UNLOCK ACTIVE\n";
            std::cout << "All enterprise features unlocked for this session.\n\n";
            PrintEnterpriseBanner();
        } else {
            std::cout << "\n✗ Dev unlock failed (env var not set correctly)\n";
        }
    }
}

} // namespace RawrXD::UI

// ============================================================================
// Feature Denial Messages (Global Scope)
// ============================================================================
namespace RawrXD::UI {

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
       << "  1. RawrXD-LicenseCreatorV2.exe --create --tier enterprise --output license.rawrlic\n"
       << "  2. Install license.rawrlic (Tools > License > Install License)\n"
       << "  3. HWID: " << mgr.GetHWIDString() << "\n"
       << "  Or: Set RAWRXD_ENTERPRISE_DEV=1 for dev unlock";
    return ss.str();
}

} // namespace RawrXD::UI
