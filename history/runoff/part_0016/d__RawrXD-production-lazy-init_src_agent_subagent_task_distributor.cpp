#include "subagent_task_distributor.hpp"
#include "subagent_manager.hpp"
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <algorithm>

// ============================================================================
// SubagentTaskDistributor Implementation
// ============================================================================

SubagentTaskDistributor::SubagentTaskDistributor(std::shared_ptr<SubagentPool> pool, QObject* parent)
    : QObject(parent), m_pool(pool)
{
    if (!m_pool) {
        qCritical() << "[SubagentTaskDistributor] Initialized with null pool!";
    }
    
    qInfo() << "[SubagentTaskDistributor] Initialized";
}

SubagentTaskDistributor::~SubagentTaskDistributor()
{
    QMutexLocker locker(&m_mutex);
    m_tasks.clear();
    m_taskHierarchy.clear();
    m_taskDependencies.clear();
}

QString SubagentTaskDistributor::distributeTask(const DistributedTask& task)
{
    if (!m_pool) {
        qCritical() << "[SubagentTaskDistributor] Pool is null";
        return QString();
    }
    
    QMutexLocker locker(&m_mutex);
    
    auto taskCopy = task;
    m_tasks[task.taskId] = taskCopy;
    m_pendingTasks.append(task.taskId);
    m_taskStartTimes[task.taskId] = QDateTime::currentMSecsSinceEpoch();
    
    locker.unlock();
    
    // Convert to Subagent::Task
    Subagent::Task subagentTask;
    subagentTask.taskId = task.taskId;
    subagentTask.description = task.description;
    subagentTask.executor = task.executor;
    subagentTask.priority = static_cast<Subagent::TaskPriority>(task.priority);
    subagentTask.maxRetries = task.maxRetries;
    subagentTask.timeoutMs = task.timeoutMs;
    subagentTask.metadata = task.metadata;
    
    QString resultId = m_pool->submitTask(subagentTask);
    
    if (!resultId.isEmpty()) {
        emit taskDistributed(task.taskId);
        qInfo() << "[SubagentTaskDistributor] Task distributed:" << task.taskId;
    }
    
    return resultId;
}

QString SubagentTaskDistributor::distributeComplexTask(const QString& description,
                                                       const QList<DistributedTask>& subtasks)
{
    QMutexLocker locker(&m_mutex);
    
    QString parentId = description + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    m_taskHierarchy[parentId] = QList<QString>();
    
    locker.unlock();
    
    for (const auto& subtask : subtasks) {
        QMutexLocker innerLocker(&m_mutex);
        m_taskHierarchy[parentId].append(subtask.taskId);
        
        auto modifiedSubtask = subtask;
        modifiedSubtask.parentTaskId = parentId;
        innerLocker.unlock();
        
        distributeTask(modifiedSubtask);
    }
    
    qInfo() << "[SubagentTaskDistributor] Complex task distributed:" << parentId
            << "with" << subtasks.size() << "subtasks";
    
    return parentId;
}

bool SubagentTaskDistributor::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    auto& task = m_tasks[taskId];
    task.status = "cancelled";
    
    m_pendingTasks.removeAll(taskId);
    
    locker.unlock();
    
    if (m_pool) {
        return m_pool->cancelTask(taskId);
    }
    
    return false;
}

bool SubagentTaskDistributor::retryTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    auto& task = m_tasks[taskId];
    
    if (task.attemptCount >= task.maxRetries) {
        qWarning() << "[SubagentTaskDistributor] Max retries exceeded for task:" << taskId;
        return false;
    }
    
    task.attemptCount++;
    task.status = "retrying";
    task.error.clear();
    
    m_failedTasks.removeAll(taskId);
    m_pendingTasks.append(taskId);
    
    locker.unlock();
    
    return distributeTask(task).isEmpty() ? false : true;
}

bool SubagentTaskDistributor::getTaskStatus(const QString& taskId, QJsonObject& outStatus) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    const auto& task = m_tasks[taskId];
    
    outStatus["taskId"] = taskId;
    outStatus["status"] = task.status;
    outStatus["description"] = task.description;
    outStatus["progress"] = getTaskProgressPercent(taskId);
    outStatus["attemptCount"] = task.attemptCount;
    outStatus["maxRetries"] = task.maxRetries;
    
    if (!task.error.isEmpty()) {
        outStatus["error"] = task.error;
    }
    
    if (task.completed) {
        outStatus["result"] = task.result;
    }
    
    return true;
}

QString SubagentTaskDistributor::launchParallelTasks(const QList<DistributedTask>& tasks)
{
    QString groupId = "parallel_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    
    for (const auto& task : tasks) {
        auto modifiedTask = task;
        modifiedTask.parentTaskId = groupId;
        modifiedTask.dependencyMode = TaskDependency::NoDelay;
        distributeTask(modifiedTask);
    }
    
    qInfo() << "[SubagentTaskDistributor] Parallel task group launched:" << groupId
            << "with" << tasks.size() << "tasks";
    
    return groupId;
}

QString SubagentTaskDistributor::launchSequentialTasks(const QList<DistributedTask>& tasks)
{
    QString groupId = "sequential_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    
    for (int i = 0; i < tasks.size(); ++i) {
        auto modifiedTask = tasks[i];
        modifiedTask.parentTaskId = groupId;
        modifiedTask.dependencyMode = TaskDependency::Sequential;
        
        if (i > 0) {
            modifiedTask.dependsOnTasks.append(tasks[i-1].taskId);
        }
        
        distributeTask(modifiedTask);
    }
    
    qInfo() << "[SubagentTaskDistributor] Sequential task group launched:" << groupId
            << "with" << tasks.size() << "tasks";
    
    return groupId;
}

QString SubagentTaskDistributor::launchConditionalTasks(const DistributedTask& condition,
                                                        const DistributedTask& trueBranch,
                                                        const DistributedTask& falseBranch)
{
    QString groupId = "conditional_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    
    auto condTask = condition;
    condTask.parentTaskId = groupId;
    distributeTask(condTask);
    
    auto trueBranchTask = trueBranch;
    trueBranchTask.parentTaskId = groupId;
    trueBranchTask.dependsOnTasks.append(condition.taskId);
    
    auto falseBranchTask = falseBranch;
    falseBranchTask.parentTaskId = groupId;
    falseBranchTask.dependsOnTasks.append(condition.taskId);
    
    qInfo() << "[SubagentTaskDistributor] Conditional task group launched:" << groupId;
    
    return groupId;
}

QJsonObject SubagentTaskDistributor::getTaskResult(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_tasks.contains(taskId)) {
        return m_tasks[taskId].result;
    }
    
    return QJsonObject();
}

double SubagentTaskDistributor::getTaskProgressPercent(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return 0.0;
    }
    
    const auto& task = m_tasks[taskId];
    
    if (task.status == "pending") {
        return 0.0;
    } else if (task.status == "running") {
        return 50.0;
    } else if (task.status == "completed") {
        return 100.0;
    } else if (task.status == "failed") {
        return -1.0;  // Error state
    }
    
    return 50.0;
}

QJsonArray SubagentTaskDistributor::getAllTaskResults(const QString& parentTaskId) const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray results;
    
    if (!parentTaskId.isEmpty() && m_taskHierarchy.contains(parentTaskId)) {
        const auto& childIds = m_taskHierarchy[parentTaskId];
        for (const auto& childId : childIds) {
            if (m_tasks.contains(childId)) {
                results.append(m_tasks[childId].result);
            }
        }
    }
    
    return results;
}

QJsonObject SubagentTaskDistributor::getTaskHierarchy(const QString& rootTaskId) const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject hierarchy;
    
    if (m_taskHierarchy.contains(rootTaskId)) {
        hierarchy["taskId"] = rootTaskId;
        
        QJsonArray children;
        const auto& childIds = m_taskHierarchy[rootTaskId];
        for (const auto& childId : childIds) {
            QJsonObject child;
            child["taskId"] = childId;
            if (m_tasks.contains(childId)) {
                child["status"] = m_tasks[childId].status;
            }
            children.append(child);
        }
        
        hierarchy["children"] = children;
        hierarchy["childCount"] = childIds.size();
    }
    
    return hierarchy;
}

QJsonObject SubagentTaskDistributor::getDistributorMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics;
    metrics["totalTasks"] = m_tasks.size();
    metrics["pendingTasks"] = m_pendingTasks.size();
    metrics["completedTasks"] = m_completedTasks.size();
    metrics["failedTasks"] = m_failedTasks.size();
    metrics["taskHierarchies"] = m_taskHierarchy.size();
    
    return metrics;
}

void SubagentTaskDistributor::onSubagentTaskCompleted(const QString& taskId, const QJsonObject& result)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return;
    }
    
    auto& task = m_tasks[taskId];
    task.status = "completed";
    task.completed = true;
    task.result = result;
    task.error.clear();
    
    m_pendingTasks.removeAll(taskId);
    m_completedTasks.append(taskId);
    
    if (m_taskEndTimes.contains(taskId)) {
        qint64 duration = QDateTime::currentMSecsSinceEpoch() - m_taskStartTimes[taskId];
        qInfo() << "[SubagentTaskDistributor] Task completed:" << taskId << "in" << duration << "ms";
    }
    
    QString parentTaskId = task.parentTaskId;
    
    locker.unlock();
    
    emit taskCompleted(taskId, result);
    
    // Check if parent task can complete
    if (!parentTaskId.isEmpty()) {
        processPendingDependencies(taskId);
    }
}

void SubagentTaskDistributor::onSubagentTaskFailed(const QString& taskId, const QString& error)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return;
    }
    
    auto& task = m_tasks[taskId];
    task.status = "failed";
    task.error = error;
    
    m_pendingTasks.removeAll(taskId);
    m_failedTasks.append(taskId);
    
    qWarning() << "[SubagentTaskDistributor] Task failed:" << taskId << "-" << error;
    
    locker.unlock();
    
    emit taskFailed(taskId, error);
}

void SubagentTaskDistributor::processPendingDependencies(const QString& completedTaskId)
{
    QMutexLocker locker(&m_mutex);
    
    // Find all tasks that depend on this task
    QStringList dependentTasks;
    
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (it.value().dependsOnTasks.contains(completedTaskId)) {
            dependentTasks.append(it.key());
        }
    }
    
    locker.unlock();
    
    for (const auto& taskId : dependentTasks) {
        if (canExecuteTask(taskId)) {
            locker.relock();
            auto task = m_tasks[taskId];
            locker.unlock();
            
            distributeTask(task);
            emit dependencyResolved(completedTaskId, taskId);
        }
    }
}

bool SubagentTaskDistributor::canExecuteTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    const auto& task = m_tasks[taskId];
    
    // Check all dependencies
    for (const auto& depId : task.dependsOnTasks) {
        if (m_tasks.contains(depId)) {
            if (m_tasks[depId].status != "completed") {
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// MultitaskingCoordinator Implementation
// ============================================================================

MultitaskingCoordinator::MultitaskingCoordinator(const QString& sessionId, QObject* parent)
    : QObject(parent), m_sessionId(sessionId)
{
    // Create subagent pool for this session
    m_pool = SubagentManager::getInstance()->createPool(sessionId + "_pool", 5);
    
    // Create task distributor
    m_distributor = std::make_shared<SubagentTaskDistributor>(m_pool, this);
    
    // Connect signals
    if (m_distributor) {
        connect(m_distributor.get(), &SubagentTaskDistributor::taskCompleted,
                this, &MultitaskingCoordinator::onDistributorTaskCompleted);
        connect(m_distributor.get(), &SubagentTaskDistributor::taskFailed,
                this, &MultitaskingCoordinator::onDistributorTaskFailed);
    }
    
    if (m_pool) {
        connect(m_pool.get(), &SubagentPool::agentAdded,
                this, &MultitaskingCoordinator::onPoolAgentAdded);
        connect(m_pool.get(), &SubagentPool::agentRemoved,
                this, &MultitaskingCoordinator::onPoolAgentRemoved);
    }
    
    // Set up metrics collection timer
    m_metricsTimer = new QTimer(this);
    connect(m_metricsTimer, &QTimer::timeout, this, &MultitaskingCoordinator::collectResourceMetrics);
    m_metricsTimer->start(5000);  // Every 5 seconds
    
    qInfo() << "[MultitaskingCoordinator] Initialized for session:" << sessionId;
}

MultitaskingCoordinator::~MultitaskingCoordinator()
{
    if (m_pool) {
        SubagentManager::getInstance()->deletePool(m_sessionId + "_pool");
    }
}

bool MultitaskingCoordinator::initializeSubagents(int agentCount)
{
    if (!m_pool) {
        return false;
    }
    
    int clampedCount = std::max(1, std::min(agentCount, 20));
    
    // Add agents
    while (m_pool->getAgentCount() < clampedCount) {
        m_pool->addAgent();
    }
    
    m_pool->setAutoScaling(m_autoScalingEnabled, 1, m_maxSubagents);
    
    qInfo() << "[MultitaskingCoordinator] Session:" << m_sessionId
            << "initialized with" << clampedCount << "subagents";
    
    return true;
}

int MultitaskingCoordinator::getSubagentCount() const
{
    if (!m_pool) {
        return 0;
    }
    return m_pool->getAgentCount();
}

int MultitaskingCoordinator::getAvailableSubagentCount() const
{
    if (!m_pool) {
        return 0;
    }
    return m_pool->getIdleAgentCount();
}

bool MultitaskingCoordinator::addSubagent()
{
    if (!m_pool) {
        return false;
    }
    
    if (m_pool->getAgentCount() >= m_maxSubagents) {
        qWarning() << "[MultitaskingCoordinator] Max subagents reached for session:" << m_sessionId;
        return false;
    }
    
    return m_pool->addAgent();
}

bool MultitaskingCoordinator::removeSubagent()
{
    if (!m_pool) {
        return false;
    }
    
    if (m_pool->getAgentCount() <= 1) {
        qWarning() << "[MultitaskingCoordinator] Cannot remove last subagent";
        return false;
    }
    
    return m_pool->removeAgent();
}

void MultitaskingCoordinator::scaleSubagents(int targetCount)
{
    int clamped = std::max(1, std::min(targetCount, m_maxSubagents));
    
    while (getSubagentCount() < clamped) {
        addSubagent();
    }
    
    while (getSubagentCount() > clamped) {
        removeSubagent();
    }
    
    qInfo() << "[MultitaskingCoordinator] Session:" << m_sessionId
            << "scaled to" << clamped << "subagents";
}

QString MultitaskingCoordinator::submitTask(const QString& description,
                                           std::function<QJsonObject()> executor,
                                           int priority)
{
    if (!m_distributor) {
        return QString();
    }
    
    SubagentTaskDistributor::DistributedTask task;
    task.taskId = description + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    task.description = description;
    task.executor = executor;
    task.priority = priority;
    task.dependencyMode = SubagentTaskDistributor::TaskDependency::NoDelay;
    
    QString taskId = m_distributor->distributeTask(task);
    
    if (!taskId.isEmpty()) {
        emit taskSubmitted(taskId);
    }
    
    return taskId;
}

QString MultitaskingCoordinator::submitComplexTask(const QString& description,
                                                  const QList<SubagentTaskDistributor::DistributedTask>& subtasks)
{
    if (!m_distributor) {
        return QString();
    }
    
    return m_distributor->distributeComplexTask(description, subtasks);
}

QString MultitaskingCoordinator::submitParallelTasks(const QList<SubagentTaskDistributor::DistributedTask>& tasks)
{
    if (!m_distributor) {
        return QString();
    }
    
    return m_distributor->launchParallelTasks(tasks);
}

QString MultitaskingCoordinator::submitSequentialTasks(const QList<SubagentTaskDistributor::DistributedTask>& tasks)
{
    if (!m_distributor) {
        return QString();
    }
    
    return m_distributor->launchSequentialTasks(tasks);
}

bool MultitaskingCoordinator::cancelTask(const QString& taskId)
{
    if (!m_distributor) {
        return false;
    }
    
    return m_distributor->cancelTask(taskId);
}

bool MultitaskingCoordinator::waitForTask(const QString& taskId, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        QMutexLocker locker(&m_mutex);
        
        if (m_taskWaitFlags.contains(taskId) && m_taskWaitFlags[taskId]) {
            return true;  // Task completed
        }
        
        locker.unlock();
        
        QThread::msleep(100);
    }
    
    return false;  // Timeout
}

QJsonObject MultitaskingCoordinator::getTaskResult(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_taskResults.contains(taskId)) {
        return m_taskResults[taskId];
    }
    
    return QJsonObject();
}

QJsonObject MultitaskingCoordinator::getTaskStatus(const QString& taskId) const
{
    if (!m_distributor) {
        return QJsonObject();
    }
    
    QJsonObject status;
    m_distributor->getTaskStatus(taskId, status);
    
    return status;
}

QJsonObject MultitaskingCoordinator::getCoordinatorMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics;
    metrics["sessionId"] = m_sessionId;
    metrics["subagentCount"] = getSubagentCount();
    metrics["availableSubagents"] = getAvailableSubagentCount();
    metrics["maxSubagents"] = m_maxSubagents;
    metrics["maxConcurrentTasks"] = m_maxConcurrentTasks;
    metrics["cpuUsagePercent"] = m_cpuUsagePercent;
    metrics["memoryUsageMB"] = static_cast<int>(m_memoryUsageMB);
    metrics["maxMemoryMB"] = static_cast<int>(m_maxMemoryMB);
    metrics["autoScalingEnabled"] = m_autoScalingEnabled;
    
    if (m_distributor) {
        metrics["distributorMetrics"] = m_distributor->getDistributorMetrics();
    }
    
    if (m_pool) {
        metrics["poolMetrics"] = m_pool->getPoolMetrics();
    }
    
    return metrics;
}

QJsonArray MultitaskingCoordinator::getActiveTasksList() const
{
    QJsonArray tasks;
    
    if (m_distributor) {
        tasks = m_distributor->getAllTaskResults();
    }
    
    return tasks;
}

void MultitaskingCoordinator::setResourceLimits(qint64 memoryMB, int cpuPercent)
{
    QMutexLocker locker(&m_mutex);
    m_maxMemoryMB = memoryMB;
    m_maxCpuPercent = cpuPercent;
}

void MultitaskingCoordinator::onDistributorTaskCompleted(const QString& taskId, const QJsonObject& result)
{
    QMutexLocker locker(&m_mutex);
    m_taskResults[taskId] = result;
    m_taskWaitFlags[taskId] = true;
    locker.unlock();
    
    emit taskCompleted(taskId, result);
}

void MultitaskingCoordinator::onDistributorTaskFailed(const QString& taskId, const QString& error)
{
    QMutexLocker locker(&m_mutex);
    m_taskWaitFlags[taskId] = false;
    locker.unlock();
    
    emit taskFailed(taskId, error);
}

void MultitaskingCoordinator::onPoolAgentAdded(const QString& agentId)
{
    emit subagentAdded(getSubagentCount());
    qInfo() << "[MultitaskingCoordinator] Session:" << m_sessionId << "agent added:" << agentId;
}

void MultitaskingCoordinator::onPoolAgentRemoved(const QString& agentId)
{
    emit subagentRemoved(getSubagentCount());
    qInfo() << "[MultitaskingCoordinator] Session:" << m_sessionId << "agent removed:" << agentId;
}

void MultitaskingCoordinator::collectResourceMetrics()
{
    enforceResourceLimits();
    
    // Emit metrics
    QJsonObject metrics = getCoordinatorMetrics();
    qDebug() << "[MultitaskingCoordinator] Session:" << m_sessionId
             << "Metrics: SubagentCount=" << getSubagentCount()
             << "AvailableSubagents=" << getAvailableSubagentCount();
}

void MultitaskingCoordinator::monitorResources()
{
    // This would integrate with system monitoring APIs to track real resources
    // For now, we'll use simulated values based on pool metrics
    
    if (m_pool) {
        QMutexLocker locker(&m_mutex);
        double loadFactor = m_pool->getAverageLoadFactor();
        m_cpuUsagePercent = loadFactor * 100.0;
        m_memoryUsageMB = (m_pool->getAgentCount() * 64);  // ~64MB per agent estimate
    }
}

void MultitaskingCoordinator::enforceResourceLimits()
{
    monitorResources();
    
    QMutexLocker locker(&m_mutex);
    
    if (m_memoryUsageMB > m_maxMemoryMB) {
        emit resourceWarning("memory", static_cast<double>(m_memoryUsageMB) / m_maxMemoryMB);
    }
    
    if (m_cpuUsagePercent > m_maxCpuPercent) {
        emit resourceWarning("cpu", m_cpuUsagePercent / m_maxCpuPercent);
    }
}
