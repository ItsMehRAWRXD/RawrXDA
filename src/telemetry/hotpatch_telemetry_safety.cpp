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
#include <cstdint>
#include <mutex>
#include <vector>
#include "replay_telemetry_fusion.h"
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

// ============================================================================
// safetyVerdictToString (declared in header)
// ============================================================================
const char* safetyVerdictToString(SafetyVerdict v) {
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

void HotpatchTelemetrySafety::initialize() {
    if (m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds = SafetyThresholds{};
    m_safeCount.store(0, std::memory_order_relaxed);
    m_unsafeCount.store(0, std::memory_order_relaxed);
    m_rollbackCount.store(0, std::memory_order_relaxed);
    m_history.clear();
    m_callbacks.clear();
    m_initialized.store(true, std::memory_order_release);
}

void HotpatchTelemetrySafety::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.clear();
    m_initialized.store(false, std::memory_order_release);
}

bool HotpatchTelemetrySafety::isInitialized() const {
    return m_initialized.load(std::memory_order_acquire);
}

// ============================================================================
// Counter Snapshots — using Fusion::CounterSnapshot from fusion header
// ============================================================================

Fusion::CounterSnapshot HotpatchTelemetrySafety::snapshotBefore() {
    Fusion::CounterSnapshot snap{};
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
    const Fusion::CounterSnapshot& before,
    const Fusion::CounterSnapshot& after) const
{
    CounterDelta d{};
    d.inference     = static_cast<int64_t>(after.inference)     - static_cast<int64_t>(before.inference);
    d.scsiFails     = static_cast<int64_t>(after.scsiFails)     - static_cast<int64_t>(before.scsiFails);
    d.agentLoop     = static_cast<int64_t>(after.agentLoop)     - static_cast<int64_t>(before.agentLoop);
    d.bytePatches   = static_cast<int64_t>(after.bytePatches)   - static_cast<int64_t>(before.bytePatches);
    d.memPatches    = static_cast<int64_t>(after.memPatches)    - static_cast<int64_t>(before.memPatches);
    d.serverPatches = static_cast<int64_t>(after.serverPatches) - static_cast<int64_t>(before.serverPatches);
    d.flushOps      = static_cast<int64_t>(after.flushOps)      - static_cast<int64_t>(before.flushOps);
    d.errors        = static_cast<int64_t>(after.errors)        - static_cast<int64_t>(before.errors);
    d.elapsedMs     = static_cast<double>(after.timestampMs)    - static_cast<double>(before.timestampMs);
    return d;
}

// ============================================================================
// Safety Evaluation
// ============================================================================

SafetyVerdict HotpatchTelemetrySafety::evaluateDelta(const CounterDelta& delta) const {
    SafetyVerdict verdict = SafetyVerdict::Safe;
    const auto& t = m_thresholds;

    // Error spike check (most critical)
    if (delta.errors > t.maxErrorSpike) {
        return SafetyVerdict::Unsafe;
    }

    // SCSI failure check
    if (delta.scsiFails > t.maxScsiFails) {
        return SafetyVerdict::Unsafe;
    }

    // Patch duration check
    if (delta.elapsedMs > t.maxPatchDurationMs) {
        verdict = SafetyVerdict::Warning;
    }

    // Inference drop check
    if (!t.allowNegativeInference && delta.inference < t.maxInferenceDrop) {
        return SafetyVerdict::Unsafe;
    }

    return verdict;
}

// ============================================================================
// Safe Patch Operations
// ============================================================================

SafePatchResult HotpatchTelemetrySafety::safePatch(const LivePatchUnit& unit) {
    // === Phase 1: Pre-patch snapshot ===
    Fusion::CounterSnapshot pre = snapshotBefore();

    // === Phase 2: Apply patch via LiveBinaryPatcher ===
    PatchResult patchResult = LiveBinaryPatcher::instance().apply_patch_unit(unit);
    if (!patchResult.success) {
        SafePatchResult r = SafePatchResult::error(patchResult.detail, patchResult.errorCode);
        r.prePatch = pre;
        r.slotId = static_cast<uint32_t>(unit.target_slot);
        recordToJournal({nowMs(), r.slotId, SafetyVerdict::Safe, {}, false, "safePatch", patchResult.detail});
        return r;
    }

    // === Phase 3: Post-patch snapshot ===
    Fusion::CounterSnapshot post = snapshotBefore(); // same sampling logic

    // === Phase 4: Compute and evaluate delta ===
    CounterDelta delta = computeDelta(pre, post);
    SafetyVerdict verdict = evaluateDelta(delta);

    SafePatchResult result = SafePatchResult::ok("Patch applied");
    result.prePatch = pre;
    result.postPatch = post;
    result.delta = delta;
    result.verdict = verdict;
    result.slotId = static_cast<uint32_t>(unit.target_slot);
    result.wasRolledBack = false;

    // === Phase 5: Auto-rollback if Unsafe ===
    if (verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        PatchResult rollResult = LiveBinaryPatcher::instance().revert_last_swap(result.slotId);
        if (rollResult.success) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            result.success = false;
            result.detail = "Patch rolled back due to safety violation";
            m_rollbackCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record event ===
    PatchSafetyEvent event{};
    event.timestampMs = nowMs();
    event.slotId = result.slotId;
    event.verdict = result.verdict;
    event.delta = delta;
    event.rolledBack = result.wasRolledBack;
    event.patchName = "safePatch";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(event);
    }

    notifyCallbacks(event);
    recordToJournal(event);

    // Update statistics
    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
    }

    return result;
}

SafePatchResult HotpatchTelemetrySafety::safeSwap(uint32_t slotId,
                                                    const uint8_t* newCode, size_t codeSize,
                                                    const char* patchName)
{
    // === Phase 1: Pre-swap snapshot ===
    Fusion::CounterSnapshot pre = snapshotBefore();

    // === Phase 2: Apply swap via LiveBinaryPatcher ===
    PatchResult swapResult = LiveBinaryPatcher::instance().swap_implementation(
        slotId, newCode, codeSize, nullptr, 0);
    if (!swapResult.success) {
        SafePatchResult r = SafePatchResult::error(swapResult.detail, swapResult.errorCode);
        r.prePatch = pre;
        r.slotId = slotId;
        return r;
    }

    // === Phase 3: Post-swap snapshot ===
    Fusion::CounterSnapshot post = snapshotBefore();

    // === Phase 4: Compute and evaluate delta ===
    CounterDelta delta = computeDelta(pre, post);
    SafetyVerdict verdict = evaluateDelta(delta);

    SafePatchResult result = SafePatchResult::ok("Swap applied");
    result.prePatch = pre;
    result.postPatch = post;
    result.delta = delta;
    result.verdict = verdict;
    result.slotId = slotId;
    result.wasRolledBack = false;

    // === Phase 5: Auto-rollback ===
    if (verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        PatchResult rollResult = LiveBinaryPatcher::instance().revert_last_swap(slotId);
        if (rollResult.success) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            result.success = false;
            result.detail = "Swap rolled back due to safety violation";
            m_rollbackCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record ===
    PatchSafetyEvent event{};
    event.timestampMs = nowMs();
    event.slotId = slotId;
    event.verdict = result.verdict;
    event.delta = delta;
    event.rolledBack = result.wasRolledBack;
    event.patchName = patchName ? patchName : "safeSwap";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(event);
    }

    notifyCallbacks(event);
    recordToJournal(event);

    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
    }

    return result;
}

SafePatchResult HotpatchTelemetrySafety::safeBatch(const LivePatchUnit* units, size_t count) {
    if (!units || count == 0) {
        return SafePatchResult::error("Null or empty batch", 1);
    }

    // === Phase 1: Pre-batch snapshot ===
    Fusion::CounterSnapshot pre = snapshotBefore();

    // === Phase 2: Apply batch ===
    PatchResult batchResult = LiveBinaryPatcher::instance().apply_batch(units, count);
    if (!batchResult.success) {
        SafePatchResult r = SafePatchResult::error(batchResult.detail, batchResult.errorCode);
        r.prePatch = pre;
        return r;
    }

    // === Phase 3: Post-batch snapshot ===
    Fusion::CounterSnapshot post = snapshotBefore();

    // === Phase 4: Evaluate ===
    CounterDelta delta = computeDelta(pre, post);
    SafetyVerdict verdict = evaluateDelta(delta);

    SafePatchResult result = SafePatchResult::ok("Batch applied");
    result.prePatch = pre;
    result.postPatch = post;
    result.delta = delta;
    result.verdict = verdict;
    result.slotId = 0; // Batch — multiple slots
    result.wasRolledBack = false;

    // === Phase 5: Batch rollback if unsafe ===
    if (verdict == SafetyVerdict::Unsafe && m_thresholds.rollbackOnUnsafe) {
        bool allReverted = true;
        for (size_t i = 0; i < count; i++) {
            uint32_t sid = static_cast<uint32_t>(units[i].target_slot);
            PatchResult rr = LiveBinaryPatcher::instance().revert_last_swap(sid);
            if (!rr.success) allReverted = false;
        }
        if (allReverted) {
            result.wasRolledBack = true;
            result.verdict = SafetyVerdict::RolledBack;
            result.success = false;
            result.detail = "Batch rolled back due to safety violation";
            m_rollbackCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === Phase 6: Record ===
    PatchSafetyEvent event{};
    event.timestampMs = nowMs();
    event.slotId = 0;
    event.verdict = result.verdict;
    event.delta = delta;
    event.rolledBack = result.wasRolledBack;
    event.patchName = "safeBatch";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(event);
    }

    notifyCallbacks(event);
    recordToJournal(event);

    switch (result.verdict) {
        case SafetyVerdict::Safe:       m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Warning:    m_safeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::Unsafe:     m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
        case SafetyVerdict::RolledBack: m_unsafeCount.fetch_add(1, std::memory_order_relaxed); break;
    }

    return result;
}

// ============================================================================
// Manual Safety Check (for custom patching workflows)
// ============================================================================

SafePatchResult HotpatchTelemetrySafety::analyzeAfter(
    const Fusion::CounterSnapshot& prePatch,
    uint32_t slotId,
    const char* patchName)
{
    Fusion::CounterSnapshot post = snapshotBefore();
    CounterDelta delta = computeDelta(prePatch, post);
    SafetyVerdict verdict = evaluateDelta(delta);

    SafePatchResult result = SafePatchResult::ok("Analysis complete");
    result.prePatch = prePatch;
    result.postPatch = post;
    result.delta = delta;
    result.verdict = verdict;
    result.slotId = slotId;
    result.wasRolledBack = false;

    if (verdict == SafetyVerdict::Unsafe) {
        result.success = false;
        result.detail = "Post-patch analysis detected unsafe delta";
    }

    PatchSafetyEvent event{};
    event.timestampMs = nowMs();
    event.slotId = slotId;
    event.verdict = verdict;
    event.delta = delta;
    event.rolledBack = false;
    event.patchName = patchName ? patchName : "analyzeAfter";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(event);
    }

    notifyCallbacks(event);
    recordToJournal(event);

    return result;
}

// ============================================================================
// History / Statistics
// ============================================================================

const std::vector<PatchSafetyEvent>& HotpatchTelemetrySafety::getHistory() const {
    return m_history;
}

size_t HotpatchTelemetrySafety::getSafeCount() const {
    return m_safeCount.load(std::memory_order_relaxed);
}

size_t HotpatchTelemetrySafety::getUnsafeCount() const {
    return m_unsafeCount.load(std::memory_order_relaxed);
}

size_t HotpatchTelemetrySafety::getRollbackCount() const {
    return m_rollbackCount.load(std::memory_order_relaxed);
}

// ============================================================================
// Replay Journal Recording
// ============================================================================

void HotpatchTelemetrySafety::recordToJournal(const PatchSafetyEvent& event) {
    ReplayJournal& journal = ReplayJournal::instance();

    ActionRecord record{};
    record.type = ReplayActionType::AgentToolCall;

    char inputBuf[512];
    snprintf(inputBuf, sizeof(inputBuf),
        "hotpatch_safety slot=%u verdict=%s rolled_back=%s name=%s",
        event.slotId,
        safetyVerdictToString(event.verdict),
        event.rolledBack ? "true" : "false",
        event.patchName.c_str());
    record.input = inputBuf;

    char outputBuf[1024];
    snprintf(outputBuf, sizeof(outputBuf),
        "delta{inference=%lld errors=%lld scsi=%lld patches=%lld elapsed=%.1fms}",
        static_cast<long long>(event.delta.inference),
        static_cast<long long>(event.delta.errors),
        static_cast<long long>(event.delta.scsiFails),
        static_cast<long long>(event.delta.bytePatches +
                                event.delta.memPatches +
                                event.delta.serverPatches),
        event.delta.elapsedMs);
    record.output = outputBuf;

    record.confidence = (event.verdict == SafetyVerdict::Safe) ? 1.0f : 0.0f;
    record.durationMs = event.delta.elapsedMs;

    journal.record(record);
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
