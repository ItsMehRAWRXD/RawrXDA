// ============================================================================
// Win32IDE_PDBSymbols.cpp — Phase 29 PDB Symbol Server Integration
// ============================================================================
//
// Phase 29: Native PDB Symbol Server — Win32IDE Command Integration
//
// This file integrates the PDBManager into the Win32IDE, providing:
//   1. PDB system initialization during IDE startup
//   2. Command routing for IDM_PDB_* commands (9400 range)
//   3. Status dialog showing loaded modules + symbol stats
//   4. Cache management (clear, size display)
//   5. Manual PDB load dialog
//   6. LSP bridge initialization (PDB → LSP server)
//
// Pattern:  PDBResult-compatible, no exceptions
// Threading: All calls on UI thread (STA)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../../include/pdb_native.h"

#include <cstdio>
#include <string>
#include <vector>
#include <shlobj.h>

// Forward declaration from pdb_lsp_bridge.cpp
namespace RawrXD { namespace PDB {
    void initPDBLSPBridge(RawrXD::LSPServer::RawrXDLSPServer* lspServer);
    uint32_t injectPDBSymbolsIntoLSP(const char* moduleName,
                                       std::vector<RawrXD::LSPServer::IndexedSymbol>& outSymbols);
    bool tryPDBDefinition(const std::string& symbolName, nlohmann::json& resultOut);
    bool tryPDBHover(const std::string& symbolName, nlohmann::json& resultOut);
} }

// ============================================================================
// Initialization — Called during IDE startup (after window creation)
// ============================================================================
void Win32IDE::initPDBSymbols() {
    if (m_pdbInitialized) return;

    OutputDebugStringA("[Phase 29] Initializing PDB Symbol Server...");

    // Configure the PDB manager with default settings
    RawrXD::PDB::SymbolServerConfig config;
    config.serverUrl = L"https://msdl.microsoft.com/download/symbols";
    config.cachePath = nullptr;  // Will resolve to %LOCALAPPDATA%\RawrXD\Symbols
    config.timeoutMs = 30000;
    config.maxRetries = 3;
    config.enableCompression = true;
    config.enabled = true;

    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    RawrXD::PDB::PDBResult r = pdb.configure(config);

    if (!r.success) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "[Phase 29] PDB symbol server configuration: %s (code %d)",
            r.detail, r.errorCode);
        OutputDebugStringA(msg);

        // Non-fatal — show warning but continue
        appendToOutput(std::string("[PDB] Configuration warning: ") + (r.detail ? r.detail : "unknown"),
                       "Output", OutputSeverity::Warning);
    } else {
        OutputDebugStringA("[Phase 29] PDB symbol server configured successfully");
        appendToOutput("[PDB] Symbol server ready (https://msdl.microsoft.com/download/symbols)",
                       "Output", OutputSeverity::Info);
    }

    // Initialize LSP bridge if LSP server is running
    if (m_lspServer) {
        RawrXD::PDB::initPDBLSPBridge(m_lspServer.get());
        OutputDebugStringA("[Phase 29] PDB-LSP bridge initialized");
    }

    // Set progress callback for download notifications
    pdb.getSymbolServer().setProgressCallback(
        [](const char* fileName, uint64_t bytesReceived, uint64_t totalBytes, void* userData) {
            auto* ide = static_cast<Win32IDE*>(userData);
            if (!ide) return;

            char msg[256];
            if (totalBytes > 0) {
                float pct = (static_cast<float>(bytesReceived) / totalBytes) * 100.0f;
                snprintf(msg, sizeof(msg), "[PDB] Downloading %s: %.1f%% (%llu / %llu bytes)",
                         fileName, pct,
                         static_cast<unsigned long long>(bytesReceived),
                         static_cast<unsigned long long>(totalBytes));
            } else {
                snprintf(msg, sizeof(msg), "[PDB] Downloading %s: %llu bytes",
                         fileName, static_cast<unsigned long long>(bytesReceived));
            }
            ide->appendToOutput(msg, "Output", OutputSeverity::Info);
        },
        this
    );

    m_pdbInitialized = true;
    OutputDebugStringA("[Phase 29] PDB Symbol Server initialization complete");
}

// ============================================================================
// Command Router — IDM_PDB_* commands (9400 range)
// ============================================================================
bool Win32IDE::handlePDBCommand(int commandId) {
    switch (commandId) {
    case IDM_PDB_LOAD:          cmdPDBLoad(); return true;
    case IDM_PDB_FETCH:         cmdPDBFetch(); return true;
    case IDM_PDB_STATUS:        cmdPDBStatus(); return true;
    case IDM_PDB_CACHE_CLEAR:   cmdPDBCacheClear(); return true;
    case IDM_PDB_ENABLE:        cmdPDBEnable(); return true;
    case IDM_PDB_RESOLVE:       cmdPDBResolve(); return true;
    default: return false;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

// ---- IDM_PDB_LOAD: Load PDB from file dialog ----
void Win32IDE::cmdPDBLoad() {
    if (!m_pdbInitialized) initPDBSymbols();

    // Open file dialog for .pdb files
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"PDB Files (*.pdb)\0*.pdb\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Load PDB Symbol File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    // Extract module name from filename
    const wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    const wchar_t* fileName = lastSlash ? lastSlash + 1 : filePath;

    char moduleNameA[260];
    WideCharToMultiByte(CP_UTF8, 0, fileName, -1, moduleNameA, 260, nullptr, nullptr);

    // Remove .pdb extension for module name
    char* dot = strrchr(moduleNameA, '.');
    if (dot) *dot = '\0';

    // Load the PDB
    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    RawrXD::PDB::PDBResult r = pdb.loadPDB(moduleNameA, filePath);

    if (r.success) {
        char msg[512];
        snprintf(msg, sizeof(msg), "[PDB] Loaded symbols for '%s' successfully", moduleNameA);
        appendToOutput(msg, "Output", OutputSeverity::Info);

        // Update status bar
        auto stats = pdb.getStats();
        char statusMsg[128];
        snprintf(statusMsg, sizeof(statusMsg), "PDB: %u modules loaded", stats.modulesLoaded);
        if (m_hwndStatusBar) {
            SetWindowTextA(m_hwndStatusBar, statusMsg);
        }
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "[PDB] Failed to load '%s': %s (code %d)",
                 moduleNameA, r.detail ? r.detail : "unknown", r.errorCode);
        appendToOutput(msg, "Output", OutputSeverity::Error);
        MessageBoxA(m_hwndMain, msg, "PDB Load Error", MB_ICONERROR | MB_OK);
    }
}

// ---- IDM_PDB_FETCH: Download PDB from symbol server ----
void Win32IDE::cmdPDBFetch() {
    if (!m_pdbInitialized) initPDBSymbols();

    // Prompt user for module name (e.g., "ntdll.dll")
    // For Phase 29 v1, use a simple input box
    char moduleName[260] = {};

    // Simple input dialog using a MessageBox prompt
    // Phase 29.2: proper dialog with PE file picker
    int result = MessageBoxA(m_hwndMain,
        "Enter the module name to fetch PDB for (e.g., ntdll.dll).\n\n"
        "Note: The PE file must be loaded to extract GUID+Age.\n"
        "Use Load PDB instead for manual PDB loading.",
        "Fetch PDB from Symbol Server",
        MB_OKCANCEL | MB_ICONINFORMATION);

    if (result != IDOK) return;

    appendToOutput("[PDB] Fetch from symbol server requires PE analysis. "
                   "Use 'Load PDB' for manual loading, or load a PE first via "
                   "Reverse Engineering → Analyze.",
                   "Output", OutputSeverity::Info);
}

// ---- IDM_PDB_STATUS: Show PDB system status ----
void Win32IDE::cmdPDBStatus() {
    if (!m_pdbInitialized) initPDBSymbols();

    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    auto stats = pdb.getStats();

    std::string statusText;
    statusText.reserve(2048);
    statusText += "=== RawrXD PDB Symbol Server Status ===\n\n";

    // Global stats
    char buf[256];
    snprintf(buf, sizeof(buf), "Modules Loaded:    %u\n", stats.modulesLoaded);
    statusText += buf;
    snprintf(buf, sizeof(buf), "Symbols Indexed:   %llu\n",
             static_cast<unsigned long long>(stats.symbolsIndexed));
    statusText += buf;
    snprintf(buf, sizeof(buf), "Lookup Count:      %llu\n",
             static_cast<unsigned long long>(stats.lookupCount));
    statusText += buf;
    snprintf(buf, sizeof(buf), "Cache Hits:        %llu\n",
             static_cast<unsigned long long>(stats.cacheHits));
    statusText += buf;
    snprintf(buf, sizeof(buf), "Cache Misses:      %llu\n",
             static_cast<unsigned long long>(stats.cacheMisses));
    statusText += buf;
    snprintf(buf, sizeof(buf), "Downloads:         %llu\n",
             static_cast<unsigned long long>(stats.downloadCount));
    statusText += buf;

    // Cache size
    uint64_t cacheSize = pdb.getSymbolServer().getCacheSizeBytes();
    if (cacheSize < 1024) {
        snprintf(buf, sizeof(buf), "Cache Size:        %llu bytes\n",
                 static_cast<unsigned long long>(cacheSize));
    } else if (cacheSize < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "Cache Size:        %.1f KB\n", cacheSize / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "Cache Size:        %.1f MB\n", cacheSize / (1024.0 * 1024.0));
    }
    statusText += buf;

    statusText += "\n--- Loaded Modules ---\n";

    // Per-module details
    for (uint32_t i = 0; i < pdb.getLoadedModuleCount(); ++i) {
        const char* name = pdb.getLoadedModuleName(i);
        if (!name) continue;

        const RawrXD::PDB::NativePDBParser* parser = pdb.getParser(name);
        if (!parser) continue;

        snprintf(buf, sizeof(buf), "\n  [%u] %s\n", i, name);
        statusText += buf;

        snprintf(buf, sizeof(buf), "      Streams:  %u\n", parser->getStreamCount());
        statusText += buf;
        snprintf(buf, sizeof(buf), "      Sections: %u\n", parser->getSectionCount());
        statusText += buf;
        snprintf(buf, sizeof(buf), "      Age:      %u\n", parser->getAge());
        statusText += buf;

        uint8_t guid[16];
        if (parser->getGuid(guid).success) {
            char guidHex[33];
            RawrXD::PDB::PDB_GuidToHex(guid, guidHex, 33);
            snprintf(buf, sizeof(buf), "      GUID:     %s\n", guidHex);
            statusText += buf;
        }
    }

    if (stats.modulesLoaded == 0) {
        statusText += "\n  (No modules loaded. Use Load PDB or analyze a PE file.)\n";
    }

    statusText += "\n=== End PDB Status ===\n";

    // Display in output panel
    appendToOutput(statusText, "Output", OutputSeverity::Info);

    // Also show in message box for immediate visibility
    MessageBoxA(m_hwndMain, statusText.c_str(), "PDB Symbol Server Status", MB_OK | MB_ICONINFORMATION);
}

// ---- IDM_PDB_CACHE_CLEAR: Clear symbol cache ----
void Win32IDE::cmdPDBCacheClear() {
    if (!m_pdbInitialized) initPDBSymbols();

    int result = MessageBoxA(m_hwndMain,
        "Clear the PDB symbol cache?\n\n"
        "This will delete all cached PDB files from\n"
        "%LOCALAPPDATA%\\RawrXD\\Symbols\n\n"
        "Downloaded PDBs will need to be re-fetched.",
        "Clear PDB Cache",
        MB_YESNO | MB_ICONWARNING);

    if (result != IDYES) return;

    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    RawrXD::PDB::PDBResult r = pdb.getSymbolServer().clearCache();

    if (r.success) {
        appendToOutput("[PDB] Symbol cache cleared successfully", "Output", OutputSeverity::Info);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "[PDB] Failed to clear cache: %s", r.detail ? r.detail : "unknown");
        appendToOutput(msg, "Output", OutputSeverity::Error);
    }
}

// ---- IDM_PDB_ENABLE: Toggle PDB system on/off ----
void Win32IDE::cmdPDBEnable() {
    m_pdbEnabled = !m_pdbEnabled;

    if (m_pdbEnabled && !m_pdbInitialized) {
        initPDBSymbols();
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "[PDB] Symbol server %s",
             m_pdbEnabled ? "enabled" : "disabled");
    appendToOutput(msg, "Output", OutputSeverity::Info);

    // Update status bar
    if (m_hwndStatusBar) {
        if (m_pdbEnabled) {
            auto stats = RawrXD::PDB::PDBManager::instance().getStats();
            char statusMsg[128];
            snprintf(statusMsg, sizeof(statusMsg), "PDB: ON (%u modules)", stats.modulesLoaded);
            SetWindowTextA(m_hwndStatusBar, statusMsg);
        } else {
            SetWindowTextA(m_hwndStatusBar, "PDB: OFF");
        }
    }
}

// ---- IDM_PDB_RESOLVE: Manually resolve a symbol by name ----
void Win32IDE::cmdPDBResolve() {
    if (!m_pdbInitialized) initPDBSymbols();

    // Get the current word under cursor in the editor
    // For Phase 29 v1, use a simple input approach via output panel
    // Phase 29.2: extract word from active editor position

    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    if (pdb.getLoadedModuleCount() == 0) {
        appendToOutput("[PDB] No modules loaded. Load a PDB first.", "Output", OutputSeverity::Warning);
        return;
    }

    // Try to get the currently selected text from the editor
    std::string selectedText;
    if (m_hwndEditor) {
        DWORD selStart = 0, selEnd = 0;
        SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
        if (selEnd > selStart && (selEnd - selStart) < 256) {
            char buf[256] = {};
            SendMessageA(m_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)buf);
            selectedText = buf;
        }
    }

    if (selectedText.empty()) {
        appendToOutput("[PDB] Select a symbol name in the editor, then use PDB → Resolve Symbol.",
                       "Output", OutputSeverity::Info);
        return;
    }

    // Resolve the symbol
    uint64_t rva = 0;
    const char* moduleName = nullptr;
    RawrXD::PDB::PDBResult r = pdb.resolveSymbol(selectedText.c_str(), &rva, &moduleName);

    if (r.success) {
        RawrXD::PDB::ResolvedSymbol sym;
        pdb.resolveRVA(moduleName, rva, &sym);

        char msg[512];
        snprintf(msg, sizeof(msg),
            "[PDB] Resolved: %s\n"
            "  Module:  %s\n"
            "  RVA:     0x%llX\n"
            "  Section: %d + 0x%X\n"
            "  Type:    %s\n"
            "  Size:    %u bytes",
            selectedText.c_str(),
            moduleName ? moduleName : "?",
            static_cast<unsigned long long>(rva),
            sym.section, sym.sectionOffset,
            sym.isFunction ? "Function" : "Data",
            sym.size);
        appendToOutput(msg, "Output", OutputSeverity::Info);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "[PDB] Symbol '%s' not found in any loaded PDB",
                 selectedText.c_str());
        appendToOutput(msg, "Output", OutputSeverity::Warning);
    }
}
