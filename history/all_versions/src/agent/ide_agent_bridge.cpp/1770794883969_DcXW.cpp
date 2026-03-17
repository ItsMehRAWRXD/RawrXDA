/**
 * @file ide_agent_bridge.cpp
 * @brief Implementation of IDE agent plugin interface
 *
 * Orchestrates full wish→plan→execute pipeline with user feedback.
 */

#include "ide_agent_bridge.hpp"
#include "json_types.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
/**
 * @brief Constructor
 */
IDEAgentBridge::IDEAgentBridge()
    : m_invoker(std::make_unique<ModelInvoker>())
    , m_executor(std::make_unique<ActionExecutor>())
{
    // Wire invoker callbacks
    m_invoker->onPlanGenerationStarted = [this](const std::string& msg) {
        if (onAgentThinkingStarted) onAgentThinkingStarted(msg);
    };

    m_invoker->onPlanGenerated = [this](const LLMResponse& response) {
        onPlanGenerated(response);
    };

    m_invoker->onInvocationError = [this](const std::string& error, bool recoverable) {
        if (onAgentError) onAgentError("Plan generation failed: " + error, recoverable);
    };

    // Wire executor callbacks (C-style function pointers)
    m_executor->registerPlanStartedCallback([](int totalActions, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        if (self->onAgentExecutionStarted) self->onAgentExecutionStarted(totalActions);
    }, this);

    m_executor->registerActionCompletedCallback([](int index, bool success, const JsonValue& result, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        self->onActionCompleted(index, success, result.toNlohmann());
    }, this);

    m_executor->registerActionFailedCallback([](int index, const std::string& error, bool recoverable, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        self->onActionFailed(index, error, recoverable);
    }, this);

    m_executor->registerProgressUpdatedCallback([](int current, int total, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        if (self->onAgentProgressUpdated) {
            auto now = std::chrono::system_clock::now();
            int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count()) - self->m_executionStartTime;
            self->onAgentProgressUpdated(current, total, elapsed);
        }
    }, this);

    m_executor->registerPlanCompletedCallback([](bool success, const JsonValue& result, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        self->onPlanCompleted(success, result.toNlohmann());
    }, this);

    m_executor->registerUserInputNeededCallback([](const std::string& query, const std::vector<std::string>& options, void* ud) {
        auto* self = static_cast<IDEAgentBridge*>(ud);
        self->onUserInputNeeded(query, options);
    }, this);

    m_projectRoot = std::filesystem::current_path().string();
}

/**
 * @brief Destructor
 */
IDEAgentBridge::~IDEAgentBridge() = default;

/**
 * @brief Initialize bridge
 */
void IDEAgentBridge::initialize(const std::string& endpoint,
                               const std::string& backend,
                               const std::string& apiKey)
{
    m_invoker->setLLMBackend(backend, endpoint, apiKey);
    fprintf(stderr, "[INFO] [IDEAgentBridge] Initialized with backend: at\n");
}

/**
 * @brief Set project root
 */
void IDEAgentBridge::setProjectRoot(const std::string& root)
{
    m_projectRoot = root;

    ExecutionContext ctx;
    ctx.projectRoot = root;
    ctx.dryRun = m_dryRun;

    m_executor->setContext(ctx);

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Project root set to:" << root;
}

/**
 * @brief Execute wish (full pipeline)
 */
void IDEAgentBridge::executeWish(const std::string& wish, bool requireApproval)
{
    if (m_isExecuting) {
        agentError("Execution already in progress", false);
        return;
    }

    if (wish.empty()) {
        agentError("Wish cannot be empty", false);
        return;
    }

    m_isExecuting = true;
    m_requireApproval = requireApproval;

    InvocationParams params;
    params.wish = wish;
    params.context = buildExecutionContext();
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Executing wish:" << wish;
    m_invoker->invokeAsync(params);
}

/**
 * @brief Plan wish (preview mode)
 */
void IDEAgentBridge::planWish(const std::string& wish)
{
    if (m_isExecuting) {
        agentError("Execution already in progress", false);
        return;
    }

    m_isExecuting = true;

    InvocationParams params;
    params.wish = wish;
    params.context = buildExecutionContext();
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Planning wish:" << wish;
    m_invoker->invokeAsync(params);
}

/**
 * @brief Approve plan
 */
void IDEAgentBridge::approvePlan()
{
    if (!m_waitingForApproval) {
        fprintf(stderr, "[WARN] %s\\n", std::string("[IDEAgentBridge] No plan waiting for approval";
        return;
    }

    m_waitingForApproval = false;
    executeCurrentPlan();
}

/**
 * @brief Reject plan
 */
void IDEAgentBridge::rejectPlan()
{
    m_waitingForApproval = false;
    m_isExecuting = false;
    executionCancelled();
    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Plan rejected by user";
}

/**
 * @brief Cancel execution
 */
void IDEAgentBridge::cancelExecution()
{
    if (m_executor->isExecuting()) {
        m_executor->cancelExecution();
    }

    m_isExecuting = false;
    m_waitingForApproval = false;
    executionCancelled();

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Execution cancelled";
}

/**
 * @brief Enable/disable dry-run mode
 */
void IDEAgentBridge::setDryRunMode(bool enabled)
{
    m_dryRun = enabled;

    ExecutionContext ctx = m_executor->context();
    ctx.dryRun = enabled;
    m_executor->setContext(ctx);

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Dry-run mode:" << (enabled ? "ON" : "OFF");
}

/**
 * @brief Set stop-on-error behavior
 */
void IDEAgentBridge::setStopOnError(bool stopOnError)
{
    m_stopOnError = stopOnError;
    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Stop on error:" << (stopOnError ? "YES" : "NO");
}

// ─────────────────────────────────────────────────────────────────────────
// Signal Handlers
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Handle plan generation
 */
void IDEAgentBridge::onPlanGenerated(const LLMResponse& response)
{
    if (!response.success) {
        m_isExecuting = false;
        agentError("Failed to generate plan: " + response.error, true);
        return;
    }

    m_currentPlan = convertToExecutionPlan(response.parsedPlan);
    agentGeneratedPlan(m_currentPlan);

    // If user approval is required, wait
    if (m_requireApproval) {
        m_waitingForApproval = true;
        planApprovalNeeded(m_currentPlan);
    } else {
        // Auto-execute
        executeCurrentPlan();
    }
}

/**
 * @brief Handle action completion
 */
void IDEAgentBridge::onActionCompleted(int index, bool success, const JsonObject& result)
{
    std::string description = m_currentPlan.actions[index].value("description").toString();
    agentExecutionProgress(index, description, success);

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Action" << (index+1) << "completed:" << (success ? "OK" : "FAILED");
}

/**
 * @brief Handle action failure
 */
void IDEAgentBridge::onActionFailed(int index, const std::string& error, bool recoverable)
{
    fprintf(stderr, "[WARN] %s\\n", std::string("[IDEAgentBridge] Action" << index << "failed:" << error;

    if (!recoverable) {
        m_isExecuting = false;
        agentError("Unrecoverable error in action " + std::to_string(index) + ": " + error, false);
    }
}

/**
 * @brief Handle plan completion
 */
void IDEAgentBridge::onPlanCompleted(bool success, const JsonObject& result)
{
    int elapsedMs = std::chrono::system_clock::time_point::currentMSecsSinceEpoch() - m_executionStartTime;

    m_isExecuting = false;

    if (success) {
        recordExecution(m_currentPlan.wish, true, result, elapsedMs);
        fprintf(stderr, "[INFO] [IDEAgentBridge] Plan completed successfully in ms\n");
        agentCompleted(result, elapsedMs);
    } else {
        recordExecution(m_currentPlan.wish, false, result, elapsedMs);
        agentError("Plan execution failed", true);
    }
}

/**
 * @brief Handle user input needed
 */
void IDEAgentBridge::onUserInputNeeded(const std::string& query, const std::vector<std::string>& options)
{
    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] User input needed:" << query;
    userInputRequested(query, options);
}

// ─────────────────────────────────────────────────────────────────────────
// Utility Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Build execution context
 */
std::string IDEAgentBridge::buildExecutionContext() const
{
    std::string context = "RawrXD IDE - GGUF Quantization Framework\n";
    context += "Project Root: " + m_projectRoot + "\n";
    context += "Dry Run Mode: " + std::string(m_dryRun ? "ENABLED" : "DISABLED") + "\n";

    return context;
}

/**
 * @brief Convert LLM plan to ExecutionPlan
 */
ExecutionPlan IDEAgentBridge::convertToExecutionPlan(const JsonArray& llmPlan)
{
    ExecutionPlan plan;
    plan.actions = llmPlan;
    plan.status = "Ready for execution";

    // Estimate time based on number of actions
    plan.estimatedTimeMs = llmPlan.size() * 2000; // ~2s per action

    return plan;
}

/**
 * @brief Execute current plan
 */
void IDEAgentBridge::executeCurrentPlan()
{
    if (m_currentPlan.actions.empty()) {
        agentError("No plan to execute", false);
        m_isExecuting = false;
        return;
    }

    m_executionStartTime = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();

    ExecutionContext ctx;
    ctx.projectRoot = m_projectRoot;
    ctx.dryRun = m_dryRun;
    ctx.timeoutMs = 30000;

    m_executor->setContext(ctx);
    m_executor->executePlan(m_currentPlan.actions, m_stopOnError);

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Plan execution started with" 
             << m_currentPlan.actions.size() << "actions";
}

/**
 * @brief Record execution in history
 */
void IDEAgentBridge::recordExecution(const std::string& wish,
                                    bool success,
                                    const JsonObject& result,
                                    int elapsedMs)
{
    JsonObject entry;
    entry["wish"] = wish;
    entry["success"] = success;
    entry["timestamp"] = std::chrono::system_clock::now().toString(/* ISO8601 */);
    entry["elapsedMs"] = elapsedMs;
    entry["result"] = result;

    m_executionHistory.push_back(entry);

    fprintf(stderr, "%s\\n", std::string("[IDEAgentBridge] Execution recorded:" << (success ? "SUCCESS" : "FAILED")
             << "in" << elapsedMs << "ms";
}

