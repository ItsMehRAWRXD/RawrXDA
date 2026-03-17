#include "subagent_manager.hpp"
#include <QTimer>
#include <QThreadPool>
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <random>

// ============================================================================
// Subagent Implementation
// ============================================================================

Subagent::Subagent(const QString& agentId, QObject* parent)
    : QObject(parent), m_agentId(agentId), m_status(Status::Idle)
{
    m_timeoutTimer = new QTimer(this);
    connect(m_timeoutTimer, &QTimer::timeout, this, &Subagent::onTaskTimeout);
    m_lastActivityTime = QDateTime::currentDateTime();
}

Subagent::~Subagent()
{
    terminate();
}

QString Subagent::getStatusString() const
{
    switch (m_status) {
    case Status::Idle: return "Idle";
    case Status::Busy: return "Busy";
    case Status::Waiting: return "Waiting";
    case Status::Failed: return "Failed";
    case Status::Paused: return "Paused";
    case Status::Terminated: return "Terminated";
    default: return "Unknown";
    }
}

bool Subagent::executeTask(const Task& task)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_status != Status::Idle && m_status != Status::Failed) {
        qWarning() << "[Subagent]" << m_agentId << "Cannot execute task - agent status is" << getStatusString();
        return false;
    }

    m_currentTask = std::make_shared<Task>(task);
    m_currentTaskId = task.taskId;
    m_status = Status::Busy;
    m_taskTimer.start();
    
    // Set up timeout
    m_timeoutTimer->start(task.timeoutMs);
    
    qInfo() << "[Subagent]" << m_agentId << "Starting task" << task.taskId;
    emit taskStarted(task.taskId);
    emit statusChanged(static_cast<int>(Status::Busy));
    
    locker.unlock();
    
    // Execute task in this thread (caller's responsibility to handle threading)
    executeTaskInternal();
    
    return true;
}

void Subagent::executeTaskInternal()
{
    try {
        QMutexLocker locker(&m_mutex);
        
        if (!m_currentTask) {
            return;
        }
        
        auto task = m_currentTask;
        locker.unlock();
        
        qInfo() << "[Subagent]" << m_agentId << "Executing task" << task->taskId;
        
        QJsonObject result = task->executor();
        
        locker.relock();
        m_timeoutTimer->stop();
        
        qint64 duration = m_taskTimer.elapsed();
        
        // Update metrics
        m_tasksCompleted++;
        m_recentTaskDurations.push_back(static_cast<double>(duration));
        if (m_recentTaskDurations.size() > 100) {
            m_recentTaskDurations.erase(m_recentTaskDurations.begin());
        }
        
        double total = 0.0;
        for (double d : m_recentTaskDurations) {
            total += d;
        }
        m_averageTaskDurationMs = total / m_recentTaskDurations.size();
        
        m_status = Status::Idle;
        m_currentTaskId.clear();
        m_lastActivityTime = QDateTime::currentDateTime();
        
        locker.unlock();
        
        emit taskCompleted(task->taskId, result);
        emit statusChanged(static_cast<int>(Status::Idle));
        
        qInfo() << "[Subagent]" << m_agentId << "Completed task" << task->taskId 
                << "in" << duration << "ms";
        
    } catch (const std::exception& e) {
        QMutexLocker locker(&m_mutex);
        m_timeoutTimer->stop();
        m_tasksFailed++;
        m_status = Status::Failed;
        m_lastActivityTime = QDateTime::currentDateTime();
        
        QString errorMsg = QString::fromStdString(std::string(e.what()));
        QString taskId = m_currentTaskId;
        m_currentTaskId.clear();
        
        locker.unlock();
        
        emit taskFailed(taskId, errorMsg);
        emit statusChanged(static_cast<int>(Status::Failed));
        
        qCritical() << "[Subagent]" << m_agentId << "Task failed:" << errorMsg;
    }
}

bool Subagent::cancelCurrentTask()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_status != Status::Busy) {
        return false;
    }
    
    m_timeoutTimer->stop();
    QString taskId = m_currentTaskId;
    m_currentTaskId.clear();
    m_status = Status::Idle;
    m_lastActivityTime = QDateTime::currentDateTime();
    
    locker.unlock();
    
    emit taskCancelled(taskId);
    emit statusChanged(static_cast<int>(Status::Idle));
    
    qInfo() << "[Subagent]" << m_agentId << "Cancelled task" << taskId;
    
    return true;
}

double Subagent::getSuccessRate() const
{
    if (m_tasksCompleted + m_tasksFailed == 0) {
        return 1.0;
    }
    return static_cast<double>(m_tasksCompleted) / (m_tasksCompleted + m_tasksFailed);
}

QJsonObject Subagent::getMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics;
    metrics["agentId"] = m_agentId;
    metrics["status"] = getStatusString();
    metrics["tasksCompleted"] = static_cast<int>(m_tasksCompleted);
    metrics["tasksFailed"] = static_cast<int>(m_tasksFailed);
    metrics["averageTaskDurationMs"] = m_averageTaskDurationMs;
    metrics["successRate"] = getSuccessRate();
    metrics["lastActivityTime"] = m_lastActivityTime.toString(Qt::ISODate);
    
    return metrics;
}

void Subagent::pause()
{
    QMutexLocker locker(&m_mutex);
    if (m_status == Status::Busy) {
        m_paused = true;
        m_status = Status::Paused;
        emit statusChanged(static_cast<int>(Status::Paused));
    }
}

void Subagent::resume()
{
    QMutexLocker locker(&m_mutex);
    if (m_paused) {
        m_paused = false;
        m_status = Status::Busy;
        emit statusChanged(static_cast<int>(Status::Busy));
    }
}

void Subagent::terminate()
{
    QMutexLocker locker(&m_mutex);
    m_timeoutTimer->stop();
    cancelCurrentTask();
    m_status = Status::Terminated;
    emit statusChanged(static_cast<int>(Status::Terminated));
}

void Subagent::onTaskTimeout()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_status == Status::Busy && m_currentTask) {
        QString taskId = m_currentTaskId;
        m_tasksFailed++;
        m_status = Status::Failed;
        m_currentTaskId.clear();
        
        locker.unlock();
        
        emit taskFailed(taskId, "Task execution timeout");
        emit statusChanged(static_cast<int>(Status::Failed));
        
        qWarning() << "[Subagent]" << m_agentId << "Task timeout for" << taskId;
    }
}

// ============================================================================
// SubagentPool Implementation
// ============================================================================

SubagentPool::SubagentPool(const QString& poolId, int maxAgents, QObject* parent)
    : QObject(parent), m_poolId(poolId), m_maxAgents(maxAgents)
{
    // Create initial agents
    for (int i = 0; i < 5 && i < maxAgents; ++i) {
        addAgent();
    }
    m_currentDesiredAgents = std::min(5, maxAgents);
    
    // Set up auto-scaling timer
    m_autoScalingTimer = new QTimer(this);
    connect(m_autoScalingTimer, &QTimer::timeout, this, &SubagentPool::evaluateAutoScaling);
    m_autoScalingTimer->start(5000);  // Check every 5 seconds
    
    qInfo() << "[SubagentPool]" << m_poolId << "Created with max" << maxAgents << "agents";
}

SubagentPool::~SubagentPool()
{
    terminatePool();
}

int SubagentPool::getIdleAgentCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& agent : m_agents) {
        if (agent && agent->isIdle()) {
            count++;
        }
    }
    return count;
}

int SubagentPool::getBusyAgentCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& agent : m_agents) {
        if (agent && agent->isBusy()) {
            count++;
        }
    }
    return count;
}

QString SubagentPool::submitTask(const Subagent::Task& task)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_paused) {
        qWarning() << "[SubagentPool]" << m_poolId << "Cannot submit task - pool is paused";
        return QString();
    }
    
    // Queue the task
    QueuedTask queuedTask;
    queuedTask.taskId = task.taskId;
    queuedTask.task = task;
    queuedTask.enqueuedAt = QDateTime::currentDateTime();
    
    m_taskQueue.enqueue(queuedTask);
    
    qInfo() << "[SubagentPool]" << m_poolId << "Task queued:" << task.taskId
            << "Queue size:" << m_taskQueue.size();
    
    locker.unlock();
    
    emit taskQueued(task.taskId);
    m_taskAvailable.wakeOne();
    
    // Try to process immediately
    processTaskQueue();
    
    return task.taskId;
}

bool SubagentPool::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    // Check if task is queued
    for (auto it = m_taskQueue.begin(); it != m_taskQueue.end(); ++it) {
        if (it->taskId == taskId) {
            m_taskQueue.erase(it);
            qInfo() << "[SubagentPool]" << m_poolId << "Cancelled queued task:" << taskId;
            return true;
        }
    }
    
    // Check if task is being executed
    if (m_taskToAgentMap.contains(taskId)) {
        QString agentId = m_taskToAgentMap[agentId];
        m_taskToAgentMap.remove(taskId);
        
        if (m_agents.contains(agentId)) {
            m_agents[agentId]->cancelCurrentTask();
            return true;
        }
    }
    
    return false;
}

bool SubagentPool::pausePool()
{
    QMutexLocker locker(&m_mutex);
    m_paused = true;
    
    for (auto& agent : m_agents) {
        if (agent) {
            agent->pause();
        }
    }
    
    qInfo() << "[SubagentPool]" << m_poolId << "Pool paused";
    return true;
}

bool SubagentPool::resumePool()
{
    QMutexLocker locker(&m_mutex);
    m_paused = false;
    
    for (auto& agent : m_agents) {
        if (agent) {
            agent->resume();
        }
    }
    
    locker.unlock();
    
    qInfo() << "[SubagentPool]" << m_poolId << "Pool resumed";
    
    // Process queued tasks
    processTaskQueue();
    
    return true;
}

void SubagentPool::terminatePool()
{
    QMutexLocker locker(&m_mutex);
    
    m_taskQueue.clear();
    m_taskToAgentMap.clear();
    
    for (auto& agent : m_agents) {
        if (agent) {
            agent->terminate();
        }
    }
    m_agents.clear();
    
    if (m_autoScalingTimer) {
        m_autoScalingTimer->stop();
    }
    
    qInfo() << "[SubagentPool]" << m_poolId << "Pool terminated";
    emit poolTerminated();
}

bool SubagentPool::addAgent()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_agents.size() >= m_maxAgents) {
        qWarning() << "[SubagentPool]" << m_poolId << "Cannot add more agents - max reached";
        return false;
    }
    
    QString agentId = m_poolId + "_agent_" + QString::number(m_agents.size());
    auto agent = std::make_shared<Subagent>(agentId, this);
    
    // Connect signals
    connect(agent.get(), &Subagent::taskCompleted, this, &SubagentPool::onSubagentTaskCompleted);
    connect(agent.get(), &Subagent::taskFailed, this, &SubagentPool::onSubagentTaskFailed);
    connect(agent.get(), &Subagent::statusChanged, this, &SubagentPool::onSubagentStatusChanged);
    
    m_agents[agentId] = agent;
    
    locker.unlock();
    
    emit agentAdded(agentId);
    qInfo() << "[SubagentPool]" << m_poolId << "Agent added:" << agentId;
    
    return true;
}

bool SubagentPool::removeAgent(const QString& agentId)
{
    QMutexLocker locker(&m_mutex);
    
    QString toRemove = agentId;
    
    // If not specified, remove the least busy agent
    if (toRemove.isEmpty()) {
        int minTasks = INT_MAX;
        for (auto& agent : m_agents) {
            if (agent->isIdle()) {
                toRemove = agent->getAgentId();
                break;
            }
        }
    }
    
    if (m_agents.contains(toRemove)) {
        m_agents[toRemove]->terminate();
        m_agents.remove(toRemove);
        
        locker.unlock();
        
        emit agentRemoved(toRemove);
        qInfo() << "[SubagentPool]" << m_poolId << "Agent removed:" << toRemove;
        return true;
    }
    
    return false;
}

Subagent* SubagentPool::getIdleAgent()
{
    QMutexLocker locker(&m_mutex);
    
    for (auto& agent : m_agents) {
        if (agent && agent->isIdle()) {
            return agent.get();
        }
    }
    
    return nullptr;
}

Subagent* SubagentPool::getAgent(const QString& agentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_agents.contains(agentId)) {
        return m_agents[agentId].get();
    }
    
    return nullptr;
}

QList<Subagent*> SubagentPool::getAllAgents() const
{
    QMutexLocker locker(&m_mutex);
    
    QList<Subagent*> result;
    for (const auto& agent : m_agents) {
        if (agent) {
            result.append(agent.get());
        }
    }
    return result;
}

QJsonObject SubagentPool::getPoolMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics;
    metrics["poolId"] = m_poolId;
    metrics["totalAgents"] = m_agents.size();
    metrics["maxAgents"] = m_maxAgents;
    metrics["idleAgents"] = getIdleAgentCount();
    metrics["busyAgents"] = getBusyAgentCount();
    metrics["pendingTasks"] = m_taskQueue.size();
    metrics["loadBalancingStrategy"] = m_loadBalancingStrategy;
    metrics["autoScalingEnabled"] = m_autoScalingEnabled;
    metrics["currentDesiredAgents"] = m_currentDesiredAgents;
    
    return metrics;
}

QJsonArray SubagentPool::getAllAgentMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray metrics;
    for (const auto& agent : m_agents) {
        if (agent) {
            metrics.append(agent->getMetrics());
        }
    }
    
    return metrics;
}

double SubagentPool::getAverageLoadFactor() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_agents.isEmpty()) {
        return 0.0;
    }
    
    int busyCount = getBusyAgentCount();
    return static_cast<double>(busyCount) / m_agents.size();
}

void SubagentPool::setMaxAgents(int max)
{
    QMutexLocker locker(&m_mutex);
    m_maxAgents = std::max(1, std::min(max, 20));  // Clamp to 1-20
    qInfo() << "[SubagentPool]" << m_poolId << "Max agents set to" << m_maxAgents;
}

void SubagentPool::setLoadBalancingStrategy(const QString& strategy)
{
    QMutexLocker locker(&m_mutex);
    m_loadBalancingStrategy = strategy;
    qInfo() << "[SubagentPool]" << m_poolId << "Load balancing strategy:" << strategy;
}

void SubagentPool::setAutoScaling(bool enabled, int minAgents, int maxAgents)
{
    QMutexLocker locker(&m_mutex);
    m_autoScalingEnabled = enabled;
    m_minAgents = minAgents;
    m_maxAgents = maxAgents;
    qInfo() << "[SubagentPool]" << m_poolId << "Auto-scaling" << (enabled ? "enabled" : "disabled");
}

void SubagentPool::processTaskQueue()
{
    QMutexLocker locker(&m_mutex);
    
    while (!m_taskQueue.isEmpty()) {
        Subagent* agent = getIdleAgent();
        if (!agent) {
            break;  // No idle agents available
        }
        
        QueuedTask queuedTask = m_taskQueue.dequeue();
        
        m_taskToAgentMap[queuedTask.taskId] = agent->getAgentId();
        
        locker.unlock();
        
        bool success = agent->executeTask(queuedTask.task);
        
        locker.relock();
        
        if (!success) {
            m_taskQueue.prepend(queuedTask);  // Re-queue if failed
            break;
        }
        
        emit taskStarted(queuedTask.taskId, agent->getAgentId());
    }
}

void SubagentPool::onSubagentTaskCompleted(const QString& taskId, const QJsonObject& result)
{
    QMutexLocker locker(&m_mutex);
    m_taskToAgentMap.remove(taskId);
    locker.unlock();
    
    emit taskCompleted(taskId, result);
    processTaskQueue();
}

void SubagentPool::onSubagentTaskFailed(const QString& taskId, const QString& error)
{
    QMutexLocker locker(&m_mutex);
    m_taskToAgentMap.remove(taskId);
    locker.unlock();
    
    emit taskFailed(taskId, error);
    processTaskQueue();
}

void SubagentPool::onSubagentStatusChanged(int newStatus)
{
    // Try to process more tasks when status changes
    processTaskQueue();
}

void SubagentPool::evaluateAutoScaling()
{
    if (!m_autoScalingEnabled) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    double loadFactor = getAverageLoadFactor();
    int currentAgents = m_agents.size();
    
    // Scale up if load is high
    if (loadFactor > 0.8 && currentAgents < m_maxAgents) {
        int targetAgents = currentAgents + 1;
        locker.unlock();
        
        for (int i = currentAgents; i < targetAgents; ++i) {
            addAgent();
        }
        
        locker.relock();
        m_currentDesiredAgents = targetAgents;
        qInfo() << "[SubagentPool]" << m_poolId << "Auto-scaled up to" << targetAgents << "agents";
    }
    // Scale down if load is low
    else if (loadFactor < 0.3 && currentAgents > m_minAgents) {
        int targetAgents = std::max(m_minAgents, currentAgents - 1);
        locker.unlock();
        
        if (currentAgents > targetAgents) {
            removeAgent();
        }
        
        locker.relock();
        m_currentDesiredAgents = targetAgents;
        qInfo() << "[SubagentPool]" << m_poolId << "Auto-scaled down to" << targetAgents << "agents";
    }
}

// ============================================================================
// SubagentManager Implementation
// ============================================================================

SubagentManager* SubagentManager::s_instance = nullptr;
QMutex SubagentManager::s_mutex;

SubagentManager* SubagentManager::getInstance()
{
    if (s_instance == nullptr) {
        QMutexLocker locker(&s_mutex);
        if (s_instance == nullptr) {
            s_instance = new SubagentManager();
        }
    }
    return s_instance;
}

SubagentManager::~SubagentManager()
{
    for (auto& pool : m_pools) {
        if (pool) {
            pool->terminatePool();
        }
    }
    m_pools.clear();
}

std::shared_ptr<SubagentPool> SubagentManager::createPool(const QString& poolId, int agentCount)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_pools.contains(poolId)) {
        qWarning() << "[SubagentManager] Pool already exists:" << poolId;
        return m_pools[poolId];
    }
    
    int clampedCount = std::max(1, std::min(agentCount, m_maxAgentsPerPool));
    auto pool = std::make_shared<SubagentPool>(poolId, clampedCount);
    
    m_pools[poolId] = pool;
    
    locker.unlock();
    
    emit poolCreated(poolId);
    
    qInfo() << "[SubagentManager] Pool created:" << poolId << "with" << clampedCount << "agents";
    
    return pool;
}

std::shared_ptr<SubagentPool> SubagentManager::getPool(const QString& poolId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_pools.contains(poolId)) {
        return m_pools[poolId];
    }
    
    return nullptr;
}

bool SubagentManager::deletePool(const QString& poolId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_pools.contains(poolId)) {
        auto pool = m_pools[poolId];
        m_pools.remove(poolId);
        
        locker.unlock();
        
        pool->terminatePool();
        emit poolDeleted(poolId);
        
        qInfo() << "[SubagentManager] Pool deleted:" << poolId;
        return true;
    }
    
    return false;
}

QStringList SubagentManager::getPoolIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_pools.keys();
}

QString SubagentManager::submitTaskToPool(const QString& poolId, const Subagent::Task& task)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_pools.contains(poolId)) {
        qWarning() << "[SubagentManager] Pool not found:" << poolId;
        return QString();
    }
    
    auto pool = m_pools[poolId];
    locker.unlock();
    
    return pool->submitTask(task);
}

bool SubagentManager::cancelTask(const QString& poolId, const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_pools.contains(poolId)) {
        return false;
    }
    
    auto pool = m_pools[poolId];
    locker.unlock();
    
    return pool->cancelTask(taskId);
}

QJsonObject SubagentManager::getGlobalMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics;
    metrics["totalPools"] = m_pools.size();
    metrics["maxPoolsPerSession"] = m_maxPoolsPerSession;
    metrics["maxAgentsPerPool"] = m_maxAgentsPerPool;
    metrics["maxConcurrentTasks"] = m_maxConcurrentTasks;
    metrics["globalMemoryLimitMB"] = static_cast<int>(m_globalMemoryLimitMB);
    metrics["globalCpuLimitPercent"] = m_globalCpuLimitPercent;
    
    int totalAgents = 0;
    int totalActiveTasks = 0;
    
    for (const auto& pool : m_pools) {
        if (pool) {
            totalAgents += pool->getAgentCount();
            totalActiveTasks += (pool->getBusyAgentCount() + pool->getPendingTaskCount());
        }
    }
    
    metrics["totalAgents"] = totalAgents;
    metrics["totalActiveTasks"] = totalActiveTasks;
    
    return metrics;
}

QJsonArray SubagentManager::getAllPoolMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray metrics;
    for (const auto& pool : m_pools) {
        if (pool) {
            metrics.append(pool->getPoolMetrics());
        }
    }
    
    return metrics;
}

int SubagentManager::getActiveTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const auto& pool : m_pools) {
        if (pool) {
            count += pool->getBusyAgentCount();
        }
    }
    
    return count;
}
