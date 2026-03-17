// ============================================================================
// lsp_hotpatch_bridge.hpp — Main LSP ↔ Hotpatch Bridge
// ============================================================================
// Connects the UnifiedHotpatchManager, GGUFDiagnosticProvider, and
// HotpatchSymbolProvider to the RawrXD_LSPServer via custom JSON-RPC methods.
//
// This is the central orchestrator that:
//   1. Registers custom method handlers with the LSP server
//   2. Translates JSON-RPC requests into hotpatch operations
//   3. Serializes hotpatch results back to JSON-RPC responses
//   4. Forwards hotpatch events as LSP notifications
//   5. Provides go-to-definition, hover, and completion for hotpatch symbols
//
// Thread model: All handlers run on the LSP dispatch thread.
// Error handling: PatchResult-style returns (no exceptions).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp_bridge_protocol.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>

// Forward declarations
struct PatchResult;
namespace nlohmann { class json; }

namespace RawrXD {
    namespace LSPServer {
        class RawrXDLSPServer;
    }
}

// ---------------------------------------------------------------------------
// LSPHotpatchBridge — Orchestrates LSP ↔ Hotpatch integration
// ---------------------------------------------------------------------------
class LSPHotpatchBridge {
public:
    static LSPHotpatchBridge& instance();

    // ---- Lifecycle ----
    
    // Attach to an existing LSP server and register all custom handlers.
    // Must be called after the LSP server is created but before it starts.
    PatchResult attach(RawrXD::LSPServer::RawrXDLSPServer* server);
    
    // Detach from the LSP server and unregister handlers.
    PatchResult detach();

    // ---- Manual Triggers ----
    
    // Force a diagnostic refresh and push to the LSP client.
    PatchResult refreshDiagnostics();
    
    // Force a symbol index rebuild.
    PatchResult rebuildSymbolIndex();

    // ---- Hotpatch Event Forwarding ----
    
    // Called by the UnifiedHotpatchManager event system.
    // Translates HotpatchEvents into rawrxd/hotpatch/event notifications.
    static void onHotpatchEvent(const struct HotpatchEvent* event, void* userData);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> requestsHandled{0};
        std::atomic<uint64_t> notificationsSent{0};
        std::atomic<uint64_t> diagnosticRefreshes{0};
        std::atomic<uint64_t> symbolRebuilds{0};
        std::atomic<uint64_t> errors{0};
    };

    const Stats& getStats() const { return m_stats; }
    bool isAttached() const { return m_attached.load(); }

private:
    LSPHotpatchBridge();
    ~LSPHotpatchBridge();
    LSPHotpatchBridge(const LSPHotpatchBridge&) = delete;
    LSPHotpatchBridge& operator=(const LSPHotpatchBridge&) = delete;

    // ---- JSON-RPC Method Handlers ----
    // Each handler takes (id, params) and writes the response via m_server.
    
    void handleHotpatchList(int id, const nlohmann::json& params);
    void handleHotpatchApply(int id, const nlohmann::json& params);
    void handleHotpatchRevert(int id, const nlohmann::json& params);
    void handleHotpatchDiagnostics(int id, const nlohmann::json& params);
    void handleGGUFModelInfo(int id, const nlohmann::json& params);
    void handleGGUFTensorList(int id, const nlohmann::json& params);
    void handleGGUFValidate(int id, const nlohmann::json& params);
    void handleWorkspaceSymbols(int id, const nlohmann::json& params);
    void handleWorkspaceStats(int id, const nlohmann::json& params);

    // ---- Serialization Helpers ----
    nlohmann::json symbolToJson(const RawrXD::LSPBridge::HotpatchSymbolEntry& sym);
    nlohmann::json diagToJson(const RawrXD::LSPBridge::HotpatchDiagEntry& diag);

    // State
    RawrXD::LSPServer::RawrXDLSPServer* m_server = nullptr;
    std::atomic<bool>                    m_attached{false};
    std::mutex                           m_mutex;
    Stats                                m_stats;

    // Handler result storage — handlers write here, lambdas return it.
    // Thread-safe because all handlers run on the single LSP dispatch thread.
    nlohmann::json                       m_lastResult;
};
