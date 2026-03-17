// Tool Composition Framework Implementation
#include "tool_composition_framework.h"
#include "../agentic_executor.h"
#include "advanced_planning_engine.h"
#include "../error_recovery_system.h"
#include "distributed_tracer.h"
#include "error_analysis_system.h"
#include <QDebug>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QApplication>
#include <QDir>
#include <QtConcurrent>
#include <QThread>
#include <algorithm>
#include <cmath>

// ComposableTool Implementation
ComposableTool::ComposableTool(const QString& id, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_version("1.0.0")
    , m_isChainable(true)
    , m_supportsParallelExecution(false)
    , m_defaultTimeoutMs(30000)
    , m_maxConcurrentInstances(1)
{
    qInfo() << "[ComposableTool]" << m_id << "created";
}

ComposableTool::~ComposableTool()
{
    cleanup();
    qInfo() << "[ComposableTool]" << m_id << "destroyed";
}

ToolExecutionResult ComposableTool::execute(const QVariantMap& input, const ToolExecutionContext& context)
{
    QString executionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QWriteLocker locker(&m_lock);
    m_activeExecutions.append(executionId);
    locker.unlock();
    
    emit executionStarted(executionId);
    
    ToolExecutionResult result;
    result.toolId = m_id;
    result.executionId = executionId;
    result.timestamp = QDateTime::currentDateTime();
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // Validate input
        if (!validateInput(input)) {
            result.success = false;
            result.errorMessage = "Input validation failed";
            emit executionFailed(executionId, result.errorMessage);
            return result;
        }
        
        // Check timeout
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(context.timeoutMs);
        
        // Execute the tool
        result = doExecute(input, context);
        result.executionId = executionId;
        result.executionTimeMs = timer.elapsed();
        
        if (result.success) {
            emit executionCompleted(executionId, result);
        } else {
            emit executionFailed(executionId, result.errorMessage);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("Exception during execution: %1").arg(e.what());
        result.executionTimeMs = timer.elapsed();
        emit executionFailed(executionId, result.errorMessage);
    }
    
    // Update metrics
    QWriteLocker metricsLocker(&m_lock);
    QJsonObject metrics = m_executionMetrics;
    int totalExecutions = metrics["total_executions"].toInt(0) + 1;
    int successfulExecutions = metrics["successful_executions"].toInt(0);
    if (result.success) successfulExecutions++;
    
    metrics["total_executions"] = totalExecutions;
    metrics["successful_executions"] = successfulExecutions;
    metrics["success_rate"] = double(successfulExecutions) / double(totalExecutions);
    metrics["average_execution_time_ms"] = (metrics["average_execution_time_ms"].toDouble() * (totalExecutions - 1) + result.executionTimeMs) / totalExecutions;
    metrics["last_execution"] = result.timestamp.toString(Qt::ISODate);
    
    m_executionMetrics = metrics;
    m_activeExecutions.removeAll(executionId);
    
    return result;
}

bool ComposableTool::validateInput(const QVariantMap& input) const
{
    QReadLocker locker(&m_lock);
    
    // Check required parameters
    for (const QString& param : m_inputParameters) {
        if (!input.contains(param)) {
            qWarning() << "[ComposableTool]" << m_id << "missing required parameter:" << param;
            return false;
        }
    }
    
    // Validate against schema if provided
    if (!m_parameterSchema.isEmpty()) {
        // Basic type validation
        for (auto it = input.begin(); it != input.end(); ++it) {
            QString key = it.key();
            if (m_parameterSchema.contains(key)) {
                QJsonObject paramSchema = m_parameterSchema[key].toObject();
                QString expectedType = paramSchema["type"].toString();
                
                QVariant::Type actualType = it.value().type();
                
                // Simple type checking
                if (expectedType == "string" && actualType != QVariant::String) {
                    return false;
                }
                if (expectedType == "number" && actualType != QVariant::Double && actualType != QVariant::Int) {
                    return false;
                }
                if (expectedType == "boolean" && actualType != QVariant::Bool) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

QJsonObject ComposableTool::getExecutionMetrics() const
{
    QReadLocker locker(&m_lock);
    return m_executionMetrics;
}

void ComposableTool::cleanup()
{
    QWriteLocker locker(&m_lock);
    m_activeExecutions.clear();
    m_executionMetrics = QJsonObject();
}

bool ComposableTool::canChainWith(const ComposableTool* nextTool) const
{
    if (!nextTool || !m_isChainable || !nextTool->isChainable()) {
        return false;
    }
    
    QReadLocker locker(&m_lock);
    QReadLocker nextLocker(&nextTool->m_lock);
    
    // Check if output parameters match input parameters
    for (const QString& outputParam : m_outputParameters) {
        if (nextTool->m_inputParameters.contains(outputParam)) {
            return true; // At least one parameter can be passed
        }
    }
    
    // Check format compatibility
    for (const QString& format : m_supportedFormats) {
        if (nextTool->m_supportedFormats.contains(format)) {
            return true;
        }
    }
    
    return false;
}

QVariantMap ComposableTool::transformOutputForChaining(const QVariantMap& output, const ComposableTool* nextTool) const
{
    Q_UNUSED(nextTool)
    // Default implementation - no transformation
    return output;
}

QJsonObject ComposableTool::toJson() const
{
    QReadLocker locker(&m_lock);
    
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["description"] = m_description;
    json["category"] = m_category;
    json["version"] = m_version;
    json["input_parameters"] = QJsonArray::fromStringList(m_inputParameters);
    json["output_parameters"] = QJsonArray::fromStringList(m_outputParameters);
    json["parameter_schema"] = m_parameterSchema;
    json["required_dependencies"] = QJsonArray::fromStringList(m_requiredDependencies);
    json["supported_formats"] = QJsonArray::fromStringList(m_supportedFormats);
    json["is_chainable"] = m_isChainable;
    json["supports_parallel_execution"] = m_supportsParallelExecution;
    json["default_timeout_ms"] = m_defaultTimeoutMs;
    json["max_concurrent_instances"] = m_maxConcurrentInstances;
    json["execution_metrics"] = m_executionMetrics;
    
    return json;
}

// ToolCompositionFramework Implementation
ToolCompositionFramework::ToolCompositionFramework(QObject* parent)
    : QObject(parent)
{
    m_uptimeTimer.start();
    initializeComponents();
    setupTimers();
    qInfo() << "[ToolCompositionFramework] Initialized";
}

ToolCompositionFramework::~ToolCompositionFramework()
{
    // Stop all running executions
    QWriteLocker locker(&m_toolsLock);
    for (const auto& pair : m_tools) {
        pair.second->cleanup();
    }
    m_tools.clear();
    
    qInfo() << "[ToolCompositionFramework] Destroyed";
}

void ToolCompositionFramework::initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner)
{
    m_agenticExecutor = executor;
    m_planningEngine = planner;
    
    if (executor && planner) {
        m_initialized = true;
        connectSignals();
        qInfo() << "[ToolCompositionFramework] Initialization completed";
    } else {
        qWarning() << "[ToolCompositionFramework] Failed to initialize - missing components";
    }
}

bool ToolCompositionFramework::registerTool(std::unique_ptr<ComposableTool> tool)
{
    if (!tool) return false;
    
    QString toolId = tool->id();
    
    // Connect tool signals
    connect(tool.get(), &ComposableTool::executionStarted,
            this, &ToolCompositionFramework::executionStarted);
    connect(tool.get(), &ComposableTool::executionCompleted,
            this, &ToolCompositionFramework::onToolExecutionCompleted);
    connect(tool.get(), &ComposableTool::executionFailed,
            this, &ToolCompositionFramework::onToolExecutionFailed);
    
    {
        QWriteLocker locker(&m_toolsLock);
        m_tools[toolId] = std::move(tool);
    }
    
    // Update usage statistics
    QJsonObject stats = m_usageStatistics;
    stats["total_registered_tools"] = stats["total_registered_tools"].toInt(0) + 1;
    stats["last_tool_registered"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_usageStatistics = stats;
    
    emit toolRegistered(toolId);
    qInfo() << "[ToolCompositionFramework] Registered tool:" << toolId;
    
    return true;
}

bool ToolCompositionFramework::unregisterTool(const QString& toolId)
{
    QWriteLocker locker(&m_toolsLock);
    auto it = m_tools.find(toolId);
    if (it != m_tools.end()) {
        it->second->cleanup();
        m_tools.erase(it);
        locker.unlock();
        
        emit toolUnregistered(toolId);
        qInfo() << "[ToolCompositionFramework] Unregistered tool:" << toolId;
        return true;
    }
    
    return false;
}

ComposableTool* ToolCompositionFramework::getTool(const QString& toolId)
{
    QReadLocker locker(&m_toolsLock);
    auto it = m_tools.find(toolId);
    return (it != m_tools.end()) ? it->second.get() : nullptr;
}

const ComposableTool* ToolCompositionFramework::getTool(const QString& toolId) const
{
    QReadLocker locker(&m_toolsLock);
    auto it = m_tools.find(toolId);
    return (it != m_tools.end()) ? it->second.get() : nullptr;
}

QStringList ToolCompositionFramework::getAvailableTools() const
{
    QReadLocker locker(&m_toolsLock);
    QStringList tools;
    for (const auto& pair : m_tools) {
        tools.append(pair.first);
    }
    return tools;
}

QStringList ToolCompositionFramework::getToolsByCategory(const QString& category) const
{
    QReadLocker locker(&m_toolsLock);
    QStringList tools;
    for (const auto& pair : m_tools) {
        if (pair.second->category() == category) {
            tools.append(pair.first);
        }
    }
    return tools;
}

QString ToolCompositionFramework::createExecutionContext(const QJsonObject& environment)
{
    QString contextId = generateContextId();
    
    ToolExecutionContext context;
    context.contextId = contextId;
    context.environment = environment;
    context.timeoutMs = m_defaultTimeoutMs;
    context.maxChainDepth = 10;
    context.allowDynamicChaining = true;
    
    {
        QWriteLocker locker(&m_contextsLock);
        m_contexts[contextId] = context;
    }
    
    qInfo() << "[ToolCompositionFramework] Created execution context:" << contextId;
    return contextId;
}

QString ToolCompositionFramework::executeTool(const QString& toolId, const QVariantMap& input, const QString& contextId)
{
    ComposableTool* tool = getTool(toolId);
    if (!tool) {
        qWarning() << "[ToolCompositionFramework] Tool not found:" << toolId;
        return QString();
    }
    
    // Get or create execution context
    ToolExecutionContext context;
    if (!contextId.isEmpty()) {
        QReadLocker locker(&m_contextsLock);
        auto it = m_contexts.find(contextId);
        if (it != m_contexts.end()) {
            context = it->second;
        } else {
            qWarning() << "[ToolCompositionFramework] Context not found:" << contextId;
            return QString();
        }
    } else {
        context.contextId = generateContextId();
        context.timeoutMs = m_defaultTimeoutMs;
    }
    
    QString executionId = generateExecutionId();
    
    // Execute synchronously
    ToolExecutionResult result = tool->execute(input, context);
    result.executionId = executionId;
    
    // Store result
    {
        QWriteLocker locker(&m_resultsLock);
        m_executionResults[executionId] = result;
    }
    
    // Record execution data for learning
    if (m_learningEnabled) {
        recordExecutionData(executionId, result);
    }
    
    qInfo() << "[ToolCompositionFramework] Executed tool" << toolId 
            << "with execution ID:" << executionId;
    
    return executionId;
}

QString ToolCompositionFramework::executeToolAsync(const QString& toolId, const QVariantMap& input, const QString& contextId)
{
    ComposableTool* tool = getTool(toolId);
    if (!tool) {
        qWarning() << "[ToolCompositionFramework] Tool not found:" << toolId;
        return QString();
    }
    
    QString executionId = generateExecutionId();
    
    // Add to execution queue
    QMutexLocker locker(&m_executionMutex);
    m_executionQueue.enqueue(executionId);
    
    // Store execution parameters for later processing
    QJsonObject execParams;
    execParams["tool_id"] = toolId;
    execParams["input"] = QJsonObject::fromVariantMap(input);
    execParams["context_id"] = contextId;
    execParams["execution_id"] = executionId;
    
    // Store parameters temporarily (in real implementation, use proper storage)
    // m_executionParameters[executionId] = execParams;
    
    qInfo() << "[ToolCompositionFramework] Queued async execution:" << executionId;
    
    return executionId;
}

ToolExecutionResult ToolCompositionFramework::getExecutionResult(const QString& executionId) const
{
    QReadLocker locker(&m_resultsLock);
    auto it = m_executionResults.find(executionId);
    return (it != m_executionResults.end()) ? it->second : ToolExecutionResult();
}

bool ToolCompositionFramework::isExecutionComplete(const QString& executionId) const
{
    QReadLocker locker(&m_resultsLock);
    return m_executionResults.find(executionId) != m_executionResults.end();
}

QString ToolCompositionFramework::executeToolChain(const ToolChain& chain, const QVariantMap& initialInput, const QString& contextId)
{
    if (!validateToolChain(chain)) {
        qWarning() << "[ToolCompositionFramework] Invalid tool chain:" << chain.chainId;
        return QString();
    }
    
    QString chainExecutionId = generateExecutionId();
    
    // Execute chain asynchronously
    QtConcurrent::run([this, chain, initialInput, contextId, chainExecutionId]() {
        processChainExecution(chain.chainId);
    });
    
    qInfo() << "[ToolCompositionFramework] Started chain execution:" << chainExecutionId;
    
    return chainExecutionId;
}

QString ToolCompositionFramework::createDynamicChain(const QStringList& toolIds, const QJsonObject& config)
{
    QString chainId = generateChainId();
    
    ToolChain chain;
    chain.chainId = chainId;
    chain.name = config["name"].toString("Dynamic Chain " + chainId);
    chain.description = config["description"].toString("Dynamically created tool chain");
    chain.toolSequence = toolIds;
    chain.createdAt = QDateTime::currentDateTime();
    chain.isTemplate = false;
    
    // Calculate compatibility score
    double compatibility = calculateChainCompatibility(toolIds);
    chain.successProbability = compatibility;
    
    // Store chain configuration
    QWriteLocker locker(&m_toolsLock);
    // m_activeChains[chainId] = chain; // In real implementation
    
    qInfo() << "[ToolCompositionFramework] Created dynamic chain:" << chainId 
            << "with" << toolIds.size() << "tools";
    
    return chainId;
}

double ToolCompositionFramework::calculateChainCompatibility(const QStringList& toolSequence) const
{
    if (toolSequence.size() < 2) return 1.0;
    
    double totalCompatibility = 0.0;
    int compatibilityChecks = 0;
    
    for (int i = 0; i < toolSequence.size() - 1; ++i) {
        const ComposableTool* currentTool = getTool(toolSequence[i]);
        const ComposableTool* nextTool = getTool(toolSequence[i + 1]);
        
        if (currentTool && nextTool) {
            if (currentTool->canChainWith(nextTool)) {
                totalCompatibility += 1.0;
            }
            compatibilityChecks++;
        }
    }
    
    return compatibilityChecks > 0 ? totalCompatibility / compatibilityChecks : 0.0;
}

QJsonObject ToolCompositionFramework::getPerformanceMetrics() const
{
    QJsonObject metrics;
    
    // Framework-level metrics
    metrics["uptime_ms"] = m_uptimeTimer.elapsed();
    metrics["total_registered_tools"] = static_cast<int>(m_tools.size());
    metrics["active_executions"] = m_runningExecutions.size();
    metrics["queued_executions"] = m_executionQueue.size();
    metrics["completed_executions"] = static_cast<int>(m_executionResults.size());
    
    // Performance statistics
    if (!m_executionResults.empty()) {
        qint64 totalExecutionTime = 0;
        int successfulExecutions = 0;
        
        QReadLocker locker(&m_resultsLock);
        for (const auto& pair : m_executionResults) {
            const ToolExecutionResult& result = pair.second;
            totalExecutionTime += result.executionTimeMs;
            if (result.success) successfulExecutions++;
        }
        
        metrics["average_execution_time_ms"] = double(totalExecutionTime) / m_executionResults.size();
        metrics["success_rate"] = double(successfulExecutions) / m_executionResults.size();
    }
    
    // Resource utilization
    metrics["max_concurrent_executions"] = m_maxConcurrentExecutions;
    metrics["available_threads"] = QThread::idealThreadCount();
    
    return metrics;
}

void ToolCompositionFramework::onToolExecutionCompleted(const QString& executionId, const ToolExecutionResult& result)
{
    // Store result
    {
        QWriteLocker locker(&m_resultsLock);
        m_executionResults[executionId] = result;
    }
    
    // Remove from running executions
    m_runningExecutions.remove(executionId);
    
    // Update metrics
    updatePerformanceMetrics();
    
    emit executionCompleted(executionId, result);
    qInfo() << "[ToolCompositionFramework] Tool execution completed:" << executionId;
}

void ToolCompositionFramework::onToolExecutionFailed(const QString& executionId, const QString& error)
{
    // Create failed result
    ToolExecutionResult result;
    result.executionId = executionId;
    result.success = false;
    result.errorMessage = error;
    result.timestamp = QDateTime::currentDateTime();
    
    {
        QWriteLocker locker(&m_resultsLock);
        m_executionResults[executionId] = result;
    }
    
    // Remove from running executions
    m_runningExecutions.remove(executionId);
    
    emit executionFailed(executionId, error);
    qWarning() << "[ToolCompositionFramework] Tool execution failed:" << executionId << error;
}

// Private helper methods
void ToolCompositionFramework::initializeComponents()
{
    m_errorAnalysis = new ErrorAnalysisSystem(this);
    m_tracer = new DistributedTracer(this);
}

void ToolCompositionFramework::setupTimers()
{
    m_executionProcessorTimer = new QTimer(this);
    connect(m_executionProcessorTimer, &QTimer::timeout,
            this, &ToolCompositionFramework::processExecutionQueue);
    m_executionProcessorTimer->start(100); // Process every 100ms
    
    m_metricsUpdateTimer = new QTimer(this);
    connect(m_metricsUpdateTimer, &QTimer::timeout,
            this, &ToolCompositionFramework::updatePerformanceMetrics);
    m_metricsUpdateTimer->start(5000); // Update every 5 seconds
    
    m_scheduledExecutionTimer = new QTimer(this);
    connect(m_scheduledExecutionTimer, &QTimer::timeout,
            this, &ToolCompositionFramework::processScheduledExecutions);
    m_scheduledExecutionTimer->start(1000); // Check every second
    
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout,
            this, &ToolCompositionFramework::cleanupCompletedExecutions);
    m_cleanupTimer->start(60000); // Cleanup every minute
}

void ToolCompositionFramework::connectSignals()
{
    // Connect to planning engine if available
    if (m_planningEngine) {
        // Custom connections would go here
    }
}

QString ToolCompositionFramework::generateExecutionId()
{
    return "exec_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString ToolCompositionFramework::generateContextId()
{
    return "ctx_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString ToolCompositionFramework::generateChainId()
{
    return "chain_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

void ToolCompositionFramework::processExecutionQueue()
{
    QMutexLocker locker(&m_executionMutex);
    
    // Process queued executions up to the concurrency limit
    while (m_runningExecutions.size() < m_maxConcurrentExecutions && !m_executionQueue.isEmpty()) {
        QString executionId = m_executionQueue.dequeue();
        m_runningExecutions.insert(executionId);
        
        // Process execution
        processToolExecution(executionId);
    }
}

void ToolCompositionFramework::processToolExecution(const QString& executionId)
{
    // In real implementation, retrieve execution parameters and execute tool
    qDebug() << "[ToolCompositionFramework] Processing tool execution:" << executionId;
    
    // Simulate execution for now
    QTimer::singleShot(QRandomGenerator::global()->bounded(1000, 3000), this, [this, executionId]() {
        ToolExecutionResult result;
        result.executionId = executionId;
        result.success = QRandomGenerator::global()->bounded(100) > 10; // 90% success rate
        result.executionTimeMs = QRandomGenerator::global()->bounded(100, 2000);
        result.timestamp = QDateTime::currentDateTime();
        
        if (result.success) {
            onToolExecutionCompleted(executionId, result);
        } else {
            onToolExecutionFailed(executionId, "Simulated execution failure");
        }
    });
}

void ToolCompositionFramework::updatePerformanceMetrics()
{
    m_performanceMetrics = getPerformanceMetrics();
    emit performanceMetricsUpdated(m_performanceMetrics);
}

void ToolCompositionFramework::processScheduledExecutions()
{
    QDateTime now = QDateTime::currentDateTime();
    
    // Check for scheduled executions that are ready
    QStringList readyExecutions;
    for (auto it = m_executionSchedule.begin(); it != m_executionSchedule.end(); ++it) {
        if (it->second <= now) {
            readyExecutions.append(it->first);
        }
    }
    
    // Move ready executions to the main queue
    for (const QString& executionId : readyExecutions) {
        m_executionQueue.enqueue(executionId);
        m_executionSchedule.erase(executionId);
    }
}

void ToolCompositionFramework::cleanupCompletedExecutions()
{
    // Remove old execution results to prevent memory leaks
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600); // Keep for 1 hour
    
    QWriteLocker locker(&m_resultsLock);
    for (auto it = m_executionResults.begin(); it != m_executionResults.end();) {
        if (it->second.timestamp < cutoff) {
            it = m_executionResults.erase(it);
        } else {
            ++it;
        }
    }
}

bool ToolCompositionFramework::validateToolChain(const ToolChain& chain) const
{
    if (chain.toolSequence.isEmpty()) {
        return false;
    }
    
    // Check if all tools exist
    for (const QString& toolId : chain.toolSequence) {
        if (!getTool(toolId)) {
            qWarning() << "[ToolCompositionFramework] Tool not found in chain:" << toolId;
            return false;
        }
    }
    
    // Check chain compatibility
    double compatibility = calculateChainCompatibility(chain.toolSequence);
    return compatibility > 0.5; // Require at least 50% compatibility
}

void ToolCompositionFramework::processChainExecution(const QString& chainId)
{
    qDebug() << "[ToolCompositionFramework] Processing chain execution:" << chainId;
    
    // Simulate chain execution
    QTimer::singleShot(5000, this, [this, chainId]() {
        QJsonObject results;
        results["chain_id"] = chainId;
        results["success"] = true;
        results["execution_time_ms"] = 5000;
        results["completed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        emit chainCompleted(chainId, results);
    });
}

void ToolCompositionFramework::recordExecutionData(const QString& executionId, const ToolExecutionResult& result)
{
    QJsonObject data;
    data["execution_id"] = executionId;
    data["tool_id"] = result.toolId;
    data["success"] = result.success;
    data["execution_time_ms"] = result.executionTimeMs;
    data["timestamp"] = result.timestamp.toString(Qt::ISODate);
    
    if (!result.errorMessage.isEmpty()) {
        data["error_message"] = result.errorMessage;
    }
    
    // Store in execution history
    QString key = "execution_history_" + result.toolId;
    QJsonArray history;
    if (m_executionHistory.contains(key)) {
        history = m_executionHistory[key].toArray();
    }
    
    history.append(data);
    
    // Keep only last 1000 entries per tool
    while (history.size() > 1000) {
        history.removeFirst();
    }
    
    m_executionHistory[key] = history;
}

void ToolCompositionFramework::optimizePerformance()
{
    qInfo() << "[ToolCompositionFramework] Starting performance optimization";
    
    // Analyze tool usage patterns
    QReadLocker locker(&m_toolsLock);
    
    // Calculate average execution times per tool
    for (const auto& [id, tool] : m_tools) {
        QString key = "execution_history_" + id;
        if (m_executionHistory.contains(key)) {
            QJsonArray history = m_executionHistory[key].toArray();
            
            if (history.isEmpty()) continue;
            
            qint64 totalTime = 0;
            int successCount = 0;
            
            for (const QJsonValue& entry : history) {
                QJsonObject exec = entry.toObject();
                totalTime += exec["execution_time_ms"].toVariant().toLongLong();
                if (exec["success"].toBool()) successCount++;
            }
            
            double avgTime = static_cast<double>(totalTime) / history.size();
            double successRate = static_cast<double>(successCount) / history.size();
            
            qDebug() << "[ToolCompositionFramework] Tool" << id 
                     << "avg time:" << avgTime << "ms, success rate:" << successRate * 100 << "%";
        }
    }
    
    
    // Emit optimization suggestions via chainOptimizationSuggested signal
    emit chainOptimizationSuggested("performance_analysis", QStringList{"Analysis completed"});
}

QVariantMap ComposableTool::preprocessChainedInput(const QVariantMap& input,
                                                    const ComposableTool* previousTool) const
{
    // Default implementation: pass input through unchanged
    Q_UNUSED(previousTool);
    return input;
}

void ToolCompositionFramework::loadConfiguration(const QJsonObject& config)
{
    qDebug() << "[ToolCompositionFramework] Loading configuration";
    
    // Load tool chain templates
    if (config.contains("chainTemplates")) {
        QJsonArray templates = config["chainTemplates"].toArray();
        for (const QJsonValue& tmpl : templates) {
            QJsonObject tmplObj = tmpl.toObject();
            QString chainId = tmplObj["chainId"].toString();
            // Store template configuration
            qDebug() << "[ToolCompositionFramework] Loaded chain template:" << chainId;
        }
    }
    
    // Load performance thresholds
    if (config.contains("performanceThresholds")) {
        QJsonObject thresholds = config["performanceThresholds"].toObject();
        qDebug() << "[ToolCompositionFramework] Loaded performance thresholds";
    }
}

