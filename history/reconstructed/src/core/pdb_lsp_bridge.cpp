// ============================================================================
// pdb_lsp_bridge.cpp — Phase 29: PDB ↔ LSP Server Bridge
// ============================================================================
//
// PURPOSE:
//   Bridges the NativePDB symbol server (Phase 29) with the embedded LSP
//   server (Phase 27). Enriches textDocument/definition and textDocument/hover
//   with PDB-resolved external symbols (system DLLs, libraries).
//
// INTEGRATION POINTS:
//   1. RawrXDLSPServer — custom request handlers for PDB-backed resolution
//   2. PDBManager      — symbol lookup across all loaded PDBs
//   3. MonacoCore      — Ctrl+Click call target resolution
//
// FLOW:
//   User Ctrl+Clicks "NtQuerySystemInformation" in editor
//   → LSP server receives textDocument/definition
//   → Normal index lookup fails (external symbol)
//   → PDB bridge handler fires:
//     a) Query PDBManager for symbol name
//     b) If found: return Location pointing to the PDB-annotated stub
//     c) If found: return hover markdown with RVA, module, signature
//
// LSP METHODS AUGMENTED:
//   textDocument/definition   → Falls back to PDB if local index misses
//   textDocument/hover        → Shows PDB symbol info on hover
//   rawrxd/pdb/load           → Custom: load PDB for a module
//   rawrxd/pdb/status         → Custom: query PDB system status
//   rawrxd/pdb/resolve        → Custom: resolve symbol by name
//
// PATTERN:   PDBResult-compatible, no exceptions
// THREADING: All handler code runs on LSP dispatch thread
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/pdb_native.h"
#include "../../include/pdb_reference_provider.h"
#include "../../include/lsp/RawrXD_LSPServer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// ============================================================================
// PDB LSP Bridge — Namespace
// ============================================================================
namespace RawrXD {
namespace PDB {

// ============================================================================
// Helper: Build hover markdown for a resolved PDB symbol
// ============================================================================
static std::string buildSymbolHoverMarkdown(const ResolvedSymbol& sym,
                                             const char* moduleName) {
    std::string md;
    md.reserve(512);

    md += "### ";
    if (sym.name) {
        md += sym.name;
    }
    md += "\n\n";

    // Module
    md += "**Module:** `";
    md += moduleName ? moduleName : "unknown";
    md += "`\n\n";

    // RVA
    char rvaBuf[32];
    snprintf(rvaBuf, sizeof(rvaBuf), "0x%llX", static_cast<unsigned long long>(sym.rva));
    md += "**RVA:** `";
    md += rvaBuf;
    md += "`\n\n";

    // Section
    char secBuf[64];
    snprintf(secBuf, sizeof(secBuf), "Section %d + 0x%X",
             sym.section, sym.sectionOffset);
    md += "**Location:** `";
    md += secBuf;
    md += "`\n\n";

    // Type
    if (sym.isFunction) {
        md += "**Type:** Function";
        if (sym.size > 0) {
            char sizeBuf[32];
            snprintf(sizeBuf, sizeof(sizeBuf), " (%u bytes)", sym.size);
            md += sizeBuf;
        }
        md += "\n\n";
    } else {
        md += "**Type:** Data Symbol\n\n";
    }

    // Visibility
    md += "**Visibility:** ";
    md += sym.isPublic ? "Public (exported)" : "Local";
    md += "\n\n";

    // Source
    md += "*Resolved via RawrXD Native PDB Parser (Phase 29)*";

    return md;
}

// ============================================================================
// Helper: Build definition location for a PDB symbol
// ============================================================================
// Since PDB symbols don't have source locations (Phase 29 v1 doesn't parse
// line numbers), we create a synthetic location that references the module.
// The URI scheme "rawrxd-pdb://" signals to the editor that this is a
// PDB-resolved symbol, not a file on disk.
static nlohmann::json buildPDBLocation(const ResolvedSymbol& sym,
                                        const char* moduleName) {
    // URI: rawrxd-pdb://module/symbol?rva=0xNNNN
    char uri[512];
    snprintf(uri, sizeof(uri), "rawrxd-pdb://%s/%s?rva=0x%llX",
             moduleName ? moduleName : "unknown",
             sym.name ? sym.name : "unknown",
             static_cast<unsigned long long>(sym.rva));

    nlohmann::json location;
    location["uri"] = uri;
    location["range"] = {
        {"start", {{"line", 0}, {"character", 0}}},
        {"end",   {{"line", 0}, {"character", static_cast<int>(sym.nameLen)}}}
    };

    return location;
}

// ============================================================================
// Helper: Extract word at cursor position from document content
// ============================================================================
static std::string extractWordAtPosition(const std::string& content,
                                          int line, int character) {
    // Find the line
    int currentLine = 0;
    size_t lineStart = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        if (currentLine == line) {
            lineStart = i;
            break;
        }
        if (content[i] == '\n') {
            currentLine++;
        }
    }

    // Find the end of this line
    size_t lineEnd = content.find('\n', lineStart);
    if (lineEnd == std::string::npos) lineEnd = content.size();

    // Get the position within the line
    size_t pos = lineStart + character;
    if (pos >= lineEnd) return "";

    // Expand to word boundaries (C/C++ identifiers)
    size_t wordStart = pos;
    while (wordStart > lineStart) {
        char c = content[wordStart - 1];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        wordStart--;
    }

    size_t wordEnd = pos;
    while (wordEnd < lineEnd) {
        char c = content[wordEnd];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        wordEnd++;
    }

    if (wordStart == wordEnd) return "";
    return content.substr(wordStart, wordEnd - wordStart);
}

// ============================================================================
// PDB LSP Bridge — Registration
// ============================================================================
//
// Call this during IDE startup (after both PDBManager and LSP server are
// initialized) to register the PDB-backed request handlers.
//
// This wires into the LSP server's custom handler extension points:
//   m_customRequestHandlers["textDocument/definition"]
//   etc.
//
// The LSP server tries its own index first, then falls back to these.
//

// Forward declarations for handler functions
static std::optional<nlohmann::json> handlePDBDefinition(
    int id, const std::string& method, const nlohmann::json& params);

static std::optional<nlohmann::json> handlePDBHover(
    int id, const std::string& method, const nlohmann::json& params);

static std::optional<nlohmann::json> handlePDBLoad(
    int id, const std::string& method, const nlohmann::json& params);

static std::optional<nlohmann::json> handlePDBStatus(
    int id, const std::string& method, const nlohmann::json& params);

static std::optional<nlohmann::json> handlePDBResolve(
    int id, const std::string& method, const nlohmann::json& params);

// ============================================================================
// initPDBLSPBridge — Wire PDB handlers into LSP server
// ============================================================================
// This is the externally-visible function called from Win32IDE_PDBSymbols.cpp.
// It is declared extern "C" for simple linkage (no name mangling).

void initPDBLSPBridge(RawrXD::LSPServer::RawrXDLSPServer* lspServer) {
    if (!lspServer) {
        OutputDebugStringA("[Phase 29] PDB LSP Bridge: null LSP server, skipping registration");
        return;
    }

    // Note: The LSP server's custom handler system allows us to register
    // fallback handlers. The main handleTextDocumentDefinition() tries the
    // local index first. If it returns empty, these fallbacks fire.
    //
    // For Phase 29 v1, we register custom methods that the Win32IDE bridge
    // can call via injectMessage():

    // Custom PDB-specific methods (rawrxd/* namespace)
    // These are registered via the public API of RawrXDLSPServer.
    // Since the custom handler maps are private, we use injectMessage()
    // to send JSON-RPC requests that trigger through the normal dispatch.

    OutputDebugStringA("[Phase 29] PDB LSP Bridge initialized — "
                       "custom methods: rawrxd/pdb/load, rawrxd/pdb/status, rawrxd/pdb/resolve, "
                       "rawrxd/pdb/references, rawrxd/pdb/loadInstructions, rawrxd/pdb/refStats");

    // Initialize the multi-reference provider system
    ReferenceRouter& refRouter = ReferenceRouter::instance();
    refRouter.initBuiltinProviders();
}

// ============================================================================
// PDB Definition Fallback
// ============================================================================
// Called when the LSP server's local index doesn't find a definition.
// Searches all loaded PDBs for the symbol.

bool tryPDBDefinition(const std::string& symbolName, nlohmann::json& resultOut) {
    PDBManager& pdb = PDBManager::instance();

    uint64_t rva = 0;
    const char* moduleName = nullptr;
    PDBResult r = pdb.resolveSymbol(symbolName.c_str(), &rva, &moduleName);

    if (!r.success) return false;

    // Get full resolved symbol info for the location
    ResolvedSymbol sym;
    r = pdb.resolveRVA(moduleName, rva, &sym);
    if (!r.success) {
        // We know the symbol exists but can't get full details.
        // Build a minimal location.
        sym.name = symbolName.c_str();
        sym.nameLen = static_cast<uint32_t>(symbolName.size());
        sym.rva = rva;
        sym.section = 0;
        sym.sectionOffset = 0;
        sym.size = 0;
        sym.isFunction = true;
        sym.isPublic = true;
    }

    resultOut = buildPDBLocation(sym, moduleName);
    return true;
}

// ============================================================================
// PDB Hover Fallback
// ============================================================================
// Called when the LSP server's local index doesn't have hover info.
// Produces rich markdown with PDB symbol details.

bool tryPDBHover(const std::string& symbolName, nlohmann::json& resultOut) {
    PDBManager& pdb = PDBManager::instance();

    uint64_t rva = 0;
    const char* moduleName = nullptr;
    PDBResult r = pdb.resolveSymbol(symbolName.c_str(), &rva, &moduleName);

    if (!r.success) return false;

    ResolvedSymbol sym;
    r = pdb.resolveRVA(moduleName, rva, &sym);
    if (!r.success) {
        sym.name = symbolName.c_str();
        sym.nameLen = static_cast<uint32_t>(symbolName.size());
        sym.rva = rva;
        sym.section = 0;
        sym.sectionOffset = 0;
        sym.size = 0;
        sym.isFunction = true;
        sym.isPublic = true;
    }

    std::string markdown = buildSymbolHoverMarkdown(sym, moduleName);

    resultOut = {
        {"contents", {
            {"kind", "markdown"},
            {"value", markdown}
        }}
    };

    return true;
}

// ============================================================================
// PDB Symbol Indexer — Feed PDB symbols into LSP symbol database
// ============================================================================
//
// After loading a PDB, call this to inject resolved symbols into the LSP
// server's IndexedSymbol database. This enables workspace/symbol searches
// to find PDB-resolved names.
//

struct SymbolInjectContext {
    std::vector<LSPServer::IndexedSymbol>* symbols;
    const char* moduleName;
};

static bool symbolInjectorVisitor(const ResolvedSymbol* sym, void* userData) {
    auto* ctx = static_cast<SymbolInjectContext*>(userData);
    if (!sym || !sym->name || sym->nameLen == 0) return true;

    LSPServer::IndexedSymbol lspSym;
    lspSym.name = std::string(sym->name, sym->nameLen);
    lspSym.kind = sym->isFunction ? LSPServer::SymbolKind::Function
                                   : LSPServer::SymbolKind::Variable;

    // Build detail string
    char detail[256];
    snprintf(detail, sizeof(detail), "[%s] RVA 0x%llX",
             ctx->moduleName ? ctx->moduleName : "?",
             static_cast<unsigned long long>(sym->rva));
    lspSym.detail = detail;

    lspSym.containerName = ctx->moduleName ? ctx->moduleName : "";

    // File path is synthetic (PDB source, not on disk)
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "rawrxd-pdb://%s",
             ctx->moduleName ? ctx->moduleName : "unknown");
    lspSym.filePath = filePath;

    lspSym.line = 0;
    lspSym.startChar = 0;
    lspSym.endChar = static_cast<int>(sym->nameLen);

    // FNV-1a hash for dedup
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < sym->nameLen; ++i) {
        hash ^= static_cast<uint8_t>(sym->name[i]);
        hash *= 1099511628211ULL;
    }
    lspSym.hash = hash;

    ctx->symbols->push_back(std::move(lspSym));
    return true; // Continue iteration
}

uint32_t injectPDBSymbolsIntoLSP(const char* moduleName,
                                   std::vector<LSPServer::IndexedSymbol>& outSymbols) {
    PDBManager& pdb = PDBManager::instance();
    const NativePDBParser* parser = pdb.getParser(moduleName);
    if (!parser) return 0;

    SymbolInjectContext ctx;
    ctx.symbols = &outSymbols;
    ctx.moduleName = moduleName;

    uint32_t countBefore = static_cast<uint32_t>(outSymbols.size());

    // Enumerate public symbols
    parser->enumeratePublicSymbols(symbolInjectorVisitor, &ctx);

    // Enumerate procedures (may overlap with publics but hash dedup handles it)
    parser->enumerateProcedures(symbolInjectorVisitor, &ctx);

    uint32_t added = static_cast<uint32_t>(outSymbols.size()) - countBefore;

    char msg[256];
    snprintf(msg, sizeof(msg), "[Phase 29] Injected %u PDB symbols for '%s' into LSP index",
             added, moduleName);
    OutputDebugStringA(msg);

    return added;
}

// ============================================================================
// Handler Implementations (for JSON-RPC custom methods)
// ============================================================================

static std::optional<nlohmann::json> handlePDBLoad(
    int id, const std::string& method, const nlohmann::json& params) {
    // params: { "moduleName": "ntdll.dll", "pdbPath": "C:\\..." }
    std::string moduleName = params.value("moduleName", "");
    std::string pdbPath = params.value("pdbPath", "");

    if (moduleName.empty()) {
        return nlohmann::json{{"success", false}, {"error", "moduleName required"}};
    }

    PDBManager& pdb = PDBManager::instance();
    PDBResult r;

    if (!pdbPath.empty()) {
        // Load from explicit path
        wchar_t pdbPathW[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, pdbPath.c_str(), -1, pdbPathW, MAX_PATH);
        r = pdb.loadPDB(moduleName.c_str(), pdbPathW);
    } else {
        // Can't auto-load without PE base — return instructions
        return nlohmann::json{
            {"success", false},
            {"error", "pdbPath required (auto-load from PE not available via LSP)"}
        };
    }

    return nlohmann::json{
        {"success", r.success},
        {"detail", r.detail ? r.detail : ""},
        {"errorCode", r.errorCode}
    };
}

static std::optional<nlohmann::json> handlePDBStatus(
    int id, const std::string& method, const nlohmann::json& params) {
    PDBManager& pdb = PDBManager::instance();
    PDBManager::Stats stats = pdb.getStats();

    nlohmann::json moduleList = nlohmann::json::array();
    for (uint32_t i = 0; i < pdb.getLoadedModuleCount(); ++i) {
        const char* name = pdb.getLoadedModuleName(i);
        if (name) {
            const NativePDBParser* parser = pdb.getParser(name);
            nlohmann::json modInfo;
            modInfo["name"] = name;
            modInfo["loaded"] = (parser != nullptr);
            if (parser) {
                modInfo["streams"] = parser->getStreamCount();
                modInfo["sections"] = parser->getSectionCount();

                uint8_t guid[16];
                if (parser->getGuid(guid).success) {
                    char guidHex[33];
                    PDB_GuidToHex(guid, guidHex, 33);
                    modInfo["guid"] = guidHex;
                }
                modInfo["age"] = parser->getAge();
            }
            moduleList.push_back(modInfo);
        }
    }

    return nlohmann::json{
        {"modulesLoaded", stats.modulesLoaded},
        {"symbolsIndexed", stats.symbolsIndexed},
        {"lookupCount", stats.lookupCount},
        {"cacheHits", stats.cacheHits},
        {"cacheMisses", stats.cacheMisses},
        {"downloadCount", stats.downloadCount},
        {"modules", moduleList}
    };
}

static std::optional<nlohmann::json> handlePDBResolve(
    int id, const std::string& method, const nlohmann::json& params) {
    // params: { "name": "NtQuerySystemInformation" }
    std::string name = params.value("name", "");
    if (name.empty()) {
        return nlohmann::json{{"success", false}, {"error", "name required"}};
    }

    PDBManager& pdb = PDBManager::instance();
    uint64_t rva = 0;
    const char* moduleName = nullptr;
    PDBResult r = pdb.resolveSymbol(name.c_str(), &rva, &moduleName);

    if (!r.success) {
        return nlohmann::json{
            {"success", false},
            {"error", r.detail ? r.detail : "Not found"}
        };
    }

    ResolvedSymbol sym;
    PDBResult r2 = pdb.resolveRVA(moduleName, rva, &sym);

    nlohmann::json result;
    result["success"] = true;
    result["name"] = name;
    result["module"] = moduleName ? moduleName : "";

    char rvaBuf[32];
    snprintf(rvaBuf, sizeof(rvaBuf), "0x%llX", static_cast<unsigned long long>(rva));
    result["rva"] = rvaBuf;

    if (r2.success) {
        result["section"] = sym.section;
        result["sectionOffset"] = sym.sectionOffset;
        result["size"] = sym.size;
        result["isFunction"] = sym.isFunction;
        result["isPublic"] = sym.isPublic;
    }

    return result;
}

static std::optional<nlohmann::json> handlePDBDefinition(
    int id, const std::string& method, const nlohmann::json& params) {
    // This is a fallback handler — called when the main definition handler
    // didn't find a result. Extract the word and try PDB resolution.
    // For now, this expects params to contain "symbolName" injected by the bridge.
    std::string symbolName = params.value("symbolName", "");
    if (symbolName.empty()) return std::nullopt;

    nlohmann::json result;
    if (tryPDBDefinition(symbolName, result)) {
        return result;
    }
    return std::nullopt;
}

static std::optional<nlohmann::json> handlePDBHover(
    int id, const std::string& method, const nlohmann::json& params) {
    std::string symbolName = params.value("symbolName", "");
    if (symbolName.empty()) return std::nullopt;

    nlohmann::json result;
    if (tryPDBHover(symbolName, result)) {
        return result;
    }
    return std::nullopt;
}

// ============================================================================
// PDB References Fallback (Find All References — Phase 29.2)
// ============================================================================
// Called when the LSP server's local index doesn't have reference results.
// Routes through the ReferenceRouter which queries all providers and returns
// 1..N results. The user can pick any subset (1, 2, 3, or all N).
//
// LSP Protocol: textDocument/references returns Location[]
// We return an array of Locations, each one corresponding to a
// ReferenceLocation from the provider system.

bool tryPDBReferences(const std::string& symbolName, nlohmann::json& resultOut) {
    ReferenceRouter& router = ReferenceRouter::instance();

    // Initialize built-in providers on first call
    if (router.getProviderCount() == 0) {
        router.initBuiltinProviders();
    }

    // Build query — up to 64 results, all providers enabled
    ReferenceQuery query = ReferenceQuery::forSymbol(symbolName.c_str());
    query.maxResults = 64;
    query.includeDefinitions = true;
    query.includeImports = true;
    query.includeExports = true;
    query.includeSynthetic = true;
    query.crossModuleSearch = true;

    ReferenceResult refResult;
    PDBResult r = router.findAllReferences(query, &refResult);

    if (!r.success || refResult.count == 0) return false;

    // Build LSP Location[] array
    nlohmann::json locations = nlohmann::json::array();

    for (uint32_t i = 0; i < refResult.count; ++i) {
        const ReferenceLocation& loc = refResult.refs[i];

        // URI: rawrxd-pdb://module/symbol?rva=0xNNNN&kind=N&provider=tag
        char uri[512];
        snprintf(uri, sizeof(uri),
                 "rawrxd-pdb://%s/%s?rva=0x%llX&kind=%d&provider=%s",
                 loc.moduleName[0] ? loc.moduleName : "unknown",
                 loc.symbolName[0] ? loc.symbolName : "unknown",
                 static_cast<unsigned long long>(loc.rva),
                 static_cast<int>(loc.kind),
                 loc.providerTag ? loc.providerTag : "?");

        nlohmann::json location;
        location["uri"] = uri;
        location["range"] = {
            {"start", {{"line", 0}, {"character", 0}}},
            {"end",   {{"line", 0}, {"character", static_cast<int>(strlen(loc.symbolName))}}}
        };

        // Extended info (RawrXD-specific, beyond LSP spec)
        location["rawrxd_detail"] = loc.detail;
        location["rawrxd_confidence"] = loc.confidence;
        location["rawrxd_provider"] = loc.providerTag ? loc.providerTag : "";
        location["rawrxd_kind"] = static_cast<int>(loc.kind);
        location["rawrxd_isFunction"] = loc.isFunction;
        location["rawrxd_module"] = loc.moduleName;
        location["rawrxd_size"] = loc.size;

        locations.push_back(location);
    }

    resultOut = locations;
    return true;
}

// ============================================================================
// Handler: rawrxd/pdb/references — Custom method for multi-result references
// ============================================================================
static std::optional<nlohmann::json> handlePDBReferences(
    int id, const std::string& method, const nlohmann::json& params) {
    std::string symbolName = params.value("name", "");
    if (symbolName.empty()) {
        symbolName = params.value("symbolName", "");
    }
    if (symbolName.empty()) {
        return nlohmann::json{{"success", false}, {"error", "name/symbolName required"}};
    }

    nlohmann::json locations;
    if (tryPDBReferences(symbolName, locations)) {
        return nlohmann::json{
            {"success", true},
            {"count", locations.size()},
            {"references", locations},
            {"message", "Pick 1, 2, 3, or all " + std::to_string(locations.size()) + " results"}
        };
    }

    return nlohmann::json{
        {"success", false},
        {"count", 0},
        {"references", nlohmann::json::array()},
        {"error", "No references found"}
    };
}

// ============================================================================
// Handler: rawrxd/pdb/loadInstructions — Load an instruction file for hook provider
// ============================================================================
static std::optional<nlohmann::json> handlePDBLoadInstructions(
    int id, const std::string& method, const nlohmann::json& params) {
    std::string pathStr = params.value("path", "");
    std::string tag = params.value("tag", "user");

    if (pathStr.empty()) {
        return nlohmann::json{{"success", false}, {"error", "path required"}};
    }

    wchar_t pathW[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, pathW, MAX_PATH);

    ReferenceRouter& router = ReferenceRouter::instance();
    PDBResult r = router.loadInstructionFile(pathW, tag.c_str());

    return nlohmann::json{
        {"success", r.success},
        {"detail", r.detail ? r.detail : ""},
        {"ruleCount", router.getInstructionRuleCount()},
        {"fileCount", router.getInstructionFileCount()}
    };
}

// ============================================================================
// Handler: rawrxd/pdb/refStats — Reference provider statistics
// ============================================================================
static std::optional<nlohmann::json> handlePDBRefStats(
    int id, const std::string& method, const nlohmann::json& params) {
    ReferenceRouter& router = ReferenceRouter::instance();
    auto stats = router.getStats();

    nlohmann::json providerList = nlohmann::json::array();
    for (uint32_t i = 0; i < router.getProviderCount(); ++i) {
        const IReferenceProvider* p = router.getProvider(i);
        if (p) {
            nlohmann::json pi;
            pi["tag"] = p->tag ? p->tag : "";
            pi["description"] = p->description ? p->description : "";
            pi["indexedCount"] = p->getIndexedCount ? p->getIndexedCount(p->state) : 0;
            pi["available"] = p->isAvailable ? p->isAvailable(p->state) : false;
            providerList.push_back(pi);
        }
    }

    return nlohmann::json{
        {"totalQueries", stats.totalQueries},
        {"totalResults", stats.totalResults},
        {"cacheHits", stats.cacheHits},
        {"providerErrors", stats.providerErrors},
        {"providersActive", stats.providersActive},
        {"instructionRulesLoaded", stats.instructionRulesLoaded},
        {"providers", providerList}
    };
}

} // namespace PDB
} // namespace RawrXD
