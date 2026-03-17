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
#include "../../include/lsp/RawrXD_LSPServer.h"

#include <cstdio>
#include <string>
#include <vector>
#include <shlobj.h>
#include <commdlg.h>     // OPENFILENAMEW, GetOpenFileNameW
#include <richedit.h>    // EM_GETSELTEXT

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
    case IDM_PDB_IMPORTS:       cmdPDBImports(); return true;
    case IDM_PDB_EXPORTS:       cmdPDBExports(); return true;
    case IDM_PDB_IAT_STATUS:    cmdPDBIATStatus(); return true;
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

// ============================================================================
// Phase 29.3: PE Import Table Provider — IDM_PDB_IMPORTS
// Parses PE import directory from a loaded executable/DLL
// ============================================================================

// Internal PE parsing structures (subset of winnt.h IMAGE_* defs)
#pragma pack(push, 1)
struct PE_ImportDescriptor {
    uint32_t OriginalFirstThunk;  // RVA to Import Lookup Table (INT)
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;                // RVA to DLL name (ASCII)
    uint32_t FirstThunk;          // RVA to Import Address Table (IAT)
};

struct PE_ExportDirectory {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;                // RVA to DLL name
    uint32_t Base;                // Ordinal base
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;  // RVA to function RVA array
    uint32_t AddressOfNames;      // RVA to name RVA array
    uint32_t AddressOfNameOrdinals; // RVA to ordinal array
};
#pragma pack(pop)

// Helper: read PE file and resolve RVA to file offset
static bool peResolveRVA(const uint8_t* base, uint64_t fileSize, uint32_t rva,
                          uint32_t* outFileOffset)
{
    if (!base || !outFileOffset || fileSize < 64) return false;

    // DOS header
    if (base[0] != 'M' || base[1] != 'Z') return false;
    uint32_t peOff = *reinterpret_cast<const uint32_t*>(base + 0x3C);
    if (peOff + 4 > fileSize) return false;
    if (memcmp(base + peOff, "PE\0\0", 4) != 0) return false;

    // COFF header
    uint16_t numSections = *reinterpret_cast<const uint16_t*>(base + peOff + 6);
    uint16_t optHdrSize  = *reinterpret_cast<const uint16_t*>(base + peOff + 20);

    // Section headers start after optional header
    uint32_t secOff = peOff + 24 + optHdrSize;

    for (uint16_t i = 0; i < numSections; ++i) {
        uint32_t s = secOff + i * 40;
        if (s + 40 > fileSize) break;

        uint32_t virtAddr   = *reinterpret_cast<const uint32_t*>(base + s + 12);
        uint32_t rawSize    = *reinterpret_cast<const uint32_t*>(base + s + 16);
        uint32_t rawPtr     = *reinterpret_cast<const uint32_t*>(base + s + 20);
        uint32_t virtSize   = *reinterpret_cast<const uint32_t*>(base + s + 8);

        uint32_t sectionEnd = virtAddr + (rawSize > virtSize ? rawSize : virtSize);
        if (rva >= virtAddr && rva < sectionEnd) {
            uint32_t delta = rva - virtAddr;
            if (delta < rawSize) {
                *outFileOffset = rawPtr + delta;
                return true;
            }
        }
    }
    return false;
}

// Helper: get PE import/export directory RVA from data directory
static bool peGetDataDirectory(const uint8_t* base, uint64_t fileSize, int index,
                                uint32_t* rvaOut, uint32_t* sizeOut)
{
    if (!base || fileSize < 64) return false;
    uint32_t peOff = *reinterpret_cast<const uint32_t*>(base + 0x3C);
    if (peOff + 24 > fileSize) return false;

    uint16_t magic = *reinterpret_cast<const uint16_t*>(base + peOff + 24);
    uint32_t ddOff;
    if (magic == 0x20B) {        // PE32+
        ddOff = peOff + 24 + 112;
    } else if (magic == 0x10B) { // PE32
        ddOff = peOff + 24 + 96;
    } else {
        return false;
    }

    uint32_t entryOff = ddOff + index * 8;
    if (entryOff + 8 > fileSize) return false;

    *rvaOut  = *reinterpret_cast<const uint32_t*>(base + entryOff);
    *sizeOut = *reinterpret_cast<const uint32_t*>(base + entryOff + 4);
    return (*rvaOut != 0);
}

void Win32IDE::cmdPDBImports()
{
    if (!m_pdbInitialized) initPDBSymbols();

    // Open file dialog for PE files
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"PE Files (*.exe;*.dll)\0*.exe;*.dll\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select PE File — Import Table Analysis";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    // Memory-map the PE file
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        appendToOutput("[PDB] Failed to open PE file", "Output", OutputSeverity::Error);
        return;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        appendToOutput("[PDB] Failed to map PE file", "Output", OutputSeverity::Error);
        return;
    }

    const uint8_t* base = static_cast<const uint8_t*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        appendToOutput("[PDB] Failed to map view of PE file", "Output", OutputSeverity::Error);
        return;
    }

    uint64_t fsize = static_cast<uint64_t>(fileSize.QuadPart);

    // Get import directory (data directory index 1)
    uint32_t importRVA = 0, importSize = 0;
    if (!peGetDataDirectory(base, fsize, 1, &importRVA, &importSize)) {
        appendToOutput("[PDB] No import directory found in PE", "Output", OutputSeverity::Warning);
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    // Resolve RVA to file offset
    uint32_t importFileOff = 0;
    if (!peResolveRVA(base, fsize, importRVA, &importFileOff)) {
        appendToOutput("[PDB] Failed to resolve import directory RVA", "Output", OutputSeverity::Error);
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    // Parse import descriptors
    std::string output;
    output.reserve(8192);
    output += "=== PE Import Table ===\n\n";

    char buf[512];
    char pathA[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathA, MAX_PATH, nullptr, nullptr);
    snprintf(buf, sizeof(buf), "File: %s\n\n", pathA);
    output += buf;

    uint32_t totalImports = 0;
    uint32_t totalDLLs = 0;

    const PE_ImportDescriptor* desc = reinterpret_cast<const PE_ImportDescriptor*>(base + importFileOff);

    while (importFileOff + sizeof(PE_ImportDescriptor) <= fsize &&
           desc->Name != 0)
    {
        // Resolve DLL name RVA
        uint32_t nameOff = 0;
        if (peResolveRVA(base, fsize, desc->Name, &nameOff) && nameOff < fsize) {
            const char* dllName = reinterpret_cast<const char*>(base + nameOff);
            snprintf(buf, sizeof(buf), "  [%u] %s\n", totalDLLs, dllName);
            output += buf;

            // Parse Import Lookup Table (INT) to get function names
            uint32_t iltRVA = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
            uint32_t iltOff = 0;
            if (iltRVA && peResolveRVA(base, fsize, iltRVA, &iltOff)) {
                // Detect PE32+ vs PE32 for thunk size
                uint32_t peOff = *reinterpret_cast<const uint32_t*>(base + 0x3C);
                uint16_t magic = *reinterpret_cast<const uint16_t*>(base + peOff + 24);
                bool isPE32Plus = (magic == 0x20B);

                int funcIdx = 0;
                while (iltOff + (isPE32Plus ? 8 : 4) <= fsize) {
                    uint64_t thunkVal;
                    if (isPE32Plus) {
                        thunkVal = *reinterpret_cast<const uint64_t*>(base + iltOff);
                        iltOff += 8;
                    } else {
                        thunkVal = *reinterpret_cast<const uint32_t*>(base + iltOff);
                        iltOff += 4;
                    }

                    if (thunkVal == 0) break;

                    // Check ordinal import bit
                    bool isOrdinal = isPE32Plus ? (thunkVal & 0x8000000000000000ULL) != 0
                                                : (thunkVal & 0x80000000UL) != 0;
                    if (isOrdinal) {
                        uint16_t ordinal = static_cast<uint16_t>(thunkVal & 0xFFFF);
                        snprintf(buf, sizeof(buf), "      [%d] Ordinal #%u\n", funcIdx, ordinal);
                    } else {
                        // Hint/Name table entry
                        uint32_t hintRVA = static_cast<uint32_t>(thunkVal & 0x7FFFFFFF);
                        uint32_t hintOff = 0;
                        if (peResolveRVA(base, fsize, hintRVA, &hintOff) && hintOff + 2 < fsize) {
                            uint16_t hint = *reinterpret_cast<const uint16_t*>(base + hintOff);
                            const char* funcName = reinterpret_cast<const char*>(base + hintOff + 2);
                            snprintf(buf, sizeof(buf), "      [%d] %s (hint: %u)\n", funcIdx, funcName, hint);
                        } else {
                            snprintf(buf, sizeof(buf), "      [%d] <unresolvable RVA 0x%X>\n", funcIdx, hintRVA);
                        }
                    }
                    output += buf;
                    ++funcIdx;
                    ++totalImports;

                    if (funcIdx > 10000) break; // Safety cap
                }
            }

            ++totalDLLs;
        }

        ++desc;
        importFileOff += sizeof(PE_ImportDescriptor);
        if (totalDLLs > 1000) break; // Safety cap
    }

    snprintf(buf, sizeof(buf), "\n--- Total: %u DLLs, %u imported functions ---\n", totalDLLs, totalImports);
    output += buf;

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    appendToOutput(output, "Output", OutputSeverity::Info);

    // Also cross-reference with loaded PDB symbols
    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    if (pdb.getLoadedModuleCount() > 0) {
        appendToOutput("[PDB] Cross-referencing imports with loaded PDB symbols...", "Output", OutputSeverity::Info);
        // Phase 29.4: deep cross-reference with PDB type info
    }
}

// ============================================================================
// Phase 29.3: PE Export Table Provider — IDM_PDB_EXPORTS
// ============================================================================
void Win32IDE::cmdPDBExports()
{
    if (!m_pdbInitialized) initPDBSymbols();

    // Open file dialog for PE files (typically DLLs)
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"PE Files (*.dll;*.exe;*.sys)\0*.dll;*.exe;*.sys\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select PE File — Export Table Analysis";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    // Memory-map the PE file
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        appendToOutput("[PDB] Failed to open PE file", "Output", OutputSeverity::Error);
        return;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        appendToOutput("[PDB] Failed to map PE file", "Output", OutputSeverity::Error);
        return;
    }

    const uint8_t* base = static_cast<const uint8_t*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        appendToOutput("[PDB] Failed to map view of PE file", "Output", OutputSeverity::Error);
        return;
    }

    uint64_t fsize = static_cast<uint64_t>(fileSize.QuadPart);

    // Get export directory (data directory index 0)
    uint32_t exportRVA = 0, exportSize = 0;
    if (!peGetDataDirectory(base, fsize, 0, &exportRVA, &exportSize)) {
        appendToOutput("[PDB] No export directory found in PE (normal for .exe without exports)",
                       "Output", OutputSeverity::Warning);
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    uint32_t exportFileOff = 0;
    if (!peResolveRVA(base, fsize, exportRVA, &exportFileOff) ||
        exportFileOff + sizeof(PE_ExportDirectory) > fsize) {
        appendToOutput("[PDB] Failed to resolve export directory RVA", "Output", OutputSeverity::Error);
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    const PE_ExportDirectory* expDir = reinterpret_cast<const PE_ExportDirectory*>(base + exportFileOff);

    std::string output;
    output.reserve(8192);
    output += "=== PE Export Table ===\n\n";

    char buf[512];
    char pathA[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathA, MAX_PATH, nullptr, nullptr);
    snprintf(buf, sizeof(buf), "File: %s\n", pathA);
    output += buf;

    // Resolve DLL name
    uint32_t dllNameOff = 0;
    if (peResolveRVA(base, fsize, expDir->Name, &dllNameOff) && dllNameOff < fsize) {
        snprintf(buf, sizeof(buf), "DLL Name: %s\n", reinterpret_cast<const char*>(base + dllNameOff));
        output += buf;
    }

    snprintf(buf, sizeof(buf), "Ordinal Base: %u\nFunctions: %u\nNamed: %u\n\n",
             expDir->Base, expDir->NumberOfFunctions, expDir->NumberOfNames);
    output += buf;

    // Resolve function name array
    uint32_t namesOff = 0, ordinalsOff = 0, functionsOff = 0;
    bool hasNames = peResolveRVA(base, fsize, expDir->AddressOfNames, &namesOff);
    bool hasOrdinals = peResolveRVA(base, fsize, expDir->AddressOfNameOrdinals, &ordinalsOff);
    bool hasFunctions = peResolveRVA(base, fsize, expDir->AddressOfFunctions, &functionsOff);

    if (hasNames && hasOrdinals && hasFunctions) {
        for (uint32_t i = 0; i < expDir->NumberOfNames && i < 50000; ++i) {
            // Get function name
            uint32_t nameRVA = *reinterpret_cast<const uint32_t*>(base + namesOff + i * 4);
            uint16_t ordIdx  = *reinterpret_cast<const uint16_t*>(base + ordinalsOff + i * 2);
            uint32_t funcRVA = *reinterpret_cast<const uint32_t*>(base + functionsOff + ordIdx * 4);

            uint32_t funcNameOff = 0;
            const char* funcName = "<unknown>";
            if (peResolveRVA(base, fsize, nameRVA, &funcNameOff) && funcNameOff < fsize) {
                funcName = reinterpret_cast<const char*>(base + funcNameOff);
            }

            // Check for forwarder (RVA within export directory range)
            bool isForwarder = (funcRVA >= exportRVA && funcRVA < exportRVA + exportSize);
            if (isForwarder) {
                uint32_t fwdOff = 0;
                const char* fwdName = "?";
                if (peResolveRVA(base, fsize, funcRVA, &fwdOff) && fwdOff < fsize) {
                    fwdName = reinterpret_cast<const char*>(base + fwdOff);
                }
                snprintf(buf, sizeof(buf), "  [%u] %s → %s (forwarded)\n",
                         expDir->Base + ordIdx, funcName, fwdName);
            } else {
                snprintf(buf, sizeof(buf), "  [%u] %s (RVA: 0x%08X)\n",
                         expDir->Base + ordIdx, funcName, funcRVA);
            }
            output += buf;
        }
    }

    // Check for ordinal-only exports (functions without names)
    if (hasFunctions && expDir->NumberOfFunctions > expDir->NumberOfNames) {
        uint32_t unnamedCount = expDir->NumberOfFunctions - expDir->NumberOfNames;
        snprintf(buf, sizeof(buf), "\n  + %u ordinal-only exports (no name)\n", unnamedCount);
        output += buf;
    }

    snprintf(buf, sizeof(buf), "\n--- Total: %u named exports, %u total functions ---\n",
             expDir->NumberOfNames, expDir->NumberOfFunctions);
    output += buf;

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);

    appendToOutput(output, "Output", OutputSeverity::Info);
}

// ============================================================================
// Phase 29.3: IAT Status — Show import address table resolution status
// ============================================================================
void Win32IDE::cmdPDBIATStatus()
{
    if (!m_pdbInitialized) initPDBSymbols();

    RawrXD::PDB::PDBManager& pdb = RawrXD::PDB::PDBManager::instance();
    auto stats = pdb.getStats();

    std::string output;
    output.reserve(2048);
    output += "=== Import Address Table Status ===\n\n";

    char buf[256];
    snprintf(buf, sizeof(buf), "PDB Modules Loaded:   %u\n", stats.modulesLoaded);
    output += buf;
    snprintf(buf, sizeof(buf), "Symbols Indexed:      %llu\n",
             static_cast<unsigned long long>(stats.symbolsIndexed));
    output += buf;
    snprintf(buf, sizeof(buf), "Symbol Lookups:       %llu\n",
             static_cast<unsigned long long>(stats.lookupCount));
    output += buf;

    // Per-module IAT coverage
    output += "\n--- Per-Module Coverage ---\n";
    for (uint32_t i = 0; i < pdb.getLoadedModuleCount(); ++i) {
        const char* name = pdb.getLoadedModuleName(i);
        if (!name) continue;

        const RawrXD::PDB::NativePDBParser* parser = pdb.getParser(name);
        if (!parser) continue;

        // Count public symbols as a proxy for IAT coverage
        uint32_t pubSymCount = 0;
        parser->enumeratePublicSymbols(
            [](const RawrXD::PDB::ResolvedSymbol*, void* ud) -> bool {
                (*static_cast<uint32_t*>(ud))++;
                return true;
            }, &pubSymCount);

        snprintf(buf, sizeof(buf), "  %s: %u symbols\n", name, pubSymCount);
        output += buf;
    }

    if (stats.modulesLoaded == 0) {
        output += "\n  No modules loaded. Use PDB → Load or PDB → Imports.\n";
    }

    output += "\n=== End IAT Status ===\n";
    appendToOutput(output, "Output", OutputSeverity::Info);
}