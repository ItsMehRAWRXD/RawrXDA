#pragma once
// =============================================================================
// AgentModeController.hpp — Unified agent mode state machine
//
// Replaces the fragmented ask_mode_handler / plan_mode_handler /
// agentic_mode_switcher trio with a single, explicit transition table.
//
// Design rules (zero-bloat):
//   • Single source of truth for current mode — no separate bool flags.
//   • CanTransition() encodes the valid transition matrix.
//   • Enter() runs mode-specific teardown + setup via callbacks.
//   • No virtual dispatch, no heap allocation, no exceptions.
// =============================================================================

#include <functional>
#include <string>
#include "agentic_mode_switcher.hpp"   // AgenticMode enum

namespace RawrXD {

// ---------------------------------------------------------------------------
// Transition matrix — which mode changes are permitted
// ---------------------------------------------------------------------------

// Every mode can transition to every other mode EXCEPT:
//   • DeepResearch → Agent is blocked (too disruptive mid-research).
//   • DeepThink → Plan is blocked (different output contract).
// Adjust as policy evolves — all business logic lives here, not scattered.
inline bool AgentModeCanTransition(AgenticMode from, AgenticMode to) {
    if (from == to) return false;                                    // no-op
    if (from == AgenticMode::DeepResearch && to == AgenticMode::Agent) return false;
    if (from == AgenticMode::DeepThink    && to == AgenticMode::Plan)  return false;
    return true;
}

// ---------------------------------------------------------------------------
// AgentModeController
// ---------------------------------------------------------------------------

class AgentModeController {
public:
    // Callback signatures — implement in Win32IDE or CLI as needed.
    using ModeCallback = std::function<void(AgenticMode prev, AgenticMode next)>;

    AgentModeController()  = default;
    ~AgentModeController() = default;

    // Not copyable — single owner.
    AgentModeController(const AgentModeController&)            = delete;
    AgentModeController& operator=(const AgentModeController&) = delete;
    AgentModeController(AgentModeController&&)                 = default;
    AgentModeController& operator=(AgentModeController&&)      = default;

    // ---- Registration -------------------------------------------------------

    /// Called before mode switches out (teardown current mode UI/state).
    void setOnLeave(ModeCallback cb)  { onLeave_  = std::move(cb); }

    /// Called after mode switches in (setup next mode UI/state).
    void setOnEnter(ModeCallback cb)  { onEnter_  = std::move(cb); }

    /// Called if a requested transition is blocked by the policy matrix.
    void setOnBlocked(ModeCallback cb) { onBlocked_ = std::move(cb); }

    // ---- Queries ------------------------------------------------------------

    AgenticMode current() const  { return current_; }
    AgenticMode previous() const { return previous_; }

    bool isActive(AgenticMode m) const { return current_ == m; }
    bool canSwitchTo(AgenticMode m) const {
        return AgentModeCanTransition(current_, m);
    }

    const char* currentName() const { return AgenticModeToString(current_); }

    // ---- Transitions --------------------------------------------------------

    /// Attempt to switch to the target mode.
    /// Returns true on success, false if blocked by the transition policy.
    bool switchTo(AgenticMode target) {
        if (!AgentModeCanTransition(current_, target)) {
            if (onBlocked_) onBlocked_(current_, target);
            return false;
        }
        AgenticMode prev = current_;
        if (onLeave_)  onLeave_(prev, target);
        previous_ = prev;
        current_  = target;
        if (onEnter_) onEnter_(prev, target);
        return true;
    }

    /// Revert to the previous mode (one-level undo).
    bool revert() {
        return switchTo(previous_);
    }

    /// Force-set mode, bypassing the transition policy.
    /// Use only during initialisation or after a hard reset.
    void forceSet(AgenticMode mode) {
        previous_ = current_;
        current_  = mode;
        if (onEnter_) onEnter_(previous_, current_);
    }

    // ---- Serialisation (persist mode across IDE sessions) ------------------

    int toInt()         const { return AgenticModeToInt(current_); }
    void fromInt(int v)       { forceSet(AgenticModeFromInt(v)); }
    std::string toString() const { return AgenticModeToString(current_); }

private:
    AgenticMode current_  = AgenticMode::Ask;
    AgenticMode previous_ = AgenticMode::Ask;

    ModeCallback onLeave_;
    ModeCallback onEnter_;
    ModeCallback onBlocked_;
};

} // namespace RawrXD
