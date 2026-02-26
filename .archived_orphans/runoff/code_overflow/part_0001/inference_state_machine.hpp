// ============================================================================
// inference_state_machine.hpp — Unified State Machine for Inference Plane
// ============================================================================
// Deterministic, formally-verifiable state machine governing the entire
// inference lifecycle: Init → Load → Ready → Prefetch → Compute → Sample →
// Stream → Cooldown → Ready (loop) or Drain → Shutdown.
//
// Every transition is guarded, logged, and auditable. Invalid transitions
// return PatchResult::error() — no exceptions, no undefined behavior.
//
// Architecture:
//   - States encode the complete inference plane disposition
//   - Guards are composable predicates (function pointers)
//   - Actions fire on entry/exit/transition (function pointer callbacks)
//   - Ring-buffer event log for post-mortem and live debugging
//   - Thread-safe: one owner thread drives transitions; observers can poll
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"   // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>

namespace RawrXD {
namespace Inference {

// ============================================================================
// InferenceState — all valid states for the inference plane
// ============================================================================
enum class InferenceState : uint8_t {
    Uninitialized   = 0,   // No model loaded, no resources allocated
    Initializing    = 1,   // Allocating memory, mapping GGUF
    ModelLoading    = 2,   // Streaming tensors from disk/network
    Ready           = 3,   // Model loaded, idle, awaiting request
    Prefetching     = 4,   // Async layer prefetch in flight
    Computing       = 5,   // Transformer forward pass active
    Sampling        = 6,   // Token sampling (top-p/top-k/mirostat)
    Streaming       = 7,   // Token emitted, streaming to client
    Cooldown        = 8,   // Post-generation KV cache compaction
    Draining        = 9,   // Finishing in-flight requests before shutdown
    ShuttingDown    = 10,  // Releasing resources
    Shutdown        = 11,  // Terminal state, safe to destroy
    Faulted         = 12,  // Unrecoverable error (OOM, GPU hang, etc.)
    HotpatchPending = 13,  // Paused for hotpatch application
    Recovering      = 14,  // Recovering from fault via agentic correction

    _COUNT          = 15
};

const char* inferenceStateName(InferenceState s);

// ============================================================================
// InferenceEvent — triggers for state transitions
// ============================================================================
enum class InferenceEvent : uint8_t {
    Initialize      = 0,   // Trigger init
    ModelLoaded     = 1,   // All tensors present
    RequestReceived = 2,   // New inference request
    PrefetchDone    = 3,   // Async prefetch completed
    LayerComputed   = 4,   // One layer forward pass done
    ForwardDone     = 5,   // All layers computed
    TokenSampled    = 6,   // Token selected
    TokenStreamed    = 7,   // Token sent to client
    GenerationDone  = 8,   // EOS or max tokens
    CooldownDone    = 9,   // KV compaction finished
    DrainRequested  = 10,  // Graceful shutdown signal
    DrainComplete   = 11,  // All in-flight done
    ShutdownDone    = 12,  // Resources freed
    FaultDetected   = 13,  // Critical error
    RecoveryDone    = 14,  // Fault cleared
    HotpatchRequest = 15,  // Hotpatch layer wants to apply
    HotpatchApplied = 16,  // Hotpatch completed
    Reset           = 17,  // Force return to Uninitialized

    _COUNT          = 18
};

const char* inferenceEventName(InferenceEvent e);

// ============================================================================
// Transition Guard — predicate that must return true for transition to fire
// ============================================================================
typedef bool (*TransitionGuardFn)(InferenceState from, InferenceEvent event, void* ctx);

// ============================================================================
// Transition Action — fired on entry, exit, or during transition
// ============================================================================
typedef void (*TransitionActionFn)(InferenceState from, InferenceState to,
                                    InferenceEvent event, void* ctx);

// ============================================================================
// TransitionRule — one row in the transition table
// ============================================================================
struct TransitionRule {
    InferenceState      fromState;
    InferenceEvent      event;
    InferenceState      toState;
    TransitionGuardFn   guard;          // nullptr = always allowed
    TransitionActionFn  onTransition;   // nullptr = no action
    const char*         description;    // Human-readable label
};

// ============================================================================
// Transition Log Entry — ring-buffer element
// ============================================================================
struct TransitionLogEntry {
    uint64_t        sequenceId;
    uint64_t        timestampUs;        // Microseconds since epoch
    InferenceState  fromState;
    InferenceState  toState;
    InferenceEvent  event;
    bool            accepted;           // false if guard rejected
    const char*     detail;
    double          durationUs;         // Time spent in fromState
};

// ============================================================================
// StateMachineStats
// ============================================================================
struct StateMachineStats {
    std::atomic<uint64_t> totalTransitions{0};
    std::atomic<uint64_t> rejectedTransitions{0};
    std::atomic<uint64_t> faultCount{0};
    std::atomic<uint64_t> recoveryCount{0};
    std::atomic<uint64_t> hotpatchPauses{0};
    double                avgTransitionUs = 0.0;
    double                maxTransitionUs = 0.0;
    uint64_t              stateResidencyUs[static_cast<size_t>(InferenceState::_COUNT)] = {};
};

// ============================================================================
// StateMachineResult
// ============================================================================
struct StateMachineResult {
    bool            success;
    const char*     detail;
    int             errorCode;
    InferenceState  previousState;
    InferenceState  currentState;

    static StateMachineResult ok(InferenceState prev, InferenceState curr,
                                  const char* msg = "Transition accepted") {
        StateMachineResult r;
        r.success       = true;
        r.detail        = msg;
        r.errorCode     = 0;
        r.previousState = prev;
        r.currentState  = curr;
        return r;
    }

    static StateMachineResult error(InferenceState current, const char* msg, int code = -1) {
        StateMachineResult r;
        r.success       = false;
        r.detail        = msg;
        r.errorCode     = code;
        r.previousState = current;
        r.currentState  = current;
        return r;
    }
};

// ============================================================================
// InferenceStateMachine — the core engine
// ============================================================================
class InferenceStateMachine {
public:
    static constexpr size_t LOG_RING_SIZE = 4096;

    InferenceStateMachine();
    ~InferenceStateMachine();

    // Non-copyable
    InferenceStateMachine(const InferenceStateMachine&) = delete;
    InferenceStateMachine& operator=(const InferenceStateMachine&) = delete;

    // ---- Initialization ----
    PatchResult initialize();
    PatchResult registerTransitionTable(const TransitionRule* rules, size_t count);
    PatchResult setActionContext(void* ctx);

    // ---- State Transitions ----
    StateMachineResult processEvent(InferenceEvent event);
    StateMachineResult forceState(InferenceState state, const char* reason);

    // ---- Queries ----
    InferenceState currentState() const;
    const char*    currentStateName() const;
    bool           isInState(InferenceState state) const;
    bool           canAcceptEvent(InferenceEvent event) const;
    uint64_t       timeInCurrentStateUs() const;

    // ---- Callbacks ----
    void setOnEntryCallback(TransitionActionFn fn);
    void setOnExitCallback(TransitionActionFn fn);
    void setOnFaultCallback(TransitionActionFn fn);

    // ---- Log Access ----
    size_t                          logSize() const;
    const TransitionLogEntry&       logEntry(size_t index) const;
    std::vector<TransitionLogEntry> recentLog(size_t count) const;
    void                            clearLog();

    // ---- Statistics ----
    StateMachineStats getStats() const;
    void              resetStats();

    // ---- Debug / Visualization ----
    // Dump state machine as Graphviz DOT for visualization
    std::string exportDot() const;
    // Dump transition log as JSON
    std::string exportLogJson(size_t maxEntries = 256) const;

    // ---- Built-in Transition Table ----
    // Installs the default inference-plane transition rules
    PatchResult installDefaultTransitions();

private:
    const TransitionRule* findRule(InferenceState from, InferenceEvent event) const;
    void recordTransition(InferenceState from, InferenceState to,
                          InferenceEvent event, bool accepted,
                          const char* detail, double durationUs);
    void updateResidency();

    mutable std::mutex          m_mutex;
    std::atomic<InferenceState> m_currentState{InferenceState::Uninitialized};

    // Transition table
    std::vector<TransitionRule> m_rules;

    // Ring buffer log
    TransitionLogEntry  m_log[LOG_RING_SIZE];
    std::atomic<size_t> m_logHead{0};
    std::atomic<size_t> m_logTail{0};
    std::atomic<uint64_t> m_sequenceId{0};

    // Residency tracking
    std::chrono::steady_clock::time_point m_stateEntryTime;

    // Callbacks
    TransitionActionFn m_onEntry = nullptr;
    TransitionActionFn m_onExit  = nullptr;
    TransitionActionFn m_onFault = nullptr;
    void*              m_actionCtx = nullptr;

    // Stats
    StateMachineStats m_stats;
};

} // namespace Inference
} // namespace RawrXD
