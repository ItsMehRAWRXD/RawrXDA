// ============================================================================
// Win32IDE_Phase16_AgenticController.h
// Phase 16 — Agentic Executor State Machine + Watchdog Controller
// ============================================================================
// Self-contained Phase 16 controller for the Win32IDE build.
// Drives: state machine, watchdog timeout, aperture-throttle safe-return.
// Wired via: onTitanPagingHeartbeatTimer(), AgenticBridge::ExecuteAgentCommand,
//             Win32IDE::validateAndDispatchToolCall.
// ============================================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <windows.h>

// ============================================================================
// State machine enum
// ============================================================================
enum class AgenticExecutorState : uint32_t
{
    IDLE                  = 0,  ///< No active tool call
    THINKING              = 1,  ///< Model generating plan / deciding next tool
    TOOL_EXECUTING        = 2,  ///< Inside a dispatched tool call
    TOOL_TIMEOUT_RECOVERY = 3,  ///< Tool exceeded timeout — unwinding
    VRAM_THROTTLING       = 4,  ///< Aperture >95%; reject new tool calls
    ERROR_RECOVERY        = 5,  ///< Fatal safe-return triggered
    SHUTDOWN              = 6   ///< IDE shutdown in progress
};

// ============================================================================
// Snapshot (read-only export of controller state)
// ============================================================================
struct AgenticExecutorSnapshot
{
    AgenticExecutorState state;
    uint32_t             toolDepth;                 ///< Current nesting depth
    uint32_t             lastToolLatencyMs;         ///< Latency of most recent tool
    uint32_t             recoveryCount;             ///< Cumulative recovery events
    uint64_t             timestamp;                 ///< GetTickCount64() at capture
    float                apertureUtilizationPct;    ///< 0–100%
    bool                 isApertureThrottled;       ///< true when util >95%
};

// ============================================================================
// Controller class
// ============================================================================
class AgenticExecutorController
{
public:
    AgenticExecutorController();

    /// Signal a tool is about to execute.
    /// Returns false if aperture is saturated; caller must defer.
    bool BeforeToolExecution(const std::string& toolName);

    /// Signal tool completion (success or failure).
    void AfterToolExecution(bool success, uint32_t latencyMs);

    /// Override the per-tool timeout budget (default: 10 000 ms).
    void SetToolTimeoutBudgetMs(uint32_t timeoutMs);

    /// Read a thread-safe snapshot of current state.
    AgenticExecutorSnapshot GetSnapshot() const;

    /// Graceful recovery — resets tool nesting stack.
    void RequestRecovery(const std::string& reason);

    /// Force safe-return if aperture hits the critical 95% threshold.
    bool TriggerSafeReturnIfCritical();

    /// Returns true when aperture utilization exceeds the critical threshold.
    bool IsApertureThrottled() const;

    /// Drive watchdog and aperture checks from the heartbeat timer (~250 ms).
    void OnHeartbeatTick();

private:
    mutable std::mutex     m_stateMutex;
    AgenticExecutorState   m_currentState;
    uint32_t               m_toolDepth;
    uint32_t               m_lastToolLatencyMs;
    uint32_t               m_recoveryCount;
    uint32_t               m_toolTimeoutBudgetMs;
    uint32_t               m_criticalUtilizationPct10000;  ///< 950000 == 95.0%
    uint64_t               m_activeToolStartTick;
    bool                   m_watchdogArmed;
    std::string            m_activeToolName;

    void TransitionState(AgenticExecutorState newState, const std::string& reason = "");
    bool ValidateStateTransition(AgenticExecutorState from, AgenticExecutorState to) const;

    /// Query Titan DLL for current aperture utilization (0–1 000 000 = pct*10000).
    static uint32_t GetCurrentAperturePct10000();
};

// ============================================================================
// Global API — safe to call from any thread, null-guarded
// ============================================================================

/// Get the singleton controller; null before first
/// AgenticNotifyToolStart / AgenticControllerHeartbeat call.
AgenticExecutorController* GetAgenticExecutorController();

/// Called by the heartbeat timer to drive watchdog and aperture checks.
void AgenticControllerHeartbeat();

/// Notify the controller before dispatching a tool.
/// Returns false when the agentic loop should back-off (aperture saturated).
bool AgenticNotifyToolStart(const char* toolName);

/// Notify the controller that a tool call completed.
void AgenticNotifyToolEnd(bool success, uint32_t latencyMs);

/// Override the global tool timeout budget.
void AgenticSetToolTimeoutBudget(uint32_t timeoutMs);

/// Populate caller-provided snapshot; returns false if controller unavailable.
bool AgenticGetSnapshot(AgenticExecutorSnapshot* outSnapshot);
