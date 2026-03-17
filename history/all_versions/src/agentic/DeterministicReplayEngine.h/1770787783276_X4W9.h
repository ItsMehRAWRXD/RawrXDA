// ============================================================================
// DeterministicReplayEngine.h — Deterministic Agent Session Replay
// ============================================================================
// Provides bit-exact, timestamp-faithful replay of any recorded
// AgentTranscript.  Three operating modes:
//
//   ReplayMode::Verify    — Re-execute all tool calls, diff outputs
//   ReplayMode::Simulate  — Inject recorded LLM responses, skip real calls
//   ReplayMode::Audit     — Walk the transcript read-only, emit events
//
// The engine composes AgentTranscript (step-level log) with AISession
// (session-level checkpoints) and BoundedAgentLoop (execution engine).
//
// Determinism Contract:
//   Given identical:
//     1. Transcript JSON
//     2. Workspace snapshot (file content hashes)
//     3. Tool registry version
//   The replay engine MUST produce identical tool outputs at every step,
//   or report a divergence event with full diff context.
//
// Architecture:
//   ┌──────────────────────────────────────────────────────┐
//   │                  DeterministicReplayEngine            │
//   │                                                      │
//   │  ┌────────────┐  ┌──────────────┐  ┌─────────────┐  │
//   │  │ Transcript  │  │ Workspace    │  │  Replay     │  │
//   │  │ Loader      │→ │ Snapshot     │→ │  Executor   │  │
//   │  └────────────┘  └──────────────┘  └─────────────┘  │
//   │         │                │                │          │
//   │         ▼                ▼                ▼          │
//   │  ┌────────────────────────────────────────────────┐  │
//   │  │         Divergence Detector + Reporter          │  │
//   │  └────────────────────────────────────────────────┘  │
//   └──────────────────────────────────────────────────────┘
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "AgentTranscript.h"
#include "ToolCallResult.h"
#include "rawrxd_telemetry_exports.h"

namespace RawrXD {
namespace Agent {

// ============================================================================
// Replay operating modes
// ============================================================================
enum class ReplayMode {
    Verify,     // Re-execute tools, compare outputs to transcript
    Simulate,   // Inject recorded LLM responses, skip real model calls
    Audit       // Read-only walk, emit divergence events without execution
};

// ============================================================================
// Workspace file snapshot — for determinism verification
// ============================================================================
struct FileSnapshot {
    std::string path;
    uint64_t    sizeBytes   = 0;
    uint64_t    xxhash64    = 0;    // Fast non-cryptographic hash
    uint64_t    modifiedMs  = 0;    // Last-modified epoch ms
};

struct WorkspaceSnapshot {
    std::string snapshotId;
    uint64_t    capturedAtMs    = 0;
    std::vector<FileSnapshot> files;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["snapshot_id"] = snapshotId;
        j["captured_at_ms"] = capturedAtMs;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& f : files) {
            arr.push_back({
                {"path", f.path},
                {"size", f.sizeBytes},
                {"xxhash64", f.xxhash64},
                {"modified_ms", f.modifiedMs}
            });
        }
        j["files"] = arr;
        return j;
    }

    static WorkspaceSnapshot fromJson(const nlohmann::json& j) {
        WorkspaceSnapshot ws;
        ws.snapshotId = j.value("snapshot_id", "");
        ws.capturedAtMs = j.value("captured_at_ms", uint64_t(0));
        if (j.contains("files")) {
            for (const auto& fj : j["files"]) {
                FileSnapshot fs;
                fs.path = fj.value("path", "");
                fs.sizeBytes = fj.value("size", uint64_t(0));
                fs.xxhash64 = fj.value("xxhash64", uint64_t(0));
                fs.modifiedMs = fj.value("modified_ms", uint64_t(0));
                ws.files.push_back(fs);
            }
        }
        return ws;
    }
};

// ============================================================================
// Divergence event — raised when replay output differs from transcript
// ============================================================================
enum class DivergenceType {
    ToolOutputMismatch,     // Tool returned different output
    ToolOutcomeMismatch,    // Tool returned different success/failure
    FileStateMismatch,      // File hash differs from snapshot
    StepCountMismatch,      // Replay ended at different step than original
    ToolNotFound,           // Tool referenced in transcript no longer exists
    TimingAnomaly           // Replay timing deviates > 10x from recorded
};

// ============================================================================
// Failure classification — triages divergence root cause for recovery
// ============================================================================
enum class FailureClass : uint8_t {
    Unknown         = 0,    // Insufficient signal to classify
    OOM             = 1,    // Memory pressure — error counter spike + low flush
    Timeout         = 2,    // Inference latency — agentic overhead stall
    LogicDrift      = 3,    // Output mismatch with stable counters
    DiskPressure    = 4,    // NanoDisk subsystem — SCSI fails or flush backlog
    GpuStall        = 5,    // Vulkan/compute — inference counter frozen
    HotpatchCascade = 6     // Patch counters spiked — hotpatch side-effect
};

inline const char* FailureClassString(FailureClass fc) {
    switch (fc) {
        case FailureClass::OOM:             return "oom";
        case FailureClass::Timeout:         return "timeout";
        case FailureClass::LogicDrift:      return "logic_drift";
        case FailureClass::DiskPressure:    return "disk_pressure";
        case FailureClass::GpuStall:        return "gpu_stall";
        case FailureClass::HotpatchCascade: return "hotpatch_cascade";
        default:                            return "unknown";
    }
}

// ============================================================================
// TelemetrySnapshot — frozen copy of all 8 g_Counter_* values + QPC timestamp
// ============================================================================
// Captured at the instant a DivergenceEvent fires. Allows post-hoc
// classification of the root cause (inference latency vs disk pressure
// vs GPU stall) by comparing counter deltas across snapshots.
// ============================================================================
struct TelemetrySnapshot {
    uint64_t inference      = 0;    // g_Counter_Inference
    uint64_t scsiFails      = 0;    // g_Counter_ScsiFails
    uint64_t agentLoop      = 0;    // g_Counter_AgentLoop
    uint64_t bytePatches    = 0;    // g_Counter_BytePatches
    uint64_t memPatches     = 0;    // g_Counter_MemPatches
    uint64_t serverPatches  = 0;    // g_Counter_ServerPatches
    uint64_t flushOps       = 0;    // g_Counter_FlushOps
    uint64_t errors         = 0;    // g_Counter_Errors
    int64_t  qpcTimestamp   = 0;    // QueryPerformanceCounter at capture
    int64_t  qpcFrequency   = 0;    // QueryPerformanceFrequency for QPC→us

    bool valid              = false; // Set true only when counters were read

    // Microseconds since some fixed epoch (derived from QPC)
    int64_t elapsedUs(const TelemetrySnapshot& baseline) const {
        if (qpcFrequency <= 0) return 0;
        return ((qpcTimestamp - baseline.qpcTimestamp) * 1000000) / qpcFrequency;
    }

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["inference"]      = inference;
        j["scsi_fails"]     = scsiFails;
        j["agent_loop"]     = agentLoop;
        j["byte_patches"]   = bytePatches;
        j["mem_patches"]    = memPatches;
        j["server_patches"] = serverPatches;
        j["flush_ops"]      = flushOps;
        j["errors"]         = errors;
        j["qpc_timestamp"]  = qpcTimestamp;
        j["qpc_frequency"]  = qpcFrequency;
        j["valid"]          = valid;
        return j;
    }

    static TelemetrySnapshot fromJson(const nlohmann::json& j) {
        TelemetrySnapshot s;
        s.inference     = j.value("inference",      uint64_t(0));
        s.scsiFails     = j.value("scsi_fails",     uint64_t(0));
        s.agentLoop     = j.value("agent_loop",     uint64_t(0));
        s.bytePatches   = j.value("byte_patches",   uint64_t(0));
        s.memPatches    = j.value("mem_patches",     uint64_t(0));
        s.serverPatches = j.value("server_patches", uint64_t(0));
        s.flushOps      = j.value("flush_ops",      uint64_t(0));
        s.errors        = j.value("errors",         uint64_t(0));
        s.qpcTimestamp  = j.value("qpc_timestamp",  int64_t(0));
        s.qpcFrequency  = j.value("qpc_frequency",  int64_t(0));
        s.valid         = j.value("valid",          false);
        return s;
    }
};

struct DivergenceEvent {
    DivergenceType      type;
    FailureClass        classification  = FailureClass::Unknown;
    int                 stepNumber      = -1;
    std::string         toolName;
    std::string         expected;       // From transcript
    std::string         actual;         // From replay
    std::string         detail;         // Human-readable explanation
    uint64_t            timestampMs     = 0;
    TelemetrySnapshot   snapshot;       // Counter state at divergence instant

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["type"] = typeString();
        j["classification"] = FailureClassString(classification);
        j["step"] = stepNumber;
        if (!toolName.empty()) j["tool"] = toolName;
        if (!expected.empty()) j["expected"] = expected;
        if (!actual.empty())   j["actual"] = actual;
        j["detail"] = detail;
        j["timestamp_ms"] = timestampMs;
        if (snapshot.valid) j["telemetry_snapshot"] = snapshot.toJson();
        return j;
    }

    const char* typeString() const {
        switch (type) {
            case DivergenceType::ToolOutputMismatch:  return "tool_output_mismatch";
            case DivergenceType::ToolOutcomeMismatch:  return "tool_outcome_mismatch";
            case DivergenceType::FileStateMismatch:    return "file_state_mismatch";
            case DivergenceType::StepCountMismatch:    return "step_count_mismatch";
            case DivergenceType::ToolNotFound:         return "tool_not_found";
            case DivergenceType::TimingAnomaly:        return "timing_anomaly";
            default: return "unknown";
        }
    }
};

// ============================================================================
// Replay result — outcome of a full replay pass
// ============================================================================
struct ReplayResult {
    bool        deterministic       = false;    // true iff zero divergences
    int         stepsReplayed       = 0;
    int         stepsTotal          = 0;
    int         divergenceCount     = 0;
    int64_t     replayDurationMs    = 0;
    std::string originalSessionId;
    std::string replaySessionId;
    std::vector<DivergenceEvent> divergences;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["deterministic"] = deterministic;
        j["steps_replayed"] = stepsReplayed;
        j["steps_total"] = stepsTotal;
        j["divergence_count"] = divergenceCount;
        j["replay_duration_ms"] = replayDurationMs;
        j["original_session_id"] = originalSessionId;
        j["replay_session_id"] = replaySessionId;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& d : divergences) arr.push_back(d.toJson());
        j["divergences"] = arr;
        return j;
    }

    // Factory methods — PatchResult pattern
    static ReplayResult Ok(int steps, int64_t durationMs) {
        ReplayResult r;
        r.deterministic = true;
        r.stepsReplayed = steps;
        r.stepsTotal = steps;
        r.replayDurationMs = durationMs;
        return r;
    }

    static ReplayResult Diverged(int stepsReplayed, int stepsTotal,
                                  std::vector<DivergenceEvent> divs) {
        ReplayResult r;
        r.deterministic = false;
        r.stepsReplayed = stepsReplayed;
        r.stepsTotal = stepsTotal;
        r.divergenceCount = static_cast<int>(divs.size());
        r.divergences = std::move(divs);
        return r;
    }
};

// ============================================================================
// Replay progress callback
// ============================================================================
using ReplayProgressCallback = std::function<void(
    int currentStep, int totalSteps,
    const std::string& status,
    const DivergenceEvent* divergence   // null if no divergence at this step
)>;

// ============================================================================
// Replay configuration
// ============================================================================
struct ReplayConfig {
    ReplayMode      mode            = ReplayMode::Verify;
    bool            stopOnDivergence = false;    // Halt at first divergence?
    bool            captureSnapshots = true;     // Snapshot workspace before/after?
    bool            emitTimingEvents = true;     // Report timing anomalies?
    float           timingThreshold  = 10.0f;    // Multiplier for timing anomaly
    int             maxSteps         = 0;        // 0 = replay all steps
    std::string     transcriptPath;              // Source transcript JSON
    std::string     outputPath;                  // Where to write replay report
    std::string     snapshotDir;                 // Pre-replay workspace snapshot dir
};

// ============================================================================
// DeterministicReplayEngine — The core replay orchestrator
// ============================================================================
class DeterministicReplayEngine {
public:
    DeterministicReplayEngine();
    ~DeterministicReplayEngine();

    // ---- Configuration ----
    void Configure(const ReplayConfig& config);

    // ---- Transcript loading ----
    bool LoadTranscript(const std::string& jsonPath);
    bool LoadTranscriptFromJson(const nlohmann::json& transcriptJson);
    bool LoadTranscriptFromObject(const AgentTranscript& transcript);

    // ---- Workspace snapshot ----
    WorkspaceSnapshot CaptureWorkspaceSnapshot(const std::string& rootDir);
    bool ValidateWorkspaceSnapshot(const WorkspaceSnapshot& expected);

    // ---- Replay execution ----
    ReplayResult Execute();
    ReplayResult ExecuteFrom(int startStep);
    ReplayResult ExecuteRange(int startStep, int endStep);
    void Cancel();

    // ---- Callbacks ----
    void SetProgressCallback(ReplayProgressCallback callback);

    // ---- State queries ----
    bool IsRunning() const { return m_running.load(); }
    int GetCurrentStep() const { return m_currentStep.load(); }
    const ReplayResult& GetLastResult() const { return m_lastResult; }

    // ---- Report generation ----
    bool SaveReport(const std::string& path) const;
    nlohmann::json GenerateReport() const;

private:
    // ---- Internal step execution ----
    DivergenceEvent* ReplayStep(int stepIndex, const TranscriptStep& recorded);
    ToolCallResult ExecuteToolFromTranscript(const TranscriptStep& step);
    bool CompareToolOutput(const ToolCallResult& expected,
                           const ToolCallResult& actual,
                           DivergenceEvent& outDiv);

    // ---- Timing analysis ----
    bool CheckTimingAnomaly(int64_t recordedMs, int64_t replayMs,
                            DivergenceEvent& outDiv);

    // ---- Telemetry snapshot + classification ----
    static TelemetrySnapshot CaptureTelemetrySnapshot();
    static FailureClass ClassifyDivergence(const DivergenceEvent& event,
                                           const TelemetrySnapshot& preStep,
                                           const TelemetrySnapshot& postStep);
    void AnnotateDivergence(DivergenceEvent& event,
                            const TelemetrySnapshot& preStep);

    // ---- Utilities ----
    static uint64_t NowMs();
    static uint64_t ComputeXXHash64(const std::string& filePath);
    static std::string GenerateReplayId();

    // ---- State ----
    ReplayConfig                m_config;
    AgentTranscript             m_transcript;
    WorkspaceSnapshot           m_preSnapshot;
    ReplayResult                m_lastResult;
    ReplayProgressCallback      m_progressCallback;

    std::atomic<bool>           m_running{false};
    std::atomic<bool>           m_cancelled{false};
    std::atomic<int>            m_currentStep{0};
    mutable std::mutex          m_mutex;
};

} // namespace Agent
} // namespace RawrXD
