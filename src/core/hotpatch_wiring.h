// ============================================================================
// hotpatch_wiring.h — Phase 12: Hotpatcher ↔ Governor Integration
// ============================================================================
//
// Three-layer coordination for safe live code/model updates:
//
//   1. AtomicFunctionSwap — lock-free function pointer exchange (x64 only)
//   2. HotpatchCoordinator — two-phase commit (quiescence → patch → resume)
//   3. SafetyGate — validates instruction invariants during active patch
//
// When a hotpatch is requested:
//   1. Governor pauses new submissions (fast path)
//   2. Governor waits for all active tasks to reach safe points (checkpoint)
//   3. Hotpatcher applies function/model updates (atomic ops only)
//   4. Governor notifies agents to re-validate instructions
//   5. Governor resumes task queue
//
// Guarantees:
//   - No agent executes stale code during patch window
//   - Partial outputs captured if patch races (rare; advisory only)
//   - No memory corruption from concurrent swaps
//   - Patch either fully committed or fully rolled back
//   - Watchdog kills tasks that don't checkpoint within timeout
//
// Build: Part of RawrXD-Win32IDE target
// NO exceptions. Fail-closed safety model.
// ============================================================================

#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
#include <string>

// Forward declarations
class ExecutionGovernor;

// ============================================================================
// LEVEL 1: Atomic Function Pointer Swap (x64 only, lock-free)
// ============================================================================
//
// Safe non-blocking function pointer exchange for live code patching.
// Uses atomic pointer operations plus a version counter so callers can detect
// patch generation changes across a swap window.
//
// Pattern:
//   AtomicFunctionSwap<int(int)> slot;
//   slot.initialize(&old_impl);
//   bool ok = slot.swapIf(&old_impl, &new_impl);  // Atomic, no lock
//

template <typename FuncSignature>
class AtomicFunctionSwap {
public:
    using FuncPtr = FuncSignature*;

    AtomicFunctionSwap() : m_ptr(nullptr), m_version(0) {}

    // Initialize with a function pointer
    void initialize(FuncPtr initial) {
        m_ptr = initial;
        m_version = 0;
    }

    // Get current function pointer (always safe, no lock)
    FuncPtr get() const {
        return m_ptr.load(std::memory_order_acquire);
    }

    // Call the current function (always safe)
    // Usage: slot.call()(args...)
    auto call() const {
        return get();
    }

    // Compare-and-swap: update only if current == expected
    // Returns true if swap succeeded
    bool swapIf(FuncPtr expected, FuncPtr newFunc) {
        return m_ptr.compare_exchange_strong(
            expected, newFunc,
            std::memory_order_release,
            std::memory_order_acquire);
    }

    // Force swap (no compare, unconditional)
    FuncPtr swapForce(FuncPtr newFunc) {
        return m_ptr.exchange(newFunc, std::memory_order_release);
    }

    // Increment version counter (for ABA detection in higher layers)
    uint64_t incrementVersion() {
        // This is advisory; true ABA prevention requires paired pointers (128-bit).
        // For Phase 12, this is sufficient as a safety gate.
        return m_version.fetch_add(1, std::memory_order_release);
    }

    // Get current version
    uint64_t getVersion() const {
        return m_version.load(std::memory_order_acquire);
    }

private:
    std::atomic<FuncPtr> m_ptr;
    std::atomic<uint64_t> m_version;
};

// ============================================================================
// LEVEL 2: Hotpatch Lifecycle State & Operations
// ============================================================================

enum class HotpatchPhase : uint8_t {
    Idle              = 0,    // No patch in progress
    RequestPending    = 1,    // Patch requested, waiting for Governor clearance
    QuiescenceWait    = 2,    // Governor pausing new submissions
    CheckpointWait    = 3,    // Waiting for active tasks to checkpoint
    Applying          = 4,    // Patch actively being applied (brief window)
    Applied           = 5,    // Patch applied, awaiting validation run
    Validated         = 6,    // Patch validated, resuming
    Rolled            = 7,    // Patch rolled back (failure recovery)
    Error             = 8,    // Patch failed, rolled back, in error state
};

// Result of a hotpatch operation
struct HotpatchResult {
    bool            success;
    HotpatchPhase   endPhase;
    const char*     detail;
    uint64_t        durationMs;
    uint64_t        tasksQuiesced;
    bool            romped;  // "Romped" = patch applied but race detected (advisory)

    static HotpatchResult ok(HotpatchPhase phase, const char* msg, uint64_t ms = 0) {
        HotpatchResult r;
        r.success = true;
        r.endPhase = phase;
        r.detail = msg;
        r.durationMs = ms;
        r.tasksQuiesced = 0;
        r.romped = false;
        return r;
    }

    static HotpatchResult error(const char* msg) {
        HotpatchResult r;
        r.success = false;
        r.endPhase = HotpatchPhase::Error;
        r.detail = msg;
        r.durationMs = 0;
        r.tasksQuiesced = 0;
        r.romped = false;
        return r;
    }
};

// Hotpatch descriptor (what is being patched)
struct HotpatchDescriptor {
    std::string     patchId;            // Unique patch identifier
    std::string     description;        // Human-readable name
    uint64_t        targetModelId;      // Model version being patched
    uint32_t        affectedLayerCount; // Layers affected
    uint64_t        estimatedDurationMs;// How long the patch should take
    bool            rollbackRequired;   // Can we rollback if it fails?
    std::function<bool()> validateFn;   // Optional: post-patch validation
};

// ============================================================================
// LEVEL 3: Hotpatch Coordinator (Two-Phase Commit)
// ============================================================================
//
// Orchestrates safe patching windows via Governor coordination.
//
// Flow:
//   1. Client calls requestPatch(descriptor)
//   2. Coordinator queues request and signals Governor
//   3. Governor pauses new submissions → Quiescence
//   4. Governor waits for active tasks to checkpoint → Checkpoint
//   5. Coordinator applies function/model updates (atomic only)
//   6. Governor polls validators to confirm patch success
//   7. Governor resumes task queue
//

class HotpatchCoordinator {
public:
    static HotpatchCoordinator& instance();

    // Initialize the coordinator (requires Governor to be alive)
    bool initialize(ExecutionGovernor* governor);

    // Shut down the coordinator
    void shutdown();

    // Request a patch (async)
    // Returns immediately; check status with getPhase()
    bool requestPatch(const HotpatchDescriptor& descriptor);

    // Get current patch phase
    HotpatchPhase getPhase() const;

    // Get current patch ID (empty if idle)
    std::string getCurrentPatchId() const;

    // Wait for patch to reach a specific phase (with timeout)
    bool waitForPhase(HotpatchPhase targetPhase, uint64_t timeoutMs);

    // Get current patch result (only valid if phase >= Applied)
    HotpatchResult getLastResult() const;

    // Force abort current patch (rollback to last known-good state)
    bool abortPatch();

    // Poll: call periodically to drive the patch state machine
    // (Governor calls this from watchdog loop)
    void pollOnce();

    // Notify Governor that a particular task has reached a checkpoint
    // (Governor calls this when a task completes or enters safe point)
    void notifyTaskCheckpoint(uint64_t taskId);

    // Notify that all active tasks have checkpointed (or timed out waiting)
    // (Governor calls this after watchdog timeout)
    void notifyAllCheckpointed();

    // Register a patch validator (called during Applied → Validated phase)
    // Validators must run quickly (< 100ms) and be idempotent
    void registerValidator(const std::string& name,
                          std::function<bool()> validatorFn);

private:
    HotpatchCoordinator();
    ~HotpatchCoordinator();
    HotpatchCoordinator(const HotpatchCoordinator&) = delete;
    HotpatchCoordinator& operator=(const HotpatchCoordinator&) = delete;

    // State machine transitions
    void transitionTo(HotpatchPhase phase);
    void handleQuiescenceWait();
    void handleCheckpointWait();
    void handleApplying();
    void handleValidation();

    // Helpers
    bool allTasksCheckpointed() const;
    bool isApplyingTakingTooLong() const;

    ExecutionGovernor*                          m_governor;
    mutable std::atomic<HotpatchPhase>          m_phase;
    HotpatchDescriptor                          m_currentPatch;
    HotpatchResult                              m_lastResult;
    std::vector<uint64_t>                       m_checkpointedTasks;
    std::vector<std::pair<std::string, std::function<bool()>>> m_validators;
    std::atomic<uint64_t>                       m_phaseEnteredAtMs;
    std::atomic<bool>                           m_shutdown;
    mutable std::atomic<uint64_t>               m_version;

    // Phase-specific timeouts (milliseconds)
    static constexpr uint64_t QUIESCENCE_TIMEOUT_MS = 5000;
    static constexpr uint64_t CHECKPOINT_TIMEOUT_MS = 10000;
    static constexpr uint64_t APPLY_TIMEOUT_MS = 3000;
    static constexpr uint64_t VALIDATION_TIMEOUT_MS = 2000;
};

// ============================================================================
// LEVEL 4: Safety Gate (Instruction Validation)
// ============================================================================
//
// Agents check this gate before critical operations during active patches.
// Ensures instructions haven't drifted (patch didn't change rules mid-execution).
//

class PatchSafetyGate {
public:
    static PatchSafetyGate& instance();

    // Initialize (called once at startup)
    bool initialize();

    // Record instruction hash before starting a critical agent operation
    uint64_t recordInstructionChecksum();

    // Validate that instructions haven't changed since checkpoint
    // Returns true if safe, false if instructions were updated (advisory only)
    bool validateInstructionChecksum(uint64_t checkpoint);

    // Explicitly mark that a hotpatch is active (called by coordinator)
    void markPatchActive(const std::string& patchId);

    // Mark patch as complete
    void markPatchComplete();

    // Is a patch currently active?
    bool isPatchActive() const;

    // Get the active patch ID
    std::string getActivePatchId() const;

private:
    PatchSafetyGate();
    ~PatchSafetyGate();

    std::atomic<bool> m_patchActive;
    std::string m_activePatchId;
    uint64_t m_instructionChecksum;
    mutable std::atomic<uint64_t> m_version;
};

// ============================================================================
// Public API: Request a Hotpatch
// ============================================================================

// Convenience function for starting a hotpatch flow
inline bool requestHotpatch(
    const std::string& patchId,
    const std::string& description,
    uint64_t estimatedDurationMs,
    std::function<bool()> validateFn = nullptr)
{
    HotpatchDescriptor desc;
    desc.patchId = patchId;
    desc.description = description;
    desc.targetModelId = 0;  // 0 = current model
    desc.affectedLayerCount = 0;  // 0 = global (function patch)
    desc.estimatedDurationMs = estimatedDurationMs;
    desc.rollbackRequired = true;
    desc.validateFn = validateFn;

    return HotpatchCoordinator::instance().requestPatch(desc);
}
