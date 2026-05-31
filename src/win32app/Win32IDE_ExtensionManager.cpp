#include "Win32IDE.h"

// ============================================================================
// Win32IDE_ExtensionManager.cpp — extension list refresh (listExtensions)
// ============================================================================
// Other extension UI commands (search/install/VSIX/details) live in
// Win32IDE_ExtensionsPanel.cpp to avoid duplicate Win32IDE method definitions
// at link time. This TU owns listExtensions() + ExtensionLoader scan wiring.
// ============================================================================

#include "../../include/async_logger.hpp"
using namespace RawrXD;
using namespace RawrXD::Extensions;
#include "../modules/ExtensionLoader.hpp"
#include "IDELogger.h"


// File-local singleton accessor for ExtensionLoader (class has no GetInstance)
static ExtensionLoader& GetLoaderInstance()
{
    static ExtensionLoader s_loader;
    return s_loader;
}


// ── Internal helpers ───────────────────────────────────────────────────────

// Push a status line to the extension details control (best-effort; null-safe).
static void SetExtStatus(HWND hwndDetails, const std::string& msg)
{
    if (hwndDetails)
        SetWindowTextA(hwndDetails, msg.c_str());
    RAWRXD_INFO("ExtensionManager", (msg).c_str());
}

// ── listExtensions ─────────────────────────────────────────────────────────
void Win32IDE::listExtensions()
{
    // Reload installed extensions from disk then repopulate the list view.
    ExtensionLoader& loader = GetLoaderInstance();
    loader.Scan();
    loadInstalledExtensions();
    SetExtStatus(m_hwndExtensionDetails, "Extensions refreshed.");
    RAWRXD_INFO("ExtensionManager", "listExtensions: scan complete");
}
