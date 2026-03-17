// =============================================================================
// Win32IDE_Plugins.cpp — Phase 43: Native Win32 Plugin System Integration
//
// Implements:
//   - Plugin system lifecycle (init, shutdown)
//   - Plugin panel UI (list → load/unload → status)
//   - Command handlers for IDM_PLUGIN_* range (5090–5098)
//   - Hook dispatch integration (onFileSave, onChatMessage, etc.)
//   - Config-gated hot-loading
//
// Thread-safety: Plugin loader is internally mutex-protected.
// Error model: No exceptions. Structured logging via appendToOutput.
// =============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <fstream>
#include <filesystem>

// =============================================================================
// initPluginSystem — Called from deferredHeavyInit
// =============================================================================
void Win32IDE::initPluginSystem() {
    LOG_INFO("initPluginSystem: starting Phase 43 plugin system");

    m_pluginLoader = std::make_unique<RawrXD::Win32PluginLoader>();

    // Wire plugin logging to the IDE output panel
    m_pluginLoader->setLogCallback([this](const std::string& msg, int severity) {
        if (isShuttingDown()) return;

        OutputSeverity sev = OutputSeverity::Info;
        std::string prefix = "🔌 ";
        if (severity == 1) {
            sev = OutputSeverity::Warning;
            prefix = "⚠️ ";
        } else if (severity >= 2) {
            sev = OutputSeverity::Error;
            prefix = "❌ ";
        }
        appendToOutput(prefix + msg + "\n", "Plugins", sev);
    });

    // Set default plugin directory next to the executable
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    m_pluginDirectory = exeDir + "\\plugins";

    // Create plugin directory if it doesn't exist
    CreateDirectoryA(m_pluginDirectory.c_str(), nullptr);

    // Auto-scan and load plugins from the default directory
    auto dlls = m_pluginLoader->scanDirectory(m_pluginDirectory);
    for (const auto& dll : dlls) {
        m_pluginLoader->loadPlugin(dll);
    }

    size_t count = m_pluginLoader->pluginCount();
    LOG_INFO("initPluginSystem: complete. " + std::to_string(count) + " plugin(s) loaded from " + m_pluginDirectory);

    if (count > 0) {
        appendToOutput("🔌 Plugin system initialized: " + std::to_string(count) +
                       " plugin(s) loaded\n", "Plugins", OutputSeverity::Info);
    }
}

// =============================================================================
// shutdownPlugins — Called from onDestroy before terminal teardown
// =============================================================================
void Win32IDE::shutdownPlugins() {
    LOG_INFO("shutdownPlugins: unloading all plugins");
    if (m_pluginLoader) {
        m_pluginLoader->unloadAll();
        m_pluginLoader.reset();
    }
}

// =============================================================================
// handlePluginCommand — Route IDM_PLUGIN_* commands
// =============================================================================
bool Win32IDE::handlePluginCommand(int id) {
    switch (id) {
        case IDM_PLUGIN_SHOW_PANEL:    showPluginPanel();        return true;
        case IDM_PLUGIN_LOAD:          onPluginLoad();           return true;
        case IDM_PLUGIN_UNLOAD:        onPluginUnload();         return true;
        case IDM_PLUGIN_UNLOAD_ALL:    onPluginUnloadAll();      return true;
        case IDM_PLUGIN_REFRESH:       onPluginRefresh();        return true;
        case IDM_PLUGIN_SCAN_DIR:      onPluginScanDir();        return true;
        case IDM_PLUGIN_SHOW_STATUS:   onPluginShowStatus();     return true;
        case IDM_PLUGIN_TOGGLE_HOTLOAD: onPluginToggleHotLoad(); return true;
        case IDM_PLUGIN_CONFIGURE:     onPluginConfigure();      return true;
        default: return false;
    }
}

// =============================================================================
// showPluginPanel — Scrollable panel showing loaded plugins and their status
// =============================================================================
void Win32IDE::showPluginPanel() {
    if (!m_pluginLoader) {
        MessageBoxA(m_hwndMain, "Plugin system not initialized.", "Plugins", MB_OK | MB_ICONWARNING);
        return;
    }

    std::ostringstream oss;
    oss << "═══════════════════════════════════════════════════\r\n";
    oss << "  RawrXD Plugin System — Phase 43\r\n";
    oss << "═══════════════════════════════════════════════════\r\n\r\n";

    oss << "Plugin Directory: " << m_pluginDirectory << "\r\n";
    oss << "Hot-Loading: " << (m_pluginLoader->isHotLoadEnabled() ? "ENABLED" : "DISABLED") << "\r\n";
    oss << "Loaded Plugins: " << m_pluginLoader->pluginCount() << "\r\n\r\n";

    auto names = m_pluginLoader->pluginNames();
    if (names.empty()) {
        oss << "(No plugins loaded)\r\n\r\n";
        oss << "Place .dll plugins in the plugins directory and use\r\n";
        oss << "Tools → Plugins → Scan Directory to discover them.\r\n";
    } else {
        oss << "───────────────────────────────────────────────────\r\n";
        for (const auto& name : names) {
            const auto* plugin = m_pluginLoader->getPlugin(name);
            if (!plugin) continue;

            oss << "  Plugin: " << name << "\r\n";
            oss << "  Path:   " << plugin->path << "\r\n";

            if (plugin->info) {
                oss << "  Version:     " << (plugin->info->version ? plugin->info->version : "?") << "\r\n";
                oss << "  Description: " << (plugin->info->description ? plugin->info->description : "N/A") << "\r\n";
            }

            const char* stateStr = "Unknown";
            switch (plugin->state) {
                case RawrXD::PluginState::Loaded:    stateStr = "Loaded"; break;
                case RawrXD::PluginState::Active:    stateStr = "Active"; break;
                case RawrXD::PluginState::Error:     stateStr = "Error"; break;
                case RawrXD::PluginState::Unloading: stateStr = "Unloading"; break;
            }
            oss << "  State:       " << stateStr << "\r\n";
            oss << "  Hook Calls:  " << plugin->hookCalls << "\r\n";
            oss << "  Load Time:   " << plugin->loadTimeMs << "ms\r\n";

            // Show available hooks
            oss << "  Hooks:       ";
            bool hasHook = false;
            if (plugin->fn_onFileSave)    { oss << "onFileSave "; hasHook = true; }
            if (plugin->fn_onChatMessage) { oss << "onChatMessage "; hasHook = true; }
            if (plugin->fn_onCommand)     { oss << "onCommand "; hasHook = true; }
            if (plugin->fn_onModelLoad)   { oss << "onModelLoad "; hasHook = true; }
            if (!hasHook) oss << "(none)";
            oss << "\r\n";

            oss << "───────────────────────────────────────────────────\r\n";
        }
    }

    // Create scrollable popup window
    HWND hDlg = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "STATIC", "RawrXD Plugin System",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
        m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg) {
        MessageBoxA(m_hwndMain, oss.str().c_str(), "Plugin System", MB_OK);
        return;
    }

    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", oss.str().c_str(),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, 560, 400,
        hDlg, nullptr, m_hInstance, nullptr);

    HFONT hFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Add buttons
    CreateWindowExA(0, "BUTTON", "Scan Directory",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 420, 120, 30, hDlg, (HMENU)IDM_PLUGIN_SCAN_DIR, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Load Plugin...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        140, 420, 120, 30, hDlg, (HMENU)IDM_PLUGIN_LOAD, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        270, 420, 100, 30, hDlg, (HMENU)IDM_PLUGIN_REFRESH, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        490, 420, 80, 30, hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}

// =============================================================================
// onPluginLoad — Open file dialog to select a DLL plugin
// =============================================================================
void Win32IDE::onPluginLoad() {
    if (!m_pluginLoader) {
        appendToOutput("⚠️ Plugin system not initialized\n", "Plugins", OutputSeverity::Warning);
        return;
    }

    // Open file dialog
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Plugin DLLs (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Load Plugin DLL";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        if (m_pluginLoader->loadPlugin(filePath)) {
            appendToOutput("✅ Plugin loaded: " + std::string(filePath) + "\n",
                           "Plugins", OutputSeverity::Info);
        } else {
            appendToOutput("❌ Failed to load plugin: " + std::string(filePath) + "\n",
                           "Plugins", OutputSeverity::Error);
        }
    }
}

// =============================================================================
// onPluginUnload — Prompt user for plugin name and unload it
// =============================================================================
void Win32IDE::onPluginUnload() {
    if (!m_pluginLoader) return;

    auto names = m_pluginLoader->pluginNames();
    if (names.empty()) {
        MessageBoxA(m_hwndMain, "No plugins are currently loaded.", "Unload Plugin", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Build list of loaded plugins
    std::string list = "Loaded plugins:\n\n";
    for (size_t i = 0; i < names.size(); ++i) {
        list += std::to_string(i + 1) + ". " + names[i] + "\n";
    }
    list += "\nEnter plugin name to unload:";

    char nameBuf[256] = {};
    // Use simple input approach — show list then prompt
    // For now, unload the first plugin or show status
    if (names.size() == 1) {
        int choice = MessageBoxA(m_hwndMain,
            ("Unload plugin '" + names[0] + "'?").c_str(),
            "Unload Plugin",
            MB_YESNO | MB_ICONQUESTION);
        if (choice == IDYES) {
            m_pluginLoader->unloadPlugin(names[0]);
            appendToOutput("🔌 Plugin unloaded: " + names[0] + "\n", "Plugins", OutputSeverity::Info);
        }
    } else {
        // Show list and let user pick
        MessageBoxA(m_hwndMain, list.c_str(), "Loaded Plugins", MB_OK);
    }
}

// =============================================================================
// onPluginUnloadAll
// =============================================================================
void Win32IDE::onPluginUnloadAll() {
    if (!m_pluginLoader) return;

    size_t count = m_pluginLoader->pluginCount();
    if (count == 0) {
        appendToOutput("No plugins to unload.\n", "Plugins", OutputSeverity::Info);
        return;
    }

    int choice = MessageBoxA(m_hwndMain,
        ("Unload all " + std::to_string(count) + " plugin(s)?").c_str(),
        "Unload All Plugins",
        MB_YESNO | MB_ICONQUESTION);

    if (choice == IDYES) {
        m_pluginLoader->unloadAll();
        appendToOutput("🔌 All plugins unloaded.\n", "Plugins", OutputSeverity::Info);
    }
}

// =============================================================================
// onPluginRefresh — Re-scan directory and reload
// =============================================================================
void Win32IDE::onPluginRefresh() {
    if (!m_pluginLoader) return;

    appendToOutput("🔌 Refreshing plugins from " + m_pluginDirectory + "...\n",
                   "Plugins", OutputSeverity::Info);

    // Unload all, re-scan, reload
    m_pluginLoader->unloadAll();

    auto dlls = m_pluginLoader->scanDirectory(m_pluginDirectory);
    for (const auto& dll : dlls) {
        m_pluginLoader->loadPlugin(dll);
    }

    appendToOutput("🔌 Refresh complete: " + std::to_string(m_pluginLoader->pluginCount()) +
                   " plugin(s) loaded\n", "Plugins", OutputSeverity::Info);
}

// =============================================================================
// onPluginScanDir — Browse for a directory and scan it for plugins
// =============================================================================
void Win32IDE::onPluginScanDir() {
    if (!m_pluginLoader) return;

    // Use SHBrowseForFolder to let user pick a directory
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.lpszTitle = "Select Plugin Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char dirPath[MAX_PATH] = {};
        if (SHGetPathFromIDListA(pidl, dirPath)) {
            m_pluginDirectory = dirPath;
            appendToOutput("🔌 Plugin directory set to: " + m_pluginDirectory + "\n",
                           "Plugins", OutputSeverity::Info);

            auto dlls = m_pluginLoader->scanDirectory(m_pluginDirectory);
            appendToOutput("🔌 Found " + std::to_string(dlls.size()) + " DLL(s) in directory\n",
                           "Plugins", OutputSeverity::Info);

            for (const auto& dll : dlls) {
                if (!m_pluginLoader->isLoaded(
                        dll.substr(dll.find_last_of("\\/") + 1,
                                   dll.find_last_of('.') - dll.find_last_of("\\/") - 1))) {
                    m_pluginLoader->loadPlugin(dll);
                }
            }
        }
        CoTaskMemFree(pidl);
    }
}

// =============================================================================
// onPluginShowStatus — Quick status output
// =============================================================================
void Win32IDE::onPluginShowStatus() {
    if (!m_pluginLoader) {
        appendToOutput("⚠️ Plugin system not initialized\n", "Plugins", OutputSeverity::Warning);
        return;
    }

    std::ostringstream oss;
    oss << "🔌 Plugin Status\n";
    oss << "  Directory:  " << m_pluginDirectory << "\n";
    oss << "  Hot-Load:   " << (m_pluginLoader->isHotLoadEnabled() ? "ON" : "OFF") << "\n";
    oss << "  Loaded:     " << m_pluginLoader->pluginCount() << "\n";

    auto names = m_pluginLoader->pluginNames();
    for (const auto& name : names) {
        const auto* p = m_pluginLoader->getPlugin(name);
        if (p) {
            const char* stateStr = "?";
            switch (p->state) {
                case RawrXD::PluginState::Active:    stateStr = "✅ Active"; break;
                case RawrXD::PluginState::Loaded:    stateStr = "📦 Loaded"; break;
                case RawrXD::PluginState::Error:     stateStr = "❌ Error"; break;
                case RawrXD::PluginState::Unloading: stateStr = "⏳ Unloading"; break;
            }
            oss << "  [" << stateStr << "] " << name;
            if (p->info && p->info->version) {
                oss << " v" << p->info->version;
            }
            oss << " (" << p->hookCalls << " hook calls)\n";
        }
    }

    appendToOutput(oss.str(), "Plugins", OutputSeverity::Info);
}

// =============================================================================
// onPluginToggleHotLoad
// =============================================================================
void Win32IDE::onPluginToggleHotLoad() {
    if (!m_pluginLoader) return;

    bool current = m_pluginLoader->isHotLoadEnabled();
    m_pluginLoader->setHotLoadEnabled(!current);

    appendToOutput("🔌 Hot-loading " + std::string(!current ? "ENABLED" : "DISABLED") + "\n",
                   "Plugins", OutputSeverity::Info);
}

// =============================================================================
// onPluginConfigure — Show/edit plugin configuration
// =============================================================================
void Win32IDE::onPluginConfigure() {
    std::ostringstream oss;
    oss << "Plugin Configuration:\n\n";
    oss << "  Plugin Directory: " << m_pluginDirectory << "\n";
    oss << "  Hot-Loading: " << (m_pluginLoader && m_pluginLoader->isHotLoadEnabled() ? "ON" : "OFF") << "\n\n";
    oss << "Use 'Tools → Plugins → Scan Directory' to change the plugin directory.\n";
    oss << "Use 'Tools → Plugins → Toggle Hot-Load' to enable/disable hot-loading.\n";

    MessageBoxA(m_hwndMain, oss.str().c_str(), "Plugin Configuration", MB_OK | MB_ICONINFORMATION);
}
