// ============================================================================
// Win32IDE_VSCodeExtAPI.cpp — VS Code Extension API Integration for Win32IDE
// ============================================================================
//
// Phase 29 + 36: VS Code Extension API Compatibility Layer — IDE Integration
//                + QuickJS VSIX JavaScript Extension Host Integration
//
// Implements the Win32IDE member functions for initializing, routing commands,
// and managing the VS Code Extension API compatibility layer.
// Phase 36 adds QuickJS JS extension host initialization and IDM routing.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../modules/vscode_extension_api.h"
#include "../modules/vsix_loader.h"
#include "quickjs_extension_host.h"

#include <commdlg.h>    // OPENFILENAMEW, GetOpenFileNameW, GetSaveFileNameW
#include <cstdio>
#include <sstream>

// ============================================================================
// Lifecycle
// ============================================================================

void Win32IDE::initVSCodeExtensionAPI() {
    if (m_vscExtAPIInitialized) return;

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto result = api.initialize(this, m_hwndMain);

    if (result.success) {
        m_vscExtAPIInitialized = true;
        appendToOutput("[Phase 29] VS Code Extension API initialized successfully\r\n");

        // Register RawrXD-specific commands that map to IDE actions
        api.registerCommand("rawrxd.openFile", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(1001, 0), 0); // IDM_FILE_OPEN
            }
        }, this);

        api.registerCommand("rawrxd.saveFile", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(1002, 0), 0); // IDM_FILE_SAVE
            }
        }, this);

        api.registerCommand("rawrxd.toggleTerminal", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(6001, 0), 0); // IDM_VIEW_TERMINAL
            }
        }, this);

        api.registerCommand("rawrxd.hotpatchStatus", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(9000, 0), 0); // IDM_HOTPATCH_SHOW_STATUS
            }
        }, this);

        api.registerCommand("rawrxd.lspStart", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(9200, 0), 0); // IDM_LSP_SERVER_START
            }
        }, this);

        api.registerCommand("rawrxd.editorEngineCycle", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) {
                PostMessage(ide->getMainWindow(), WM_COMMAND, MAKEWPARAM(9303, 0), 0); // IDM_EDITOR_ENGINE_CYCLE_CMD
            }
        }, this);

        api.registerCommand("rawrxd.extensionAPIStats", [](void* ctx) {
            auto* ide = static_cast<Win32IDE*>(ctx);
            if (ide) ide->cmdVSCExtAPIStats();
        }, this);

        // Scan and auto-load extensions from plugins directory
        VSIXLoader& vsix = VSIXLoader::GetInstance();
        auto loadedPlugins = vsix.GetLoadedPlugins();
        for (auto* plugin : loadedPlugins) {
            if (plugin && plugin->enabled) {
                VSCodeExtensionManifest manifest;
                manifest.id = plugin->id;
                manifest.name = plugin->name;
                manifest.version = plugin->version;
                manifest.description = plugin->description;
                manifest.publisher = plugin->author;

                for (const auto& cmd : plugin->commands) {
                    VSCodeExtensionManifest::ContributedCommand cc;
                    cc.command = cmd;
                    cc.title = cmd;
                    manifest.commands.push_back(cc);
                }

                api.loadExtension(&manifest);
            }
        }

        char statusBuf[512];
        api.getStatusString(statusBuf, sizeof(statusBuf));
        appendToOutput(std::string("[Phase 29] ") + statusBuf + "\r\n");

        // ================================================================
        // Phase 36: Initialize QuickJS Extension Host
        // ================================================================
        auto& jsHost = QuickJSExtensionHost::instance();
        if (jsHost.initialize()) {
            appendToOutput("[Phase 36] QuickJS JavaScript Extension Host initialized\r\n");

            // Scan for JS extensions already extracted in extensions/js/
            std::string jsExtDir = "extensions/js";
            std::error_code ec;
            if (std::filesystem::is_directory(jsExtDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(jsExtDir, ec)) {
                    if (entry.is_directory()) {
                        std::string manifestPath = entry.path().string() + "/package.json";
                        if (std::filesystem::exists(manifestPath)) {
                            std::string extPath = entry.path().string();
                            jsHost.loadJSExtension(extPath);
                        }
                    }
                }
            }

            auto jsStats = jsHost.getStats();
            std::ostringstream jsSS;
            jsSS << "[Phase 36] QuickJS Host: "
                 << jsStats.extensionsLoaded << " JS extensions loaded, "
                 << jsStats.extensionsActive << " active\r\n";
            appendToOutput(jsSS.str());
        } else {
            appendToOutput("[Phase 36] QuickJS Extension Host init failed (non-fatal)\r\n");
        }
    } else {
        appendToOutput(std::string("[Phase 29] VS Code Extension API init failed: ") +
                     result.detail + "\r\n");
    }
}

void Win32IDE::shutdownVSCodeExtensionAPI() {
    if (!m_vscExtAPIInitialized) return;

    // Phase 36: Shutdown QuickJS host first (before C++ API shutdown)
    auto& jsHost = QuickJSExtensionHost::instance();
    jsHost.shutdown();
    appendToOutput("[Phase 36] QuickJS Extension Host shut down\r\n");

    auto& api = vscode::VSCodeExtensionAPI::instance();
    api.shutdown();
    m_vscExtAPIInitialized = false;
    appendToOutput("[Phase 29] VS Code Extension API shut down\r\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleVSCExtAPICommand(int commandId) {
    // Phase 36: QuickJS Host commands (9420-9427)
    if (commandId >= IDM_QUICKJS_HOST_STATUS && commandId <= IDM_QUICKJS_HOST_KILL_RUNTIME) {
        return handleQuickJSHostCommand(commandId);
    }

    switch (commandId) {
        case IDM_VSCEXT_API_STATUS:         cmdVSCExtAPIStatus();         return true;
        case IDM_VSCEXT_API_RELOAD:         cmdVSCExtAPIReload();         return true;
        case IDM_VSCEXT_API_LIST_COMMANDS:  cmdVSCExtAPIListCommands();   return true;
        case IDM_VSCEXT_API_LIST_PROVIDERS: cmdVSCExtAPIListProviders();  return true;
        case IDM_VSCEXT_API_DIAGNOSTICS:    cmdVSCExtAPIDiagnostics();    return true;
        case IDM_VSCEXT_API_EXTENSIONS:     cmdVSCExtAPIExtensions();     return true;
        case IDM_VSCEXT_API_STATS:          cmdVSCExtAPIStats();          return true;
        case IDM_VSCEXT_API_LOAD_NATIVE:    cmdVSCExtAPILoadNative();     return true;
        case IDM_VSCEXT_API_DEACTIVATE_ALL: cmdVSCExtAPIDeactivateAll();  return true;
        case IDM_VSCEXT_API_EXPORT_CONFIG:  cmdVSCExtAPIExportConfig();   return true;
        default: return false;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdVSCExtAPIStatus() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized. Call initVSCodeExtensionAPI() first.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    char buf[1024];
    api.getStatusString(buf, sizeof(buf));

    std::string msg = "[VSCode Extension API Status]\r\n";
    msg += buf;
    msg += "\r\n";
    appendToOutput(msg);
}

void Win32IDE::cmdVSCExtAPIReload() {
    appendToOutput("[VSCode API] Reloading extension API...\r\n");
    shutdownVSCodeExtensionAPI();
    initVSCodeExtensionAPI();
    appendToOutput("[VSCode API] Reload complete.\r\n");
}

void Win32IDE::cmdVSCExtAPIListCommands() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    size_t count = api.getCommandCount();

    std::ostringstream ss;
    ss << "[VSCode Extension API] Registered Commands: " << count << "\r\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdVSCExtAPIListProviders() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();

    std::ostringstream ss;
    ss << "[VSCode Extension API] Provider Counts:\r\n";

    static const char* providerNames[] = {
        "CompletionItem", "Hover", "Definition", "References",
        "DocumentSymbol", "CodeAction", "CodeLens", "DocumentFormatting",
        "RangeFormatting", "Rename", "SignatureHelp", "FoldingRange",
        "DocumentLink", "ColorProvider", "InlayHints", "TypeDefinition",
        "Implementation", "Declaration", "SemanticTokens"
    };

    for (int i = 0; i < static_cast<int>(ProviderType::Count); ++i) {
        size_t count = api.getProviderCount(static_cast<ProviderType>(i));
        if (count > 0) {
            ss << "  " << providerNames[i] << ": " << count << "\r\n";
        }
    }

    appendToOutput(ss.str());
}

void Win32IDE::cmdVSCExtAPIDiagnostics() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    api.publishDiagnostics();
    appendToOutput("[VSCode API] Diagnostics published to LSP server.\r\n");
}

void Win32IDE::cmdVSCExtAPIExtensions() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto stats = api.getStats();

    std::ostringstream ss;
    ss << "[VSCode Extension API] Extensions:\r\n"
       << "  Loaded:  " << stats.extensionsLoaded << "\r\n"
       << "  Active:  " << stats.extensionsActive << "\r\n"
       << "  Native:  " << stats.nativeExtensions << "\r\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdVSCExtAPIStats() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    char jsonBuf[2048];
    api.serializeStatsToJson(jsonBuf, sizeof(jsonBuf));

    std::string msg = "[VSCode Extension API Stats (JSON)]\r\n";
    msg += jsonBuf;
    msg += "\r\n";
    appendToOutput(msg);
}

void Win32IDE::cmdVSCExtAPILoadNative() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    // Open file dialog to select a native DLL extension
    wchar_t filePath[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Native Extensions (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Load Native VS Code Extension";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        // Convert to UTF-8
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Path(utf8Len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, filePath, -1, utf8Path.data(), utf8Len, nullptr, nullptr);

        // Extract extension ID from filename
        std::filesystem::path p(utf8Path);
        std::string extId = p.stem().string();

        auto& api = vscode::VSCodeExtensionAPI::instance();
        auto result = api.loadNativeExtension(utf8Path.c_str(), extId.c_str());

        if (result.success) {
            appendToOutput(std::string("[VSCode API] Native extension loaded: ") + extId + "\r\n");

            // Auto-activate
            auto activateResult = api.activateExtension(extId.c_str());
            if (activateResult.success) {
                appendToOutput(std::string("[VSCode API] Native extension activated: ") + extId + "\r\n");
            } else {
                appendToOutput(std::string("[VSCode API] Activation failed: ") + activateResult.detail + "\r\n");
            }
        } else {
            appendToOutput(std::string("[VSCode API] Load failed: ") + result.detail + "\r\n");
        }
    }
}

void Win32IDE::cmdVSCExtAPIDeactivateAll() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    appendToOutput("[VSCode API] Deactivating all extensions...\r\n");

    // Shutdown and reinitialize without extensions
    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto stats = api.getStats();
    uint64_t prevActive = stats.extensionsActive;

    shutdownVSCodeExtensionAPI();
    initVSCodeExtensionAPI();

    std::ostringstream ss;
    ss << "[VSCode API] Deactivated " << prevActive << " extensions and reinitialized.\r\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdVSCExtAPIExportConfig() {
    if (!m_vscExtAPIInitialized) {
        appendToOutput("[VSCode API] Not initialized.\r\n");
        return;
    }

    // Export current API state to JSON file
    wchar_t filePath[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Export VS Code Extension API Config";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileNameW(&ofn)) {
        auto& api = vscode::VSCodeExtensionAPI::instance();
        char jsonBuf[4096];
        api.serializeStatsToJson(jsonBuf, sizeof(jsonBuf));

        // Write to file
        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, jsonBuf, (DWORD)strlen(jsonBuf), &written, nullptr);
            CloseHandle(hFile);
            appendToOutput("[VSCode API] Config exported successfully.\r\n");
        } else {
            appendToOutput("[VSCode API] Failed to create export file.\r\n");
        }
    }
}
