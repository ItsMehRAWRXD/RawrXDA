// ============================================================================
// Win32IDE_Phase16_AgenticController.cpp
// Phase 16 — Agentic Executor State Machine + Watchdog Controller
// ============================================================================
#include "Win32IDE_Phase16_AgenticController.h"
#include "RawrXD_Exports.h"

#include <memory>
#include <mutex>
#include <sstream>

// ============================================================================
// Module-level globals
// ============================================================================
namespace
{
std::mutex                                      g_controllerMutex;
std::unique_ptr<AgenticExecutorController>      g_pController;

AgenticExecutorController* EnsureController()
{
    // Lazy-init under lock; safe for multi-threaded first-call scenarios.
    if (!g_pController)
    {
        g_pController = std::make_unique<AgenticExecutorController>();
    }
    return g_pController.get();
}
}  // namespace

// ============================================================================
// AgenticExecutorController — constructor
// ============================================================================
AgenticExecutorController::AgenticExecutorController()
    : m_currentState(AgenticExecutorState::IDLE)
    , m_toolDepth(0)
    , m_lastToolLatencyMs(0)
    , m_recoveryCount(0)
    , m_toolTimeoutBudgetMs(10000)
    , m_criticalUtilizationPct10000(950000)  // 95.0%
    , m_activeToolStartTick(0)
    , m_watchdogArmed(false)
{
}

// ============================================================================
// Aperture query helper
// ============================================================================
uint32_t AgenticExecutorController::GetCurrentAperturePct10000()
{
    RAWRXD_APERTURE_STATUS apertureStatus = {};
    if (RawrXD_GetApertureUtilization(&apertureStatus) == RAWRXD_SUCCESS)
    {
        return apertureStatus.utilization_pct10000;
    }
    return 0;
}

// ============================================================================
// BeforeToolExecution
// ============================================================================
bool AgenticExecutorController::BeforeToolExecution(const std::string& toolName)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    const uint32_t utilPct10000 = GetCurrentAperturePct10000();
    if (utilPct10000 >= m_criticalUtilizationPct10000)
    {
        TransitionState(AgenticExecutorState::VRAM_THROTTLING,
                        "High aperture utilization: " +
                            std::to_string(utilPct10000 / 10000) + "." +
                            std::to_string((utilPct10000 % 10000) / 100) + "%");
        return false;
    }

    m_toolDepth++;
    if (m_currentState == AgenticExecutorState::IDLE ||
        m_currentState == AgenticExecutorState::THINKING)
    {
        TransitionState(AgenticExecutorState::TOOL_EXECUTING,
                        "Tool starting: " + toolName);
    }

    m_activeToolName       = toolName;
    m_activeToolStartTick  = GetTickCount64();
    m_watchdogArmed        = true;
    return true;
}

// ============================================================================
// AfterToolExecution
// ============================================================================
void AgenticExecutorController::AfterToolExecution(bool success, uint32_t latencyMs)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    m_lastToolLatencyMs = latencyMs;
    m_toolDepth         = (m_toolDepth > 0) ? m_toolDepth - 1 : 0;
    m_watchdogArmed     = false;

    if (success)
    {
        if (latencyMs > m_toolTimeoutBudgetMs)
        {
            TransitionState(AgenticExecutorState::TOOL_TIMEOUT_RECOVERY,
                            "Tool exceeded timeout budget");
            m_recoveryCount++;
            return;
        }
        if (m_toolDepth == 0)
        {
            TransitionState(AgenticExecutorState::THINKING, "Tool completed");
        }
    }
    else
    {
        TransitionState(AgenticExecutorState::TOOL_TIMEOUT_RECOVERY,
                        "Tool failed or timed out");
        m_recoveryCount++;
    }
}

// ============================================================================
// SetToolTimeoutBudgetMs
// ============================================================================
void AgenticExecutorController::SetToolTimeoutBudgetMs(uint32_t timeoutMs)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_toolTimeoutBudgetMs = (timeoutMs > 0) ? timeoutMs : 10000;
}

// ============================================================================
// GetSnapshot
// ============================================================================
AgenticExecutorSnapshot AgenticExecutorController::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    AgenticExecutorSnapshot snap = {};
    snap.state               = m_currentState;
    snap.toolDepth           = m_toolDepth;
    snap.lastToolLatencyMs   = m_lastToolLatencyMs;
    snap.recoveryCount       = m_recoveryCount;
    snap.timestamp           = GetTickCount64();

    const uint32_t util10000          = GetCurrentAperturePct10000();
    snap.apertureUtilizationPct       = static_cast<float>(util10000) / 10000.0f;
    snap.isApertureThrottled          = util10000 >= m_criticalUtilizationPct10000;

    return snap;
}

// ============================================================================
// RequestRecovery
// ============================================================================
void AgenticExecutorController::RequestRecovery(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    TransitionState(AgenticExecutorState::ERROR_RECOVERY, reason);
    m_toolDepth = 0;
    m_recoveryCount++;
}

// ============================================================================
// TriggerSafeReturnIfCritical
// ============================================================================
bool AgenticExecutorController::TriggerSafeReturnIfCritical()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    const uint32_t util10000 = GetCurrentAperturePct10000();
    if (util10000 < m_criticalUtilizationPct10000)
    {
        return false;
    }

    m_toolDepth     = 0;
    m_watchdogArmed = false;
    TransitionState(AgenticExecutorState::ERROR_RECOVERY,
                    "SafeReturn: critical aperture utilization");
    m_recoveryCount++;
    return true;
}

// ============================================================================
// IsApertureThrottled
// ============================================================================
bool AgenticExecutorController::IsApertureThrottled() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return GetCurrentAperturePct10000() >= m_criticalUtilizationPct10000;
}

// ============================================================================
// OnHeartbeatTick  (called from Timer ~250 ms — no nested locks)
// ============================================================================
void AgenticExecutorController::OnHeartbeatTick()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    // Watchdog: check for runaway tool execution.
    if (m_watchdogArmed &&
        m_currentState == AgenticExecutorState::TOOL_EXECUTING)
    {
        const uint64_t elapsed =
            GetTickCount64() - m_activeToolStartTick;
        if (elapsed > m_toolTimeoutBudgetMs)
        {
            m_watchdogArmed = false;
            m_toolDepth     = 0;
            TransitionState(AgenticExecutorState::TOOL_TIMEOUT_RECOVERY,
                            "Watchdog timeout: " + m_activeToolName);
            m_recoveryCount++;
            return;
        }
    }

    // Aperture critical safe-return — checked inline to avoid nested locking.
    const uint32_t util10000 = GetCurrentAperturePct10000();
    if (util10000 >= m_criticalUtilizationPct10000)
    {
        m_toolDepth     = 0;
        m_watchdogArmed = false;
        TransitionState(AgenticExecutorState::ERROR_RECOVERY,
                        "SafeReturn: critical aperture utilization (heartbeat)");
        m_recoveryCount++;
    }
}

// ============================================================================
// TransitionState
// ============================================================================
void AgenticExecutorController::TransitionState(AgenticExecutorState newState,
                                                const std::string& reason)
{
    if (!ValidateStateTransition(m_currentState, newState))
    {
        return;
    }
    m_currentState = newState;

    std::wostringstream woss;
    woss << L"[Phase16] Agentic state -> " << static_cast<uint32_t>(newState)
         << L" (" << std::wstring(reason.begin(), reason.end()) << L")\n";
    ::OutputDebugStringW(woss.str().c_str());
}

// ============================================================================
// ValidateStateTransition — strict matrix
// ============================================================================
bool AgenticExecutorController::ValidateStateTransition(AgenticExecutorState from,
                                                        AgenticExecutorState to) const
{
    if (from == to)
    {
        return true;
    }
    switch (from)
    {
    case AgenticExecutorState::IDLE:
        return to == AgenticExecutorState::THINKING ||
               to == AgenticExecutorState::TOOL_EXECUTING ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::THINKING:
        return to == AgenticExecutorState::TOOL_EXECUTING ||
               to == AgenticExecutorState::VRAM_THROTTLING ||
               to == AgenticExecutorState::ERROR_RECOVERY ||
               to == AgenticExecutorState::IDLE ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::TOOL_EXECUTING:
        return to == AgenticExecutorState::THINKING ||
               to == AgenticExecutorState::TOOL_TIMEOUT_RECOVERY ||
               to == AgenticExecutorState::ERROR_RECOVERY ||
               to == AgenticExecutorState::VRAM_THROTTLING ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::TOOL_TIMEOUT_RECOVERY:
        return to == AgenticExecutorState::ERROR_RECOVERY ||
               to == AgenticExecutorState::THINKING ||
               to == AgenticExecutorState::IDLE ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::VRAM_THROTTLING:
        return to == AgenticExecutorState::TOOL_EXECUTING ||
               to == AgenticExecutorState::THINKING ||
               to == AgenticExecutorState::ERROR_RECOVERY ||
               to == AgenticExecutorState::IDLE ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::ERROR_RECOVERY:
        return to == AgenticExecutorState::IDLE ||
               to == AgenticExecutorState::THINKING ||
               to == AgenticExecutorState::SHUTDOWN;

    case AgenticExecutorState::SHUTDOWN:
        return false;

    default:
        return false;
    }
}

// ============================================================================
// Global API
// ============================================================================
AgenticExecutorController* GetAgenticExecutorController()
{
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    return g_pController.get();
}

void AgenticControllerHeartbeat()
{
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    EnsureController()->OnHeartbeatTick();
}

bool AgenticNotifyToolStart(const char* toolName)
{
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    const std::string name =
        (toolName && toolName[0] != '\0') ? std::string(toolName) : std::string("tool");
    return EnsureController()->BeforeToolExecution(name);
}

void AgenticNotifyToolEnd(bool success, uint32_t latencyMs)
{
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    EnsureController()->AfterToolExecution(success, latencyMs);
}

void AgenticSetToolTimeoutBudget(uint32_t timeoutMs)
{
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    EnsureController()->SetToolTimeoutBudgetMs(timeoutMs);
}

bool AgenticGetSnapshot(AgenticExecutorSnapshot* outSnapshot)
{
    if (!outSnapshot)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_controllerMutex);
    if (!g_pController)
    {
        return false;
    }
    *outSnapshot = g_pController->GetSnapshot();
    return true;
}
