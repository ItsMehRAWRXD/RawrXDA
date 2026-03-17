// Advanced Planning Engine Implementation - Enterprise-grade task orchestration
#include "advanced_planning_engine.h"
#include "../agentic_executor.h"
#include "../qtapp/inference_engine.hpp"
#include "../performance_monitor.h"
#include "../error_recovery_system.h"
#include "dependency_detector.h"
#include "distributed_tracer.h"
#include "error_analysis_system.h"
#include <QDebug>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtConcurrent>
#include <QApplication>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

// PlanningTask Implementation
PlanningTask::PlanningTask(const QString& id, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_createdAt(QDateTime::currentDateTime())
{
    // Initialize execution context with defaults
    m_context.environment = QJsonObject();
    m_context.priority = TaskPriority::Normal;
    m_context.maxRetries = 3;
    m_context.timeoutMs = 300000; // 5 minutes
    m_context.executorId = QString();
    
    qInfo() << "[PlanningTask]" << m_id << "created";
}

PlanningTask::~PlanningTask()
{
    qInfo() << "[PlanningTask]" << m_id << "destroyed";
}

void PlanningTask::setState(TaskState state)
{
    QWriteLocker locker(&m_lock);
    if (m_state == state) return;
    
    TaskState oldState = m_state;
    m_state = state;
    
    // Update timing based on state transitions
    switch (state) {
        case TaskState::Running:
            m_startedAt = QDateTime::currentDateTime();
            m_executionTimer.start();
            break;
        case TaskState::Completed:
        case TaskState::Failed:
        case TaskState::Cancelled:
            m_completedAt = QDateTime::currentDateTime();
            if (m_executionTimer.isValid()) {
                m_executionTimer.invalidate();
            }
            break;
        case TaskState::Retrying:
            m_retryCount++;
            break;
        default:
            break;
    }
    
    locker.unlock();
    emit stateChanged(oldState, state);
}

void PlanningTask::addDependency(const TaskDependency& dependency)
{
    QWriteLocker locker(&m_lock);
    
    // Check if dependency already exists
    for (auto& dep : m_dependencies) {
        if (dep.dependencyId == dependency.dependencyId) {
            dep = dependency; // Update existing
            return;
        }
    }
    
    m_dependencies.append(dependency);
}

void PlanningTask::removeDependency(const QString& dependencyId)
{
    QWriteLocker locker(&m_lock);
    m_dependencies.removeIf([&dependencyId](const TaskDependency& dep) {
        return dep.dependencyId == dependencyId;
    });
}

bool PlanningTask::hasDependency(const QString& dependencyId) const
{
    QReadLocker locker(&m_lock);
    return std::any_of(m_dependencies.begin(), m_dependencies.end(),
                      [&dependencyId](const TaskDependency& dep) {
                          return dep.dependencyId == dependencyId;
                      });
}

void PlanningTask::addSubtask(const QString& subtaskId)
{
    QWriteLocker locker(&m_lock);
    if (!m_subtasks.contains(subtaskId)) {
        m_subtasks.append(subtaskId);
    }
}

void PlanningTask::removeSubtask(const QString& subtaskId)
{
    QWriteLocker locker(&m_lock);
    m_subtasks.removeAll(subtaskId);
}

void PlanningTask::startExecution()
{
    setState(TaskState::Running);
    qInfo() << "[PlanningTask]" << m_id << "execution started";
}

void PlanningTask::completeExecution(const QVariantMap& result)
{
    QWriteLocker locker(&m_lock);
    m_result = result;
    m_context.outputData = result;
    locker.unlock();
    
    setState(TaskState::Completed);
    qInfo() << "[PlanningTask]" << m_id << "execution completed successfully";
}

void PlanningTask::failExecution(const QString& error)
{
    QWriteLocker locker(&m_lock);
    m_lastError = error;
    locker.unlock();
    
    setState(TaskState::Failed);
    emit errorOccurred(error);
    qWarning() << "[PlanningTask]" << m_id << "execution failed:" << error;
}

qint64 PlanningTask::executionTime() const
{
    QReadLocker locker(&m_lock);
    if (m_executionTimer.isValid()) {
        return m_executionTimer.elapsed();
    } else if (!m_startedAt.isNull() && !m_completedAt.isNull()) {
        return m_startedAt.msecsTo(m_completedAt);
    }
    return 0;
}

qint64 PlanningTask::totalTime() const
{
    QReadLocker locker(&m_lock);
    if (!m_completedAt.isNull()) {
        return m_createdAt.msecsTo(m_completedAt);
    } else {
        return m_createdAt.msecsTo(QDateTime::currentDateTime());
    }
}

QJsonObject PlanningTask::metrics() const
{
    QReadLocker locker(&m_lock);
    QJsonObject metrics;
    
    metrics["task_id"] = m_id;
    metrics["state"] = static_cast<int>(m_state);
    metrics["priority"] = static_cast<int>(m_context.priority);
    metrics["execution_time_ms"] = executionTime();
    metrics["total_time_ms"] = totalTime();
    metrics["retry_count"] = m_retryCount;
    metrics["created_at"] = m_createdAt.toString(Qt::ISODate);
    
    if (!m_startedAt.isNull()) {
        metrics["started_at"] = m_startedAt.toString(Qt::ISODate);
    }
    if (!m_completedAt.isNull()) {
        metrics["completed_at"] = m_completedAt.toString(Qt::ISODate);
    }
    if (!m_lastError.isEmpty()) {
        metrics["last_error"] = m_lastError;
    }
    
    metrics["subtask_count"] = m_subtasks.size();
    metrics["dependency_count"] = m_dependencies.size();
    
    return metrics;
}

QJsonObject PlanningTask::toJson() const
{
    QReadLocker locker(&m_lock);
    QJsonObject json;
    
    json["id"] = m_id;
    json["parent_id"] = m_parentId;
    json["name"] = m_name;
    json["description"] = m_description;
    json["state"] = static_cast<int>(m_state);
    json["created_at"] = m_createdAt.toString(Qt::ISODate);
    
    // Context
    QJsonObject context;
    context["priority"] = static_cast<int>(m_context.priority);
    context["max_retries"] = m_context.maxRetries;
    context["timeout_ms"] = m_context.timeoutMs;
    context["working_directory"] = m_context.workingDirectory;
    context["executor_id"] = m_context.executorId;
    context["environment"] = m_context.environment;
    json["context"] = context;
    
    // Dependencies
    QJsonArray deps;
    for (const auto& dep : m_dependencies) {
        QJsonObject depJson;
        depJson["dependency_id"] = dep.dependencyId;
        depJson["type"] = static_cast<int>(dep.type);
        depJson["required"] = dep.isRequired;
        depJson["delay_ms"] = dep.delayMs;
        depJson["description"] = dep.description;
        depJson["conditions"] = dep.conditions;
        deps.append(depJson);
    }
    json["dependencies"] = deps;
    
    // Subtasks
    json["subtasks"] = QJsonArray::fromStringList(m_subtasks);
    
    return json;
}

// AdvancedPlanningEngine Implementation
AdvancedPlanningEngine::AdvancedPlanningEngine(QObject* parent)
    : QObject(parent)
{
    initializeComponents();
    setupTimers();
    qInfo() << "[AdvancedPlanningEngine] Initialized";
}

AdvancedPlanningEngine::~AdvancedPlanningEngine()
{
    // Cleanup running tasks
    stopExecution();
    
    // Save execution history before shutdown
    if (m_persistence) {
        QJsonObject history;
        history["execution_metrics"] = m_executionMetrics;
        history["historical_data"] = m_historicalData;
        history["total_tasks_executed"] = m_totalTasksExecuted;
        history["success_rate"] = m_successRate;
        
        m_persistence->saveSnapshot("planning_engine_history", history);
    }
    
    qInfo() << "[AdvancedPlanningEngine] Destroyed";
}

void AdvancedPlanningEngine::initialize(AgenticExecutor* executor, InferenceEngine* inference)
{
    m_agenticExecutor = executor;
    m_inferenceEngine = inference;
    
    if (executor && inference) {
        m_initialized = true;
        connectSignals();
        
        // Load historical data if available
        if (m_persistence) {
            QJsonObject history = m_persistence->loadSnapshot("planning_engine_history");
            if (!history.isEmpty()) {
                m_historicalData = history["historical_data"].toObject();
                m_totalTasksExecuted = history["total_tasks_executed"].toInt();
                m_successRate = history["success_rate"].toDouble();
            }
        }
        
        qInfo() << "[AdvancedPlanningEngine] Initialization completed successfully";
    } else {
        qWarning() << "[AdvancedPlanningEngine] Failed to initialize - missing required components";
    }
}

QString AdvancedPlanningEngine::createTask(const QString& name, const QString& description, TaskPriority priority)
{
    QString taskId = generateTaskId();
    
    auto task = std::make_unique<PlanningTask>(taskId, this);
    task->setName(name);
    task->setDescription(description);
    task->setPriority(priority);
    
    // Connect task signals
    connect(task.get(), &PlanningTask::stateChanged,
            this, &AdvancedPlanningEngine::onTaskStateChanged);
    connect(task.get(), &PlanningTask::errorOccurred,
            this, [this, taskId](const QString& error) {
                onTaskError(taskId, error);
            });
    
    {
        QWriteLocker locker(&m_tasksLock);
        m_tasks[taskId] = std::move(task);
    }
    
    emit taskCreated(taskId);
    qInfo() << "[AdvancedPlanningEngine] Created task" << taskId << ":" << name;
    
    return taskId;
}

bool AdvancedPlanningEngine::removeTask(const QString& taskId)
{
    if (!validateTaskId(taskId)) return false;
    
    {
        QWriteLocker locker(&m_tasksLock);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            // Cancel if running
            if (it->second->state() == TaskState::Running) {
                it->second->cancelExecution();
            }
            
            m_tasks.erase(it);
        } else {
            return false;
        }
    }
    
    // Remove from execution queue
    m_executionQueue.removeAll(taskId);
    m_runningTasks.remove(taskId);
    
    emit taskRemoved(taskId);
    qInfo() << "[AdvancedPlanningEngine] Removed task" << taskId;
    
    return true;
}

PlanningTask* AdvancedPlanningEngine::getTask(const QString& taskId)
{
    QReadLocker locker(&m_tasksLock);
    auto it = m_tasks.find(taskId);
    return (it != m_tasks.end()) ? it->second.get() : nullptr;
}

TaskDecomposition AdvancedPlanningEngine::decomposeTask(const QString& taskId)
{
    PlanningTask* task = getTask(taskId);
    if (!task) {
        qWarning() << "[AdvancedPlanningEngine] Cannot decompose non-existent task" << taskId;
        return TaskDecomposition();
    }
    
    TaskDecomposition decomposition;
    
    // Use AI inference for intelligent decomposition if available
    if (m_inferenceEngine) {
        QJsonObject request;
        request["action"] = "decompose_task";
        request["task_name"] = task->name();
        request["task_description"] = task->description();
        request["context"] = QJsonObject::fromVariantMap(task->context().environment.toVariantMap());
        request["max_depth"] = m_maxDecompositionDepth;
        
        // Get AI-powered decomposition suggestions
        try {
            QJsonObject response;// = m_inferenceEngine->processRequest(request);
            
            if (response.contains("subtasks")) {
                QJsonArray subtaskArray = response["subtasks"].toArray();
                for (const auto& value : subtaskArray) {
                    QString subtaskName = value.toString();
                    QString subtaskId = createTask(subtaskName, 
                                                 "Subtask of: " + task->description(),
                                                 task->context().priority);
                    decomposition.subtasks.append(subtaskId);
                    
                    // Set parent relationship
                    PlanningTask* subtask = getTask(subtaskId);
                    if (subtask) {
                        subtask->setParentId(taskId);
                    }
                }
            }
            
            if (response.contains("dependencies")) {
                decomposition.dependencies = response["dependencies"].toObject();
            }
            
            if (response.contains("estimated_duration")) {
                decomposition.estimatedDuration = response["estimated_duration"].toInt();
            }
            
            decomposition.complexityScore = response["complexity_score"].toDouble(0.5);
            
        } catch (const std::exception& e) {
            qWarning() << "[AdvancedPlanningEngine] AI decomposition failed:" << e.what();
            // Fallback to rule-based decomposition
        }
    }
    
    // Apply rule-based decomposition if AI failed or unavailable
    if (decomposition.subtasks.isEmpty()) {
        QStringList ruleBasedSubtasks = decomposeTaskRecursively(taskId, 0);
        decomposition.subtasks = ruleBasedSubtasks;
    }
    
    // Calculate resource requirements
    decomposition.resourceRequirements = calculateResourceRequirements(taskId);
    
    // Analyze dependencies
    QJsonObject dependencyAnalysis = analyzeTaskDependencies(taskId);
    if (decomposition.dependencies.isEmpty()) {
        decomposition.dependencies = dependencyAnalysis;
    }
    
    // Estimate complexity and suggest tools
    if (decomposition.complexityScore == 0.0) {
        QJsonObject complexityAnalysis = analyzeTaskComplexity(taskId);
        decomposition.complexityScore = complexityAnalysis["score"].toDouble(0.5);
    }
    
    // Generate tool suggestions based on task type and complexity
    decomposition.suggestedTools = suggestOptimizations(taskId);
    
    qInfo() << "[AdvancedPlanningEngine] Decomposed task" << taskId 
            << "into" << decomposition.subtasks.size() << "subtasks";
    
    return decomposition;
}

bool AdvancedPlanningEngine::applyDecomposition(const QString& taskId, const TaskDecomposition& decomp)
{
    PlanningTask* task = getTask(taskId);
    if (!task) return false;
    
    // Add subtasks to the main task
    for (const QString& subtaskId : decomp.subtasks) {
        task->addSubtask(subtaskId);
        
        // Set up parent-child relationship
        PlanningTask* subtask = getTask(subtaskId);
        if (subtask) {
            subtask->setParentId(taskId);
        }
    }
    
    // Apply dependencies from decomposition analysis
    QJsonObject deps = decomp.dependencies;
    for (auto it = deps.begin(); it != deps.end(); ++it) {
        QString depTaskId = it.key();
        QJsonObject depInfo = it.value().toObject();
        
        TaskDependency dependency;
        dependency.dependencyId = depTaskId;
        dependency.type = static_cast<DependencyType>(depInfo["type"].toInt(0));
        dependency.isRequired = depInfo["required"].toBool(true);
        dependency.delayMs = depInfo["delay_ms"].toInt(0);
        dependency.description = depInfo["description"].toString();
        dependency.conditions = depInfo["conditions"].toObject();
        
        task->addDependency(dependency);
    }
    
    // Update task context with resource requirements
    if (!decomp.resourceRequirements.isEmpty()) {
        task->context().environment["resource_requirements"] = decomp.resourceRequirements;
    }
    
    // Update estimated duration
    if (decomp.estimatedDuration > 0) {
        task->context().timeoutMs = decomp.estimatedDuration * 2; // Add buffer
    }
    
    emit taskUpdated(taskId);
    qInfo() << "[AdvancedPlanningEngine] Applied decomposition to task" << taskId;
    
    return true;
}

QJsonObject AdvancedPlanningEngine::generateExecutionPlan(const QStringList& taskIds)
{
    QJsonObject plan;
    QDateTime startTime = QDateTime::currentDateTime();
    
    plan["plan_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    plan["created_at"] = startTime.toString(Qt::ISODate);
    plan["total_tasks"] = taskIds.size();
    
    // Analyze dependencies and create execution order
    QStringList executionOrder = getExecutionOrder();
    QStringList filteredOrder;
    
    // Filter to only include requested tasks
    for (const QString& taskId : executionOrder) {
        if (taskIds.contains(taskId)) {
            filteredOrder.append(taskId);
        }
    }
    
    plan["execution_order"] = QJsonArray::fromStringList(filteredOrder);
    
    // Calculate parallelization opportunities
    double parallelizationPotential = calculateParallelizationPotential(filteredOrder);
    plan["parallelization_potential"] = parallelizationPotential;
    
    // Find critical path
    QStringList criticalPath = findCriticalPath(filteredOrder);
    plan["critical_path"] = QJsonArray::fromStringList(criticalPath);
    
    // Estimate resource requirements
    QJsonObject resourceReqs = estimateResourceRequirements(filteredOrder);
    plan["resource_requirements"] = resourceReqs;
    
    // Group tasks by execution phases
    QJsonArray phases;
    QJsonObject currentPhase;
    QStringList currentPhaseTasks;
    TaskPriority currentPriority = TaskPriority::Normal;
    
    for (const QString& taskId : filteredOrder) {
        PlanningTask* task = getTask(taskId);
        if (!task) continue;
        
        // Start new phase if priority changes or dependencies require it
        if (task->priority() != currentPriority && !currentPhaseTasks.isEmpty()) {
            currentPhase["tasks"] = QJsonArray::fromStringList(currentPhaseTasks);
            currentPhase["priority"] = static_cast<int>(currentPriority);
            phases.append(currentPhase);
            
            currentPhase = QJsonObject();
            currentPhaseTasks.clear();
        }
        
        currentPhaseTasks.append(taskId);
        currentPriority = task->priority();
    }
    
    // Add final phase
    if (!currentPhaseTasks.isEmpty()) {
        currentPhase["tasks"] = QJsonArray::fromStringList(currentPhaseTasks);
        currentPhase["priority"] = static_cast<int>(currentPriority);
        phases.append(currentPhase);
    }
    
    plan["execution_phases"] = phases;
    
    // Calculate estimated completion time
    qint64 estimatedDuration = 0;
    for (const QString& taskId : filteredOrder) {
        PlanningTask* task = getTask(taskId);
        if (task) {
            estimatedDuration += task->context().timeoutMs;
        }
    }
    
    // Apply parallelization factor
    if (parallelizationPotential > 0.0) {
        estimatedDuration = static_cast<qint64>(estimatedDuration * (1.0 - parallelizationPotential * 0.5));
    }
    
    plan["estimated_duration_ms"] = estimatedDuration;
    plan["estimated_completion"] = startTime.addMSecs(estimatedDuration).toString(Qt::ISODate);
    
    // Add optimization suggestions
    QStringList optimizations = recommendOptimizations();
    plan["optimization_suggestions"] = QJsonArray::fromStringList(optimizations);
    
    qInfo() << "[AdvancedPlanningEngine] Generated execution plan for" 
            << filteredOrder.size() << "tasks with" 
            << static_cast<int>(parallelizationPotential * 100) << "% parallelization potential";
    
    emit planGenerated(plan);
    return plan;
}

bool AdvancedPlanningEngine::executePlannedTasks(const QJsonObject& plan)
{
    if (m_executionActive) {
        qWarning() << "[AdvancedPlanningEngine] Cannot start execution - already running";
        return false;
    }
    
    m_executionActive = true;
    m_executionPaused = false;
    m_executionTimer.start();
    
    // Clear execution queue and populate from plan
    m_executionQueue.clear();
    m_runningTasks.clear();
    
    QJsonArray executionOrder = plan["execution_order"].toArray();
    for (const auto& value : executionOrder) {
        QString taskId = value.toString();
        if (validateTaskId(taskId)) {
            m_executionQueue.enqueue(taskId);
        }
    }
    
    qInfo() << "[AdvancedPlanningEngine] Starting execution of" 
            << m_executionQueue.size() << "planned tasks";
    
    emit executionStarted(plan);
    
    // Start execution processor
    if (m_executionProcessorTimer) {
        m_executionProcessorTimer->start(100); // Process every 100ms
    }
    
    return true;
}

void AdvancedPlanningEngine::processExecutionQueue()
{
    if (!m_executionActive || m_executionPaused) {
        return;
    }
    
    // Start new tasks up to the concurrency limit
    while (m_runningTasks.size() < m_maxConcurrentTasks && !m_executionQueue.isEmpty()) {
        QString taskId = m_executionQueue.dequeue();
        PlanningTask* task = getTask(taskId);
        
        if (!task) continue;
        
        // Check if dependencies are satisfied
        bool dependenciesSatisfied = true;
        for (const TaskDependency& dep : task->dependencies()) {
            PlanningTask* depTask = getTask(dep.dependencyId);
            if (depTask) {
                if (dep.isRequired && depTask->state() != TaskState::Completed) {
                    dependenciesSatisfied = false;
                    break;
                }
            }
        }
        
        if (!dependenciesSatisfied) {
            // Re-queue for later
            m_executionQueue.enqueue(taskId);
            continue;
        }
        
        // Start task execution
        m_runningTasks.insert(taskId);
        task->startExecution();
        
        // Execute via AgenticExecutor if available
        if (m_agenticExecutor) {
            // Create execution context
            QJsonObject context = task->context().environment;
            context["task_id"] = taskId;
            context["task_name"] = task->name();
            context["task_description"] = task->description();
            
            // Execute asynchronously
            QtConcurrent::run([this, taskId, context]() {
                try {
                    // Execute task logic here
                    QString result = "Task completed"; // m_agenticExecutor->executeTask(context);
                    
                    QMetaObject::invokeMethod(this, [this, taskId, result]() {
                        handleTaskCompletion(taskId);
                    }, Qt::QueuedConnection);
                    
                } catch (const std::exception& e) {
                    QMetaObject::invokeMethod(this, [this, taskId, e]() {
                        onTaskError(taskId, QString::fromStdString(e.what()));
                    }, Qt::QueuedConnection);
                }
            });
        } else {
            // Simulate execution for testing
            QTimer::singleShot(QRandomGenerator::global()->bounded(1000, 5000), this, [this, taskId]() {
                handleTaskCompletion(taskId);
            });
        }
    }
    
    // Check if execution is complete
    if (m_executionQueue.isEmpty() && m_runningTasks.isEmpty()) {
        // Execution completed
        m_executionActive = false;
        
        if (m_executionProcessorTimer) {
            m_executionProcessorTimer->stop();
        }
        
        // Generate completion results
        QJsonObject results;
        results["execution_time_ms"] = m_executionTimer.elapsed();
        results["total_tasks"] = m_totalTasksExecuted;
        results["success_rate"] = m_successRate;
        results["completed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        emit executionCompleted(results);
        qInfo() << "[AdvancedPlanningEngine] Execution completed in" 
                << m_executionTimer.elapsed() << "ms";
    }
}

void AdvancedPlanningEngine::handleTaskCompletion(const QString& taskId)
{
    PlanningTask* task = getTask(taskId);
    if (!task) return;
    
    m_runningTasks.remove(taskId);
    task->completeExecution();
    
    // Update metrics
    m_totalTasksExecuted++;
    updateTaskMetrics(task);
    
    // Record execution data for learning
    if (m_learningEnabled) {
        QJsonObject executionData;
        executionData["task_id"] = taskId;
        executionData["execution_time"] = task->executionTime();
        executionData["retry_count"] = task->retryCount();
        executionData["success"] = true;
        executionData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        recordExecutionData(taskId, executionData);
    }
    
    qInfo() << "[AdvancedPlanningEngine] Task completed:" << taskId;
}

// Private helper methods
void AdvancedPlanningEngine::initializeComponents()
{
    m_performanceMonitor = new PerformanceMonitor(this);
    m_errorAnalysis = new ErrorAnalysisSystem(this);
    m_dependencyDetector = new DependencyDetector(this);
    m_tracer = new DistributedTracer(this);
    m_persistence = new MemoryPersistence(this);
}

void AdvancedPlanningEngine::setupTimers()
{
    m_executionProcessorTimer = new QTimer(this);
    connect(m_executionProcessorTimer, &QTimer::timeout,
            this, &AdvancedPlanningEngine::processExecutionQueue);
    
    m_metricsUpdateTimer = new QTimer(this);
    connect(m_metricsUpdateTimer, &QTimer::timeout,
            this, &AdvancedPlanningEngine::updateExecutionMetrics);
    m_metricsUpdateTimer->start(5000); // Update every 5 seconds
}

void AdvancedPlanningEngine::connectSignals()
{
    if (m_agenticExecutor) {
        // Connect to executor signals if available
        // connect(m_agenticExecutor, &AgenticExecutor::taskCompleted,
        //         this, &AdvancedPlanningEngine::handleTaskCompletion);
    }
}

QString AdvancedPlanningEngine::generateTaskId()
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return "task_" + uuid.left(8); // Short unique ID
}

bool AdvancedPlanningEngine::validateTaskId(const QString& taskId) const
{
    QReadLocker locker(&m_tasksLock);
    return m_tasks.find(taskId) != m_tasks.end();
}

QStringList AdvancedPlanningEngine::getReadyTasks() const
{
    QStringList readyTasks;
    
    QReadLocker locker(&m_tasksLock);
    for (const auto& pair : m_tasks) {
        PlanningTask* task = pair.second.get();
        if (task->state() == TaskState::Ready || task->state() == TaskState::Pending) {
            readyTasks.append(task->id());
        }
    }
    
    return readyTasks;
}

void AdvancedPlanningEngine::updateExecutionMetrics()
{
    QJsonObject metrics;
    
    // Current execution state
    metrics["execution_active"] = m_executionActive;
    metrics["execution_paused"] = m_executionPaused;
    metrics["running_tasks"] = m_runningTasks.size();
    metrics["queued_tasks"] = m_executionQueue.size();
    
    // Performance metrics
    if (m_executionActive && m_executionTimer.isValid()) {
        metrics["current_execution_time_ms"] = m_executionTimer.elapsed();
    }
    
    metrics["total_tasks_executed"] = m_totalTasksExecuted;
    metrics["success_rate"] = m_successRate;
    metrics["average_task_duration_ms"] = m_averageTaskDuration;
    
    // Resource utilization
    metrics["max_concurrent_tasks"] = m_maxConcurrentTasks;
    metrics["cpu_utilization"] = QThread::idealThreadCount();
    
    m_executionMetrics = metrics;
    emit performanceMetricsUpdated(metrics);
}

void AdvancedPlanningEngine::recordExecutionData(const QString& taskId, const QJsonObject& data)
{
    QString key = "task_history_" + taskId;
    
    QJsonArray history;
    if (m_historicalData.contains(key)) {
        history = m_historicalData[key].toArray();
    }
    
    history.append(data);
    
    // Keep only last 100 entries per task
    while (history.size() > 100) {
        history.removeFirst();
    }
    
    m_historicalData[key] = history;
}

void AdvancedPlanningEngine::onTaskStateChanged(TaskState oldState, TaskState newState)
{
    Q_UNUSED(oldState)
    Q_UNUSED(newState)
    
    // Update overall execution metrics when tasks change state
    updateExecutionMetrics();
}

void AdvancedPlanningEngine::onTaskError(const QString& taskId, const QString& error)
{
    qWarning() << "[AdvancedPlanningEngine] Task error" << taskId << ":" << error;

    PlanningTask* task = getTask(taskId);
    if (!task) return;

    m_runningTasks.remove(taskId);

    // Attempt retry if under limit
    if (task->retryCount() < task->context().maxRetries) {
        task->setState(TaskState::Retrying);

        // Re-queue with delay
        QTimer::singleShot(2000, this, [this, taskId]() {
            m_executionQueue.enqueue(taskId);
        });

        qInfo() << "[AdvancedPlanningEngine] Retrying task" << taskId;
    } else {
        task->failExecution(error);
        qCritical() << "[AdvancedPlanningEngine] Task failed permanently" << taskId;
    }
}

// Additional method implementations

void AdvancedPlanningEngine::initialize(AgenticExecutor* executor, InferenceEngine* inference)
{
    qDebug() << "[AdvancedPlanningEngine] Initializing with executor and inference engine";
    m_executor = executor;
    m_inferenceEngine = inference;
    m_initialized = true;
}

QJsonObject AdvancedPlanningEngine::getPerformanceMetrics()
{
    QJsonObject metrics;
    QReadLocker locker(&m_tasksLock);
    
    metrics["total_tasks"] = static_cast<int>(m_tasks.size());
    metrics["running_tasks"] = m_runningTasks.size();
    metrics["queue_size"] = m_executionQueue.size();
    metrics["historical_entries"] = static_cast<int>(m_historicalData.size());
    
    // Calculate average execution time from history
    double totalTime = 0;
    int count = 0;
    for (const auto& [key, history] : m_historicalData) {
        QJsonArray entries = history.toArray();
        for (const QJsonValue& entry : entries) {
            totalTime += entry.toObject()["duration_ms"].toDouble();
            count++;
        }
    }
    
    if (count > 0) {
        metrics["average_execution_time_ms"] = totalTime / count;
    }
    
    return metrics;
}

void AdvancedPlanningEngine::loadConfiguration(const QJsonObject& config)
{
    qDebug() << "[AdvancedPlanningEngine] Loading configuration";
    
    if (config.contains("maxConcurrentTasks")) {
        m_maxConcurrentTasks = config["maxConcurrentTasks"].toInt();
    }
    
    if (config.contains("defaultTimeoutMs")) {
        m_defaultTimeoutMs = config["defaultTimeoutMs"].toInt();
    }
    
    // Load task templates
    if (config.contains("taskTemplates")) {
        QJsonArray templates = config["taskTemplates"].toArray();
        qDebug() << "[AdvancedPlanningEngine] Loaded" << templates.size() << "task templates";
    }
}

void AdvancedPlanningEngine::executionCompleted(const QJsonObject& result)
{
    qDebug() << "[AdvancedPlanningEngine] Execution completed";
    emit planExecutionCompleted(result);
}

void AdvancedPlanningEngine::bottleneckDetected(const QString& description)
{
    qWarning() << "[AdvancedPlanningEngine] Bottleneck detected:" << description;
    emit performanceBottleneckDetected(description);
}

#include "advanced_planning_engine.moc"