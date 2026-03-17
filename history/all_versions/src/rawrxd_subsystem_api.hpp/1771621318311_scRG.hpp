/**
 * @file rawrxd_subsystem_api.hpp
 * @brief Agent-callable subsystem interface for RawrXD Unified CLI modes
 *
 * Promotes CLI modes from standalone tools to callable infrastructure.
 * Each mode is exposed as a deterministic, synchronous subsystem call
 * that returns structured results via PatchResult-compatible types.
 *
 * NO exceptions. NO std::function. NO Qt. Raw function pointers only.
 *
 * Contract: CLI_CONTRACT_v1.0.md
 * Date:     2026-02-10
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <atomic>

// ============================================================
// Subsystem Result Type (mirrors PatchResult contract)
// ============================================================

struct SubsystemResult {
    bool success;
    const char* detail;
    int errorCode;
    uint64_t latencyMs;
    const char* artifactPath;   // null if no artifact produced

    static SubsystemResult ok(const char* msg, uint64_t ms = 0, const char* artifact = nullptr) {
        SubsystemResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        r.latencyMs = ms;
        r.artifactPath = artifact;
        return r;
    }

    static SubsystemResult error(const char* msg, int code = -1) {
        SubsystemResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        r.latencyMs = 0;
        r.artifactPath = nullptr;
        return r;
    }
};

// ============================================================
// Subsystem Identifiers (matches CLI mode IDs 1-12)
// ============================================================

enum class SubsystemId : int {
    // ---- CLI Modes (1-18) ----
    Compile     = 1,
    Encrypt     = 2,
    Inject      = 3,
    PrivilegeEscalation = 4,  // Renamed from UACBypass
    Persist     = 5,
    Sideload    = 6,
    AVScan      = 7,
    Entropy     = 8,
    StubGen     = 9,
    Trace       = 10,
    Agent       = 11,
    BBCov       = 12,
    CovFusion   = 13,
    DynTrace    = 14,
    AgentTrace  = 15,
    GapFuzz     = 16,
    IntelPT     = 17,
    DiffCov     = 18,
    // ---- Library Modules (19-22) ----
    AnalyzerDistiller      = 19,
    StreamingOrchestrator  = 20,
    VulkanKernel           = 21,
    DiskRecovery           = 22,
    LSPDiagnostics         = 23,
    _Count      = 23
};
