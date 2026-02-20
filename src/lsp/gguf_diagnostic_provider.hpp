// ============================================================================
// gguf_diagnostic_provider.hpp — GGUF & Hotpatch Diagnostic Engine for LSP
// ============================================================================
// Validates GGUF model files, hotpatch configurations, and cross-layer state
// consistency. Produces LSP-compatible diagnostics (errors, warnings, hints).
//
// Diagnostic sources:
//   "rawrxd-gguf"     — GGUF structural validation (magic, version, tensors)
//   "rawrxd-memory"   — Memory layer patch validation (overlaps, alignment)
//   "rawrxd-byte"     — Byte layer patch validation (bounds, patterns)
//   "rawrxd-server"   — Server layer validation (duplicate names, null transforms)
//   "rawrxd-cross"    — Cross-layer conflict detection
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: PatchResult-style returns (no exceptions)
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp_bridge_protocol.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

// Forward declarations
struct PatchResult;

// ---------------------------------------------------------------------------
// GGUFDiagnosticProvider — Validates GGUF files and hotpatch state
// ---------------------------------------------------------------------------
class GGUFDiagnosticProvider {
public:
    static GGUFDiagnosticProvider& instance();

    // ---- GGUF File Validation ----
    // Validate GGUF magic, version, tensor count consistency.
    PatchResult validateGGUFFile(const char* filePath,
                                 std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // ---- Hotpatch Validation ----
    // Check memory layer for overlapping patches, alignment issues.
    PatchResult validateMemoryLayer(
        std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // Check byte layer for out-of-bounds patches, pattern mismatches.
    PatchResult validateByteLayer(
        std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // Check server layer for duplicate names, null transforms.
    PatchResult validateServerLayer(
        std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // ---- Cross-Layer Validation ----
    // Detect conflicts between layers (e.g., memory patch overlapping byte patch target).
    PatchResult validateCrossLayer(
        std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // ---- Full Validation ----
    // Run all validators and aggregate diagnostics.
    PatchResult validateAll(std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& outDiags);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalRuns{0};
        std::atomic<uint64_t> totalErrors{0};
        std::atomic<uint64_t> totalWarnings{0};
        std::atomic<uint64_t> totalHints{0};
        std::atomic<uint64_t> ggufFilesScanned{0};
        double                lastRunMs{0.0};
    };

    const Stats& getStats() const { return m_stats; }

private:
    GGUFDiagnosticProvider();
    ~GGUFDiagnosticProvider();
    GGUFDiagnosticProvider(const GGUFDiagnosticProvider&) = delete;
    GGUFDiagnosticProvider& operator=(const GGUFDiagnosticProvider&) = delete;

    void addDiag(std::vector<RawrXD::LSPBridge::HotpatchDiagEntry>& diags,
                 RawrXD::LSPBridge::HotpatchDiagSeverity severity,
                 RawrXD::LSPBridge::HotpatchDiagCode code,
                 const char* message,
                 const char* source,
                 RawrXD::LSPBridge::HotpatchLayer layer);

    mutable std::mutex m_mutex;
    Stats              m_stats;
    std::vector<std::string> m_ownedStrings;
};
