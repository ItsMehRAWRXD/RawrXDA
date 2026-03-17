/**
 * @file ide_agent_bridge.hpp
 * @brief Plugin interface connecting IDE UI to agent execution pipeline
 *
 * Orchestrates full wish→plan→execute→result flow with progress tracking
 * and observability.
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include "model_invoker.hpp"
#include "action_executor.hpp"
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @struct ExecutionPlan
 * @brief High-level execution plan with metadata
 */
struct ExecutionPlan {
    std::string wish;                           ///< Original user wish
    json actions;                               ///< Parsed actions
    std::string reasoning;                      ///< Agent's reasoning
    int estimatedTimeMs = 0;                ///< Estimated execution time
    std::string status;                         ///< Current status
};

/**
 * @class IDEAgentBridge
 * @brief Main plugin interface for IDE integration
 */
class IDEAgentBridge {
public:
    IDEAgentBridge();
    ~IDEAgentBridge();

    IDEAgentBridge(const IDEAgentBridge&) = delete;
    IDEAgentBridge& operator=(const IDEAgentBridge&) = delete;

    /**
     * @brief Initialize bridge with LLM backend configuration
     */
    void initialize(const std::string& endpoint,
                   const std::string& backend = "ollama",
                   const std::string& apiKey = std::string());

    void setProjectRoot(const std::string& root);
    std::string projectRoot() const { return m_projectRoot; }

    void executeWish(const std::string& wish, bool requireApproval = true);
    void approvePlan(int planId = -1);
    void rejectPlan(int planId = -1);
    void cancelExecution();

    void updateStatus(const std::string& status);
    void updateProgress(int current, int total);

    // Callbacks (Instead of signals)
    void planGenerated(const ExecutionPlan& plan);
    void planApprovalNeeded(const ExecutionPlan& plan);
    void executionStarted();
    void actionStarted(const std::string& actionName);
    void actionFinished(const std::string& actionName, bool success, const std::string& result);
    void progressUpdated(int current, int total, const std::string& msg);
    void executionFinished(bool success, const std::string& summary);
    void errorOccurred(const std::string& error);

private:
    std::unique_ptr<ModelInvoker> m_invoker;
    std::unique_ptr<ActionExecutor> m_executor;
    std::string m_projectRoot;
    bool m_isExecuting = false;
    mutable std::mutex m_mutex;
    ExecutionPlan m_currentPlan;
};




