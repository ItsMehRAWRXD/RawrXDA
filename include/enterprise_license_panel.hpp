// ============================================================================
// enterprise_license_panel.hpp — Enterprise License Status Panel Widget
// ============================================================================
// Win32 panel that displays enterprise license status, feature grid, and
// provides license management actions. Integrates with Win32IDE as a
// dockable/embeddable panel alongside the existing License Creator dialog.
//
// Used by: Win32IDE main window, status bar, and REPL output.
// ============================================================================

#pragma once

#include <string>
#include <cstdint>

namespace RawrXD::UI {

// ============================================================================
// Enterprise Status Bar Integration
// ============================================================================
// Call from Win32IDE's status bar paint to show license tier icon.
// Returns a short string like "ENT" / "PRO" / "TRIAL" / "FREE".

const char* GetLicenseTierBadge();

// Returns a longer status string for tooltips:
// "Enterprise | 8/8 features | 800B models | 200K context"
std::string GetLicenseStatusTooltip();

// ============================================================================
// Console / REPL Display Functions
// ============================================================================

// Print the enterprise dashboard to stdout (for REPL /license command)
void PrintLicenseDashboard();

// Print the full audit report to stdout (for REPL /license audit)
void PrintLicenseAudit();

// Print feature list to stdout (for REPL /license features)
void PrintFeatureList();

// Print startup banner enterprise info (called from main.cpp banner)
void PrintEnterpriseBanner();

// ============================================================================
// Feature Gate Display
// ============================================================================
// When a feature is denied, these produce user-friendly denial messages.

// Returns denial message for a locked feature.
// E.g., "800B Dual-Engine requires Enterprise license. Current: Community"
std::string GetFeatureDenialMessage(uint64_t featureMask);

// Returns upgrade prompt for a locked feature.
// E.g., "Upgrade to Enterprise for 800B Dual-Engine: RawrXD_KeyGen.exe --issue"
std::string GetUpgradePrompt(uint64_t featureMask);

} // namespace RawrXD::UI
