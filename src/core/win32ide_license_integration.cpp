// ============================================================================
// win32ide_license_integration.cpp — License Manager UI IDE Integration
// ============================================================================

#include "../include/license_manager_panel.h"
#include "../include/enterprise_license.h"
#include "../include/license_audit_trail.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

namespace RawrXD::License {

// ============================================================================
// IDE Menu Integration
// ============================================================================

class LicenseManagerIntegration {
public:
    static const int MENU_ID_LICENSE_MANAGER = 50001;
    static const int MENU_ID_LICENSE_INFO = 50002;
    static const int MENU_ID_LICENSE_ACTIVATE = 50003;
    static const int MENU_ID_LICENSE_AUDIT = 50004;
    static const int MENU_ID_LICENSE_SETTINGS = 50005;

    static HWND s_licensePanel;
    static HWND s_ideWindow;

    // Initialize IDE integration
    static bool initializeIDEIntegration(HWND ideWindow) {
        if (!ideWindow) return false;

        s_ideWindow = ideWindow;

        // Add menu items to IDE menu bar
        HMENU hMenuBar = GetMenu(ideWindow);
        if (!hMenuBar) return false;

        // Create "License" menu
        HMENU hLicenseMenu = CreatePopupMenu();
        if (!hLicenseMenu) return false;

        AppendMenu(hLicenseMenu, MFT_STRING, MENU_ID_LICENSE_INFO, 
                   "License Information...");
        AppendMenu(hLicenseMenu, MFT_STRING, MENU_ID_LICENSE_ACTIVATE, 
                   "Activate License...");
        AppendMenu(hLicenseMenu, MFT_SEPARATOR, 0, nullptr);
        AppendMenu(hLicenseMenu, MFT_STRING, MENU_ID_LICENSE_AUDIT, 
                   "Audit Trail...");
        AppendMenu(hLicenseMenu, MFT_STRING, MENU_ID_LICENSE_SETTINGS, 
                   "License Settings...");

        // Insert into menu bar (before Help)
        InsertMenu(hMenuBar, GetMenuItemCount(hMenuBar), MF_BYPOSITION | MF_POPUP, 
                   (UINT_PTR)hLicenseMenu, "License");

        // Redraw menu
        DrawMenuBar(ideWindow);

        return true;
    }

    // Handle menu command
    static bool handleMenuCommand(int menuID) {
        switch (menuID) {
            case MENU_ID_LICENSE_INFO:
                onLicenseInfo();
                return true;

            case MENU_ID_LICENSE_ACTIVATE:
                onActivateLicense();
                return true;

            case MENU_ID_LICENSE_AUDIT:
                onViewAuditTrail();
                return true;

            case MENU_ID_LICENSE_SETTINGS:
                onLicenseSettings();
                return true;

            default:
                return false;
        }
    }

private:
    // Show license information dialog
    static void onLicenseInfo() {
        auto& lic = EnterpriseLicenseV2::Instance();

        std::string info = "╔════════════════════════════════════════╗\n";
        info += "║        License Information              ║\n";
        info += "╚════════════════════════════════════════╝\n\n";

        info += "Current Tier: ";
        switch (lic.currentTier()) {
            case LicenseTierV2::Community:    info += "Community"; break;
            case LicenseTierV2::Professional: info += "Professional"; break;
            case LicenseTierV2::Enterprise:   info += "Enterprise"; break;
            case LicenseTierV2::Sovereign:    info += "Sovereign"; break;
            default:                          info += "Unknown"; break;
        }
        info += "\n\n";

        info += "Enabled Features: ";
        info += std::to_string(lic.enabledFeatureCount());
        info += " / 61\n\n";

        info += "License Status: ";
        if (lic.currentTier() == LicenseTierV2::Community) {
            info += "Community (Limited)\n";
        } else {
            info += "Active\n";
        }
        info += "\n";

        // Extract HWID
        char hwid_hex[33] = {};
        lic.getHardwareIDHex(hwid_hex, sizeof(hwid_hex));
        info += "Hardware ID: ";
        info += hwid_hex;
        info += "\n";

        // Show message box
        MessageBoxA(s_ideWindow, info.c_str(), "License Information", 
                    MB_OK | MB_ICONINFORMATION);
    }

    // Show license activation dialog
    static void onActivateLicense() {
        // Open file picker dialog
        OPENFILENAMEA ofn = {};
        char szFile[260] = {};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = s_ideWindow;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "License Files (*.lic)\0*.lic\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = "Select License File";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn)) {
            // Load license from file
            std::string message = "Activating license from:\n";
            message += szFile;
            message += "\n\nThis feature requires backend integration.\n";
            message += "License file has been selected.";

            MessageBoxA(s_ideWindow, message.c_str(), "License Activation", 
                        MB_OK | MB_ICONINFORMATION);
        }
    }

    // Show audit trail viewer
    static void onViewAuditTrail() {
        std::string message = "╔════════════════════════════════════════╗\n";
        message += "║        License Audit Trail              ║\n";
        message += "╚════════════════════════════════════════╝\n\n";

        auto& auditMgr = g_auditTrailManager;
        message += "Total Events: " + std::to_string(auditMgr.getTotalEvents()) + "\n";
        message += "Grants: " + std::to_string(auditMgr.getTotalGrants()) + "\n";
        message += "Denials: " + std::to_string(auditMgr.getTotalDenials()) + "\n";
        message += "Denial Rate: ";

        char buf[32];
        sprintf_s(buf, sizeof(buf), "%.2f%%\n", auditMgr.getDenialRate() * 100.0f);
        message += buf;

        message += "Anomalies Detected: " + std::to_string(auditMgr.getAnomalyCount()) + "\n";
        message += "Status: " + std::string(auditMgr.isInAnomalousState() ? 
                                            "ALERT - Anomalous State" : "Normal") + "\n";

        MessageBoxA(s_ideWindow, message.c_str(), "Audit Trail", 
                    MB_OK | MB_ICONINFORMATION);
    }

    // Show settings dialog
    static void onLicenseSettings() {
        std::string settings = "╔════════════════════════════════════════╗\n";
        settings += "║        License Settings                 ║\n";
        settings += "╚════════════════════════════════════════╝\n\n";

        settings += "Cache Location:\n";
        settings += "  C:\\ProgramData\\RawrXD\\license.cache\n\n";

        settings += "Audit Log Location:\n";
        settings += "  C:\\ProgramData\\RawrXD\\audit.log\n\n";

        settings += "Offline Grace Period: 90 days\n";
        settings += "Cache Validity: 30 days\n";
        settings += "Sync Interval: Daily\n\n";

        settings += "These settings can be modified through:\n";
        settings += "  C:\\ProgramData\\RawrXD\\license.conf\n";

        MessageBoxA(s_ideWindow, settings.c_str(), "License Settings", 
                    MB_OK | MB_ICONINFORMATION);
    }
};

// Static member initialization
HWND LicenseManagerIntegration::s_licensePanel = nullptr;
HWND LicenseManagerIntegration::s_ideWindow = nullptr;

// ============================================================================
// Public API for IDE Integration
// ============================================================================

extern "C" {

/**
 * Initialize license manager UI integration with IDE
 * 
 * @param ideWindow Handle to IDE main window
 * @return true if initialization successful
 */
bool initializeLicenseManagerUI(void* ideWindow) {
    if (!ideWindow) return false;
    return LicenseManagerIntegration::initializeIDEIntegration(
        reinterpret_cast<HWND>(ideWindow));
}

/**
 * Handle license manager menu command
 * 
 * @param menuID Menu command ID
 * @return true if command was handled
 */
bool handleLicenseManagerCommand(int menuID) {
    return LicenseManagerIntegration::handleMenuCommand(menuID);
}

}  // extern "C"

}  // namespace RawrXD::License
