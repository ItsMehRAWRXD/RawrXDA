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
};

// ============================================================
// Subsystem Parameters (union-style, mode-specific)
// ============================================================

struct InjectParams {
    uint32_t pid;
    const char* processName;    // alternative to pid (set pid=0 to use name)
};

struct StubGenParams {
    const char* inputFile;      // required — path to payload executable
};

struct TraceParams {
    uint32_t pid;               // 0 = map-only mode (no debug attach)
};

struct AnalyzerDistillerParams {
    const char* inputGGUF;      // path to GGUF model file
    const char* outputExec;     // path to write distilled .exec (null → default)
};

struct StreamingOrchestratorParams {
    const char* execFile;       // path to .exec file to stream
    uint64_t arenaBytes;        // memory arena size (0 → default 512MB)
    uint32_t threadCount;       // DEFLATE threads (0 → default 8)
};

struct VulkanKernelParams {
    const char* execFile;       // path to .exec topology file
    uint32_t workgroupSize;     // 0 → default 256
};

struct DiskRecoveryParams {
    int driveNumber;            // physical drive number (0-15), -1 = auto-scan
    bool extractKey;            // attempt AES-256 key extraction from bridge EEPROM
};

struct SubsystemParams {
    SubsystemId id;
    union {
        InjectParams inject;
        StubGenParams stubgen;
        TraceParams trace;
        AnalyzerDistillerParams analyzer;
        StreamingOrchestratorParams streaming;
        VulkanKernelParams vulkan;
        DiskRecoveryParams diskRecovery;
    };
};

// ============================================================
// Subsystem Event Callback (for progress / observability)
// ============================================================

enum class SubsystemEventType : int {
    ModeStarted,
    ModeCompleted,
    ModeFailed,
    ArtifactGenerated,
    LatencyRecorded,
    UsageError          // incomplete args — not a failure
};

struct SubsystemEvent {
    SubsystemEventType type;
    SubsystemId mode;
    uint64_t timestamp;         // GetTickCount64 value
    const char* detail;
};

// Raw function pointer callback — NOT std::function
typedef void (*SubsystemEventCallback)(const SubsystemEvent* event, void* userData);

// ============================================================
// Subsystem Registry (the promotion layer)
// ============================================================

/**
 * @class SubsystemRegistry
 * @brief Routes agent calls to CLI mode implementations
 *
 * This is the "tool → subsystem" promotion layer. It wraps the
 * process-level CLI modes into callable functions that can be
 * invoked directly by the agentic framework without spawning
 * a child process.
 *
 * Thread-safe: all dispatch goes through a mutex.
 * Stateless: each call is independent (no session state).
 */
class SubsystemRegistry {
public:
    // Singleton access (no dynamic allocation)
    static SubsystemRegistry& instance() {
        static SubsystemRegistry reg;
        return reg;
    }

    // ---- Core API ----

    /**
     * Execute a subsystem mode synchronously.
     * Returns structured result with latency and artifact info.
     *
     * @param params Mode ID + mode-specific parameters
     * @return SubsystemResult with success/failure, detail, latency
     */
    SubsystemResult invoke(const SubsystemParams& params);

    /**
     * Execute a mode by CLI switch string (e.g., "-compile", "-entropy").
     * Parses the switch and delegates to invoke().
     * This enables agents to use the same strings as the CLI contract.
     */
    SubsystemResult invokeBySwitch(const char* switchStr);

    /**
     * Check if a subsystem is available and operational.
     * Returns true if the mode ID is valid and the handler is registered.
     */
    bool isAvailable(SubsystemId id) const;

    /**
     * Get the CLI switch string for a given mode ID.
     * Returns nullptr for invalid IDs.
     */
    const char* getSwitchName(SubsystemId id) const;

    /**
     * Get execution statistics for a mode.
     */
    struct ModeStats {
        uint64_t invocationCount;
        uint64_t totalLatencyMs;
        uint64_t lastLatencyMs;
        uint64_t failureCount;
    };
    ModeStats getStats(SubsystemId id) const;

    // ---- Event System ----

    void setEventCallback(SubsystemEventCallback cb, void* userData) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_eventCallback = cb;
        m_eventUserData = userData;
    }

private:
    SubsystemRegistry();
    ~SubsystemRegistry() = default;
    SubsystemRegistry(const SubsystemRegistry&) = delete;
    SubsystemRegistry& operator=(const SubsystemRegistry&) = delete;

    // Mode handler function pointer type
    typedef SubsystemResult (*ModeHandler)(const SubsystemParams& params);

    // Internal dispatch table
    struct ModeEntry {
        SubsystemId id;
        const char* switchName;     // e.g., "-compile"
        ModeHandler handler;        // function pointer to mode impl
        ModeStats stats;
    };

    ModeEntry m_modes[static_cast<int>(SubsystemId::_Count)];
    mutable std::mutex m_mutex;

    SubsystemEventCallback m_eventCallback;
    void* m_eventUserData;

    void emitEvent(SubsystemEventType type, SubsystemId mode, const char* detail);

    // ---- Mode Implementations (wrappers around external ASM/C procs) ----
    // These are declared as static to serve as function pointers in the table.

    static SubsystemResult handleCompile(const SubsystemParams& params);
    static SubsystemResult handleEncrypt(const SubsystemParams& params);
    static SubsystemResult handleInject(const SubsystemParams& params);
    static SubsystemResult handlePrivilegeEscalation(const SubsystemParams& params);  // Renamed
    static SubsystemResult handlePersist(const SubsystemParams& params);
    static SubsystemResult handleSideload(const SubsystemParams& params);
    static SubsystemResult handleAVScan(const SubsystemParams& params);
    static SubsystemResult handleEntropy(const SubsystemParams& params);
    static SubsystemResult handleStubGen(const SubsystemParams& params);
    static SubsystemResult handleTrace(const SubsystemParams& params);
    static SubsystemResult handleAgent(const SubsystemParams& params);
    static SubsystemResult handleBBCov(const SubsystemParams& params);
    static SubsystemResult handleCovFusion(const SubsystemParams& params);
    static SubsystemResult handleDynTrace(const SubsystemParams& params);
    static SubsystemResult handleAgentTrace(const SubsystemParams& params);
    static SubsystemResult handleGapFuzz(const SubsystemParams& params);
    static SubsystemResult handleIntelPT(const SubsystemParams& params);
    static SubsystemResult handleDiffCov(const SubsystemParams& params);
    static SubsystemResult handleAnalyzerDistiller(const SubsystemParams& params);
    static SubsystemResult handleStreamingOrchestrator(const SubsystemParams& params);
    static SubsystemResult handleVulkanKernel(const SubsystemParams& params);
    static SubsystemResult handleDiskRecovery(const SubsystemParams& params);
};

// ============================================================
// Convenience macros for agent integration
// ============================================================

// Quick-invoke a mode by ID (returns SubsystemResult)
#define RAWRXD_INVOKE(mode_id)  \
    SubsystemRegistry::instance().invoke(SubsystemParams{SubsystemId::mode_id})

// Quick-invoke with parameters
#define RAWRXD_INVOKE_WITH(mode_id, param_block) \
    ([&]() -> SubsystemResult { \
        SubsystemParams p{}; \
        p.id = SubsystemId::mode_id; \
        param_block; \
        return SubsystemRegistry::instance().invoke(p); \
    })()

// Example agent usage:
//   SubsystemResult r = RAWRXD_INVOKE(Compile);
//   SubsystemResult r = RAWRXD_INVOKE(Entropy);
//   SubsystemResult r = RAWRXD_INVOKE_WITH(Inject, { p.inject.pid = 1234; });
//   SubsystemResult r = RAWRXD_INVOKE_WITH(StubGen, { p.stubgen.inputFile = "payload.exe"; });
