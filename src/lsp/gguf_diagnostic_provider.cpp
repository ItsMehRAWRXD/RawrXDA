// ============================================================================
// gguf_diagnostic_provider.cpp — GGUF & Hotpatch Diagnostic Engine for LSP
// ============================================================================
// Implementation of real-time diagnostics for GGUF model files and
// three-layer hotpatch system validation.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: PatchResult-style returns (no exceptions)
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "gguf_diagnostic_provider.hpp"
#include "core/model_memory_hotpatch.hpp"
#include "core/byte_level_hotpatcher.hpp"
#include "core/unified_hotpatch_manager.hpp"
#include "server/gguf_server_hotpatch.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace RawrXD::LSPBridge;
namespace fs = std::filesystem;

// ============================================================================
// SINGLETON
// ============================================================================

GGUFDiagnosticProvider& GGUFDiagnosticProvider::instance() {
    static GGUFDiagnosticProvider inst;
    return inst;
}

GGUFDiagnosticProvider::GGUFDiagnosticProvider()  = default;
GGUFDiagnosticProvider::~GGUFDiagnosticProvider() = default;

// ============================================================================
// HELPER — Add diagnostic to output vector
// ============================================================================

static const char* storeDiagString(std::vector<std::string>& storage, const char* s) {
    if (!s) return "";
    storage.emplace_back(s);
    return storage.back().c_str();
}

static const char* storeDiagString(std::vector<std::string>& storage, const std::string& s) {
    storage.push_back(s);
    return storage.back().c_str();
}

void GGUFDiagnosticProvider::addDiag(
    std::vector<HotpatchDiagEntry>& diags,
    HotpatchDiagSeverity severity,
    HotpatchDiagCode code,
    const char* message,
    const char* source,
    HotpatchLayer layer)
{
    HotpatchDiagEntry d{};
    d.severity      = severity;
    d.code          = code;
    d.message       = storeDiagString(m_ownedStrings, message);
    d.source        = storeDiagString(m_ownedStrings, source);
    d.layer         = layer;
    d.relatedPatch  = "";
    d.address       = 0;
    d.offset        = 0;
    diags.push_back(d);

    // Update stats
    switch (severity) {
        case HotpatchDiagSeverity::Error:       m_stats.totalErrors.fetch_add(1);   break;
        case HotpatchDiagSeverity::Warning:     m_stats.totalWarnings.fetch_add(1); break;
        case HotpatchDiagSeverity::Hint:        m_stats.totalHints.fetch_add(1);    break;
        default: break;
    }
}

// ============================================================================
// GGUF FILE VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateGGUFFile(
    const char* filePath,
    std::vector<HotpatchDiagEntry>& outDiags)
{
    if (!filePath) return PatchResult::error("null file path");

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.ggufFilesScanned.fetch_add(1);

    // Check file exists
    std::error_code ec;
    if (!fs::exists(filePath, ec) || ec) {
        addDiag(outDiags, HotpatchDiagSeverity::Error,
                HotpatchDiagCode::GGUFMagicInvalid,
                "GGUF file does not exist", "rawrxd-gguf", HotpatchLayer::Byte);
        return PatchResult::error("File not found");
    }

    uint64_t fileSize = fs::file_size(filePath, ec);
    if (ec || fileSize < 8) {
        addDiag(outDiags, HotpatchDiagSeverity::Error,
                HotpatchDiagCode::GGUFMagicInvalid,
                "File too small to be valid GGUF (< 8 bytes)",
                "rawrxd-gguf", HotpatchLayer::Byte);
        return PatchResult::error("File too small");
    }

    // Read header via byte-level direct_read (zero-copy)
    uint32_t header[2] = {0, 0};
    size_t bytesRead = 0;
    PatchResult rr = direct_read(filePath, 0, 8, header, &bytesRead);
    if (!rr.success || bytesRead < 8) {
        addDiag(outDiags, HotpatchDiagSeverity::Error,
                HotpatchDiagCode::FileMappingFailure,
                "Failed to read GGUF header",
                "rawrxd-gguf", HotpatchLayer::Byte);
        return PatchResult::error("Read failed");
    }

    // Validate GGUF magic: "GGUF" = 0x46475547 (little-endian)
    uint32_t magic   = header[0];
    uint32_t version = header[1];

    if (magic != 0x46475547) {
        std::ostringstream msg;
        msg << "Invalid GGUF magic: 0x" << std::hex << magic
            << " (expected 0x46475547)";
        addDiag(outDiags, HotpatchDiagSeverity::Error,
                HotpatchDiagCode::GGUFMagicInvalid,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-gguf", HotpatchLayer::Byte);
        return PatchResult::error("Invalid GGUF magic");
    }

    // Validate GGUF version (supported: 2, 3)
    if (version < 2 || version > 3) {
        std::ostringstream msg;
        msg << "Unsupported GGUF version: " << version << " (supported: 2, 3)";
        addDiag(outDiags, HotpatchDiagSeverity::Warning,
                HotpatchDiagCode::GGUFVersionMismatch,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-gguf", HotpatchLayer::Byte);
    }

    // Read tensor count and metadata count (bytes 8-23 in GGUF v3)
    if (version >= 3 && fileSize >= 24) {
        uint64_t counts[2] = {0, 0};
        rr = direct_read(filePath, 8, 16, counts, &bytesRead);
        if (rr.success && bytesRead >= 16) {
            uint64_t tensorCount   = counts[0];
            uint64_t metadataCount = counts[1];

            // Sanity check: tensor count shouldn't be absurd
            if (tensorCount > 100000) {
                std::ostringstream msg;
                msg << "Suspicious tensor count: " << tensorCount
                    << " (> 100,000). Possibly corrupted header.";
                addDiag(outDiags, HotpatchDiagSeverity::Warning,
                        HotpatchDiagCode::TensorChecksum,
                        storeDiagString(m_ownedStrings, msg.str()),
                        "rawrxd-gguf", HotpatchLayer::Byte);
            }

            if (metadataCount > 10000) {
                std::ostringstream msg;
                msg << "Suspicious metadata count: " << metadataCount
                    << " (> 10,000). Possibly corrupted header.";
                addDiag(outDiags, HotpatchDiagSeverity::Warning,
                        HotpatchDiagCode::TensorChecksum,
                        storeDiagString(m_ownedStrings, msg.str()),
                        "rawrxd-gguf", HotpatchLayer::Byte);
            }

            // Info diagnostic with model summary
            {
                std::ostringstream msg;
                msg << "GGUF v" << version << ": " << tensorCount << " tensors, "
                    << metadataCount << " metadata entries, "
                    << (fileSize / (1024*1024)) << " MB";
                addDiag(outDiags, HotpatchDiagSeverity::Information,
                        HotpatchDiagCode::GGUFMagicInvalid, // reuse code for info
                        storeDiagString(m_ownedStrings, msg.str()),
                        "rawrxd-gguf", HotpatchLayer::Byte);
            }
        }
    }

    return PatchResult::ok("GGUF validation complete");
}

// ============================================================================
// MEMORY LAYER VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateMemoryLayer(
    std::vector<HotpatchDiagEntry>& outDiags)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& patchStats = get_memory_patch_stats();
    uint64_t failed = patchStats.totalFailed.load();

    if (failed > 0) {
        std::ostringstream msg;
        msg << "Memory layer has " << failed << " failed patch application(s). "
            << "Check VirtualProtect permissions and address validity.";
        addDiag(outDiags, HotpatchDiagSeverity::Warning,
                HotpatchDiagCode::ProtectionFailure,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-memory", HotpatchLayer::Memory);
    }

    uint64_t protChanges = patchStats.protectionChanges.load();
    if (protChanges > 1000) {
        std::ostringstream msg;
        msg << "Excessive protection changes (" << protChanges
            << "). This may impact performance and trigger security software.";
        addDiag(outDiags, HotpatchDiagSeverity::Hint,
                HotpatchDiagCode::UnalignedAccess,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-memory", HotpatchLayer::Memory);
    }

    return PatchResult::ok("Memory layer validation complete");
}

// ============================================================================
// BYTE LAYER VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateByteLayer(
    std::vector<HotpatchDiagEntry>& outDiags)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();

    uint64_t failures = stats.totalFailures.load();
    if (failures > 0) {
        std::ostringstream msg;
        msg << "Byte layer: " << failures << " total operation failure(s). "
            << "Check file permissions and offset validity.";
        addDiag(outDiags, HotpatchDiagSeverity::Warning,
                HotpatchDiagCode::OffsetOutOfBounds,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-byte", HotpatchLayer::Byte);
    }

    return PatchResult::ok("Byte layer validation complete");
}

// ============================================================================
// SERVER LAYER VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateServerLayer(
    std::vector<HotpatchDiagEntry>& outDiags)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();

    uint64_t serverCount = stats.serverPatchCount.load();
    if (serverCount == 0) {
        addDiag(outDiags, HotpatchDiagSeverity::Information,
                HotpatchDiagCode::NullTransform,
                "No server patches registered. Inference pipeline is unmodified.",
                "rawrxd-server", HotpatchLayer::Server);
    }

    return PatchResult::ok("Server layer validation complete");
}

// ============================================================================
// CROSS-LAYER VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateCrossLayer(
    std::vector<HotpatchDiagEntry>& outDiags)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();

    uint64_t memCount    = stats.memoryPatchCount.load();
    uint64_t byteCount   = stats.bytePatchCount.load();
    uint64_t serverCount = stats.serverPatchCount.load();

    // Warn if memory and byte patches are both active (potential conflict)
    if (memCount > 0 && byteCount > 0) {
        addDiag(outDiags, HotpatchDiagSeverity::Warning,
                HotpatchDiagCode::LayerConflict,
                "Both memory and byte-level patches are active. "
                "Ensure they don't target overlapping regions of the same model.",
                "rawrxd-cross", HotpatchLayer::All);
    }

    // Info: system state summary
    {
        std::ostringstream msg;
        msg << "Hotpatch state: Memory=" << memCount
            << " Byte=" << byteCount
            << " Server=" << serverCount
            << " | Total ops=" << stats.totalOperations.load()
            << " | Failures=" << stats.totalFailures.load();
        addDiag(outDiags, HotpatchDiagSeverity::Information,
                HotpatchDiagCode::StateDesync,
                storeDiagString(m_ownedStrings, msg.str()),
                "rawrxd-cross", HotpatchLayer::All);
    }

    return PatchResult::ok("Cross-layer validation complete");
}

// ============================================================================
// FULL VALIDATION
// ============================================================================

PatchResult GGUFDiagnosticProvider::validateAll(
    std::vector<HotpatchDiagEntry>& outDiags)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    m_stats.totalRuns.fetch_add(1);

    // Clear string storage for fresh diagnostics
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ownedStrings.clear();
        m_ownedStrings.reserve(256);
    }

    // Run all validators (each acquires its own lock)
    // Note: we release and re-acquire mutex to avoid nested locks
    validateMemoryLayer(outDiags);
    validateByteLayer(outDiags);
    validateServerLayer(outDiags);
    validateCrossLayer(outDiags);

    // Scan workspace for GGUF files
    {
        const char* searchDirs[] = { ".", "./models", "../models", nullptr };
        for (int d = 0; searchDirs[d]; ++d) {
            std::error_code ec;
            if (!fs::exists(searchDirs[d], ec)) continue;
            for (auto& entry : fs::directory_iterator(searchDirs[d], ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
                if (ext == ".gguf") {
                    validateGGUFFile(entry.path().string().c_str(), outDiags);
                }
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    m_stats.lastRunMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return PatchResult::ok("Full validation complete");
}
