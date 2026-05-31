// ============================================================================
// menu_auditor.cpp — Phase 31: Menu Wire Verification Engine
// ============================================================================
//
// PURPOSE:
//   Recursive HMENU scanner that verifies every registered IDE command
//   has a corresponding menu item wired. Also verifies breadcrumb trails
//   (menu → submenu → item) and detects orphaned command IDs.
//
// FUNCTIONS:
//   verifyCommandInMenu()     — Recursive search for a command ID in HMENU
//   buildMenuBreadcrumb()     — Build human-readable path to a menu item
//   getMenuWiringReport()     — Full menu audit report
//   findOrphanedCommands()    — Commands in registry with no menu entry
//   findUnregisteredMenuItems() — Menu items with no registry entry
//
// PATTERN:   No exceptions. bool returns.
// THREADING: All calls on UI thread (STA) — HMENU is not thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/feature_registry.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Audit {

// ============================================================================
// INTERNAL: Recursive HMENU search for a specific command ID
// ============================================================================
static bool searchMenuForCommand(HMENU hMenu, int targetCmd, int depth, int maxDepth) {
    if (!hMenu || depth > maxDepth) return false;

    int itemCount = GetMenuItemCount(hMenu);
    if (itemCount <= 0) return false;

    for (int i = 0; i < itemCount; ++i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE;

        if (!GetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &mii)) {
            continue;
        }

        // Check direct match
        if (mii.wID == static_cast<UINT>(targetCmd)) {
            return true;
        }

        // Recurse into submenus
        if (mii.hSubMenu) {
            if (searchMenuForCommand(mii.hSubMenu, targetCmd, depth + 1, maxDepth)) {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// PUBLIC: Verify that a command ID exists somewhere in the menu tree
// ============================================================================
bool verifyCommandInMenu(HMENU hMenu, int commandId) {
    if (!hMenu || commandId <= 0) return false;
    return searchMenuForCommand(hMenu, commandId, 0, 10);
}

// ============================================================================
// INTERNAL: Build breadcrumb trail for a command ID
// ============================================================================
static bool buildBreadcrumbRecursive(HMENU hMenu, int targetCmd,
                                       std::vector<std::wstring>& trail,
                                       int depth, int maxDepth) {
    if (!hMenu || depth > maxDepth) return false;

    int itemCount = GetMenuItemCount(hMenu);
    if (itemCount <= 0) return false;

    for (int i = 0; i < itemCount; ++i) {
        wchar_t textBuf[256] = {};
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_STRING | MIIM_FTYPE;
        mii.dwTypeData = textBuf;
        mii.cch = 255;

        if (!GetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &mii)) {
            continue;
        }

        // Skip separators
        if (mii.fType & MFT_SEPARATOR) continue;

        std::wstring itemText(textBuf);

        // Strip accelerator keys (&)
        std::wstring cleanText;
        for (size_t c = 0; c < itemText.size(); ++c) {
            if (itemText[c] != L'&') {
                cleanText += itemText[c];
            }
        }

        // Direct match
        if (mii.wID == static_cast<UINT>(targetCmd)) {
            trail.push_back(cleanText);
            return true;
        }

        // Recurse into submenu
        if (mii.hSubMenu) {
            trail.push_back(cleanText);
            if (buildBreadcrumbRecursive(mii.hSubMenu, targetCmd, trail,
                                          depth + 1, maxDepth)) {
                return true;
            }
            trail.pop_back();
        }
    }

    return false;
}

// ============================================================================
// PUBLIC: Build human-readable breadcrumb path for a command
// ============================================================================
std::string buildMenuBreadcrumb(HMENU hMenu, int commandId) {
    if (!hMenu || commandId <= 0) return "(not found)";

    std::vector<std::wstring> trail;
    if (!buildBreadcrumbRecursive(hMenu, commandId, trail, 0, 10)) {
        return "(not wired)";
    }

    // Convert wide trail to UTF-8 path
    std::string result;
    for (size_t i = 0; i < trail.size(); ++i) {
        if (i > 0) result += " > ";

        // WideCharToMultiByte conversion
        int needed = WideCharToMultiByte(CP_UTF8, 0,
                                          trail[i].c_str(),
                                          static_cast<int>(trail[i].size()),
                                          nullptr, 0, nullptr, nullptr);
        if (needed > 0) {
            std::string utf8(static_cast<size_t>(needed), '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                                trail[i].c_str(),
                                static_cast<int>(trail[i].size()),
                                &utf8[0], needed, nullptr, nullptr);
            result += utf8;
        }
    }

    return result;
}

// ============================================================================
// INTERNAL: Collect all command IDs from an HMENU recursively
// ============================================================================
static void collectMenuCommandIds(HMENU hMenu, std::set<int>& outIds,
                                    int depth, int maxDepth) {
    if (!hMenu || depth > maxDepth) return;

    int itemCount = GetMenuItemCount(hMenu);
    if (itemCount <= 0) return;

    for (int i = 0; i < itemCount; ++i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(MENUITEMINFOW);
        mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE;

        if (!GetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &mii)) {
            continue;
        }

        // Skip separators and submenu-only items (ID 0)
        if (!(mii.fType & MFT_SEPARATOR) && mii.wID != 0) {
            outIds.insert(static_cast<int>(mii.wID));
        }

        if (mii.hSubMenu) {
            collectMenuCommandIds(mii.hSubMenu, outIds, depth + 1, maxDepth);
        }
    }
}

// ============================================================================
// PUBLIC: Find registered features with commands not in the menu
// ============================================================================
std::vector<std::string> findOrphanedCommands(HMENU hMenu) {
    std::vector<std::string> orphans;
    if (!hMenu) return orphans;

    // Collect all menu command IDs
    std::set<int> menuIds;
    collectMenuCommandIds(hMenu, menuIds, 0, 10);

    // Compare against registry
    auto features = FeatureRegistry::instance().getAllFeatures();
    for (const auto& f : features) {
        if (f.commandId != 0) {
            if (menuIds.find(f.commandId) == menuIds.end()) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s (IDM=%d, Phase=%s)",
                         f.name ? f.name : "(null)",
                         f.commandId,
                         f.phase ? f.phase : "?");
                orphans.push_back(buf);
            }
        }
    }

    return orphans;
}

// ============================================================================
// PUBLIC: Find menu items with no corresponding registry entry
// ============================================================================
std::vector<int> findUnregisteredMenuItems(HMENU hMenu) {
    std::vector<int> unregistered;
    if (!hMenu) return unregistered;

    // Collect all menu command IDs
    std::set<int> menuIds;
    collectMenuCommandIds(hMenu, menuIds, 0, 10);

    // Build set of registered command IDs
    auto features = FeatureRegistry::instance().getAllFeatures();
    std::set<int> registeredIds;
    for (const auto& f : features) {
        if (f.commandId != 0) {
            registeredIds.insert(f.commandId);
        }
    }

    // Find menu IDs not in registry
    for (int id : menuIds) {
        if (registeredIds.find(id) == registeredIds.end()) {
            unregistered.push_back(id);
        }
    }

    return unregistered;
}

// ============================================================================
// PUBLIC: Generate full menu wiring audit report
// ============================================================================
std::string getMenuWiringReport(HMENU hMenu) {
    std::ostringstream oss;

    oss << "── Menu Wiring Audit ────────────────────────────────────────────\n\n";

    if (!hMenu) {
        oss << "  ERROR: No menu handle provided.\n";
        return oss.str();
    }

    // Collect all menu items
    std::set<int> menuIds;
    collectMenuCommandIds(hMenu, menuIds, 0, 10);
    oss << "  Total menu items found: " << menuIds.size() << "\n\n";

    // Check each registered feature
    auto features = FeatureRegistry::instance().getAllFeatures();
    int wiredCount = 0, unwiredCount = 0;

    oss << "  " << std::left
        << std::setw(32) << "Feature"
        << std::setw(8)  << "IDM"
        << std::setw(8)  << "Wired"
        << "Breadcrumb"
        << "\n";
    oss << "  " << std::string(80, '-') << "\n";

    for (const auto& f : features) {
        if (f.commandId != 0) {
            bool wired = (menuIds.find(f.commandId) != menuIds.end());
            std::string breadcrumb = wired
                ? buildMenuBreadcrumb(hMenu, f.commandId)
                : "(NOT WIRED)";

            oss << "  " << std::left
                << std::setw(32) << (f.name ? f.name : "(null)")
                << std::setw(8)  << f.commandId
                << std::setw(8)  << (wired ? "YES" : "NO")
                << breadcrumb
                << "\n";

            if (wired) wiredCount++;
            else unwiredCount++;
        }
    }

    oss << "\n  Wired: " << wiredCount
        << "  |  Not Wired: " << unwiredCount << "\n";

    // Orphaned commands
    auto orphans = findOrphanedCommands(hMenu);
    if (!orphans.empty()) {
        oss << "\n  ── Orphaned Commands (registered but not in menu) ────────\n";
        for (const auto& o : orphans) {
            oss << "    • " << o << "\n";
        }
    }

    // Unregistered menu items
    auto unreg = findUnregisteredMenuItems(hMenu);
    if (!unreg.empty()) {
        oss << "\n  ── Unregistered Menu Items (in menu but not in registry) ──\n";
        for (int id : unreg) {
            oss << "    • IDM=" << id
                << "  path=" << buildMenuBreadcrumb(hMenu, id) << "\n";
        }
    }

    oss << "\n";
    return oss.str();
}

} // namespace Audit
} // namespace RawrXD
