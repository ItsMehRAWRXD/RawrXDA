/**
 * @file ide_agent_bridge.cpp
 * @brief Implementation of IDE agent plugin interface
 *
 * Orchestrates full wish→plan→execute pipeline with user feedback.
 * ActionExecutor uses simple_json::JsonValue; we convert to nlohmann::json for callbacks.
 */

#include "ide_agent_bridge.hpp"
#include "action_executor.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    inline nlohmann::json jsonValueToNlohmann(const JsonValue& v) {
        try {
            return nlohmann::json::parse(v.toJsonString());
        } catch (...) {
            return nlohmann::json::object();
        }
    }
}

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
        self->onActionCompleted(index, success, jsonValueToNlohmann(result));
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
        self->onPlanCompleted(success, jsonValueToNlohmann(result));
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
    fprintf(stderr, "[INFO] [IDEAgentBridge] Initialized with backend: %s at %s\n",
            backend.c_str(), endpoint.c_str());
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

    fprintf(stderr, "[IDEAgentBridge] Project root set to: %s\n", root.c_str());
}

/**
 * @brief Execute wish (full pipeline)
 */
void IDEAgentBridge::executeWish(const std::string& wish, bool requireApproval)
{
    if (m_isExecuting) {
        if (onAgentError) onAgentError("Execution already in progress", false);
        return;
    }

    if (wish.empty()) {
        if (onAgentError) onAgentError("Wish cannot be empty", false);
        return;
    }

    m_isExecuting = true;
    m_requireApproval = requireApproval;

    m_lastParams.wish = wish;
    m_lastParams.context = buildExecutionContext();
    m_lastParams.availableTools = {"search_files", "file_edit", "run_build",
                                  "execute_tests", "commit_git", "invoke_command"};
    m_retriesLeft = m_maxRetries;
    m_currentRetryBackoffMs = m_retryBackoffMs;

    fprintf(stderr, "[IDEAgentBridge] Executing wish: %s\n", wish.c_str());
    m_invoker->invokeAsync(m_lastParams);
}

/**
 * @brief Plan wish (preview mode)
 */
void IDEAgentBridge::planWish(const std::string& wish)
{
    if (m_isExecuting) {
        if (onAgentError) onAgentError("Execution already in progress", false);
        return;
    }

    m_isExecuting = true;

    m_lastParams.wish = wish;
    m_lastParams.context = buildExecutionContext();
    m_lastParams.availableTools = {"search_files", "file_edit", "run_build",
                                  "execute_tests", "commit_git", "invoke_command"};
    m_retriesLeft = m_maxRetries;
    m_currentRetryBackoffMs = m_retryBackoffMs;

    fprintf(stderr, "[IDEAgentBridge] Planning wish: %s\n", wish.c_str());
    m_invoker->invokeAsync(m_lastParams);
}

/**
 * @brief Approve plan
 */
void IDEAgentBridge::approvePlan()
{
    if (!m_waitingForApproval) {
        fprintf(stderr, "[WARN] [IDEAgentBridge] No plan waiting for approval\n");
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
    if (onExecutionCancelled) onExecutionCancelled();
    fprintf(stderr, "[IDEAgentBridge] Plan rejected by user\n");
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
    if (onExecutionCancelled) onExecutionCancelled();

    fprintf(stderr, "[IDEAgentBridge] Execution cancelled\n");
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

    fprintf(stderr, "[IDEAgentBridge] Dry-run mode: %s\n", enabled ? "ON" : "OFF");
}

/**
 * @brief Set stop-on-error behavior
 */
void IDEAgentBridge::setStopOnError(bool stopOnError)
{
    m_stopOnError = stopOnError;
    fprintf(stderr, "[IDEAgentBridge] Stop on error: %s\n", stopOnError ? "YES" : "NO");
}

void IDEAgentBridge::setRetryPolicy(int maxRetries, int initialBackoffMs)
{
    m_maxRetries = maxRetries;
    m_retryBackoffMs = initialBackoffMs;
    fprintf(stderr, "[IDEAgentBridge] Retry policy: maxRetries=%d, backoff=%d ms\n", maxRetries, initialBackoffMs);
}

void IDEAgentBridge::retryPlanGeneration()
{
    if (m_retriesLeft <= 0) return;

    int backoff = m_currentRetryBackoffMs;
    m_retriesLeft--;
    if (m_currentRetryBackoffMs < 30000)
        m_currentRetryBackoffMs = (m_currentRetryBackoffMs * 2 <= 30000) ? m_currentRetryBackoffMs * 2 : 30000;

    fprintf(stderr, "[IDEAgentBridge] Retrying plan generation in %d ms (%d left)\n", backoff, m_retriesLeft);

    std::thread([this, backoff]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
        if (onAgentThinkingStarted) onAgentThinkingStarted(m_lastParams.wish + " (retry)");
        m_invoker->invokeAsync(m_lastParams);
    }).detach();
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
        if (m_retriesLeft > 0) {
            retryPlanGeneration();
            return;
        }
        m_isExecuting = false;
        if (onAgentError) onAgentError("Failed to generate plan: " + response.error, true);
        return;
    }

    m_currentPlan = convertToExecutionPlan(response.parsedPlan);
    if (onAgentGeneratedPlan) onAgentGeneratedPlan(m_currentPlan);

    // If user approval is required, wait
    if (m_requireApproval) {
        m_waitingForApproval = true;
        if (onPlanApprovalNeeded) onPlanApprovalNeeded(m_currentPlan);
    } else {
        // Auto-execute
        executeCurrentPlan();
    }
}

/**
 * @brief Handle action completion
 */
void IDEAgentBridge::onActionCompleted(int index, bool success, const nlohmann::json& result)
{
    std::string description;
    if (m_currentPlan.actions.is_array()
        && index >= 0
        && static_cast<size_t>(index) < m_currentPlan.actions.size()
        && m_currentPlan.actions[index].is_object()) {
        description = m_currentPlan.actions[index].value("description", std::string(""));
    }

    if (onAgentExecutionProgress) onAgentExecutionProgress(index, description, success);

    fprintf(stderr, "[IDEAgentBridge] Action %d completed: %s\n",
            index + 1, success ? "OK" : "FAILED");
}

/**
 * @brief Handle action failure
 */
void IDEAgentBridge::onActionFailed(int index, const std::string& error, bool recoverable)
{
    fprintf(stderr, "[WARN] [IDEAgentBridge] Action %d failed: %s\n", index, error.c_str());

    if (!recoverable) {
        m_isExecuting = false;
        if (onAgentError) onAgentError("Unrecoverable error in action " + std::to_string(index) + ": " + error, false);
    }
}

/**
 * @brief Handle plan completion
 */
void IDEAgentBridge::onPlanCompleted(bool success, const nlohmann::json& result)
{
    auto now = std::chrono::system_clock::now();
    int elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count()) - m_executionStartTime;

    m_isExecuting = false;

    if (success) {
        recordExecution(m_currentPlan.wish, true, result, elapsedMs);
        fprintf(stderr, "[INFO] [IDEAgentBridge] Plan completed successfully in %d ms\n", elapsedMs);
        if (onAgentCompleted) onAgentCompleted(result, elapsedMs);
    } else {
        recordExecution(m_currentPlan.wish, false, result, elapsedMs);
        if (onAgentError) onAgentError("Plan execution failed", true);
    }
}

/**
 * @brief Handle user input needed
 */
void IDEAgentBridge::onUserInputNeeded(const std::string& query, const std::vector<std::string>& options)
{
    fprintf(stderr, "[IDEAgentBridge] User input needed: %s\n", query.c_str());
    if (onUserInputRequested) onUserInputRequested(query, options);
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
ExecutionPlan IDEAgentBridge::convertToExecutionPlan(const nlohmann::json& llmPlan)
{
    ExecutionPlan plan;
    plan.actions = llmPlan;
    plan.status = "Ready for execution";

    // Estimate time based on number of actions
    plan.estimatedTimeMs = static_cast<int>(llmPlan.size()) * 2000; // ~2s per action

    return plan;
}

/**
 * @brief Execute current plan
 */
void IDEAgentBridge::executeCurrentPlan()
{
    if (m_currentPlan.actions.empty()) {
        if (onAgentError) onAgentError("No plan to execute", false);
        m_isExecuting = false;
        return;
    }

    {
        auto now = std::chrono::system_clock::now();
        m_executionStartTime = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
    }

    ExecutionContext ctx;
    ctx.projectRoot = m_projectRoot;
    ctx.dryRun = m_dryRun;
    ctx.timeoutMs = 30000;

    m_executor->setContext(ctx);
    JsonValue actionsJson = JsonValue::parse(m_currentPlan.actions.dump());
    m_executor->executePlan(actionsJson, m_stopOnError);

    fprintf(stderr, "[IDEAgentBridge] Plan execution started with %d actions\n",
            static_cast<int>(m_currentPlan.actions.size()));
}

/**
 * @brief Record execution in history
 */
void IDEAgentBridge::recordExecution(const std::string& wish,
                                    bool success,
                                    const nlohmann::json& result,
                                    int elapsedMs)
{
    nlohmann::json entry;
    entry["wish"] = wish;
    entry["success"] = success;

    // Generate ISO8601 timestamp
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        entry["timestamp"] = std::string(buf);
    }

    entry["elapsedMs"] = elapsedMs;
    entry["result"] = result;

    m_executionHistory.push_back(entry);

    fprintf(stderr, "[IDEAgentBridge] Execution recorded: %s in %d ms\n",
            success ? "SUCCESS" : "FAILED", elapsedMs);
}

