// ============================================================================
// hotpatch_telemetry_safety.cpp — Hotpatch Telemetry Safety Layer
// ============================================================================
// Wraps LiveBinaryPatcher with telemetry-based safety checks:
//   1. Capture counter snapshot BEFORE patch
//   2. Apply patch via LiveBinaryPatcher
//   3. Capture counter snapshot AFTER patch
//   4. Compute delta and evaluate (Safe / Warning / Unsafe)
//   5. Auto-rollback on Unsafe if thresholds.rollbackOnUnsafe is set
//   6. Record event to ReplayJournal for deterministic replay audit
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "hotpatch_telemetry_safety.h"
#include "rawrxd_telemetry_exports.h"
#include "live_binary_patcher.hpp"
#include "deterministic_replay.h"

#include <cstring>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Safety {

// ============================================================================
// Timing Helper
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

static const char* verdictToString(SafetyVerdict v) {
    switch (v) {
        case SafetyVerdict::Safe:       return "Safe";
        case SafetyVerdict::Warning:    return "Warning";
        case SafetyVerdict::Unsafe:     return "Unsafe";
        case SafetyVerdict::RolledBack: return "RolledBack";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Singleton
// ============================================================================

HotpatchTelemetrySafety& HotpatchTelemetrySafety::instance() {
    static HotpatchTelemetrySafety inst;
    return inst;
}

HotpatchTelemetrySafety::HotpatchTelemetrySafety() = default;
HotpatchTelemetrySafety::~HotpatchTelemetrySafety() { shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult HotpatchTelemetrySafety::initialize(const SafetyThresholds& thresholds) {
    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::ok("Already initialized");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds = thresholds;
    m_historyCount = 0;
    m_totalSafe = 0;
    m_totalWarning = 0;
    m_totalUnsafe = 0;
    m_totalRolledBack = 0;
    m_history.clear();
    m_callbacks.clear();
    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("HotpatchTelemetrySafety initialized");
}

PatchResult HotpatchTelemetrySafety::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::ok("Not initialized");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.clear();
    m_initialized.store(false, std::memory_order_release);
    return PatchResult::ok("HotpatchTelemetrySafety shut down");
}

bool HotpatchTelemetrySafety::isInitialized() const {
    return m_initialized.load(std::memory_order_acquire);
}

// ============================================================================
// Counter Snapshots
// ============================================================================

CounterSnapshot HotpatchTelemetrySafety::snapshotBefore() const {
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

CounterSnapshot HotpatchTelemetrySafety::snapshotAfter() const {
    // Identical reads — separated for clarity and potential future
    // configurable delay (e.g., post-patch stabilization window)
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
// Delta Computation
// ============================================================================

CounterDelta HotpatchTelemetrySafety::computeDelta(
    const CounterSnapshot& pre,
    const CounterSnapshot& post) const
{
    CounterDelta d{};
    d.inferenceDelta     = static_cast<int64_t>(post.inference)     - static_cast<int64_t>(pre.inference);
    d.scsiFailsDelta     = static_cast<int64_t>(post.scsiFails)     - static_cast<int64_t>(pre.scsiFails);
    d.agentLoopDelta     = static_cast<int64_t>(post.agentLoop)     - static_cast<int64_t>(pre.agentLoop);
    d.bytePatchesDelta   = static_cast<int64_t>(post.bytePatches)   - static_cast<int64_t>(pre.bytePatches);
    d.memPatchesDelta    = static_cast<int64_t>(post.memPatches)    - static_cast<int64_t>(pre.memPatches);
    d.serverPatchesDelta = static_cast<int64_t>(post.serverPatches) - static_cast<int64_t>(pre.serverPatches);
    d.flushOpsDelta      = static_cast<int64_t>(post.flushOps)      - static_cast<int64_t>(pre.flushOps);
    d.errorsDelta        = static_cast<int64_t>(post.errors)        - static_cast<int64_t>(pre.errors);
    d.elapsedMs          = static_cast<int64_t>(post.timestampMs)   - static_cast<int64_t>(pre.timestampMs);
    return d;
}

// ============================================================================
// Safety Evaluation
// ============================================================================

SafetyVerdict HotpatchTelemetrySafety::evaluateDelta(const CounterDelta& delta) const {
    SafetyVerdict verdict = SafetyVerdict::Safe;
    const auto& t = m_thresholds;

    // Error spike check (most critical)
    if (delta.errorsDelta > static_cast<int64_t>(t.maxErrorSpike)) {
        return SafetyVerdict::Unsafe;
    }

    // SCSI failure check
    if (delta.scsiFailsDelta > static_cast<int64_t>(t.maxScsiFails)) {
        return SafetyVerdict::Unsafe;
    }

    // Inference drop check (negative means counter went down, which shouldn't normally happen)
    if (delta.inferenceDelta < t.maxInferenceDrop) {
        verdict = SafetyVerdict::Warning;
    }

    // Abnormal patch delta detection (e.g., unexpected counter moves during patch)
    int64_t totalPatchDelta = delta.bytePatchesDelta + delta.memPatchesDelta + delta.serverPatchesDelta;
    if (totalPatchDelta > static_cast<int64_t>(t.maxPatchDelta)) {
        verdict = std::max(verdict, SafetyVerdict::Warning);
    }

    return verdict;
}

// ============================================================================
// Safe Patch Operations
// ============================================================================

SafePatchResult HotpatchTelemetrySafety::safePatch(const LivePatchUnit& unit) {
    SafePatchResult result{};
    result.wasRolledBack = false;
    result.slotId = static_cast<uint32_t>(unit.target_slot);

    // === Phase 1: Pre-patch snapshot ===
    result.pre = snapshotBefore();

    // === Phase 2: Apply patch via LiveBinaryPatcher ===
    PatchResult patchResult = LiveBinaryPatcher::instance().apply_patch_unit(unit);
    if (!patchResult.success) {
        result.verdict = SafetyVerdict::Safe; // Patch didn't apply, no safety concern
        result.patchResult = patchResult;
        recordEvent(result, "safePatch_failed");
        return result;
    }

    // === Phase 3: Post-patch snapshot ===
    result.post = snapshotAfter();

    // === Phase 4: Compute and evaluate delta ===
    result.delta = computeDelta(result.pre, result.post);
    result.verdict = evaluateDelta(result.delta);
    result.patchResult = patchResult;

    // === Phase 5: Auto-rollback if Unsafe and configured ===
    if (result.verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        PatchResult rollResult = LiveBinaryPatcher::instance().revert_last_swap(result.slotId);
        if (rollResult.success) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            m_totalRolledBack.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record to history and replay journal ===
    recordEvent(result, "safePatch");
    recordToJournal(result);

    // Update statistics
    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_totalSafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_totalWarning.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_totalUnsafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: /* Already counted above */ break;
    }

    return result;
}

SafePatchResult HotpatchTelemetrySafety::safeSwap(uint32_t slotId,
                                                    const uint8_t* newCode, size_t codeSize,
                                                    const RVARelocation* relocs, size_t relocCount)
{
    SafePatchResult result{};
    result.wasRolledBack = false;
    result.slotId = slotId;

    // === Phase 1: Pre-swap snapshot ===
    result.pre = snapshotBefore();

    // === Phase 2: Apply swap via LiveBinaryPatcher ===
    PatchResult swapResult = LiveBinaryPatcher::instance().swap_implementation(
        slotId, newCode, codeSize, relocs, relocCount);
    if (!swapResult.success) {
        result.verdict = SafetyVerdict::Safe;
        result.patchResult = swapResult;
        recordEvent(result, "safeSwap_failed");
        return result;
    }

    // === Phase 3: Post-swap snapshot ===
    result.post = snapshotAfter();

    // === Phase 4: Compute and evaluate delta ===
    result.delta = computeDelta(result.pre, result.post);
    result.verdict = evaluateDelta(result.delta);
    result.patchResult = swapResult;

    // === Phase 5: Auto-rollback ===
    if (result.verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        PatchResult rollResult = LiveBinaryPatcher::instance().revert_last_swap(slotId);
        if (rollResult.success) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            m_totalRolledBack.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record ===
    recordEvent(result, "safeSwap");
    recordToJournal(result);

    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_totalSafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_totalWarning.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_totalUnsafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: break;
    }

    return result;
}

SafePatchResult HotpatchTelemetrySafety::safeBatch(const LivePatchUnit* units, size_t count) {
    SafePatchResult result{};
    result.wasRolledBack = false;
    result.slotId = 0; // Batch applies to multiple slots

    if (!units || count == 0) {
        result.verdict = SafetyVerdict::Safe;
        result.patchResult = PatchResult::error("Null or empty batch", 1);
        return result;
    }

    // === Phase 1: Pre-batch snapshot ===
    result.pre = snapshotBefore();

    // === Phase 2: Apply batch ===
    PatchResult batchResult = LiveBinaryPatcher::instance().apply_batch(units, count);
    if (!batchResult.success) {
        result.verdict = SafetyVerdict::Safe;
        result.patchResult = batchResult;
        recordEvent(result, "safeBatch_failed");
        return result;
    }

    // === Phase 3: Post-batch snapshot ===
    result.post = snapshotAfter();

    // === Phase 4: Evaluate ===
    result.delta = computeDelta(result.pre, result.post);
    result.verdict = evaluateDelta(result.delta);
    result.patchResult = batchResult;

    // === Phase 5: Batch rollback is more complex — apply_batch already has
    //              transactional rollback built in, so we'd need to revert all.
    //              Record warning but don't auto-rollback batches unless critical. ===
    if (result.verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        // For batch operations, revert each slot individually
        bool allReverted = true;
        for (size_t i = 0; i < count; i++) {
            uint32_t sid = static_cast<uint32_t>(units[i].target_slot);
            PatchResult rr = LiveBinaryPatcher::instance().revert_last_swap(sid);
            if (!rr.success) allReverted = false;
        }
        if (allReverted) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            m_totalRolledBack.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record ===
    recordEvent(result, "safeBatch");
    recordToJournal(result);

    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_totalSafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_totalWarning.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_totalUnsafe.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: break;
    }

    return result;
}

// ============================================================================
// History / Event Recording
// ============================================================================

void HotpatchTelemetrySafety::recordEvent(const SafePatchResult& result, const char* label) {
    PatchSafetyEvent evt{};
    evt.timestampMs = nowMs();
    evt.verdict = result.verdict;
    evt.slotId = result.slotId;
    evt.wasRolledBack = result.wasRolledBack;
    evt.delta = result.delta;
    evt.label = label ? label : "";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Ring-buffer style, keep last MAX_HISTORY entries
        if (m_history.size() >= MAX_HISTORY) {
            m_history.erase(m_history.begin());
        }
        m_history.push_back(std::move(evt));
        m_historyCount.fetch_add(1, std::memory_order_relaxed);
    }

    // Notify callbacks
    notifyCallbacks(m_history.back());
}

void HotpatchTelemetrySafety::recordToJournal(const SafePatchResult& result) {
    // Record into DeterministicReplayEngine's journal for audit trail
    ReplayJournal& journal = ReplayJournal::instance();

    ActionRecord record{};
    record.type = ReplayActionType::AgentToolCall;

    // Build a structured input description
    char inputBuf[512];
    snprintf(inputBuf, sizeof(inputBuf),
        "hotpatch_safety_check slot=%u verdict=%s rolled_back=%s",
        result.slotId,
        verdictToString(result.verdict),
        result.wasRolledBack ? "true" : "false");
    record.input = inputBuf;

    // Build structured output with delta info
    char outputBuf[1024];
    snprintf(outputBuf, sizeof(outputBuf),
        "delta{inference=%lld errors=%lld scsi=%lld patches=%lld elapsed=%lldms} "
        "verdict=%s patch_success=%s",
        static_cast<long long>(result.delta.inferenceDelta),
        static_cast<long long>(result.delta.errorsDelta),
        static_cast<long long>(result.delta.scsiFailsDelta),
        static_cast<long long>(result.delta.bytePatchesDelta +
                                result.delta.memPatchesDelta +
                                result.delta.serverPatchesDelta),
        static_cast<long long>(result.delta.elapsedMs),
        verdictToString(result.verdict),
        result.patchResult.success ? "true" : "false");
    record.output = outputBuf;

    record.confidence = (result.verdict == SafetyVerdict::Safe) ? 1.0f : 0.0f;
    record.durationMs = static_cast<double>(result.delta.elapsedMs);

    journal.recordAction(record);
}

// ============================================================================
// Statistics
// ============================================================================

SafetyStats HotpatchTelemetrySafety::getStats() const {
    SafetyStats stats{};
    stats.totalEvents   = m_historyCount.load(std::memory_order_relaxed);
    stats.totalSafe     = m_totalSafe.load(std::memory_order_relaxed);
    stats.totalWarning  = m_totalWarning.load(std::memory_order_relaxed);
    stats.totalUnsafe   = m_totalUnsafe.load(std::memory_order_relaxed);
    stats.totalRolledBack = m_totalRolledBack.load(std::memory_order_relaxed);
    return stats;
}

std::vector<PatchSafetyEvent> HotpatchTelemetrySafety::getHistory(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (maxEntries == 0 || maxEntries >= m_history.size()) {
        return m_history;
    }
    // Return last N entries
    return std::vector<PatchSafetyEvent>(
        m_history.end() - static_cast<ptrdiff_t>(maxEntries),
        m_history.end());
}

// ============================================================================
// Export — JSON
// ============================================================================

std::string HotpatchTelemetrySafety::exportStatsJSON() const {
    SafetyStats s = getStats();

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total_events\": "    << s.totalEvents << ",\n";
    ss << "  \"total_safe\": "      << s.totalSafe << ",\n";
    ss << "  \"total_warning\": "   << s.totalWarning << ",\n";
    ss << "  \"total_unsafe\": "    << s.totalUnsafe << ",\n";
    ss << "  \"total_rolled_back\": " << s.totalRolledBack << "\n";
    ss << "}";
    return ss.str();
}

std::string HotpatchTelemetrySafety::exportEventJSON(const PatchSafetyEvent& evt) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"timestamp_ms\": " << evt.timestampMs << ",\n";
    ss << "  \"verdict\": \""    << verdictToString(evt.verdict) << "\",\n";
    ss << "  \"slot_id\": "      << evt.slotId << ",\n";
    ss << "  \"was_rolled_back\": " << (evt.wasRolledBack ? "true" : "false") << ",\n";
    ss << "  \"label\": \""      << evt.label << "\",\n";
    ss << "  \"delta\": {\n";
    ss << "    \"inference\": "     << evt.delta.inferenceDelta << ",\n";
    ss << "    \"scsi_fails\": "    << evt.delta.scsiFailsDelta << ",\n";
    ss << "    \"agent_loop\": "    << evt.delta.agentLoopDelta << ",\n";
    ss << "    \"byte_patches\": "  << evt.delta.bytePatchesDelta << ",\n";
    ss << "    \"mem_patches\": "   << evt.delta.memPatchesDelta << ",\n";
    ss << "    \"server_patches\": " << evt.delta.serverPatchesDelta << ",\n";
    ss << "    \"flush_ops\": "     << evt.delta.flushOpsDelta << ",\n";
    ss << "    \"errors\": "        << evt.delta.errorsDelta << ",\n";
    ss << "    \"elapsed_ms\": "    << evt.delta.elapsedMs << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string HotpatchTelemetrySafety::exportHistoryJSON(size_t maxEntries) const {
    auto events = getHistory(maxEntries);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"stats\": " << exportStatsJSON() << ",\n";
    ss << "  \"events\": [\n";
    for (size_t i = 0; i < events.size(); i++) {
        ss << "    " << exportEventJSON(events[i]);
        if (i + 1 < events.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Export — Prometheus
// ============================================================================

size_t HotpatchTelemetrySafety::exportSafetyPrometheus(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize < 2048) return 0;

    SafetyStats s = getStats();
    size_t pos = 0;

    auto append = [&](const char* str) {
        size_t len = strlen(str);
        if (pos + len < bufferSize) {
            memcpy(buffer + pos, str, len);
            pos += len;
        }
    };

    char line[256];

    append("# HELP rawrxd_hotpatch_safety_total Total hotpatch safety events\n");
    append("# TYPE rawrxd_hotpatch_safety_total counter\n");
    snprintf(line, sizeof(line), "rawrxd_hotpatch_safety_total %llu\n",
             static_cast<unsigned long long>(s.totalEvents));
    append(line);

    append("# HELP rawrxd_hotpatch_verdict Hotpatch verdicts by type\n");
    append("# TYPE rawrxd_hotpatch_verdict gauge\n");
    snprintf(line, sizeof(line), "rawrxd_hotpatch_verdict{verdict=\"safe\"} %llu\n",
             static_cast<unsigned long long>(s.totalSafe));
    append(line);
    snprintf(line, sizeof(line), "rawrxd_hotpatch_verdict{verdict=\"warning\"} %llu\n",
             static_cast<unsigned long long>(s.totalWarning));
    append(line);
    snprintf(line, sizeof(line), "rawrxd_hotpatch_verdict{verdict=\"unsafe\"} %llu\n",
             static_cast<unsigned long long>(s.totalUnsafe));
    append(line);
    snprintf(line, sizeof(line), "rawrxd_hotpatch_verdict{verdict=\"rolled_back\"} %llu\n",
             static_cast<unsigned long long>(s.totalRolledBack));
    append(line);

    append("\n");
    buffer[pos] = '\0';
    return pos;
}

// ============================================================================
// Thresholds
// ============================================================================

void HotpatchTelemetrySafety::setThresholds(const SafetyThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds = thresholds;
}

const SafetyThresholds& HotpatchTelemetrySafety::getThresholds() const {
    return m_thresholds;
}

// ============================================================================
// Callbacks
// ============================================================================

void HotpatchTelemetrySafety::registerCallback(PatchSafetyCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({cb, userData});
}

void HotpatchTelemetrySafety::removeCallback(PatchSafetyCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [cb](const CallbackEntry& e) { return e.callback == cb; }),
        m_callbacks.end());
}

void HotpatchTelemetrySafety::notifyCallbacks(const PatchSafetyEvent& event) const {
    for (const auto& entry : m_callbacks) {
        if (entry.callback) {
            entry.callback(event, entry.userData);
        }
    }
}

} // namespace Safety
} // namespace RawrXD
