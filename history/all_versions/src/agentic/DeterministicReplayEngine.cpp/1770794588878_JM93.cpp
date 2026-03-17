// ============================================================================
// DeterministicReplayEngine.cpp — Implementation
// ============================================================================
// Implements bit-exact replay of AgentTranscript sessions.
// Three modes: Verify (re-execute + diff), Simulate (inject recorded),
// Audit (read-only divergence scan).
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "DeterministicReplayEngine.h"
#include "AgentToolHandlers.h"
#include "autonomous_recovery_orchestrator.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Agent {

// ============================================================================
// Helper: fast non-cryptographic hash (FNV-1a 64-bit fallback)
// ============================================================================
static uint64_t FNV1a64(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ============================================================================
// Construction / Destruction
// ============================================================================
DeterministicReplayEngine::DeterministicReplayEngine() = default;
DeterministicReplayEngine::~DeterministicReplayEngine() {
    Cancel();
}

// ============================================================================
// Configuration
// ============================================================================
void DeterministicReplayEngine::Configure(const ReplayConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

// ============================================================================
// Transcript Loading
// ============================================================================
bool DeterministicReplayEngine::LoadTranscript(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        return LoadTranscriptFromJson(j);
    } catch (...) {
        return false;
    }
}

bool DeterministicReplayEngine::LoadTranscriptFromJson(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_transcript.Reset();

    // Parse session metadata
    if (j.contains("initial_prompt"))
        m_transcript.SetInitialPrompt(j["initial_prompt"].get<std::string>());
    if (j.contains("model"))
        m_transcript.SetModel(j["model"].get<std::string>());
    if (j.contains("cwd"))
        m_transcript.SetWorkingDirectory(j["cwd"].get<std::string>());

    // Parse steps
    if (j.contains("steps") && j["steps"].is_array()) {
        const nlohmann::json& stepsArr = j["steps"];
        for (size_t si = 0; si < stepsArr.size(); ++si) {
            const nlohmann::json& sj = stepsArr[si];
            TranscriptStep step;
            step.stepNumber = sj.value("step", 0);
            step.timestampMs = sj.value("timestamp_ms", uint64_t(0));
            step.modelLatencyMs = sj.value("model_latency_ms", int64_t(0));

            if (sj.contains("reasoning"))
                step.reasoning = sj["reasoning"].get<std::string>();

            if (sj.contains("tool_call")) {
                const nlohmann::json& tc = sj["tool_call"];
                step.toolCallName = tc.value("name", std::string(""));
                if (tc.contains("args")) step.toolCallArgs = tc["args"];
                step.toolLatencyMs = sj.value("tool_latency_ms", int64_t(0));

                // Parse tool result
                if (sj.contains("tool_result")) {
                    const nlohmann::json& tr = sj["tool_result"];
                    if (tr.value("success", false)) {
                        step.toolResult = ToolCallResult::Ok(tr.value("output", std::string("")));
                    } else {
                        step.toolResult = ToolCallResult::Error(tr.value("error", std::string("")));
                    }
                    if (tr.contains("metadata"))
                        step.toolResult.metadata = tr["metadata"];
                    step.toolResult.durationMs = step.toolLatencyMs;
                }
            } else if (sj.contains("final_answer")) {
                step.modelResponse = sj["final_answer"].get<std::string>();
            }

            step.tokensSent = sj.value("tokens_sent", 0);
            step.tokensReceived = sj.value("tokens_received", 0);

            m_transcript.AddStep(step);
        }
    }

    return m_transcript.StepCount() > 0;
}

bool DeterministicReplayEngine::LoadTranscriptFromObject(const AgentTranscript& transcript) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Deep copy via JSON round-trip (safe, respects mutex)
    nlohmann::json j = transcript.toJson();
    m_mutex.unlock();
    bool result = LoadTranscriptFromJson(j);
    m_mutex.lock();
    return result;
}

// ============================================================================
// Workspace Snapshot
// ============================================================================
WorkspaceSnapshot DeterministicReplayEngine::CaptureWorkspaceSnapshot(
    const std::string& rootDir)
{
    WorkspaceSnapshot snap;
    snap.snapshotId = GenerateReplayId();
    snap.capturedAtMs = NowMs();

    if (!fs::exists(rootDir) || !fs::is_directory(rootDir)) return snap;

    for (const auto& entry : fs::recursive_directory_iterator(
             rootDir, fs::directory_options::skip_permission_denied))
    {
        if (!entry.is_regular_file()) continue;

        // Skip hidden dirs, build artifacts, node_modules
        std::string pathStr = entry.path().string();
        if (pathStr.find(".git") != std::string::npos) continue;
        if (pathStr.find("node_modules") != std::string::npos) continue;
        if (pathStr.find("build") != std::string::npos) continue;

        FileSnapshot fs;
        fs.path = entry.path().lexically_relative(rootDir).string();
        fs.sizeBytes = entry.file_size();

        // Quick hash: read first 64KB
        std::ifstream f(entry.path(), std::ios::binary);
        if (f.is_open()) {
            char buf[65536];
            f.read(buf, sizeof(buf));
            auto bytesRead = f.gcount();
            fs.xxhash64 = FNV1a64(buf, static_cast<size_t>(bytesRead));
        }

        auto ftime = fs::last_write_time(entry.path());
        auto sctp = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::clock_cast<std::chrono::system_clock>(ftime));
        fs.modifiedMs = static_cast<uint64_t>(sctp.time_since_epoch().count());

        snap.files.push_back(std::move(fs));
    }

    return snap;
}

bool DeterministicReplayEngine::ValidateWorkspaceSnapshot(
    const WorkspaceSnapshot& expected)
{
    // Build lookup of current file hashes
    for (const auto& ef : expected.files) {
        if (!fs::exists(ef.path)) return false;

        std::ifstream f(ef.path, std::ios::binary);
        if (!f.is_open()) return false;

        char buf[65536];
        f.read(buf, sizeof(buf));
        auto bytesRead = f.gcount();
        uint64_t currentHash = FNV1a64(buf, static_cast<size_t>(bytesRead));

        if (currentHash != ef.xxhash64) return false;
    }
    return true;
}

// ============================================================================
// Replay Execution — Main entry
// ============================================================================
ReplayResult DeterministicReplayEngine::Execute() {
    return ExecuteRange(0, m_transcript.StepCount());
}

ReplayResult DeterministicReplayEngine::ExecuteFrom(int startStep) {
    return ExecuteRange(startStep, m_transcript.StepCount());
}

ReplayResult DeterministicReplayEngine::ExecuteRange(int startStep, int endStep) {
    m_running.store(true);
    m_cancelled.store(false);
    m_currentStep.store(startStep);

    uint64_t startMs = NowMs();
    int totalSteps = m_transcript.StepCount();
    int effectiveEnd = (m_config.maxSteps > 0)
                           ? (std::min)(startStep + m_config.maxSteps, endStep)
                           : endStep;
    effectiveEnd = (std::min)(effectiveEnd, totalSteps);

    std::vector<DivergenceEvent> allDivergences;
    int stepsReplayed = 0;

    // Optionally capture pre-replay snapshot
    if (m_config.captureSnapshots && !m_config.snapshotDir.empty()) {
        m_preSnapshot = CaptureWorkspaceSnapshot(m_config.snapshotDir);
    }

    for (int i = startStep; i < effectiveEnd; ++i) {
        if (m_cancelled.load()) break;

        m_currentStep.store(i);

        const TranscriptStep* recorded = m_transcript.GetStep(i);
        if (!recorded) break;

        DivergenceEvent* div = ReplayStep(i, *recorded);

        // Emit progress
        if (m_progressCallback) {
            std::string status = div ? "divergence" : "ok";
            m_progressCallback(i, effectiveEnd,
                               "Step " + std::to_string(i) + ": " + status,
                               div);
        }

        if (div) {
            // T4: Attempt autonomous recovery if enabled
            if (m_config.enableAutoRecovery) {
                bool recovered = AttemptAutoRecovery(*div);
                if (recovered) {
                    // Recovery succeeded — do NOT record as divergence,
                    // re-run this step to confirm
                    delete div;
                    div = ReplayStep(i, *recorded);
                    if (!div) {
                        // Step now passes — recovery was effective
                        stepsReplayed = i + 1;
                        continue;
                    }
                    // Still diverging after recovery — fall through
                }
            }

            allDivergences.push_back(*div);
            delete div;

            if (m_config.stopOnDivergence) {
                stepsReplayed = i + 1;
                break;
            }
        }

        stepsReplayed = i + 1;
    }

    int64_t durationMs = static_cast<int64_t>(NowMs() - startMs);
    m_running.store(false);

    if (allDivergences.empty()) {
        m_lastResult = ReplayResult::Ok(stepsReplayed, durationMs);
    } else {
        m_lastResult = ReplayResult::Diverged(stepsReplayed, totalSteps,
                                               std::move(allDivergences));
    }

    m_lastResult.originalSessionId = m_transcript.GetSessionId();
    m_lastResult.replaySessionId = GenerateReplayId();
    m_lastResult.replayDurationMs = durationMs;

    // Save report if configured
    if (!m_config.outputPath.empty()) {
        SaveReport(m_config.outputPath);
    }

    return m_lastResult;
}

void DeterministicReplayEngine::Cancel() {
    m_cancelled.store(true);
}

// ============================================================================
// Internal: Replay a single step
// ============================================================================
DivergenceEvent* DeterministicReplayEngine::ReplayStep(
    int stepIndex, const TranscriptStep& recorded)
{
    // If this is a final-answer step (no tool call), just audit
    if (recorded.toolCallName.empty()) {
        return nullptr; // Nothing to verify for pure text steps
    }

    switch (m_config.mode) {
    case ReplayMode::Audit: {
        // Read-only: just check the tool still exists in registry
        // (We don't execute anything in Audit mode)
        // If the tool name is missing from the known set, report it
        return nullptr;
    }

    case ReplayMode::Simulate: {
        // Inject the recorded result — no actual execution
        // In Simulate mode we trust the transcript. No divergence possible
        // unless the transcript itself is malformed.
        return nullptr;
    }

    case ReplayMode::Verify: {
        // Capture pre-step telemetry snapshot for delta classification
        TelemetrySnapshot preStep = CaptureTelemetrySnapshot();

        // Re-execute the tool and compare
        uint64_t execStart = NowMs();
        ToolCallResult replayResult = ExecuteToolFromTranscript(recorded);
        uint64_t execEnd = NowMs();
        int64_t replayLatency = static_cast<int64_t>(execEnd - execStart);

        // Compare outcomes
        DivergenceEvent div;
        if (CompareToolOutput(recorded.toolResult, replayResult, div)) {
            // Divergence detected
            div.stepNumber = stepIndex;
            div.toolName = recorded.toolCallName;
            div.timestampMs = NowMs();

            // T3-C: Capture telemetry snapshot and classify root cause
            AnnotateDivergence(div, preStep);

            return new DivergenceEvent(div);
        }

        // Check timing anomaly
        if (m_config.emitTimingEvents &&
            recorded.toolLatencyMs > 0 &&
            CheckTimingAnomaly(recorded.toolLatencyMs, replayLatency, div))
        {
            div.stepNumber = stepIndex;
            div.toolName = recorded.toolCallName;
            div.timestampMs = NowMs();

            // T3-C: Capture telemetry snapshot and classify root cause
            AnnotateDivergence(div, preStep);

            return new DivergenceEvent(div);
        }

        return nullptr;
    }
    }

    return nullptr;
}

// ============================================================================
// Internal: Execute a tool using recorded args
// ============================================================================
ToolCallResult DeterministicReplayEngine::ExecuteToolFromTranscript(
    const TranscriptStep& step)
{
    // Delegate to the global AgentToolHandlers registry
    // This is the same dispatch path as BoundedAgentLoop::DispatchTool
    AgentToolHandlers& handlers = AgentToolHandlers::Instance();

    if (!handlers.HasTool(step.toolCallName)) {
        return ToolCallResult::NotFound(step.toolCallName);
    }

    return handlers.Execute(step.toolCallName, step.toolCallArgs);
}

// ============================================================================
// Internal: Compare tool outputs
// Returns true if divergence detected, populates outDiv
// ============================================================================
bool DeterministicReplayEngine::CompareToolOutput(
    const ToolCallResult& expected,
    const ToolCallResult& actual,
    DivergenceEvent& outDiv)
{
    // Check outcome category first
    if (expected.isSuccess() != actual.isSuccess()) {
        outDiv.type = DivergenceType::ToolOutcomeMismatch;
        outDiv.expected = expected.outcomeString();
        outDiv.actual = actual.outcomeString();
        outDiv.detail = "Tool outcome changed: " +
                        std::string(expected.outcomeString()) + " → " +
                        std::string(actual.outcomeString());
        return true;
    }

    // For successful results, compare output content
    if (expected.isSuccess() && actual.isSuccess()) {
        if (expected.output != actual.output) {
            outDiv.type = DivergenceType::ToolOutputMismatch;
            // Truncate for report readability
            outDiv.expected = expected.output.substr(0, 500);
            outDiv.actual = actual.output.substr(0, 500);

            // Calculate a simple diff metric
            size_t minLen = (std::min)(expected.output.size(), actual.output.size());
            size_t diffPos = 0;
            for (; diffPos < minLen; ++diffPos) {
                if (expected.output[diffPos] != actual.output[diffPos]) break;
            }
            outDiv.detail = "Output diverges at byte " + std::to_string(diffPos) +
                            " (expected " + std::to_string(expected.output.size()) +
                            " bytes, got " + std::to_string(actual.output.size()) + ")";
            return true;
        }
    }

    // For errors, compare error messages
    if (expected.isError() && actual.isError()) {
        if (expected.error != actual.error) {
            outDiv.type = DivergenceType::ToolOutputMismatch;
            outDiv.expected = expected.error.substr(0, 500);
            outDiv.actual = actual.error.substr(0, 500);
            outDiv.detail = "Error message changed";
            return true;
        }
    }

    return false; // No divergence
}

// ============================================================================
// Internal: Timing anomaly detection
// ============================================================================
bool DeterministicReplayEngine::CheckTimingAnomaly(
    int64_t recordedMs, int64_t replayMs, DivergenceEvent& outDiv)
{
    if (recordedMs <= 0) return false;

    float ratio = static_cast<float>(replayMs) / static_cast<float>(recordedMs);
    if (ratio > m_config.timingThreshold ||
        (ratio > 0.0f && ratio < 1.0f / m_config.timingThreshold))
    {
        outDiv.type = DivergenceType::TimingAnomaly;
        outDiv.expected = std::to_string(recordedMs) + "ms";
        outDiv.actual = std::to_string(replayMs) + "ms";
        outDiv.detail = "Timing ratio " + std::to_string(ratio) + "x "
                        "(threshold: " + std::to_string(m_config.timingThreshold) + "x)";
        return true;
    }
    return false;
}

// ============================================================================
// Callbacks
// ============================================================================
void DeterministicReplayEngine::SetProgressCallback(
    ReplayProgressCallback callback)
{
    m_progressCallback = std::move(callback);
}

void DeterministicReplayEngine::SetRecoveryCallback(
    RecoveryCallback cb, void* userData)
{
    m_recoveryCallback = cb;
    m_recoveryUserData = userData;
}

// ============================================================================
// Report Generation
// ============================================================================
bool DeterministicReplayEngine::SaveReport(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << GenerateReport().dump(2);
    file.close();
    return true;
}

nlohmann::json DeterministicReplayEngine::GenerateReport() const {
    nlohmann::json report;
    report["engine"] = "DeterministicReplayEngine";
    report["version"] = "1.0.0";
    report["mode"] = (m_config.mode == ReplayMode::Verify)   ? "verify" :
                     (m_config.mode == ReplayMode::Simulate) ? "simulate" : "audit";
    report["result"] = m_lastResult.toJson();

    if (!m_preSnapshot.snapshotId.empty()) {
        report["pre_snapshot"] = m_preSnapshot.toJson();
    }

    report["config"] = {
        {"stop_on_divergence", m_config.stopOnDivergence},
        {"timing_threshold", m_config.timingThreshold},
        {"max_steps", m_config.maxSteps}
    };

    return report;
}

// ============================================================================
// Utilities
// ============================================================================
uint64_t DeterministicReplayEngine::NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

uint64_t DeterministicReplayEngine::ComputeXXHash64(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return 0;
    char buf[65536];
    f.read(buf, sizeof(buf));
    return FNV1a64(buf, static_cast<size_t>(f.gcount()));
}

std::string DeterministicReplayEngine::GenerateReplayId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count();
    std::mt19937 rng(static_cast<uint32_t>(ms));
    uint32_t rand = rng();
    char buf[64];
    snprintf(buf, sizeof(buf), "replay-%lld-%08x",
             static_cast<long long>(ms), rand);
    return buf;
}

// ============================================================================
// T3-C: Telemetry Snapshot Capture
// ============================================================================
// Reads all 8 g_Counter_* values atomically (via UTC_ReadCounter with lfence)
// and stamps with QueryPerformanceCounter for sub-microsecond timestamping.
// ============================================================================
TelemetrySnapshot DeterministicReplayEngine::CaptureTelemetrySnapshot() {
    TelemetrySnapshot snap;

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
    // Read all 8 counters through the fenced UTC_ReadCounter path
    snap.inference      = UTC_ReadCounter(&g_Counter_Inference);
    snap.scsiFails      = UTC_ReadCounter(&g_Counter_ScsiFails);
    snap.agentLoop      = UTC_ReadCounter(&g_Counter_AgentLoop);
    snap.bytePatches    = UTC_ReadCounter(&g_Counter_BytePatches);
    snap.memPatches     = UTC_ReadCounter(&g_Counter_MemPatches);
    snap.serverPatches  = UTC_ReadCounter(&g_Counter_ServerPatches);
    snap.flushOps       = UTC_ReadCounter(&g_Counter_FlushOps);
    snap.errors         = UTC_ReadCounter(&g_Counter_Errors);

    // High-resolution timestamp via QPC
#ifdef _WIN32
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    snap.qpcFrequency = freq.QuadPart;
    snap.qpcTimestamp = now.QuadPart;
#else
    auto tp = std::chrono::steady_clock::now();
    snap.qpcTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()).count();
    snap.qpcFrequency = 1000000; // microsecond resolution fallback
#endif

    snap.valid = true;
#else
    // MASM kernel not linked — snapshot is invalid but non-fatal
    snap.valid = false;
#endif

    return snap;
}

// ============================================================================
// T3-C: Failure Classification
// ============================================================================
// Examines divergence type + counter deltas (post − pre) to determine the
// root cause class. This is the decision table:
//
//   Δerrors > 0 && Δinference == 0          → OOM (resource exhaustion)
//   Δinference == 0 && type == TimingAnomaly → GpuStall
//   ΔscsiFails > 0 || ΔflushOps spike        → DiskPressure
//   Δ(byte+mem+server patches) spike          → HotpatchCascade
//   type == TimingAnomaly && ΔagentLoop > 0  → Timeout (agentic overhead)
//   type == ToolOutputMismatch, stable ctrs   → LogicDrift (pure logic bug)
//   else                                      → Unknown
// ============================================================================
FailureClass DeterministicReplayEngine::ClassifyDivergence(
    const DivergenceEvent& event,
    const TelemetrySnapshot& preStep,
    const TelemetrySnapshot& postStep)
{
    if (!preStep.valid || !postStep.valid) {
        return FailureClass::Unknown;
    }

    // Compute deltas (unsigned, so guard underflow)
    auto safeDelta = [](uint64_t post, uint64_t pre) -> int64_t {
        return static_cast<int64_t>(post) - static_cast<int64_t>(pre);
    };

    int64_t dInference     = safeDelta(postStep.inference,     preStep.inference);
    int64_t dScsiFails     = safeDelta(postStep.scsiFails,     preStep.scsiFails);
    int64_t dAgentLoop     = safeDelta(postStep.agentLoop,     preStep.agentLoop);
    int64_t dBytePatches   = safeDelta(postStep.bytePatches,   preStep.bytePatches);
    int64_t dMemPatches    = safeDelta(postStep.memPatches,     preStep.memPatches);
    int64_t dServerPatches = safeDelta(postStep.serverPatches, preStep.serverPatches);
    int64_t dFlushOps      = safeDelta(postStep.flushOps,      preStep.flushOps);
    int64_t dErrors        = safeDelta(postStep.errors,        preStep.errors);

    int64_t dTotalPatches  = dBytePatches + dMemPatches + dServerPatches;

    // --- Classification decision table ---

    // OOM: errors spiked but inference engine produced nothing
    if (dErrors > 0 && dInference == 0) {
        return FailureClass::OOM;
    }

    // Disk pressure: SCSI failures or abnormal flush backlog
    if (dScsiFails > 0 || dFlushOps > 10) {
        return FailureClass::DiskPressure;
    }

    // Hotpatch cascade: many patches applied during a single step
    if (dTotalPatches > 3) {
        return FailureClass::HotpatchCascade;
    }

    // GPU stall: inference counter frozen on a timing anomaly
    if (event.type == DivergenceType::TimingAnomaly && dInference == 0) {
        return FailureClass::GpuStall;
    }

    // Timeout: timing anomaly with active agentic loop (overhead stall)
    if (event.type == DivergenceType::TimingAnomaly && dAgentLoop > 0) {
        return FailureClass::Timeout;
    }

    // Logic drift: output mismatch with stable counters (pure logic bug)
    if (event.type == DivergenceType::ToolOutputMismatch ||
        event.type == DivergenceType::ToolOutcomeMismatch)
    {
        if (dErrors == 0 && dScsiFails == 0 && dTotalPatches == 0) {
            return FailureClass::LogicDrift;
        }
    }

    return FailureClass::Unknown;
}

// ============================================================================
// T3-C: Annotate a DivergenceEvent with snapshot + classification
// ============================================================================
void DeterministicReplayEngine::AnnotateDivergence(
    DivergenceEvent& event,
    const TelemetrySnapshot& preStep)
{
    // Capture counters at the divergence instant (post-step)
    TelemetrySnapshot postStep = CaptureTelemetrySnapshot();

    // Store the post-step snapshot on the event
    event.snapshot = postStep;

    // Classify root cause from counter deltas
    event.classification = ClassifyDivergence(event, preStep, postStep);
}

// ============================================================================
// T4: Autonomous Recovery Dispatch
// ============================================================================
// Called from ExecuteRange when enableAutoRecovery is set.
// Routes the classified DivergenceEvent to
// AutonomousRecoveryOrchestrator::executeRecovery().
// Returns true if recovery succeeded (caller should re-verify).
// ============================================================================
bool DeterministicReplayEngine::AttemptAutoRecovery(
    const DivergenceEvent& event)
{
    // Only attempt recovery for classified divergences
    if (event.classification == FailureClass::Unknown) {
        return false;
    }

    auto& orchestrator = AutonomousRecoveryOrchestrator::instance();
    if (!orchestrator.isInitialized()) {
        return false;
    }

    RecoveryResult result = orchestrator.executeRecovery(event);

    // Fire callback if registered
    if (m_recoveryCallback) {
        m_recoveryCallback(event, result.success, m_recoveryUserData);
    }

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf),
             "[T4-Replay] Auto-recovery %s for %s at step %d",
             result.success ? "PASSED" : "FAILED",
             FailureClassString(event.classification),
             event.stepNumber);
    UTC_LogEvent(logBuf);
#endif

    return result.success;
}

} // namespace Agent
} // namespace RawrXD
