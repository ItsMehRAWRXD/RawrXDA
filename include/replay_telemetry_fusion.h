// ============================================================================
// replay_telemetry_fusion.h — Replay + Telemetry Fusion Engine
// ============================================================================
// Combines DeterministicReplayEngine and UTC MASM counters into a unified
// enterprise-grade validation pipeline that can detect:
//
//   1. Logical divergence    — replay action sequence mismatches
//   2. Performance divergence — throughput regression between replay runs
//   3. Tool output drift     — tool results differ between sessions
//   4. Loop count anomalies  — UTC counter vs replay journal disagreement
//
// Architecture:
//   ┌─────────────────────────────────────────────────────────────┐
//   │            ReplayTelemetryFusion (singleton)                │
//   │                                                             │
//   │  ┌───────────────────┐       ┌───────────────────────────┐  │
//   │  │ DeterministicReplay│       │ UTC MASM Counters        │  │
//   │  │ Journal (C++)      │◄─────►│ (lock-free g_Counter_*)  │  │
//   │  └────────┬──────────┘       └──────────┬────────────────┘  │
//   │           │                              │                  │
//   │           └──────────┬───────────────────┘                  │
//   │                      ▼                                      │
//   │          ┌───────────────────────┐                          │
//   │          │  FusionSnapshot       │                          │
//   │          │  (counters + actions  │                          │
//   │          │   + hashes + timing)  │                          │
//   │          └───────────┬───────────┘                          │
//   │                      │                                      │
//   │          ┌───────────▼───────────┐                          │
//   │          │  DivergenceReport     │                          │
//   │          │  (type, severity,     │                          │
//   │          │   detail, pass/fail)  │                          │
//   │          └───────────────────────┘                          │
//   └─────────────────────────────────────────────────────────────┘
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

// Forward declarations — avoids circular includes
class ReplayJournal;
struct ActionRecord;
struct SessionSnapshot;

namespace RawrXD {
namespace Fusion {

// ============================================================================
// Divergence Types
// ============================================================================
enum class DivergenceType : uint8_t {
    None                = 0,
    LogicalDivergence   = 1,  // Action sequence mismatch
    PerformanceDivergence = 2, // Throughput/latency regression
    ToolOutputDrift     = 3,  // Tool call results differ
    LoopCountAnomaly    = 4,  // UTC counter vs replay journal mismatch
    CounterSpikeAnomaly = 5,  // Sudden counter jump (e.g., error spike)
    SessionMismatch     = 6,  // Session structure differs
    HotpatchDivergence  = 7,  // Counter delta abnormal across hotpatch
};

const char* divergenceTypeToString(DivergenceType t);

// ============================================================================
// Divergence Severity
// ============================================================================
enum class DivergenceSeverity : uint8_t {
    Info     = 0,  // Informational — within tolerance
    Warning  = 1,  // Near threshold — warrants investigation
    Error    = 2,  // Exceeds threshold — CI gate fail
    Critical = 3,  // Structural mismatch — fatal
};

// ============================================================================
// Fusion Result (PatchResult-pattern)
// ============================================================================
struct FusionResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static FusionResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static FusionResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Counter Snapshot — Atomic capture of all UTC counters
// ============================================================================
struct CounterSnapshot {
    uint64_t inference;
    uint64_t scsiFails;
    uint64_t agentLoop;
    uint64_t bytePatches;
    uint64_t memPatches;
    uint64_t serverPatches;
    uint64_t flushOps;
    uint64_t errors;
    uint64_t timestampMs;    // QPC-based timestamp when snapshot was taken

    uint64_t totalPatches() const {
        return bytePatches + memPatches + serverPatches;
    }
};

// ============================================================================
// Fusion Snapshot — Combined replay + telemetry state at a point in time
// ============================================================================
struct FusionSnapshot {
    CounterSnapshot     counters;
    uint64_t            replaySequenceId;      // Current position in replay journal
    uint64_t            replaySessionId;
    uint64_t            replayActionCount;     // Total actions recorded in the session
    uint64_t            replayToolCalls;       // Count of AgentToolCall actions
    std::vector<uint64_t> toolOutputHashes;    // FNV-1a hashes of tool results
    double              sessionDurationMs;
    std::string         label;

    FusionSnapshot() : replaySequenceId(0), replaySessionId(0),
                       replayActionCount(0), replayToolCalls(0),
                       sessionDurationMs(0.0) {}
};

// ============================================================================
// Divergence Entry — Single detected divergence
// ============================================================================
struct DivergenceEntry {
    DivergenceType      type;
    DivergenceSeverity  severity;
    std::string         metric;        // Which metric diverged
    double              expected;      // Baseline value
    double              actual;        // Current value
    double              deltaPct;      // Percentage change
    double              threshold;     // Threshold that was exceeded
    std::string         detail;        // Human-readable explanation
    uint64_t            timestampMs;
};

// ============================================================================
// Divergence Report — Full comparison result
// ============================================================================
struct DivergenceReport {
    bool                        passed;        // True if no Error/Critical divergences
    std::string                 summary;
    uint64_t                    baselineSessionId;
    uint64_t                    currentSessionId;
    std::vector<DivergenceEntry> entries;
    FusionSnapshot              baselineSnapshot;
    FusionSnapshot              currentSnapshot;
    uint64_t                    timestampMs;

    int countBySeverity(DivergenceSeverity sev) const {
        int c = 0;
        for (const auto& e : entries) {
            if (e.severity == sev) c++;
        }
        return c;
    }
};

// ============================================================================
// Divergence Thresholds — Configurable per-metric limits
// ============================================================================
struct DivergenceThresholds {
    double loopCountTolerancePct   = 5.0;   // UTC counter vs replay action count
    double throughputDropPct       = 3.0;   // GEMM/FlashAttn throughput
    double latencyIncreasePct      = 5.0;   // Inference latency
    double toolOutputDriftPct      = 10.0;  // % of tool outputs that may differ
    double counterSpikePct         = 50.0;  // Max sudden counter jump
    double errorSpikeTolerance     = 5.0;   // Max new errors during validation
    double hotpatchDeltaPct        = 50.0;  // Max counter delta across hotpatch
};

// ============================================================================
// Replay Telemetry Fusion — Divergence callback
// ============================================================================
using DivergenceCallback = void(*)(const DivergenceEntry& entry, void* userData);

// ============================================================================
// ReplayTelemetryFusion — Main fusion engine
// ============================================================================
class ReplayTelemetryFusion {
public:
    static ReplayTelemetryFusion& instance();

    // ---- Lifecycle ----
    FusionResult initialize();
    FusionResult shutdown();
    bool isInitialized() const;

    // ---- Counter Snapshots ----
    // Capture all UTC MASM counters atomically (acquire-fence reads).
    CounterSnapshot captureCounters() const;

    // ---- Fusion Snapshots ----
    // Capture combined replay + telemetry snapshot.
    FusionSnapshot captureFusionSnapshot(const char* label = nullptr) const;

    // ---- Divergence Detection ----
    // Compare current state against a baseline snapshot.
    DivergenceReport compare(const FusionSnapshot& baseline,
                              const FusionSnapshot& current) const;

    // Compare current state against the last saved baseline.
    DivergenceReport compareAgainstBaseline() const;

    // ---- Baseline Management ----
    // Save the current state as the baseline.
    FusionResult saveBaseline(const char* label = nullptr);

    // Load a baseline from the last saved snapshot.
    FusionResult loadBaseline();

    // Get the current baseline.
    const FusionSnapshot& getBaseline() const;

    // ---- Export ----
    // Export a fusion snapshot to JSON string.
    std::string exportSnapshotJSON(const FusionSnapshot& snap) const;

    // Export a divergence report to JSON string.
    std::string exportReportJSON(const DivergenceReport& report) const;

    // Export Prometheus metrics augmented with replay data.
    // Buffer must be >= 16KB.
    size_t exportFusedPrometheus(char* buffer, size_t bufferSize) const;

    // ---- Thresholds ----
    void setThresholds(const DivergenceThresholds& thresholds);
    const DivergenceThresholds& getThresholds() const;

    // ---- Callbacks ----
    void registerDivergenceCallback(DivergenceCallback cb, void* userData);
    void removeDivergenceCallback(DivergenceCallback cb);

    // ---- Utility ----
    // Compute FNV-1a hash of a string (for tool output hashing).
    static uint64_t fnv1a(const char* data, size_t length);
    static uint64_t fnv1a(const std::string& s);

private:
    ReplayTelemetryFusion();
    ~ReplayTelemetryFusion();
    ReplayTelemetryFusion(const ReplayTelemetryFusion&) = delete;
    ReplayTelemetryFusion& operator=(const ReplayTelemetryFusion&) = delete;

    // Internal divergence checkers
    void checkLoopCountAnomaly(const FusionSnapshot& baseline,
                                const FusionSnapshot& current,
                                DivergenceReport& report) const;
    void checkToolOutputDrift(const FusionSnapshot& baseline,
                               const FusionSnapshot& current,
                               DivergenceReport& report) const;
    void checkCounterSpikes(const FusionSnapshot& baseline,
                             const FusionSnapshot& current,
                             DivergenceReport& report) const;
    void checkSessionStructure(const FusionSnapshot& baseline,
                                const FusionSnapshot& current,
                                DivergenceReport& report) const;

    void notifyCallbacks(const DivergenceEntry& entry) const;

    mutable std::mutex          m_mutex;
    std::atomic<bool>           m_initialized{false};
    FusionSnapshot              m_baseline;
    DivergenceThresholds        m_thresholds;

    struct CallbackEntry {
        DivergenceCallback callback;
        void*              userData;
    };
    std::vector<CallbackEntry>  m_callbacks;
};

} // namespace Fusion
} // namespace RawrXD
