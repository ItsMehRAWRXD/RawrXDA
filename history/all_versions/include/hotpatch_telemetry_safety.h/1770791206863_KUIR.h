// ============================================================================
// hotpatch_telemetry_safety.h — Live Hotpatch Safety via UTC Counters
// ============================================================================
// Wraps Layer-5 binary patching (LiveBinaryPatcher) with telemetry-based
// safety checks:
//
//   1. Pre-patch counter snapshot  (all UTC counters sampled atomically)
//   2. Patch application           (trampoline install / code swap)
//   3. Post-patch counter snapshot (immediate re-sample)
//   4. Delta analysis              (fail + rollback if abnormal)
//
// This ensures that runtime mutations cannot silently corrupt the system.
// Any counter spike (error, inference drop, etc.) triggers automatic
// rollback via LiveBinaryPatcher::revert_last_swap().
//
// Architecture:
//   ┌─────────────────────────────────────────────────────────────┐
//   │           HotpatchTelemetrySafety (guard wrapper)           │
//   │                                                             │
//   │  ┌──────────────┐   ┌─────────────────┐  ┌──────────────┐  │
//   │  │ UTC Counters  │   │ LiveBinaryPatcher│  │ ReplayFusion │  │
//   │  │ (MASM atomics)│   │ (Layer 5)        │  │ (optional)   │  │
//   │  └──────┬───────┘   └────────┬─────────┘  └──────┬───────┘  │
//   │         │                    │                    │          │
//   │  captureCounters()    apply_patch_unit()   captureFusion()  │
//   │         │                    │                    │          │
//   │         ▼                    ▼                    ▼          │
//   │  ┌───────────────────────────────────────────────────────┐   │
//   │  │            SafePatchResult                            │   │
//   │  │  pre/post snapshots + delta analysis + rollback flag  │   │
//   │  └───────────────────────────────────────────────────────┘   │
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
#include <mutex>
#include <atomic>

// Forward declarations
struct LivePatchUnit;
struct PatchResult;
class LiveBinaryPatcher;

// CounterSnapshot / FusionSnapshot are defined at global scope in
// replay_telemetry_fusion.h.  Import them into the RawrXD::Fusion namespace
// so the rest of this header can reference Fusion::CounterSnapshot, etc.
#include "replay_telemetry_fusion.h"

namespace RawrXD {
namespace Fusion {
    using ::CounterSnapshot;
    using ::FusionSnapshot;
}
}

namespace RawrXD {
namespace Safety {

// ============================================================================
// Counter Delta — Computed difference between pre/post patch snapshots
// ============================================================================
struct CounterDelta {
    int64_t  inference;      // Change in g_Counter_Inference
    int64_t  scsiFails;      // Change in g_Counter_ScsiFails
    int64_t  agentLoop;      // Change in g_Counter_AgentLoop
    int64_t  bytePatches;    // Change in g_Counter_BytePatches
    int64_t  memPatches;     // Change in g_Counter_MemPatches
    int64_t  serverPatches;  // Change in g_Counter_ServerPatches
    int64_t  flushOps;       // Change in g_Counter_FlushOps
    int64_t  errors;         // Change in g_Counter_Errors
    double   elapsedMs;      // Time between snapshots
};

// ============================================================================
// Safety Verdict — Result of delta analysis
// ============================================================================
enum class SafetyVerdict : uint8_t {
    Safe       = 0,   // All deltas within thresholds
    Warning    = 1,   // Some deltas near thresholds
    Unsafe     = 2,   // At least one delta exceeds threshold → rollback
    RolledBack = 3,   // Patch was applied and then reverted
};

const char* safetyVerdictToString(SafetyVerdict v);

// ============================================================================
// Safety Thresholds — Configurable per-counter limits for patch safety
// ============================================================================
struct SafetyThresholds {
    int64_t maxInferenceDrop  = -100;  // Max drop in inference counter (negative = drop)
    int64_t maxErrorSpike     = 5;     // Max new errors allowed during patch
    int64_t maxScsiFails      = 3;     // Max new SCSI failures
    double  maxPatchDurationMs = 5000.0; // Max time for patch to complete
    bool    allowNegativeInference = false; // If false, any inference drop = unsafe
    bool    rollbackOnUnsafe  = true;  // Auto-revert if verdict is Unsafe
};

// ============================================================================
// Safe Patch Result — Full audit record of a patch attempt
// ============================================================================
struct SafePatchResult {
    bool            success;          // Overall success (patch applied AND safe)
    const char*     detail;
    int             errorCode;

    SafetyVerdict   verdict;
    CounterDelta    delta;

    // Snapshots for audit
    Fusion::CounterSnapshot prePatch;
    Fusion::CounterSnapshot postPatch;

    uint32_t        slotId;           // Which function slot was patched
    bool            wasRolledBack;    // True if patch was reverted due to safety

    static SafePatchResult ok(const char* msg = "Safe patch applied") {
        SafePatchResult r{};
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        r.verdict = SafetyVerdict::Safe;
        r.wasRolledBack = false;
        return r;
    }
    static SafePatchResult error(const char* msg, int code = -1) {
        SafePatchResult r{};
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        r.verdict = SafetyVerdict::Unsafe;
        r.wasRolledBack = false;
        return r;
    }
};

// ============================================================================
// Patch Safety Event — Logged to replay journal
// ============================================================================
struct PatchSafetyEvent {
    uint64_t        timestampMs;
    uint32_t        slotId;
    SafetyVerdict   verdict;
    CounterDelta    delta;
    bool            rolledBack;
    std::string     patchName;
    std::string     detail;
};

// ============================================================================
// Safety Callback — Invoked on each patch safety check
// ============================================================================
using PatchSafetyCallback = void(*)(const PatchSafetyEvent& event, void* userData);

// ============================================================================
// HotpatchTelemetrySafety — Guard wrapper around LiveBinaryPatcher
// ============================================================================
class HotpatchTelemetrySafety {
public:
    static HotpatchTelemetrySafety& instance();

    // ---- Lifecycle ----
    void initialize();
    void shutdown();
    bool isInitialized() const;

    // ---- Safe Patching ----

    // Apply a patch unit with full safety: snapshot → patch → snapshot → validate.
    // If delta is abnormal, automatically rolls back the patch.
    SafePatchResult safePatch(const LivePatchUnit& unit);

    // Apply a raw code swap with safety checks.
    SafePatchResult safeSwap(uint32_t slotId,
                              const uint8_t* newCode, size_t codeSize,
                              const char* patchName = nullptr);

    // Apply a batch of patches transactionally with safety.
    // If any single patch's post-delta is abnormal, the entire batch is rolled back.
    SafePatchResult safeBatch(const LivePatchUnit* units, size_t count);

    // ---- Manual Safety Check ----

    // Take a pre-patch snapshot (for custom patching workflows).
    Fusion::CounterSnapshot snapshotBefore();

    // Take a post-patch snapshot and analyze delta.
    SafePatchResult analyzeAfter(const Fusion::CounterSnapshot& prePatch,
                                  uint32_t slotId,
                                  const char* patchName = nullptr);

    // ---- Thresholds ----
    void setThresholds(const SafetyThresholds& thresholds);
    const SafetyThresholds& getThresholds() const;

    // ---- History ----
    const std::vector<PatchSafetyEvent>& getHistory() const;
    size_t getSafeCount() const;
    size_t getUnsafeCount() const;
    size_t getRollbackCount() const;

    // ---- Callbacks ----
    void registerCallback(PatchSafetyCallback cb, void* userData);
    void removeCallback(PatchSafetyCallback cb);

private:
    HotpatchTelemetrySafety();
    ~HotpatchTelemetrySafety();
    HotpatchTelemetrySafety(const HotpatchTelemetrySafety&) = delete;
    HotpatchTelemetrySafety& operator=(const HotpatchTelemetrySafety&) = delete;

    // Compute delta between two counter snapshots.
    CounterDelta computeDelta(const Fusion::CounterSnapshot& before,
                               const Fusion::CounterSnapshot& after) const;

    // Evaluate delta against thresholds.
    SafetyVerdict evaluateDelta(const CounterDelta& delta) const;

    // Notify all registered callbacks.
    void notifyCallbacks(const PatchSafetyEvent& event) const;

    // Record event to replay journal (if available).
    void recordToJournal(const PatchSafetyEvent& event);

    mutable std::mutex          m_mutex;
    std::atomic<bool>           m_initialized{false};
    SafetyThresholds            m_thresholds;
    std::vector<PatchSafetyEvent> m_history;
    std::atomic<uint64_t>       m_safeCount{0};
    std::atomic<uint64_t>       m_unsafeCount{0};
    std::atomic<uint64_t>       m_rollbackCount{0};

    struct CallbackEntry {
        PatchSafetyCallback callback;
        void*               userData;
    };
    std::vector<CallbackEntry>  m_callbacks;
};

} // namespace Safety
} // namespace RawrXD
