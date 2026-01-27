/**
 * @file ide_agent_bridge.cpp
 * @brief Implementation of IDE agent plugin interface
 *
 * Orchestrates full wish→plan→execute pipeline with user feedback.
 */

#include "ide_agent_bridge.hpp"

#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QDir>

/**
 * @brief Constructor
 */
IDEAgentBridge::IDEAgentBridge(QObject* parent)
    : QObject(parent)
    , m_invoker(std::make_unique<ModelInvoker>(this))
    , m_executor(std::make_unique<ActionExecutor>(this))
{
    // Connect invoker signals
    connect(m_invoker.get(), &ModelInvoker::planGenerationStarted,
            this, &IDEAgentBridge::agentThinkingStarted);

    connect(m_invoker.get(), &ModelInvoker::planGenerated,
            this, &IDEAgentBridge::onPlanGenerated);

    connect(m_invoker.get(), &ModelInvoker::invocationError,
            this, [this](const QString& error, bool recoverable) {
                emit agentError("Plan generation failed: " + error, recoverable);
            });

    // Connect executor signals
    connect(m_executor.get(), &ActionExecutor::planStarted,
            this, &IDEAgentBridge::agentExecutionStarted);

    connect(m_executor.get(), &ActionExecutor::actionCompleted,
            this, &IDEAgentBridge::onActionCompleted);

    connect(m_executor.get(), &ActionExecutor::actionFailed,
            this, &IDEAgentBridge::onActionFailed);

    connect(m_executor.get(), &ActionExecutor::progressUpdated,
            this, [this](int current, int total) {
                emit agentProgressUpdated(current, total, 
                    QDateTime::currentMSecsSinceEpoch() - m_executionStartTime);
            });

    connect(m_executor.get(), &ActionExecutor::planCompleted,
            this, &IDEAgentBridge::onPlanCompleted);

    connect(m_executor.get(), &ActionExecutor::userInputNeeded,
            this, &IDEAgentBridge::onUserInputNeeded);

    m_projectRoot = QDir::currentPath();
}

/**
 * @brief Destructor
 */
IDEAgentBridge::~IDEAgentBridge() = default;

/**
 * @brief Initialize bridge
 */
void IDEAgentBridge::initialize(const QString& endpoint,
                               const QString& backend,
                               const QString& apiKey)
{
    m_invoker->setLLMBackend(backend, endpoint, apiKey);
    qInfo() << "[IDEAgentBridge] Initialized with backend:" << backend << "at" << endpoint;
}

/**
 * @brief Set project root
 */
void IDEAgentBridge::setProjectRoot(const QString& root)
{
    m_projectRoot = root;

    ExecutionContext ctx;
    ctx.projectRoot = root;
    ctx.dryRun = m_dryRun;

    m_executor->setContext(ctx);

    qDebug() << "[IDEAgentBridge] Project root set to:" << root;
}

/**
 * @brief Execute wish (full pipeline)
 */
void IDEAgentBridge::executeWish(const QString& wish, bool requireApproval)
{
    if (m_isExecuting) {
        emit agentError("Execution already in progress", false);
        return;
    }

    if (wish.isEmpty()) {
        emit agentError("Wish cannot be empty", false);
        return;
    }

    m_isExecuting = true;
    m_requireApproval = requireApproval;

    InvocationParams params;
    params.wish = wish;
    params.context = buildExecutionContext();
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};

    qDebug() << "[IDEAgentBridge] Executing wish:" << wish;
    m_invoker->invokeAsync(params);
}

/**
 * @brief Plan wish (preview mode)
 */
void IDEAgentBridge::planWish(const QString& wish)
{
    if (m_isExecuting) {
        emit agentError("Execution already in progress", false);
        return;
    }

    m_isExecuting = true;

    InvocationParams params;
    params.wish = wish;
    params.context = buildExecutionContext();
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};

    qDebug() << "[IDEAgentBridge] Planning wish:" << wish;
    m_invoker->invokeAsync(params);
}

/**
 * @brief Approve plan
 */
void IDEAgentBridge::approvePlan()
{
    if (!m_waitingForApproval) {
        qWarning() << "[IDEAgentBridge] No plan waiting for approval";
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
    emit executionCancelled();
    qDebug() << "[IDEAgentBridge] Plan rejected by user";
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
    emit executionCancelled();

    qDebug() << "[IDEAgentBridge] Execution cancelled";
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

    qDebug() << "[IDEAgentBridge] Dry-run mode:" << (enabled ? "ON" : "OFF");
}

/**
 * @brief Set stop-on-error behavior
 */
void IDEAgentBridge::setStopOnError(bool stopOnError)
{
    m_stopOnError = stopOnError;
    qDebug() << "[IDEAgentBridge] Stop on error:" << (stopOnError ? "YES" : "NO");
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
        emit agentError("Failed to generate plan: " + response.error, true);
        return;
    }

    m_currentPlan = convertToExecutionPlan(response.parsedPlan);
    emit agentGeneratedPlan(m_currentPlan);

    // If user approval is required, wait
    if (m_requireApproval) {
        m_waitingForApproval = true;
        emit planApprovalNeeded(m_currentPlan);
    } else {
        // Auto-execute
        executeCurrentPlan();
    }
}

/**
 * @brief Handle action completion
 */
void IDEAgentBridge::onActionCompleted(int index, bool success, const QJsonObject& result)
{
    QString description = m_currentPlan.actions[index].toObject().value("description").toString();
    emit agentExecutionProgress(index, description, success);

    qDebug() << "[IDEAgentBridge] Action" << (index+1) << "completed:" << (success ? "OK" : "FAILED");
}

/**
 * @brief Handle action failure
 */
void IDEAgentBridge::onActionFailed(int index, const QString& error, bool recoverable)
{
    qWarning() << "[IDEAgentBridge] Action" << index << "failed:" << error;

    if (!recoverable) {
        m_isExecuting = false;
        emit agentError("Unrecoverable error in action " + QString::number(index) + ": " + error, false);
    }
}

/**
 * @brief Handle plan completion
 */
void IDEAgentBridge::onPlanCompleted(bool success, const QJsonObject& result)
{
    int elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_executionStartTime;

    m_isExecuting = false;

    if (success) {
        recordExecution(m_currentPlan.wish, true, result, elapsedMs);
        qInfo() << "[IDEAgentBridge] Plan completed successfully in" << elapsedMs << "ms";
        emit agentCompleted(result, elapsedMs);
    } else {
        recordExecution(m_currentPlan.wish, false, result, elapsedMs);
        emit agentError("Plan execution failed", true);
    }
}

/**
 * @brief Handle user input needed
 */
void IDEAgentBridge::onUserInputNeeded(const QString& query, const QStringList& options)
{
    qDebug() << "[IDEAgentBridge] User input needed:" << query;
    emit userInputRequested(query, options);
}

// ─────────────────────────────────────────────────────────────────────────
// Utility Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Build execution context
 */
QString IDEAgentBridge::buildExecutionContext() const
{
    QString context = "RawrXD IDE - GGUF Quantization Framework\n";
    context += "Project Root: " + m_projectRoot + "\n";
    context += "Dry Run Mode: " + QString(m_dryRun ? "ENABLED" : "DISABLED") + "\n";

    return context;
}

/**
 * @brief Convert LLM plan to ExecutionPlan
 */
ExecutionPlan IDEAgentBridge::convertToExecutionPlan(const QJsonArray& llmPlan)
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
    if (m_currentPlan.actions.isEmpty()) {
        emit agentError("No plan to execute", false);
        m_isExecuting = false;
        return;
    }

    m_executionStartTime = QDateTime::currentMSecsSinceEpoch();

    ExecutionContext ctx;
    ctx.projectRoot = m_projectRoot;
    ctx.dryRun = m_dryRun;
    ctx.timeoutMs = 30000;

    m_executor->setContext(ctx);
    m_executor->executePlan(m_currentPlan.actions, m_stopOnError);

    qDebug() << "[IDEAgentBridge] Plan execution started with" 
             << m_currentPlan.actions.size() << "actions";
}

/**
 * @brief Record execution in history
 */
void IDEAgentBridge::recordExecution(const QString& wish,
                                    bool success,
                                    const QJsonObject& result,
                                    int elapsedMs)
{
    QJsonObject entry;
    entry["wish"] = wish;
    entry["success"] = success;
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry["elapsedMs"] = elapsedMs;
    entry["result"] = result;

    m_executionHistory.append(entry);

    qDebug() << "[IDEAgentBridge] Execution recorded:" << (success ? "SUCCESS" : "FAILED")
             << "in" << elapsedMs << "ms";
}
