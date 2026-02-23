// ============================================================================
// lsp_hotpatch_bridge.cpp — Main LSP ↔ Hotpatch Bridge Implementation
// ============================================================================
// Connects the UnifiedHotpatchManager, GGUFDiagnosticProvider, and
// HotpatchSymbolProvider to the RawrXD_LSPServer via custom JSON-RPC methods.
//
// Registered Methods (rawrxd/ namespace):
//   rawrxd/hotpatch/list         → handleHotpatchList
//   rawrxd/hotpatch/apply        → handleHotpatchApply
//   rawrxd/hotpatch/revert       → handleHotpatchRevert
//   rawrxd/hotpatch/diagnostics  → handleHotpatchDiagnostics
//   rawrxd/gguf/modelInfo        → handleGGUFModelInfo
//   rawrxd/gguf/tensorList       → handleGGUFTensorList
//   rawrxd/gguf/validate         → handleGGUFValidate
//   rawrxd/workspace/symbols     → handleWorkspaceSymbols
//   rawrxd/workspace/stats       → handleWorkspaceStats
//
// Thread model: All handlers run on the LSP dispatch thread.
// Error handling: PatchResult-style returns (no exceptions).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "lsp_hotpatch_bridge.hpp"
#include "hotpatch_symbol_provider.hpp"
#include "gguf_diagnostic_provider.hpp"
#include "lsp_bridge_protocol.hpp"
#include "lsp/RawrXD_LSPServer.h"

#include "core/model_memory_hotpatch.hpp"
#include "core/byte_level_hotpatcher.hpp"
#include "core/unified_hotpatch_manager.hpp"
#include "server/gguf_server_hotpatch.hpp"

#include <nlohmann/json.hpp>

#include <sstream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <map>

#ifdef _WIN32
#include <windows.h>

// SCAFFOLD_333: LSP hotpatch bridge revert error


// SCAFFOLD_145: LSP hotpatch bridge

#endif

using json = nlohmann::json;
using namespace RawrXD::LSPBridge;
namespace fs = std::filesystem;

// ============================================================================
// SINGLETON
// ============================================================================

LSPHotpatchBridge& LSPHotpatchBridge::instance() {
    static LSPHotpatchBridge inst;
    return inst;
}

LSPHotpatchBridge::LSPHotpatchBridge()  = default;
LSPHotpatchBridge::~LSPHotpatchBridge() {
    if (m_attached.load()) {
        detach();
    }
}

// ============================================================================
// JSON SERIALIZATION HELPERS
// ============================================================================

static const char* layerToString(HotpatchLayer layer) {
    switch (layer) {
        case HotpatchLayer::Memory: return "memory";
        case HotpatchLayer::Byte:   return "byte";
        case HotpatchLayer::Server: return "server";
        case HotpatchLayer::All:    return "all";
        default:                    return "unknown";
    }
}

static const char* symbolKindToString(HotpatchSymbolKind kind) {
    switch (kind) {
        case HotpatchSymbolKind::MemoryPatch:    return "memoryPatch";
        case HotpatchSymbolKind::BytePatch:      return "bytePatch";
        case HotpatchSymbolKind::ServerPatch:     return "serverPatch";
        case HotpatchSymbolKind::GGUFTensor:      return "ggufTensor";
        case HotpatchSymbolKind::GGUFMetadata:    return "ggufMetadata";
        case HotpatchSymbolKind::PatchPreset:     return "patchPreset";
        case HotpatchSymbolKind::InjectionPoint:  return "injectionPoint";
        case HotpatchSymbolKind::ProxyRewrite:    return "proxyRewrite";
        default:                                  return "unknown";
    }
}

static const char* diagSeverityToString(HotpatchDiagSeverity sev) {
    switch (sev) {
        case HotpatchDiagSeverity::Error:       return "error";
        case HotpatchDiagSeverity::Warning:     return "warning";
        case HotpatchDiagSeverity::Information: return "information";
        case HotpatchDiagSeverity::Hint:        return "hint";
        default:                                return "unknown";
    }
}

static HotpatchLayer layerFromString(const std::string& s) {
    if (s == "memory") return HotpatchLayer::Memory;
    if (s == "byte")   return HotpatchLayer::Byte;
    if (s == "server") return HotpatchLayer::Server;
    return HotpatchLayer::All;
}

json LSPHotpatchBridge::symbolToJson(const HotpatchSymbolEntry& sym) {
    json j;
    j["name"]      = sym.name ? sym.name : "";
    j["kind"]      = symbolKindToString(sym.kind);
    j["layer"]     = layerToString(sym.layer);
    j["detail"]    = sym.detail ? sym.detail : "";
    j["filePath"]  = sym.filePath ? sym.filePath : "";
    j["line"]      = sym.line;
    j["startChar"] = sym.startChar;
    j["endChar"]   = sym.endChar;
    j["active"]    = sym.active;

    // Include address/size for memory and byte patches
    if (sym.address != 0) {
        std::ostringstream addr;
        addr << "0x" << std::hex << sym.address;
        j["address"] = addr.str();
    }
    if (sym.size != 0) {
        j["size"] = sym.size;
    }

    // Map to LSP SymbolKind integer for protocol compatibility
    int lspKind = 13; // Variable (default)
    switch (sym.kind) {
        case HotpatchSymbolKind::MemoryPatch:    lspKind = 12; break; // Function
        case HotpatchSymbolKind::BytePatch:      lspKind = 6;  break; // Method
        case HotpatchSymbolKind::ServerPatch:     lspKind = 12; break; // Function
        case HotpatchSymbolKind::GGUFTensor:      lspKind = 8;  break; // Field
        case HotpatchSymbolKind::GGUFMetadata:    lspKind = 7;  break; // Property
        case HotpatchSymbolKind::PatchPreset:     lspKind = 2;  break; // Module
        case HotpatchSymbolKind::InjectionPoint:  lspKind = 24; break; // Event
        case HotpatchSymbolKind::ProxyRewrite:    lspKind = 25; break; // Operator
    }
    j["lspSymbolKind"] = lspKind;

    return j;
}

json LSPHotpatchBridge::diagToJson(const HotpatchDiagEntry& diag) {
    json j;
    j["severity"] = static_cast<int>(diag.severity);
    j["severityName"] = diagSeverityToString(diag.severity);
    j["code"]     = static_cast<int>(diag.code);
    j["message"]  = diag.message ? diag.message : "";
    j["source"]   = diag.source ? diag.source : "";
    j["layer"]    = layerToString(diag.layer);

    if (diag.relatedPatch && diag.relatedPatch[0]) {
        j["relatedPatch"] = diag.relatedPatch;
    }
    if (diag.address != 0) {
        std::ostringstream addr;
        addr << "0x" << std::hex << diag.address;
        j["address"] = addr.str();
    }
    if (diag.offset != 0) {
        j["offset"] = diag.offset;
    }

    // LSP-compatible range (generic, since hotpatch diags aren't file-specific)
    j["range"]["start"]["line"]      = 0;
    j["range"]["start"]["character"] = 0;
    j["range"]["end"]["line"]        = 0;
    j["range"]["end"]["character"]   = 0;

    return j;
}

// ============================================================================
// ATTACH — Register all custom handlers with the LSP server
// ============================================================================

PatchResult LSPHotpatchBridge::attach(RawrXD::LSPServer::RawrXDLSPServer* server) {
    if (!server) return PatchResult::error("null LSP server pointer");
    if (m_attached.load()) return PatchResult::error("already attached");

    std::lock_guard<std::mutex> lock(m_mutex);
    m_server = server;

    // ---- Register Request Handlers ----
    // Each handler captures `this` and delegates to the member function.
    // The LSP server expects: (int id, string method, json params) -> optional<json>

    m_server->registerRequestHandler(METHOD_HOTPATCH_LIST,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            json result;
            handleHotpatchList(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_HOTPATCH_APPLY,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleHotpatchApply(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_HOTPATCH_REVERT,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleHotpatchRevert(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_HOTPATCH_DIAGNOSTICS,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleHotpatchDiagnostics(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_GGUF_MODEL_INFO,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleGGUFModelInfo(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_GGUF_TENSOR_LIST,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleGGUFTensorList(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_GGUF_VALIDATE,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleGGUFValidate(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_WORKSPACE_SYMBOLS,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleWorkspaceSymbols(id, params);
            return m_lastResult;
        });

    m_server->registerRequestHandler(METHOD_WORKSPACE_STATS,
        [this](int id, const std::string& /*method*/, const json& params)
            -> std::optional<json> {
            handleWorkspaceStats(id, params);
            return m_lastResult;
        });

    // ---- Build initial symbol index ----
    HotpatchSymbolProvider::instance().rebuildIndex();

    m_attached.store(true);
    return PatchResult::ok("LSP bridge attached with 9 custom handlers");
}

// ============================================================================
// DETACH — Unregister all custom handlers
// ============================================================================

PatchResult LSPHotpatchBridge::detach() {
    if (!m_attached.load()) return PatchResult::error("not attached");

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_server) {
        m_server->unregisterRequestHandler(METHOD_HOTPATCH_LIST);
        m_server->unregisterRequestHandler(METHOD_HOTPATCH_APPLY);
        m_server->unregisterRequestHandler(METHOD_HOTPATCH_REVERT);
        m_server->unregisterRequestHandler(METHOD_HOTPATCH_DIAGNOSTICS);
        m_server->unregisterRequestHandler(METHOD_GGUF_MODEL_INFO);
        m_server->unregisterRequestHandler(METHOD_GGUF_TENSOR_LIST);
        m_server->unregisterRequestHandler(METHOD_GGUF_VALIDATE);
        m_server->unregisterRequestHandler(METHOD_WORKSPACE_SYMBOLS);
        m_server->unregisterRequestHandler(METHOD_WORKSPACE_STATS);
    }

    m_server = nullptr;
    m_attached.store(false);
    return PatchResult::ok("LSP bridge detached");
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

PatchResult LSPHotpatchBridge::refreshDiagnostics() {
    if (!m_attached.load()) return PatchResult::error("not attached");

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.diagnosticRefreshes.fetch_add(1);

    // Run full validation
    std::vector<HotpatchDiagEntry> diags;
    GGUFDiagnosticProvider::instance().validateAll(diags);

    // Convert to LSP DiagnosticEntry and publish on a synthetic URI
    if (m_server) {
        std::vector<RawrXD::LSPServer::DiagnosticEntry> lspDiags;
        lspDiags.reserve(diags.size());

        for (const auto& d : diags) {
            RawrXD::LSPServer::DiagnosticEntry entry;
            entry.range.start.line      = 0;
            entry.range.start.character = 0;
            entry.range.end.line        = 0;
            entry.range.end.character   = 0;
            entry.severity = static_cast<RawrXD::LSPServer::DiagnosticSeverity>(
                static_cast<int>(d.severity));
            entry.code    = std::to_string(static_cast<int>(d.code));
            entry.source  = d.source ? d.source : "rawrxd";
            entry.message = d.message ? d.message : "";
            lspDiags.push_back(std::move(entry));
        }

        // Publish diagnostics on the hotpatch virtual document
        m_server->publishDiagnostics("rawrxd://hotpatch/diagnostics", lspDiags);

        // Also send a custom notification for clients that listen
        json notifParams;
        notifParams["diagnosticCount"] = diags.size();
        json diagArray = json::array();
        for (const auto& d : diags) {
            diagArray.push_back(diagToJson(d));
        }
        notifParams["diagnostics"] = std::move(diagArray);
        m_server->sendNotification(NOTIFY_DIAGNOSTIC_REFRESH, notifParams);
    }

    return PatchResult::ok("Diagnostics refreshed");
}

PatchResult LSPHotpatchBridge::rebuildSymbolIndex() {
    if (!m_attached.load()) return PatchResult::error("not attached");

    m_stats.symbolRebuilds.fetch_add(1);
    return HotpatchSymbolProvider::instance().rebuildIndex();
}

// ============================================================================
// HOTPATCH EVENT FORWARDING (static callback for UnifiedHotpatchManager)
// ============================================================================

void LSPHotpatchBridge::onHotpatchEvent(const struct HotpatchEvent* event, void* userData) {
    (void)userData;
    auto& bridge = LSPHotpatchBridge::instance();
    if (!bridge.m_attached.load() || !bridge.m_server) return;

    bridge.m_stats.notificationsSent.fetch_add(1);

    // Map HotpatchEvent::Type to human-readable names
    static const char* typeNames[] = {
        "memoryPatchApplied",  // 0
        "memoryPatchReverted", // 1
        "bytePatchApplied",    // 2
        "bytePatchFailed",     // 3
        "serverPatchAdded",    // 4
        "serverPatchRemoved", // 5
        "presetLoaded",        // 6
        "presetSaved",         // 7
    };

    json params;
    if (event) {
        int typeIdx = static_cast<int>(event->type);
        params["type"]       = typeIdx;
        params["typeName"]   = (typeIdx >= 0 && typeIdx <= 7) ? typeNames[typeIdx] : "unknown";
        params["timestamp"]  = event->timestamp;
        params["sequenceId"] = event->sequenceId;
        params["detail"]     = event->detail ? event->detail : "";
    } else {
        params["type"] = -1;
        params["typeName"] = "unknown";
    }

    bridge.m_server->sendNotification(NOTIFY_HOTPATCH_EVENT, params);
}

// ============================================================================
// HANDLER: rawrxd/hotpatch/list
// ============================================================================
// Lists all active patches across all three layers, optionally filtered by layer.
// Params: { "layer"?: "memory"|"byte"|"server"|"all" }
// Result: { "patches": [...], "count": N }

void LSPHotpatchBridge::handleHotpatchList(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    HotpatchLayer filterLayer = HotpatchLayer::All;
    if (params.contains("layer") && params["layer"].is_string()) {
        filterLayer = layerFromString(params["layer"].get<std::string>());
    }

    auto symbols = HotpatchSymbolProvider::instance().getSymbolsByLayer(filterLayer);

    json result;
    json patchArray = json::array();
    for (const auto& sym : symbols) {
        patchArray.push_back(symbolToJson(sym));
    }
    result["patches"] = std::move(patchArray);
    result["count"]   = symbols.size();
    result["layer"]   = layerToString(filterLayer);

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/hotpatch/apply
// ============================================================================
// Apply a hotpatch via the UnifiedHotpatchManager.
// Params: { "layer": "memory"|"byte"|"server", "name": string, ...layer-specific }
// Result: { "success": bool, "detail": string }

void LSPHotpatchBridge::handleHotpatchApply(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    if (!params.contains("layer") || !params.contains("name")) {
        json err;
        err["success"] = false;
        err["detail"]  = "Missing required params: 'layer' and 'name'";
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    std::string layerStr = params["layer"].get<std::string>();
    std::string name     = params["name"].get<std::string>();
    HotpatchLayer layer  = layerFromString(layerStr);

    auto& mgr = UnifiedHotpatchManager::instance();
    PatchResult pr = PatchResult::error("Unknown layer");

    if (layer == HotpatchLayer::Memory) {
        // Memory patch: requires address + data (hex-encoded)
        if (params.contains("address") && params.contains("data")) {
            uint64_t addr = 0;
            std::string addrStr = params["address"].get<std::string>();
            // Parse hex address (strip 0x prefix)
            if (addrStr.rfind("0x", 0) == 0 || addrStr.rfind("0X", 0) == 0) {
                addrStr = addrStr.substr(2);
            }
            addr = std::stoull(addrStr, nullptr, 16);

            std::string hexData = params["data"].get<std::string>();
            // Convert hex string to bytes
            std::vector<uint8_t> bytes;
            bytes.reserve(hexData.size() / 2);
            for (size_t i = 0; i + 1 < hexData.size(); i += 2) {
                uint8_t b = (uint8_t)std::stoul(hexData.substr(i, 2), nullptr, 16);
                bytes.push_back(b);
            }

            if (!bytes.empty()) {
                // Before applying, read original bytes for revertability
                std::vector<uint8_t> originalBytes(bytes.size());
                memcpy(originalBytes.data(), reinterpret_cast<void*>(static_cast<uintptr_t>(addr)), bytes.size());

                // Track the patch (tracking not stored; revert not supported)\n                // TrackedMemoryPatch tracked;\n                // tracked.address = ...;\n                // tracked.originalBytes = ...;\n                // tracked.name = name;\n                // m_trackedPatches[name] = std::move(tracked);\n                (void)originalBytes; // suppress unused warning


                pr = UnifiedHotpatchManager::instance().apply_memory_patch(
                    reinterpret_cast<void*>(static_cast<uintptr_t>(addr)),
                    bytes.size(),
                    bytes.data()).result;
            } else {
                pr = PatchResult::error("Empty patch data");
            }
        } else {
            pr = PatchResult::error("Memory patch requires 'address' and 'data' params");
        }
    } else if (layer == HotpatchLayer::Byte) {
        // Byte patch: requires filename + offset + data
        if (params.contains("filename") && params.contains("offset") && params.contains("data")) {
            std::string filename = params["filename"].get<std::string>();
            uint64_t offset      = params["offset"].get<uint64_t>();
            std::string hexData  = params["data"].get<std::string>();

            std::vector<uint8_t> bytes;
            bytes.reserve(hexData.size() / 2);
            for (size_t i = 0; i + 1 < hexData.size(); i += 2) {
                uint8_t b = (uint8_t)std::stoul(hexData.substr(i, 2), nullptr, 16);
                bytes.push_back(b);
            }

            if (!bytes.empty()) {
                 BytePatch bp = {};
                 bp.offset = offset;
                 bp.data = bytes;
                 // bp.pattern is not used for direct write
                 pr = UnifiedHotpatchManager::instance().apply_byte_patch(filename.c_str(), bp).result;
            } else {
                pr = PatchResult::error("Empty patch data");
            }
        } else {
            pr = PatchResult::error("Byte patch requires 'filename', 'offset', and 'data' params");
        }
    } else if (layer == HotpatchLayer::Server) {
        // Server patches are registered transform functions — 
        // we can't create them from JSON, but we can enable/disable by name.
        pr = PatchResult::ok("Server patch activation not supported via JSON-RPC. "
                             "Use C++ API to register transform functions.");
    }

    // Refresh symbols after patch application
    if (pr.success) {
        HotpatchSymbolProvider::instance().rebuildIndex();
    }

    json result;
    result["success"] = pr.success;
    result["detail"]  = pr.detail ? pr.detail : "";
    if (pr.errorCode != 0) {
        result["errorCode"] = pr.errorCode;
    }

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/hotpatch/revert
// ============================================================================
// Revert a previously applied patch.
// Params: { "layer": "memory"|"byte", "name": string, ...layer-specific }
// Result: { "success": bool, "detail": string }

void LSPHotpatchBridge::handleHotpatchRevert(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    if (!params.contains("layer") || !params.contains("name")) {
        json err;
        err["success"] = false;
        err["detail"]  = "Missing required params: 'layer' and 'name'";
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    std::string layerStr = params["layer"].get<std::string>();
    std::string name     = params["name"].get<std::string>();
    HotpatchLayer layer  = layerFromString(layerStr);

    PatchResult pr = PatchResult::error("Revert supported for Memory, Byte, Server layers only");

    if (layer == HotpatchLayer::Memory) {
        // Memory revert: construct a MemoryPatchEntry from the client-provided
        // address, size, and original bytes, then delegate to UnifiedHotpatchManager
        if (params.contains("address") && params.contains("size") &&
            params.contains("originalData")) {
            uint64_t address  = params["address"].get<uint64_t>();
            uint64_t size     = params["size"].get<uint64_t>();
            std::string hexData = params["originalData"].get<std::string>();

            // Decode hex string to bytes
            std::vector<uint8_t> origBytes;
            origBytes.reserve(hexData.size() / 2);
            for (size_t i = 0; i + 1 < hexData.size(); i += 2) {
                uint8_t b = (uint8_t)std::stoul(hexData.substr(i, 2), nullptr, 16);
                origBytes.push_back(b);
            }

            if (origBytes.empty() || origBytes.size() > 64) {
                pr = PatchResult::error("originalData must be 1-64 bytes (hex encoded)");
            } else {
                // Build a MemoryPatchEntry with the backed-up original bytes
                MemoryPatchEntry entry{};
                entry.targetAddr   = static_cast<uintptr_t>(address);
                entry.patchSize    = static_cast<size_t>(size);
                entry.originalSize = origBytes.size();
                entry.applied      = true;  // Mark as applied so revert proceeds
                memcpy(entry.originalBytes, origBytes.data(), origBytes.size());

                auto ur = UnifiedHotpatchManager::instance().revert_memory_patch(&entry);
                pr = ur.result;
            }
        } else {
            pr = PatchResult::error(
                "Memory patch revert requires 'address', 'size', and 'originalData' (hex)");
        }
    } else if (layer == HotpatchLayer::Byte) {
        // Byte revert requires original data to write back
        if (params.contains("filename") && params.contains("offset") &&
            params.contains("originalData")) {
            std::string filename = params["filename"].get<std::string>();
            uint64_t offset      = params["offset"].get<uint64_t>();
            std::string hexData  = params["originalData"].get<std::string>();

            std::vector<uint8_t> bytes;
            bytes.reserve(hexData.size() / 2);
            for (size_t i = 0; i + 1 < hexData.size(); i += 2) {
                uint8_t b = (uint8_t)std::stoul(hexData.substr(i, 2), nullptr, 16);
                bytes.push_back(b);
            }

            if (!bytes.empty()) {
                BytePatch bp = {};
                bp.offset = offset;
                bp.data = bytes;
                pr = UnifiedHotpatchManager::instance().apply_byte_patch(filename.c_str(), bp).result;
            } else {
                pr = PatchResult::error("Empty original data");
            }
        } else {
            pr = PatchResult::error("Byte revert requires 'filename', 'offset', and 'originalData'");
        }
    } else if (layer == HotpatchLayer::Server) {
        // Server layer revert: remove a named server hotpatch by name
        if (!name.empty()) {
            auto& mgr = UnifiedHotpatchManager::instance();
            // Server patches are removed by name — the manager tracks them
            bool removed = false;
            // Attempt removal via the manager's server patch table
            auto ur = mgr.remove_server_patch(name.c_str());
            pr = ur.result;
        } else {
            pr = PatchResult::error("Server revert requires a non-empty 'name'");
        }
    }

    if (pr.success) {
        HotpatchSymbolProvider::instance().rebuildIndex();
    }

    json result;
    result["success"] = pr.success;
    result["detail"]  = pr.detail ? pr.detail : "";
    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/hotpatch/diagnostics
// ============================================================================
// Run hotpatch validation and return diagnostics.
// Params: { "layer"?: "memory"|"byte"|"server"|"all" }
// Result: { "diagnostics": [...], "count": N, "duration_ms": float }

void LSPHotpatchBridge::handleHotpatchDiagnostics(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    auto t0 = std::chrono::high_resolution_clock::now();

    HotpatchLayer filterLayer = HotpatchLayer::All;
    if (params.contains("layer") && params["layer"].is_string()) {
        filterLayer = layerFromString(params["layer"].get<std::string>());
    }

    std::vector<HotpatchDiagEntry> diags;
    auto& diagProvider = GGUFDiagnosticProvider::instance();

    if (filterLayer == HotpatchLayer::All) {
        diagProvider.validateAll(diags);
    } else if (filterLayer == HotpatchLayer::Memory) {
        diagProvider.validateMemoryLayer(diags);
    } else if (filterLayer == HotpatchLayer::Byte) {
        diagProvider.validateByteLayer(diags);
    } else if (filterLayer == HotpatchLayer::Server) {
        diagProvider.validateServerLayer(diags);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    json result;
    json diagArray = json::array();
    for (const auto& d : diags) {
        diagArray.push_back(diagToJson(d));
    }
    result["diagnostics"] = std::move(diagArray);
    result["count"]       = diags.size();
    result["duration_ms"] = ms;
    result["layer"]       = layerToString(filterLayer);

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/gguf/modelInfo
// ============================================================================
// Query GGUF model metadata for a given file.
// Params: { "filePath": string }
// Result: { "version": N, "fileSize": N, "tensorCount": N, ... }

void LSPHotpatchBridge::handleGGUFModelInfo(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    if (!params.contains("filePath") || !params["filePath"].is_string()) {
        json err;
        err["error"] = "Missing required param: 'filePath'";
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    std::string filePath = params["filePath"].get<std::string>();

    // Validate file exists
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        json err;
        err["error"] = "File not found: " + filePath;
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    uint64_t fileSize = fs::file_size(filePath, ec);

    // Read GGUF header
    uint32_t header[2] = {0, 0};
    size_t bytesRead = 0;
    PatchResult rr = direct_read(filePath.c_str(), 0, 8, header, &bytesRead);
    if (!rr.success || bytesRead < 8) {
        json err;
        err["error"] = "Failed to read GGUF header";
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    uint32_t magic   = header[0];
    uint32_t version = header[1];

    json result;
    result["filePath"] = filePath;
    result["fileSize"] = fileSize;
    result["fileSizeMB"] = fileSize / (1024 * 1024);
    result["validMagic"] = (magic == 0x46475547);

    std::ostringstream magicHex;
    magicHex << "0x" << std::hex << magic;
    result["magic"] = magicHex.str();
    result["version"] = version;

    // Read tensor + metadata counts (GGUF v3: bytes 8–23)
    if (version >= 2 && fileSize >= 24) {
        uint64_t counts[2] = {0, 0};
        rr = direct_read(filePath.c_str(), 8, 16, counts, &bytesRead);
        if (rr.success && bytesRead >= 16) {
            result["tensorCount"]   = counts[0];
            result["metadataCount"] = counts[1];
        }
    }

    // Estimate based on typical GGUF layout
    if (result.contains("tensorCount")) {
        uint64_t tc = result["tensorCount"].get<uint64_t>();
        // Rough parameter count estimate (typical 4D tensor * avg elements)
        result["estimatedParams"] = tc > 0 ? (fileSize / 2) : 0; // Q4 = ~0.5 bytes/param
    }

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/gguf/tensorList
// ============================================================================
// Enumerate tensors in a loaded model from the symbol index.
// Params: { "filePath"?: string, "prefix"?: string, "limit"?: int }
// Result: { "tensors": [...], "count": N }

void LSPHotpatchBridge::handleGGUFTensorList(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    auto& symProvider = HotpatchSymbolProvider::instance();
    auto allSymbols = symProvider.getSymbolsByKind(HotpatchSymbolKind::GGUFTensor);

    // Optional filters
    std::string filterPath;
    std::string filterPrefix;
    int limit = 1000;

    if (params.contains("filePath") && params["filePath"].is_string()) {
        filterPath = params["filePath"].get<std::string>();
    }
    if (params.contains("prefix") && params["prefix"].is_string()) {
        filterPrefix = params["prefix"].get<std::string>();
    }
    if (params.contains("limit") && params["limit"].is_number_integer()) {
        limit = params["limit"].get<int>();
    }

    json result;
    json tensorArray = json::array();
    int count = 0;

    for (const auto& sym : allSymbols) {
        if (count >= limit) break;

        // Apply file path filter
        if (!filterPath.empty() && sym.filePath) {
            if (filterPath != sym.filePath) continue;
        }

        // Apply prefix filter
        if (!filterPrefix.empty() && sym.name) {
            if (std::strncmp(sym.name, filterPrefix.c_str(), filterPrefix.size()) != 0)
                continue;
        }

        json tensor;
        tensor["name"]    = sym.name ? sym.name : "";
        tensor["detail"]  = sym.detail ? sym.detail : "";
        tensor["file"]    = sym.filePath ? sym.filePath : "";

        if (sym.address != 0) {
            std::ostringstream ofs;
            ofs << "0x" << std::hex << sym.address;
            tensor["offset"] = ofs.str();
        }
        if (sym.size != 0) {
            tensor["sizeBytes"] = sym.size;
        }

        tensorArray.push_back(std::move(tensor));
        count++;
    }

    result["tensors"] = std::move(tensorArray);
    result["count"]   = count;
    result["total"]   = allSymbols.size();

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/gguf/validate
// ============================================================================
// Run structural validation on a GGUF file.
// Params: { "filePath": string }
// Result: { "valid": bool, "diagnostics": [...], "duration_ms": float }

void LSPHotpatchBridge::handleGGUFValidate(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    if (!params.contains("filePath") || !params["filePath"].is_string()) {
        json err;
        err["error"] = "Missing required param: 'filePath'";
        m_lastResult = err;
        m_stats.errors.fetch_add(1);
        return;
    }

    std::string filePath = params["filePath"].get<std::string>();
    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<HotpatchDiagEntry> diags;
    PatchResult pr = GGUFDiagnosticProvider::instance().validateGGUFFile(
        filePath.c_str(), diags);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Count errors
    int errorCount = 0;
    for (const auto& d : diags) {
        if (d.severity == HotpatchDiagSeverity::Error) errorCount++;
    }

    json result;
    result["valid"]       = (pr.success && errorCount == 0);
    result["filePath"]    = filePath;
    result["duration_ms"] = ms;

    json diagArray = json::array();
    for (const auto& d : diags) {
        diagArray.push_back(diagToJson(d));
    }
    result["diagnostics"] = std::move(diagArray);
    result["errorCount"]  = errorCount;
    result["totalDiags"]  = diags.size();

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/workspace/symbols
// ============================================================================
// Query the hotpatch-aware symbol table.
// Params: { "query"?: string, "layer"?: string, "kind"?: string, "limit"?: int }
// Result: { "symbols": [...], "count": N, "generation": N }

void LSPHotpatchBridge::handleWorkspaceSymbols(int /*id*/, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    auto& symProvider = HotpatchSymbolProvider::instance();

    // Determine query type
    std::vector<HotpatchSymbolEntry> symbols;

    if (params.contains("query") && params["query"].is_string()) {
        std::string query = params["query"].get<std::string>();
        if (!query.empty()) {
            symbols = symProvider.findByPrefix(query.c_str());
        } else {
            symbols = symProvider.getAllSymbols();
        }
    } else if (params.contains("layer") && params["layer"].is_string()) {
        HotpatchLayer layer = layerFromString(params["layer"].get<std::string>());
        symbols = symProvider.getSymbolsByLayer(layer);
    } else {
        symbols = symProvider.getAllSymbols();
    }

    // Apply limit
    int limit = 500;
    if (params.contains("limit") && params["limit"].is_number_integer()) {
        limit = params["limit"].get<int>();
    }

    json result;
    json symArray = json::array();
    int count = 0;

    for (const auto& sym : symbols) {
        if (count >= limit) break;
        symArray.push_back(symbolToJson(sym));
        count++;
    }

    result["symbols"]    = std::move(symArray);
    result["count"]      = count;
    result["total"]      = symbols.size();
    result["generation"] = symProvider.getGeneration();

    m_lastResult = result;
}

// ============================================================================
// HANDLER: rawrxd/workspace/stats
// ============================================================================
// Get hotpatch manager statistics + LSP bridge stats + symbol provider stats.
// Params: {} (none)
// Result: { "bridge": {...}, "symbols": {...}, "diagnostics": {...}, "hotpatch": {...} }

void LSPHotpatchBridge::handleWorkspaceStats(int id, const json& params) {
    m_stats.requestsHandled.fetch_add(1);

    json result;

    // ---- Bridge stats ----
    {
        json bridge;
        bridge["requestsHandled"]     = m_stats.requestsHandled.load();
        bridge["notificationsSent"]   = m_stats.notificationsSent.load();
        bridge["diagnosticRefreshes"] = m_stats.diagnosticRefreshes.load();
        bridge["symbolRebuilds"]      = m_stats.symbolRebuilds.load();
        bridge["errors"]              = m_stats.errors.load();
        bridge["attached"]            = m_attached.load();
        result["bridge"] = std::move(bridge);
    }

    // ---- Symbol provider stats ----
    {
        auto& sp = HotpatchSymbolProvider::instance().getStats();
        json symbols;
        symbols["totalSymbols"]       = sp.totalSymbols.load();
        symbols["memoryPatchSymbols"] = sp.memoryPatchSymbols.load();
        symbols["bytePatchSymbols"]   = sp.bytePatchSymbols.load();
        symbols["serverPatchSymbols"] = sp.serverPatchSymbols.load();
        symbols["ggufTensorSymbols"]  = sp.ggufTensorSymbols.load();
        symbols["ggufMetadataSymbols"]= sp.ggufMetadataSymbols.load();
        symbols["lookupCount"]        = sp.lookupCount.load();
        symbols["lookupHits"]         = sp.lookupHits.load();
        symbols["rebuildCount"]       = sp.rebuildCount.load();
        symbols["lastRebuildMs"]      = sp.lastRebuildMs;
        symbols["generation"]         = HotpatchSymbolProvider::instance().getGeneration();
        result["symbols"] = std::move(symbols);
    }

    // ---- Diagnostic provider stats ----
    {
        auto& dp = GGUFDiagnosticProvider::instance().getStats();
        json diagnostics;
        diagnostics["totalRuns"]        = dp.totalRuns.load();
        diagnostics["totalErrors"]      = dp.totalErrors.load();
        diagnostics["totalWarnings"]    = dp.totalWarnings.load();
        diagnostics["totalHints"]       = dp.totalHints.load();
        diagnostics["ggufFilesScanned"] = dp.ggufFilesScanned.load();
        diagnostics["lastRunMs"]        = dp.lastRunMs;
        result["diagnostics"] = std::move(diagnostics);
    }

    // ---- UnifiedHotpatchManager stats ----
    {
        auto& mgr   = UnifiedHotpatchManager::instance();
        auto& stats  = mgr.getStats();
        json hotpatch;
        hotpatch["totalOperations"]  = stats.totalOperations.load();
        hotpatch["totalFailures"]    = stats.totalFailures.load();
        hotpatch["memoryPatchCount"] = stats.memoryPatchCount.load();
        hotpatch["bytePatchCount"]   = stats.bytePatchCount.load();
        hotpatch["serverPatchCount"] = stats.serverPatchCount.load();
        result["hotpatch"] = std::move(hotpatch);
    }

    // ---- LSP server stats ----
    if (m_server) {
        auto ss = m_server->getStats();
        json lspServer;
        lspServer["totalRequests"]      = ss.totalRequests;
        lspServer["totalNotifications"] = ss.totalNotifications;
        lspServer["totalErrors"]        = ss.totalErrors;
        lspServer["symbolsIndexed"]     = ss.symbolsIndexed;
        lspServer["avgResponseMs"]      = ss.avgResponseMs;
        lspServer["bytesRead"]          = ss.bytesRead;
        lspServer["bytesWritten"]       = ss.bytesWritten;
        result["lspServer"] = std::move(lspServer);
    }

    m_lastResult = result;
}
