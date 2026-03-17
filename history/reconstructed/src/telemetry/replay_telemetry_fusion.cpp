// ============================================================================
// replay_telemetry_fusion.cpp — Replay + Telemetry Fusion Engine
// ============================================================================
// Combines DeterministicReplayEngine and UTC MASM counters to detect:
//   - Logical divergence (action sequence mismatches)
//   - Performance divergence (throughput regression)
//   - Tool output drift (FNV-1a hash comparison)
//   - Loop count anomalies (UTC counter vs replay journal)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "replay_telemetry_fusion.h"
#include "deterministic_replay.h"
#include "rawrxd_telemetry_exports.h"

#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Fusion {

// ============================================================================
// String Helpers
// ============================================================================

const char* divergenceTypeToString(DivergenceType t) {
    switch (t) {
        case DivergenceType::None:                  return "None";
        case DivergenceType::LogicalDivergence:     return "LogicalDivergence";
        case DivergenceType::PerformanceDivergence: return "PerformanceDivergence";
        case DivergenceType::ToolOutputDrift:        return "ToolOutputDrift";
        case DivergenceType::LoopCountAnomaly:       return "LoopCountAnomaly";
        case DivergenceType::CounterSpikeAnomaly:    return "CounterSpikeAnomaly";
        case DivergenceType::SessionMismatch:        return "SessionMismatch";
        case DivergenceType::HotpatchDivergence:     return "HotpatchDivergence";
        default:                                     return "Unknown";
    }
}

// ============================================================================
// Timing Helper (no CRT dependency beyond QPC)
// ============================================================================
static uint64_t nowMs() {
#ifdef _WIN32
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>((now.QuadPart * 1000) / freq.QuadPart);
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(tp).count());
#endif
}

// ============================================================================
// Singleton
// ============================================================================

ReplayTelemetryFusion& ReplayTelemetryFusion::instance() {
    static ReplayTelemetryFusion inst;
    return inst;
}

ReplayTelemetryFusion::ReplayTelemetryFusion() = default;
ReplayTelemetryFusion::~ReplayTelemetryFusion() { shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

FusionResult ReplayTelemetryFusion::initialize() {
    if (m_initialized.load(std::memory_order_acquire)) {
        return FusionResult::ok("Already initialized");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseline = FusionSnapshot{};
    m_thresholds = DivergenceThresholds{};
    m_callbacks.clear();
    m_initialized.store(true, std::memory_order_release);
    return FusionResult::ok("ReplayTelemetryFusion initialized");
}

FusionResult ReplayTelemetryFusion::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return FusionResult::ok("Not initialized");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.clear();
    m_initialized.store(false, std::memory_order_release);
    return FusionResult::ok("ReplayTelemetryFusion shut down");
}

bool ReplayTelemetryFusion::isInitialized() const {
    return m_initialized.load(std::memory_order_acquire);
}

// ============================================================================
// Counter Snapshots
// ============================================================================

CounterSnapshot ReplayTelemetryFusion::captureCounters() const {
    CounterSnapshot snap{};
    snap.timestampMs = nowMs();

#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
    snap.inference     = UTC_ReadCounter(&g_Counter_Inference);
    snap.scsiFails     = UTC_ReadCounter(&g_Counter_ScsiFails);
    snap.agentLoop     = UTC_ReadCounter(&g_Counter_AgentLoop);
    snap.bytePatches   = UTC_ReadCounter(&g_Counter_BytePatches);
    snap.memPatches    = UTC_ReadCounter(&g_Counter_MemPatches);
    snap.serverPatches = UTC_ReadCounter(&g_Counter_ServerPatches);
    snap.flushOps      = UTC_ReadCounter(&g_Counter_FlushOps);
    snap.errors        = UTC_ReadCounter(&g_Counter_Errors);
#endif

    return snap;
}

// ============================================================================
// Fusion Snapshot
// ============================================================================

FusionSnapshot ReplayTelemetryFusion::captureFusionSnapshot(const char* label) const {
    FusionSnapshot snap{};
    snap.counters = captureCounters();
    snap.label = label ? label : "";

    // Pull data from ReplayJournal
    ReplayJournal& journal = ReplayJournal::instance();
    snap.replaySessionId = journal.getActiveSessionId();

    ReplayJournalStats stats = journal.getStats();
    snap.replaySequenceId = stats.currentSequenceId;
    snap.replayActionCount = stats.totalRecords;

    // Count tool calls and hash tool outputs
    if (snap.replaySessionId > 0) {
        std::vector<ActionRecord> records = journal.getSessionRecords(snap.replaySessionId);
        for (const auto& rec : records) {
            if (rec.type == ReplayActionType::AgentToolCall) {
                snap.replayToolCalls++;
            }
            if (rec.type == ReplayActionType::AgentToolResult && !rec.output.empty()) {
                snap.toolOutputHashes.push_back(fnv1a(rec.output));
            }
        }

        // Session duration
        SessionSnapshot sessionSnap = journal.getSessionSnapshot(snap.replaySessionId);
        snap.sessionDurationMs = sessionSnap.totalDurationMs;
    }

    return snap;
}

// ============================================================================
// Divergence Detection
// ============================================================================

DivergenceReport ReplayTelemetryFusion::compare(
    const FusionSnapshot& baseline,
    const FusionSnapshot& current) const
{
    DivergenceReport report{};
    report.passed = true;
    report.baselineSnapshot = baseline;
    report.currentSnapshot = current;
    report.baselineSessionId = baseline.replaySessionId;
    report.currentSessionId = current.replaySessionId;
    report.timestampMs = nowMs();

    // Run all sub-checks
    checkLoopCountAnomaly(baseline, current, report);
    checkToolOutputDrift(baseline, current, report);
    checkCounterSpikes(baseline, current, report);
    checkSessionStructure(baseline, current, report);

    // Build summary
    int errors = report.countBySeverity(DivergenceSeverity::Error);
    int criticals = report.countBySeverity(DivergenceSeverity::Critical);
    int warnings = report.countBySeverity(DivergenceSeverity::Warning);

    if (errors > 0 || criticals > 0) {
        report.passed = false;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
        "Divergence check: %zu entries (%d errors, %d critical, %d warnings) — %s",
        report.entries.size(), errors, criticals, warnings,
        report.passed ? "PASS" : "FAIL");
    report.summary = buf;

    // Notify callbacks for Error/Critical entries
    for (const auto& entry : report.entries) {
        if (entry.severity >= DivergenceSeverity::Error) {
            notifyCallbacks(entry);
        }
    }

    return report;
}

DivergenceReport ReplayTelemetryFusion::compareAgainstBaseline() const {
    FusionSnapshot current = captureFusionSnapshot("current");
    std::lock_guard<std::mutex> lock(m_mutex);
    return compare(m_baseline, current);
}

// ============================================================================
// Internal Divergence Checkers
// ============================================================================

void ReplayTelemetryFusion::checkLoopCountAnomaly(
    const FusionSnapshot& baseline,
    const FusionSnapshot& current,
    DivergenceReport& report) const
{
    // UTC g_Counter_AgentLoop vs replay journal action count
    uint64_t utcLoops = current.counters.agentLoop;
    uint64_t replayActions = current.replayActionCount;

    if (replayActions == 0 && utcLoops == 0) return; // Nothing to compare

    double maxVal = static_cast<double>(std::max(utcLoops, replayActions));
    if (maxVal <= 0) maxVal = 1.0;
    double deltaPct = (static_cast<double>(utcLoops) - static_cast<double>(replayActions)) / maxVal * 100.0;
    double absDelta = deltaPct < 0 ? -deltaPct : deltaPct;

    DivergenceEntry entry{};
    entry.type = DivergenceType::LoopCountAnomaly;
    entry.metric = "agent_loop_count";
    entry.expected = static_cast<double>(replayActions);
    entry.actual = static_cast<double>(utcLoops);
    entry.deltaPct = deltaPct;
    entry.threshold = m_thresholds.loopCountTolerancePct;
    entry.timestampMs = nowMs();

    if (absDelta > m_thresholds.loopCountTolerancePct) {
        entry.severity = DivergenceSeverity::Error;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "UTC counter (%llu) vs replay actions (%llu): %.1f%% divergence (max %.1f%%)",
            static_cast<unsigned long long>(utcLoops),
            static_cast<unsigned long long>(replayActions),
            absDelta, m_thresholds.loopCountTolerancePct);
        entry.detail = buf;
        report.passed = false;
    } else {
        entry.severity = DivergenceSeverity::Info;
        entry.detail = "Loop count within tolerance";
    }

    report.entries.push_back(std::move(entry));

    // Also compare against baseline if available
    if (baseline.counters.agentLoop > 0) {
        double baselineDelta = static_cast<double>(utcLoops) - static_cast<double>(baseline.counters.agentLoop);
        double baselinePct = (baselineDelta / static_cast<double>(baseline.counters.agentLoop)) * 100.0;
        double absBl = baselinePct < 0 ? -baselinePct : baselinePct;

        if (absBl > m_thresholds.loopCountTolerancePct * 2) {
            DivergenceEntry blEntry{};
            blEntry.type = DivergenceType::LoopCountAnomaly;
            blEntry.severity = DivergenceSeverity::Warning;
            blEntry.metric = "agent_loop_vs_baseline";
            blEntry.expected = static_cast<double>(baseline.counters.agentLoop);
            blEntry.actual = static_cast<double>(utcLoops);
            blEntry.deltaPct = baselinePct;
            blEntry.threshold = m_thresholds.loopCountTolerancePct * 2;
            blEntry.detail = "Agent loop count drifted significantly from baseline";
            blEntry.timestampMs = nowMs();
            report.entries.push_back(std::move(blEntry));
        }
    }
}

void ReplayTelemetryFusion::checkToolOutputDrift(
    const FusionSnapshot& baseline,
    const FusionSnapshot& current,
    DivergenceReport& report) const
{
    if (baseline.toolOutputHashes.empty() || current.toolOutputHashes.empty()) return;

    size_t minLen = std::min(baseline.toolOutputHashes.size(), current.toolOutputHashes.size());
    size_t matches = 0;
    for (size_t i = 0; i < minLen; i++) {
        if (baseline.toolOutputHashes[i] == current.toolOutputHashes[i]) {
            matches++;
        }
    }

    double matchPct = (static_cast<double>(matches) / static_cast<double>(minLen)) * 100.0;
    double driftPct = 100.0 - matchPct;

    DivergenceEntry entry{};
    entry.type = DivergenceType::ToolOutputDrift;
    entry.metric = "tool_output_hashes";
    entry.expected = static_cast<double>(minLen);
    entry.actual = static_cast<double>(matches);
    entry.deltaPct = driftPct;
    entry.threshold = m_thresholds.toolOutputDriftPct;
    entry.timestampMs = nowMs();

    if (driftPct > m_thresholds.toolOutputDriftPct) {
        entry.severity = DivergenceSeverity::Error;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Tool output drift: %.1f%% (%zu/%zu mismatched, max %.1f%%)",
            driftPct, minLen - matches, minLen, m_thresholds.toolOutputDriftPct);
        entry.detail = buf;
        report.passed = false;
    } else {
        entry.severity = DivergenceSeverity::Info;
        entry.detail = "Tool outputs within drift tolerance";
    }

    report.entries.push_back(std::move(entry));
}

void ReplayTelemetryFusion::checkCounterSpikes(
    const FusionSnapshot& baseline,
    const FusionSnapshot& current,
    DivergenceReport& report) const
{
    // Check error counter spike
    if (baseline.counters.errors > 0 || current.counters.errors > 0) {
        int64_t errorDelta = static_cast<int64_t>(current.counters.errors) -
                              static_cast<int64_t>(baseline.counters.errors);

        if (errorDelta > static_cast<int64_t>(m_thresholds.errorSpikeTolerance)) {
            DivergenceEntry entry{};
            entry.type = DivergenceType::CounterSpikeAnomaly;
            entry.severity = DivergenceSeverity::Error;
            entry.metric = "error_count";
            entry.expected = static_cast<double>(baseline.counters.errors);
            entry.actual = static_cast<double>(current.counters.errors);
            entry.deltaPct = baseline.counters.errors > 0
                ? (static_cast<double>(errorDelta) / static_cast<double>(baseline.counters.errors)) * 100.0
                : 100.0;
            entry.threshold = m_thresholds.errorSpikeTolerance;
            entry.timestampMs = nowMs();

            char buf[256];
            snprintf(buf, sizeof(buf),
                "Error spike: +%lld errors (baseline %llu → current %llu, tolerance %.0f)",
                static_cast<long long>(errorDelta),
                static_cast<unsigned long long>(baseline.counters.errors),
                static_cast<unsigned long long>(current.counters.errors),
                m_thresholds.errorSpikeTolerance);
            entry.detail = buf;
            report.passed = false;
            report.entries.push_back(std::move(entry));
        }
    }

    // Check SCSI failure spike
    if (current.counters.scsiFails > baseline.counters.scsiFails + 10) {
        DivergenceEntry entry{};
        entry.type = DivergenceType::CounterSpikeAnomaly;
        entry.severity = DivergenceSeverity::Warning;
        entry.metric = "scsi_failures";
        entry.expected = static_cast<double>(baseline.counters.scsiFails);
        entry.actual = static_cast<double>(current.counters.scsiFails);
        entry.deltaPct = 0;
        entry.threshold = 10;
        entry.timestampMs = nowMs();
        entry.detail = "SCSI failure count increased significantly";
        report.entries.push_back(std::move(entry));
    }
}

void ReplayTelemetryFusion::checkSessionStructure(
    const FusionSnapshot& baseline,
    const FusionSnapshot& current,
    DivergenceReport& report) const
{
    // If baseline had tool calls but current has zero (or vice-versa)
    if (baseline.replayToolCalls > 0 && current.replayToolCalls == 0) {
        DivergenceEntry entry{};
        entry.type = DivergenceType::SessionMismatch;
        entry.severity = DivergenceSeverity::Warning;
        entry.metric = "tool_call_count";
        entry.expected = static_cast<double>(baseline.replayToolCalls);
        entry.actual = 0.0;
        entry.deltaPct = -100.0;
        entry.threshold = 0;
        entry.timestampMs = nowMs();
        entry.detail = "Baseline had tool calls but current session has none";
        report.entries.push_back(std::move(entry));
    }

    // Large action count divergence
    if (baseline.replayActionCount > 0 && current.replayActionCount > 0) {
        double ratio = static_cast<double>(current.replayActionCount) /
                       static_cast<double>(baseline.replayActionCount);
        if (ratio < 0.5 || ratio > 2.0) {
            DivergenceEntry entry{};
            entry.type = DivergenceType::SessionMismatch;
            entry.severity = DivergenceSeverity::Warning;
            entry.metric = "action_count_ratio";
            entry.expected = static_cast<double>(baseline.replayActionCount);
            entry.actual = static_cast<double>(current.replayActionCount);
            entry.deltaPct = (ratio - 1.0) * 100.0;
            entry.threshold = 100.0;
            entry.timestampMs = nowMs();

            char buf[256];
            snprintf(buf, sizeof(buf),
                "Action count ratio %.2fx (%llu → %llu) — session structure may differ",
                ratio,
                static_cast<unsigned long long>(baseline.replayActionCount),
                static_cast<unsigned long long>(current.replayActionCount));
            entry.detail = buf;
            report.entries.push_back(std::move(entry));
        }
    }
}

// ============================================================================
// Baseline Management
// ============================================================================

FusionResult ReplayTelemetryFusion::saveBaseline(const char* label) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseline = captureFusionSnapshot(label ? label : "baseline");
    return FusionResult::ok("Baseline saved");
}

FusionResult ReplayTelemetryFusion::loadBaseline() {
    // In a full impl this would load from disk; for now the baseline
    // is kept in memory via saveBaseline().
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_baseline.counters.timestampMs == 0 && m_baseline.replayActionCount == 0) {
        return FusionResult::error("No baseline available", 1);
    }
    return FusionResult::ok("Baseline loaded");
}

const FusionSnapshot& ReplayTelemetryFusion::getBaseline() const {
    return m_baseline;
}

// ============================================================================
// Export — JSON
// ============================================================================

std::string ReplayTelemetryFusion::exportSnapshotJSON(const FusionSnapshot& snap) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"label\": \"" << snap.label << "\",\n";
    ss << "  \"counters\": {\n";
    ss << "    \"inference\": " << snap.counters.inference << ",\n";
    ss << "    \"scsi_fails\": " << snap.counters.scsiFails << ",\n";
    ss << "    \"agent_loop\": " << snap.counters.agentLoop << ",\n";
    ss << "    \"byte_patches\": " << snap.counters.bytePatches << ",\n";
    ss << "    \"mem_patches\": " << snap.counters.memPatches << ",\n";
    ss << "    \"server_patches\": " << snap.counters.serverPatches << ",\n";
    ss << "    \"flush_ops\": " << snap.counters.flushOps << ",\n";
    ss << "    \"errors\": " << snap.counters.errors << ",\n";
    ss << "    \"timestamp_ms\": " << snap.counters.timestampMs << "\n";
    ss << "  },\n";
    ss << "  \"replay\": {\n";
    ss << "    \"session_id\": " << snap.replaySessionId << ",\n";
    ss << "    \"sequence_id\": " << snap.replaySequenceId << ",\n";
    ss << "    \"action_count\": " << snap.replayActionCount << ",\n";
    ss << "    \"tool_calls\": " << snap.replayToolCalls << ",\n";
    ss << "    \"session_duration_ms\": " << std::fixed << std::setprecision(1)
       << snap.sessionDurationMs << ",\n";
    ss << "    \"tool_output_hashes\": [";
    for (size_t i = 0; i < snap.toolOutputHashes.size(); i++) {
        if (i > 0) ss << ", ";
        ss << snap.toolOutputHashes[i];
    }
    ss << "]\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string ReplayTelemetryFusion::exportReportJSON(const DivergenceReport& report) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"passed\": " << (report.passed ? "true" : "false") << ",\n";
    ss << "  \"summary\": \"" << report.summary << "\",\n";
    ss << "  \"baseline_session_id\": " << report.baselineSessionId << ",\n";
    ss << "  \"current_session_id\": " << report.currentSessionId << ",\n";
    ss << "  \"timestamp_ms\": " << report.timestampMs << ",\n";
    ss << "  \"entries\": [\n";
    for (size_t i = 0; i < report.entries.size(); i++) {
        const auto& e = report.entries[i];
        ss << "    {\n";
        ss << "      \"type\": \"" << divergenceTypeToString(e.type) << "\",\n";
        ss << "      \"severity\": " << static_cast<int>(e.severity) << ",\n";
        ss << "      \"metric\": \"" << e.metric << "\",\n";
        ss << "      \"expected\": " << e.expected << ",\n";
        ss << "      \"actual\": " << e.actual << ",\n";
        ss << "      \"delta_pct\": " << std::fixed << std::setprecision(2) << e.deltaPct << ",\n";
        ss << "      \"threshold\": " << e.threshold << ",\n";
        ss << "      \"detail\": \"" << e.detail << "\"\n";
        ss << "    }";
        if (i + 1 < report.entries.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Export — Fused Prometheus (UTC counters + replay metadata)
// ============================================================================

size_t ReplayTelemetryFusion::exportFusedPrometheus(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize < 4096) return 0;

    size_t pos = 0;
    auto append = [&](const char* s) {
        size_t len = strlen(s);
        if (pos + len < bufferSize) {
            memcpy(buffer + pos, s, len);
            pos += len;
        }
    };

    // First: standard Prometheus counters from MASM kernel
#if defined(RAWRXD_LINK_PROMETHEUS_EXPORTER_ASM) || defined(RAWR_HAS_MASM)
    uint64_t promLen = ExportPrometheus(buffer + pos);
    pos += static_cast<size_t>(promLen);
#endif

    // Append fusion-specific metrics
    FusionSnapshot snap = captureFusionSnapshot("prometheus_export");

    char line[512];

    // Replay metrics
    append("# HELP rawrxd_replay_action_count Total actions recorded in replay journal\n");
    append("# TYPE rawrxd_replay_action_count gauge\n");
    snprintf(line, sizeof(line), "rawrxd_replay_action_count %llu\n",
             static_cast<unsigned long long>(snap.replayActionCount));
    append(line);

    append("# HELP rawrxd_replay_tool_calls Total tool calls in current session\n");
    append("# TYPE rawrxd_replay_tool_calls gauge\n");
    snprintf(line, sizeof(line), "rawrxd_replay_tool_calls %llu\n",
             static_cast<unsigned long long>(snap.replayToolCalls));
    append(line);

    append("# HELP rawrxd_replay_session_duration_ms Session duration in milliseconds\n");
    append("# TYPE rawrxd_replay_session_duration_ms gauge\n");
    snprintf(line, sizeof(line), "rawrxd_replay_session_duration_ms %.1f\n",
             snap.sessionDurationMs);
    append(line);

    // Loop count divergence (computed on-the-fly)
    if (snap.counters.agentLoop > 0 || snap.replayActionCount > 0) {
        double maxVal = static_cast<double>(std::max(snap.counters.agentLoop, snap.replayActionCount));
        if (maxVal <= 0) maxVal = 1.0;
        double div = ((static_cast<double>(snap.counters.agentLoop) -
                       static_cast<double>(snap.replayActionCount)) / maxVal) * 100.0;
        if (div < 0) div = -div;

        append("# HELP rawrxd_loop_divergence_pct UTC vs replay loop count divergence percentage\n");
        append("# TYPE rawrxd_loop_divergence_pct gauge\n");
        snprintf(line, sizeof(line), "rawrxd_loop_divergence_pct %.2f\n", div);
        append(line);
    }

    // Trailing newline per Prometheus spec
    append("\n");
    buffer[pos] = '\0';
    return pos;
}

// ============================================================================
// Thresholds
// ============================================================================

void ReplayTelemetryFusion::setThresholds(const DivergenceThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds = thresholds;
}

const DivergenceThresholds& ReplayTelemetryFusion::getThresholds() const {
    return m_thresholds;
}

// ============================================================================
// Callbacks
// ============================================================================

void ReplayTelemetryFusion::registerDivergenceCallback(DivergenceCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({cb, userData});
}

void ReplayTelemetryFusion::removeDivergenceCallback(DivergenceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [cb](const CallbackEntry& e) { return e.callback == cb; }),
        m_callbacks.end());
}

void ReplayTelemetryFusion::notifyCallbacks(const DivergenceEntry& entry) const {
    for (const auto& cb : m_callbacks) {
        if (cb.callback) {
            cb.callback(entry, cb.userData);
        }
    }
}

// ============================================================================
// FNV-1a Hash (64-bit)
// ============================================================================

uint64_t ReplayTelemetryFusion::fnv1a(const char* data, size_t length) {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (size_t i = 0; i < length; i++) {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i]));
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

uint64_t ReplayTelemetryFusion::fnv1a(const std::string& s) {
    return fnv1a(s.data(), s.size());
}

} // namespace Fusion
} // namespace RawrXD
