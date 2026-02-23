// ============================================================================
// inference_state_machine.cpp — Unified State Machine for Inference Plane
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "inference_state_machine.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>

// SCAFFOLD_120: Inference state machine


namespace RawrXD {
namespace Inference {

// ============================================================================
// String tables
// ============================================================================
static const char* s_stateNames[] = {
    "Uninitialized", "Initializing", "ModelLoading", "Ready",
    "Prefetching", "Computing", "Sampling", "Streaming",
    "Cooldown", "Draining", "ShuttingDown", "Shutdown",
    "Faulted", "HotpatchPending", "Recovering"
};

static const char* s_eventNames[] = {
    "Initialize", "ModelLoaded", "RequestReceived", "PrefetchDone",
    "LayerComputed", "ForwardDone", "TokenSampled", "TokenStreamed",
    "GenerationDone", "CooldownDone", "DrainRequested", "DrainComplete",
    "ShutdownDone", "FaultDetected", "RecoveryDone", "HotpatchRequest",
    "HotpatchApplied", "Reset"
};

const char* inferenceStateName(InferenceState s) {
    auto idx = static_cast<uint8_t>(s);
    if (idx < static_cast<uint8_t>(InferenceState::_COUNT))
        return s_stateNames[idx];
    return "UNKNOWN";
}

const char* inferenceEventName(InferenceEvent e) {
    auto idx = static_cast<uint8_t>(e);
    if (idx < static_cast<uint8_t>(InferenceEvent::_COUNT))
        return s_eventNames[idx];
    return "UNKNOWN";
}

// ============================================================================
// Default transition table
// ============================================================================
static const TransitionRule s_defaultRules[] = {
    // Init chain
    { InferenceState::Uninitialized, InferenceEvent::Initialize,
      InferenceState::Initializing, nullptr, nullptr,
      "Begin system initialization" },

    { InferenceState::Initializing, InferenceEvent::ModelLoaded,
      InferenceState::ModelLoading, nullptr, nullptr,
      "Start loading model tensors" },

    { InferenceState::ModelLoading, InferenceEvent::ModelLoaded,
      InferenceState::Ready, nullptr, nullptr,
      "Model fully loaded, enter ready state" },

    // Inference loop
    { InferenceState::Ready, InferenceEvent::RequestReceived,
      InferenceState::Prefetching, nullptr, nullptr,
      "New request triggers prefetch of layer 0" },

    { InferenceState::Prefetching, InferenceEvent::PrefetchDone,
      InferenceState::Computing, nullptr, nullptr,
      "Prefetch complete, begin forward pass" },

    { InferenceState::Computing, InferenceEvent::LayerComputed,
      InferenceState::Computing, nullptr, nullptr,
      "Layer computed, continue forward pass (self-loop)" },

    { InferenceState::Computing, InferenceEvent::ForwardDone,
      InferenceState::Sampling, nullptr, nullptr,
      "All layers computed, begin sampling" },

    { InferenceState::Sampling, InferenceEvent::TokenSampled,
      InferenceState::Streaming, nullptr, nullptr,
      "Token selected, stream to client" },

    { InferenceState::Streaming, InferenceEvent::TokenStreamed,
      InferenceState::Prefetching, nullptr, nullptr,
      "Token sent, prefetch next batch of layers" },

    { InferenceState::Streaming, InferenceEvent::GenerationDone,
      InferenceState::Cooldown, nullptr, nullptr,
      "EOS or max tokens reached, begin cooldown" },

    { InferenceState::Cooldown, InferenceEvent::CooldownDone,
      InferenceState::Ready, nullptr, nullptr,
      "KV compaction done, return to ready" },

    // Hotpatch pause (from any hot path state)
    { InferenceState::Ready, InferenceEvent::HotpatchRequest,
      InferenceState::HotpatchPending, nullptr, nullptr,
      "Pause for hotpatch application" },

    { InferenceState::HotpatchPending, InferenceEvent::HotpatchApplied,
      InferenceState::Ready, nullptr, nullptr,
      "Hotpatch applied, resume ready" },

    // Fault handling
    { InferenceState::Computing, InferenceEvent::FaultDetected,
      InferenceState::Faulted, nullptr, nullptr,
      "Fault during computation" },

    { InferenceState::Sampling, InferenceEvent::FaultDetected,
      InferenceState::Faulted, nullptr, nullptr,
      "Fault during sampling" },

    { InferenceState::Prefetching, InferenceEvent::FaultDetected,
      InferenceState::Faulted, nullptr, nullptr,
      "Fault during prefetch" },

    { InferenceState::Streaming, InferenceEvent::FaultDetected,
      InferenceState::Faulted, nullptr, nullptr,
      "Fault during streaming" },

    { InferenceState::Faulted, InferenceEvent::Initialize,
      InferenceState::Recovering, nullptr, nullptr,
      "Begin recovery from fault" },

    { InferenceState::Recovering, InferenceEvent::RecoveryDone,
      InferenceState::Ready, nullptr, nullptr,
      "Recovery complete, return to ready" },

    // Graceful shutdown
    { InferenceState::Ready, InferenceEvent::DrainRequested,
      InferenceState::Draining, nullptr, nullptr,
      "Drain requested from ready" },

    { InferenceState::Cooldown, InferenceEvent::DrainRequested,
      InferenceState::Draining, nullptr, nullptr,
      "Drain requested during cooldown" },

    { InferenceState::Draining, InferenceEvent::DrainComplete,
      InferenceState::ShuttingDown, nullptr, nullptr,
      "All requests drained, shutting down" },

    { InferenceState::ShuttingDown, InferenceEvent::ShutdownDone,
      InferenceState::Shutdown, nullptr, nullptr,
      "Shutdown complete, terminal state" },

    // Universal reset (from any state)
    { InferenceState::Faulted, InferenceEvent::Reset,
      InferenceState::Uninitialized, nullptr, nullptr,
      "Force reset from faulted" },

    { InferenceState::Shutdown, InferenceEvent::Reset,
      InferenceState::Uninitialized, nullptr, nullptr,
      "Reset from shutdown" },
};

// ============================================================================
// Construction / Destruction
// ============================================================================

InferenceStateMachine::InferenceStateMachine() {
    m_stateEntryTime = std::chrono::steady_clock::now();
    std::memset(m_log, 0, sizeof(m_log));
}

InferenceStateMachine::~InferenceStateMachine() = default;

// ============================================================================
// Initialization
// ============================================================================

PatchResult InferenceStateMachine::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentState.store(InferenceState::Uninitialized, std::memory_order_release);
    m_stateEntryTime = std::chrono::steady_clock::now();
    m_logHead.store(0, std::memory_order_release);
    m_logTail.store(0, std::memory_order_release);
    m_sequenceId.store(0, std::memory_order_release);
    return PatchResult::ok("State machine initialized");
}

PatchResult InferenceStateMachine::registerTransitionTable(const TransitionRule* rules,
                                                            size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rules.clear();
    m_rules.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        m_rules.push_back(rules[i]);
    }
    return PatchResult::ok("Transition table registered");
}

PatchResult InferenceStateMachine::setActionContext(void* ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_actionCtx = ctx;
    return PatchResult::ok("Action context set");
}

PatchResult InferenceStateMachine::installDefaultTransitions() {
    return registerTransitionTable(s_defaultRules,
                                   sizeof(s_defaultRules) / sizeof(s_defaultRules[0]));
}

// ============================================================================
// State Transitions
// ============================================================================

StateMachineResult InferenceStateMachine::processEvent(InferenceEvent event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    InferenceState current = m_currentState.load(std::memory_order_acquire);
    double residencyUs = std::chrono::duration<double, std::micro>(
        now - m_stateEntryTime).count();

    // Find matching rule
    const TransitionRule* rule = findRule(current, event);
    if (!rule) {
        m_stats.rejectedTransitions.fetch_add(1, std::memory_order_relaxed);
        recordTransition(current, current, event, false,
                         "No matching transition rule", residencyUs);
        return StateMachineResult::error(current,
            "No transition rule for event in current state", -1);
    }

    // Evaluate guard
    if (rule->guard && !rule->guard(current, event, m_actionCtx)) {
        m_stats.rejectedTransitions.fetch_add(1, std::memory_order_relaxed);
        recordTransition(current, current, event, false,
                         "Guard rejected transition", residencyUs);
        return StateMachineResult::error(current,
            "Transition guard rejected", -2);
    }

    // Fire exit callback
    if (m_onExit) {
        m_onExit(current, rule->toState, event, m_actionCtx);
    }

    // Execute transition
    InferenceState previous = current;
    m_currentState.store(rule->toState, std::memory_order_release);

    // Update residency tracking
    auto stateIdx = static_cast<size_t>(previous);
    if (stateIdx < static_cast<size_t>(InferenceState::_COUNT)) {
        m_stats.stateResidencyUs[stateIdx] += static_cast<uint64_t>(residencyUs);
    }
    m_stateEntryTime = now;

    // Fire transition action
    if (rule->onTransition) {
        rule->onTransition(previous, rule->toState, event, m_actionCtx);
    }

    // Fire entry callback
    if (m_onEntry) {
        m_onEntry(previous, rule->toState, event, m_actionCtx);
    }

    // Track fault/recovery counts
    if (rule->toState == InferenceState::Faulted) {
        m_stats.faultCount.fetch_add(1, std::memory_order_relaxed);
        if (m_onFault) {
            m_onFault(previous, rule->toState, event, m_actionCtx);
        }
    }
    if (previous == InferenceState::Recovering &&
        rule->toState == InferenceState::Ready) {
        m_stats.recoveryCount.fetch_add(1, std::memory_order_relaxed);
    }
    if (rule->toState == InferenceState::HotpatchPending) {
        m_stats.hotpatchPauses.fetch_add(1, std::memory_order_relaxed);
    }

    // Record log entry
    m_stats.totalTransitions.fetch_add(1, std::memory_order_relaxed);
    recordTransition(previous, rule->toState, event, true,
                     rule->description, residencyUs);

    // Update avg/max timing
    uint64_t total = m_stats.totalTransitions.load(std::memory_order_relaxed);
    if (total > 0) {
        m_stats.avgTransitionUs =
            ((m_stats.avgTransitionUs * (total - 1)) + residencyUs) / total;
    }
    if (residencyUs > m_stats.maxTransitionUs) {
        m_stats.maxTransitionUs = residencyUs;
    }

    return StateMachineResult::ok(previous, rule->toState, rule->description);
}

StateMachineResult InferenceStateMachine::forceState(InferenceState state,
                                                       const char* reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    InferenceState previous = m_currentState.load(std::memory_order_acquire);
    m_currentState.store(state, std::memory_order_release);

    auto now = std::chrono::steady_clock::now();
    double residencyUs = std::chrono::duration<double, std::micro>(
        now - m_stateEntryTime).count();
    m_stateEntryTime = now;

    recordTransition(previous, state, InferenceEvent::Reset, true,
                     reason, residencyUs);

    return StateMachineResult::ok(previous, state, reason);
}

// ============================================================================
// Queries
// ============================================================================

InferenceState InferenceStateMachine::currentState() const {
    return m_currentState.load(std::memory_order_acquire);
}

const char* InferenceStateMachine::currentStateName() const {
    return inferenceStateName(m_currentState.load(std::memory_order_acquire));
}

bool InferenceStateMachine::isInState(InferenceState state) const {
    return m_currentState.load(std::memory_order_acquire) == state;
}

bool InferenceStateMachine::canAcceptEvent(InferenceEvent event) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    InferenceState current = m_currentState.load(std::memory_order_acquire);
    return findRule(current, event) != nullptr;
}

uint64_t InferenceStateMachine::timeInCurrentStateUs() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration<double, std::micro>(now - m_stateEntryTime).count());
}

// ============================================================================
// Callbacks
// ============================================================================

void InferenceStateMachine::setOnEntryCallback(TransitionActionFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onEntry = fn;
}

void InferenceStateMachine::setOnExitCallback(TransitionActionFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onExit = fn;
}

void InferenceStateMachine::setOnFaultCallback(TransitionActionFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onFault = fn;
}

// ============================================================================
// Log
// ============================================================================

size_t InferenceStateMachine::logSize() const {
    size_t head = m_logHead.load(std::memory_order_acquire);
    size_t tail = m_logTail.load(std::memory_order_acquire);
    if (head >= tail) return head - tail;
    return LOG_RING_SIZE - tail + head;
}

const TransitionLogEntry& InferenceStateMachine::logEntry(size_t index) const {
    size_t tail = m_logTail.load(std::memory_order_acquire);
    return m_log[(tail + index) % LOG_RING_SIZE];
}

std::vector<TransitionLogEntry> InferenceStateMachine::recentLog(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t available = logSize();
    size_t n = (count < available) ? count : available;
    std::vector<TransitionLogEntry> result;
    result.reserve(n);
    size_t head = m_logHead.load(std::memory_order_acquire);
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (head - n + i + LOG_RING_SIZE) % LOG_RING_SIZE;
        result.push_back(m_log[idx]);
    }
    return result;
}

void InferenceStateMachine::clearLog() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logHead.store(0, std::memory_order_release);
    m_logTail.store(0, std::memory_order_release);
}

// ============================================================================
// Stats
// ============================================================================

StateMachineStats InferenceStateMachine::getStats() const {
    // Copy atomics into a snapshot
    StateMachineStats snap;
    snap.totalTransitions.store(
        m_stats.totalTransitions.load(std::memory_order_relaxed));
    snap.rejectedTransitions.store(
        m_stats.rejectedTransitions.load(std::memory_order_relaxed));
    snap.faultCount.store(
        m_stats.faultCount.load(std::memory_order_relaxed));
    snap.recoveryCount.store(
        m_stats.recoveryCount.load(std::memory_order_relaxed));
    snap.hotpatchPauses.store(
        m_stats.hotpatchPauses.load(std::memory_order_relaxed));
    snap.avgTransitionUs = m_stats.avgTransitionUs;
    snap.maxTransitionUs = m_stats.maxTransitionUs;
    std::memcpy(snap.stateResidencyUs, m_stats.stateResidencyUs,
                sizeof(m_stats.stateResidencyUs));
    return snap;
}

void InferenceStateMachine::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalTransitions.store(0);
    m_stats.rejectedTransitions.store(0);
    m_stats.faultCount.store(0);
    m_stats.recoveryCount.store(0);
    m_stats.hotpatchPauses.store(0);
    m_stats.avgTransitionUs = 0.0;
    m_stats.maxTransitionUs = 0.0;
    std::memset(m_stats.stateResidencyUs, 0, sizeof(m_stats.stateResidencyUs));
}

// ============================================================================
// Debug / Visualization
// ============================================================================

std::string InferenceStateMachine::exportDot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream dot;
    dot << "digraph InferenceStateMachine {\n";
    dot << "  rankdir=LR;\n";
    dot << "  node [shape=box, style=rounded];\n\n";

    // Highlight current state
    InferenceState current = m_currentState.load(std::memory_order_acquire);
    for (uint8_t i = 0; i < static_cast<uint8_t>(InferenceState::_COUNT); ++i) {
        auto st = static_cast<InferenceState>(i);
        if (st == current) {
            dot << "  " << inferenceStateName(st)
                << " [style=\"rounded,filled\", fillcolor=lightblue];\n";
        }
    }
    dot << "\n";

    // Edges from transition table
    for (const auto& rule : m_rules) {
        dot << "  " << inferenceStateName(rule.fromState)
            << " -> " << inferenceStateName(rule.toState)
            << " [label=\"" << inferenceEventName(rule.event) << "\"];\n";
    }

    dot << "}\n";
    return dot.str();
}

std::string InferenceStateMachine::exportLogJson(size_t maxEntries) const {
    auto entries = recentLog(maxEntries);
    std::ostringstream json;
    json << "[\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        json << "  {\"seq\":" << e.sequenceId
             << ",\"from\":\"" << inferenceStateName(e.fromState) << "\""
             << ",\"to\":\"" << inferenceStateName(e.toState) << "\""
             << ",\"event\":\"" << inferenceEventName(e.event) << "\""
             << ",\"accepted\":" << (e.accepted ? "true" : "false")
             << ",\"durationUs\":" << e.durationUs
             << ",\"detail\":\"" << (e.detail ? e.detail : "") << "\""
             << "}";
        if (i + 1 < entries.size()) json << ",";
        json << "\n";
    }
    json << "]";
    return json.str();
}

// ============================================================================
// Private helpers
// ============================================================================

const TransitionRule* InferenceStateMachine::findRule(InferenceState from,
                                                       InferenceEvent event) const {
    for (const auto& rule : m_rules) {
        if (rule.fromState == from && rule.event == event)
            return &rule;
    }
    return nullptr;
}

void InferenceStateMachine::recordTransition(InferenceState from, InferenceState to,
                                               InferenceEvent event, bool accepted,
                                               const char* detail, double durationUs) {
    size_t head = m_logHead.load(std::memory_order_acquire);
    size_t idx = head % LOG_RING_SIZE;

    auto now = std::chrono::steady_clock::now();
    auto epoch = now.time_since_epoch();
    uint64_t tsUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());

    m_log[idx].sequenceId  = m_sequenceId.fetch_add(1, std::memory_order_relaxed);
    m_log[idx].timestampUs = tsUs;
    m_log[idx].fromState   = from;
    m_log[idx].toState     = to;
    m_log[idx].event       = event;
    m_log[idx].accepted    = accepted;
    m_log[idx].detail      = detail;
    m_log[idx].durationUs  = durationUs;

    size_t newHead = (head + 1) % LOG_RING_SIZE;
    m_logHead.store(newHead, std::memory_order_release);

    // Advance tail if ring is full
    size_t tail = m_logTail.load(std::memory_order_acquire);
    if (newHead == tail) {
        m_logTail.store((tail + 1) % LOG_RING_SIZE, std::memory_order_release);
    }
}

void InferenceStateMachine::updateResidency() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::micro>(
        now - m_stateEntryTime).count();
    auto idx = static_cast<size_t>(m_currentState.load(std::memory_order_acquire));
    if (idx < static_cast<size_t>(InferenceState::_COUNT)) {
        m_stats.stateResidencyUs[idx] += static_cast<uint64_t>(elapsed);
    }
    m_stateEntryTime = now;
}

} // namespace Inference
} // namespace RawrXD
