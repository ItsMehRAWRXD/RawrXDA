/**
 * @file ide_agent_bridge.cpp
 * @brief Implementation of IDE agent plugin interface
 *
 * Orchestrates full wish→plan→execute pipeline with user feedback.
 */

#include "ide_agent_bridge.hpp"

/**
 * @brief Constructor
 */
IDEAgentBridge::IDEAgentBridge()
    : m_invoker(std::make_unique<ModelInvoker>())
    , m_executor(std::make_unique<ActionExecutor>())
    , m_projectRoot("")
{
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
}

/**
 * @brief Set project root
 */
void IDEAgentBridge::setProjectRoot(const std::string& root)
{
    m_projectRoot = root;
    
    ExecutionContext ctx;
    ctx.projectRoot = root;
    m_executor->setContext(ctx);
}

/**
 * @brief Execute wish (full pipeline)
 */
void IDEAgentBridge::executeWish(const std::string& wish, bool requireApproval)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isExecuting) {
        errorOccurred("Execution already in progress");
        return;
    }

    if (wish.empty()) {
        errorOccurred("Wish cannot be empty");
        return;
    }

    m_isExecuting = true;

    InvocationParams params;
    params.wish = wish;
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};

    m_invoker->invokeAsync(params);
}

/**
 * @brief Approve plan
 */
void IDEAgentBridge::approvePlan(int)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isExecuting) return;

    executionStarted();
    m_executor->executePlanAsync(m_currentPlan.actions);
}

/**
 * @brief Reject plan
 */
void IDEAgentBridge::rejectPlan(int)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isExecuting = false;
    updateStatus("Plan rejected by user");
}

/**
 * @brief Cancel execution
 */
void IDEAgentBridge::cancelExecution()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executor->cancelExecution();
    m_isExecuting = false;
    updateStatus("Execution cancelled");
}

/**
 * @brief Update status message
 */
void IDEAgentBridge::updateStatus(const std::string& status)
{
    progressUpdated(0, 100, status);
}

/**
 * @brief Update progress
 */
void IDEAgentBridge::updateProgress(int current, int total)
{
    progressUpdated(current, total, "");
}

/**
 * @brief Handle plan generation
 */
void IDEAgentBridge::planGenerated(const ExecutionPlan& plan) {
    m_currentPlan = plan;
    planApprovalNeeded(plan);
}

/**
 * @brief Handle plan approval needed
 */
void IDEAgentBridge::planApprovalNeeded(const ExecutionPlan&) {}

/**
 * @brief Handle execution start
 */
void IDEAgentBridge::executionStarted() {}

/**
 * @brief Handle action start
 */
void IDEAgentBridge::actionStarted(const std::string&) {}

/**
 * @brief Handle action finish
 */
void IDEAgentBridge::actionFinished(const std::string&, bool, const std::string&) {}

/**
 * @brief Handle progress update
 */
void IDEAgentBridge::progressUpdated(int, int, const std::string&) {}

/**
 * @brief Handle execution finish
 */
void IDEAgentBridge::executionFinished(bool, const std::string&) {}

/**
 * @brief Handle error
 */
void IDEAgentBridge::errorOccurred(const std::string&) {}





