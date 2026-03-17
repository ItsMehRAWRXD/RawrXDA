/**
 * @file agent_orchestrator.hpp
 * @brief Autonomous Agent Orchestrator for RawrXD
 *
 * Implements the core autonomous loop (Plan -> Execute -> Verify -> Heal).
 * Pure C++20 - Zero Qt dependencies
 */

#pragma once

#include "model_invoker.hpp"
#include "action_executor.hpp"
#include "agent_diagnostic_parser.hpp"
#include "tool_execution_engine.hpp"
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {

/**
 * @enum AgentState
 * @brief Internal states of the autonomous agent
 */
enum class AgentState {
    Idle,           ///< Waiting for a wish
    Thinking,       ///< Generating a plan
    Executing,      ///< Running tools
    Verifying,      ///< Checking if task is complete
    SelfHealing,    ///< Fixing errors detected during verification/execution
    Paused,         ///< Waiting for user input
    Completed,      ///< Task finished successfully
    Failed          ///< Task failed after maximum retries
};

/**
 * @struct AgentStep
 * @brief Represents a single iteration in the agent loop
 */
struct AgentStep {
    AgentState state;
    std::string planReasoning;
    nlohmann::json actions;  // Array
    nlohmann::json results;  // Object
    int64_t timestamp;
};

/**
 * @class AgentOrchestrator
 * @brief The "Soul" of the agentic IDE. Manages the autonomous loop.
 */
class AgentOrchestrator {
public:
    // Callback types for event notifications
    using MissionStartedCallback = std::function<void(const std::string& wish)>;
    using StateChangedCallback = std::function<void(AgentState newState)>;
    using StepCompletedCallback = std::function<void(const AgentStep& step)>;
    using MissionCompletedCallback = std::function<void(bool success, const std::string& summary)>;
    using ProgressUpdatedCallback = std::function<void(const std::string& status, int percent)>;
    using ErrorEncounteredCallback = std::function<void(const std::string& error, bool autoHealing)>;
    using UserInputRequiredCallback = std::function<void(const std::string& prompt, nlohmann::json& response)>;

    explicit AgentOrchestrator(ModelInvoker* invoker, ActionExecutor* executor);
    void setDiagnosticParser(AgentDiagnosticParser* parser) { m_parser = parser; }

    /**
     * @brief Start an autonomous mission
     */
    void startMission(const std::string& wish);

    /**
     * @brief Stop/Cancel the current mission
     */
    void stopMission();

    /**
     * @brief Resume after pause/user input
     */
    void resumeMission();

    // Configuration
    void setMaxRetries(int retries) { m_maxRetries = retries; }
    void setProjectContext(const std::string& context) { m_projectContext = context; }

    // Callback setters
    void setMissionStartedCallback(MissionStartedCallback cb) { m_onMissionStarted = std::move(cb); }
    void setStateChangedCallback(StateChangedCallback cb) { m_onStateChanged = std::move(cb); }
    void setStepCompletedCallback(StepCompletedCallback cb) { m_onStepCompleted = std::move(cb); }
    void setMissionCompletedCallback(MissionCompletedCallback cb) { m_onMissionCompleted = std::move(cb); }
    void setProgressUpdatedCallback(ProgressUpdatedCallback cb) { m_onProgressUpdated = std::move(cb); }
    void setErrorEncounteredCallback(ErrorEncounteredCallback cb) { m_onErrorEncountered = std::move(cb); }
    void setUserInputRequiredCallback(UserInputRequiredCallback cb) { m_onUserInputRequired = std::move(cb); }

    // Methods for invoker/executor to call back
    void onPlanGenerated(const nlohmann::json& plan, const std::string& reasoning);
    void onExecutionCompleted(bool success, const nlohmann::json& results);
    void onHealPlanGenerated(const nlohmann::json& plan, const std::string& reasoning);

private:
    void nextIteration();
    void think();
    void execute();
    void verify();
    void heal(const std::string& error);
    
    std::string buildFullContext() const;

    // Core components
    ModelInvoker* m_invoker;
    ActionExecutor* m_executor;
    AgentDiagnosticParser* m_parser = nullptr;

    // Mission state
    AgentState m_state = AgentState::Idle;
    std::string m_currentWish;
    std::string m_projectContext;
    
    std::deque<AgentStep> m_history;
    int m_retryCount = 0;
    int m_maxRetries = 5;
    bool m_stopRequested = false;

    int64_t m_missionStartTime = 0;

    // Callbacks
    MissionStartedCallback m_onMissionStarted;
    StateChangedCallback m_onStateChanged;
    StepCompletedCallback m_onStepCompleted;
    MissionCompletedCallback m_onMissionCompleted;
    ProgressUpdatedCallback m_onProgressUpdated;
    ErrorEncounteredCallback m_onErrorEncountered;
    UserInputRequiredCallback m_onUserInputRequired;
};

} // namespace RawrXD
