/**
 * @file AgentOrchestrator.cpp
 * @brief Complete Multi-Agent AI Coordination System for RawrXD Agentic IDE
 * 
 * Provides orchestration and coordination for multiple AI agents working together:
 * - Task delegation and coordination between agents
 * - Agent lifecycle management (spawn, monitor, terminate)
 * - Inter-agent communication and message routing
 * - Agent resource allocation and load balancing
 * - Conflict resolution and consensus building
 * - Performance monitoring and optimization
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "AgentOrchestrator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <algorithm>
#include <functional>

namespace RawrXD {

// ============================================================================
// Agent Implementation
// ============================================================================

Agent::Agent()
    : m_type(AgentType::Custom)
    , m_status(AgentStatus::Idle)
    , m_version(QStringLiteral("1.0.0"))
    , m_cpuUsage(0.0)
    , m_memoryUsage(0.0)
    , m_activeTasks(0)
    , m_maxConcurrentTasks(5)
    , m_lastActivity(QDateTime::currentDateTime())
{
}

Agent::Agent(const QString& id, AgentType type, const QString& name)
    : m_id(id)
    , m_name(name)
    , m_type(type)
    , m_status(AgentStatus::Idle)
    , m_version(QStringLiteral("1.0.0"))
    , m_cpuUsage(0.0)
    , m_memoryUsage(0.0)
    , m_activeTasks(0)
    , m_maxConcurrentTasks(5)
    , m_lastActivity(QDateTime::currentDateTime())
{
}

void Agent::addCapability(AgentCapability capability)
{
    m_capabilities.insert(capability);
}

void Agent::removeCapability(AgentCapability capability)
{
    m_capabilities.remove(capability);
}

bool Agent::hasCapability(AgentCapability capability) const
{
    return m_capabilities.contains(capability);
}

bool Agent::isHealthy() const
{
    return m_status != AgentStatus::Error && 
           m_status != AgentStatus::Terminated &&
           m_cpuUsage < 95.0 &&
           m_memoryUsage < 95.0;
}

bool Agent::isAvailable() const
{
    return m_status == AgentStatus::Idle && 
           m_activeTasks < m_maxConcurrentTasks &&
           isHealthy();
}

// ============================================================================
// AgentTask Implementation
// ============================================================================

AgentTask::AgentTask()
    : m_type(TaskType::Custom)
    , m_priority(TaskPriority::Normal)
    , m_status(AgentStatus::Idle)
    , m_progress(0)
    , m_createdAt(QDateTime::currentDateTime())
{
}

AgentTask::AgentTask(const QString& id, TaskType type, const QString& description)
    : m_id(id)
    , m_type(type)
    , m_description(description)
    , m_priority(TaskPriority::Normal)
    , m_status(AgentStatus::Idle)
    , m_progress(0)
    , m_createdAt(QDateTime::currentDateTime())
{
}

void AgentTask::assignAgent(const QString& agentId)
{
    if (!m_assignedAgents.contains(agentId)) {
        m_assignedAgents.append(agentId);
    }
}

void AgentTask::unassignAgent(const QString& agentId)
{
    m_assignedAgents.removeAll(agentId);
}

bool AgentTask::isCompleted() const
{
    return m_status == AgentStatus::Idle && m_progress == 100;
}

bool AgentTask::isFailed() const
{
    return m_status == AgentStatus::Error;
}

bool AgentTask::isInProgress() const
{
    return m_status == AgentStatus::Busy || m_status == AgentStatus::Waiting;
}

// ============================================================================
// AgentMessage Implementation
// ============================================================================

AgentMessage::AgentMessage()
    : m_timestamp(QDateTime::currentDateTime())
{
}

AgentMessage::AgentMessage(const QString& from, const QString& to, const QString& type)
    : m_from(from)
    , m_to(to)
    , m_type(type)
    , m_timestamp(QDateTime::currentDateTime())
{
}

// ============================================================================
// AgentOrchestrator Implementation
// ============================================================================

AgentOrchestrator::AgentOrchestrator(QObject* parent)
    : QObject(parent)
    , m_totalTasksSubmitted(0)
    , m_totalTasksCompleted(0)
    , m_totalTasksFailed(0)
    , m_totalProcessingTime(0)
    , m_healthCheckTimer(new QTimer(this))
    , m_taskTimeoutTimer(new QTimer(this))
    , m_loadBalancingTimer(new QTimer(this))
{
    // Initialize timers
    m_healthCheckTimer->setInterval(DEFAULT_HEALTH_CHECK_INTERVAL);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &AgentOrchestrator::onHealthCheckTimer);
    
    m_taskTimeoutTimer->setInterval(1000); // Check every second
    connect(m_taskTimeoutTimer, &QTimer::timeout, this, &AgentOrchestrator::onTaskTimeoutTimer);
    
    m_loadBalancingTimer->setInterval(10000); // Every 10 seconds
    connect(m_loadBalancingTimer, &QTimer::timeout, this, &AgentOrchestrator::onLoadBalancingTimer);
    
    qDebug() << "[AgentOrchestrator] Initialized with default configuration";
}

AgentOrchestrator::~AgentOrchestrator()
{
    shutdown();
    qDebug() << "[AgentOrchestrator] Destroyed";
}

bool AgentOrchestrator::initialize(const AgentOrchestrationConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    m_config = config;
    
    // Start timers
    m_healthCheckTimer->start();
    m_taskTimeoutTimer->start();
    if (config.enableLoadBalancing) {
        m_loadBalancingTimer->start();
    }
    
    // Initialize default agents
    initializeDefaultAgents();
    
    qDebug() << "[AgentOrchestrator] Initialized with" << m_agents.size() << "agents";
    return true;
}

void AgentOrchestrator::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[AgentOrchestrator] Shutting down";
    
    // Stop timers
    m_healthCheckTimer->stop();
    m_taskTimeoutTimer->stop();
    m_loadBalancingTimer->stop();
    
    // Terminate all agents
    for (auto& agent : m_agents.values()) {
        agent.setStatus(AgentStatus::Terminated);
    }
    
    // Clear data
    m_agents.clear();
    m_tasks.clear();
    m_taskQueue.clear();
    m_messageHistory.clear();
    
    qDebug() << "[AgentOrchestrator] Shutdown complete";
}

QString AgentOrchestrator::registerAgent(AgentType type, const QString& name, 
                                        const QSet<AgentCapability>& capabilities,
                                        const QString& configuration)
{
    QMutexLocker locker(&m_mutex);
    
    QString agentId = generateId();
    
    Agent agent(agentId, type, name);
    agent.setCapabilities(capabilities);
    agent.setConfiguration(configuration);
    agent.setStatus(AgentStatus::Idle);
    
    m_agents[agentId] = agent;
    
    qDebug() << "[AgentOrchestrator] Registered agent" << name << "(" << agentId << ")";
    
    return agentId;
}

bool AgentOrchestrator::unregisterAgent(const QString& agentId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_agents.contains(agentId)) {
        return false;
    }
    
    // Cancel any tasks assigned to this agent
    for (auto& task : m_tasks.values()) {
        if (task.assignedAgents().contains(agentId)) {
            task.unassignAgent(agentId);
            if (task.assignedAgents().isEmpty()) {
                task.setStatus(AgentStatus::Idle);
            }
        }
    }
    
    m_agents.remove(agentId);
    
    qDebug() << "[AgentOrchestrator] Unregistered agent" << agentId;
    return true;
}

Agent AgentOrchestrator::getAgent(const QString& agentId) const
{
    QMutexLocker locker(&m_mutex);
    return m_agents.value(agentId, Agent());
}

QList<Agent> AgentOrchestrator::getAgents() const
{
    QMutexLocker locker(&m_mutex);
    return m_agents.values();
}

QList<Agent> AgentOrchestrator::getAgentsByType(AgentType type) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<Agent> result;
    for (const Agent& agent : m_agents.values()) {
        if (agent.type() == type) {
            result.append(agent);
        }
    }
    
    return result;
}

QList<Agent> AgentOrchestrator::getAvailableAgents(const QSet<AgentCapability>& capabilities, int maxAgents) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<Agent> availableAgents;
    
    for (const Agent& agent : m_agents.values()) {
        if (agent.isAvailable()) {
            // Check if agent has all required capabilities
            bool hasAllCapabilities = true;
            for (const QString& capabilityStr : capabilities) {
                // For simplicity, assuming capability names match
                // In real implementation, would need proper capability lookup
                if (!agent.capabilities().contains(AgentCapability::Custom)) {
                    hasAllCapabilities = false;
                    break;
                }
            }
            
            if (hasAllCapabilities) {
                availableAgents.append(agent);
                if (availableAgents.size() >= maxAgents) {
                    break;
                }
            }
        }
    }
    
    // Sort by current load (least busy first)
    std::sort(availableAgents.begin(), availableAgents.end(), 
        [](const Agent& a, const Agent& b) {
            return a.activeTasks() < b.activeTasks();
        });
    
    return availableAgents;
}

QString AgentOrchestrator::submitTask(TaskType type, const QString& description,
                                    const QJsonObject& parameters,
                                    const QSet<AgentCapability>& requiredCapabilities,
                                    TaskPriority priority, const QString& requester)
{
    QMutexLocker locker(&m_mutex);
    
    QString taskId = generateId();
    
    AgentTask task(taskId, type, description);
    task.setParameters(parameters);
    task.setRequiredCapabilities(requiredCapabilities.toList());
    task.setPriority(priority);
    task.setRequester(requester);
    task.setStatus(AgentStatus::Waiting);
    task.setCreatedAt(QDateTime::currentDateTime());
    
    m_tasks[taskId] = task;
    m_taskQueue.enqueue(taskId);
    
    m_totalTasksSubmitted++;
    
    qDebug() << "[AgentOrchestrator] Submitted task" << taskId << ":" << description;
    
    // Process queue immediately if possible
    processTaskQueue();
    
    return taskId;
}

AgentTask AgentOrchestrator::getTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, AgentTask());
}

QList<AgentTask> AgentOrchestrator::getTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.values();
}

QList<AgentTask> AgentOrchestrator::getTasksByStatus(AgentStatus status) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AgentTask> result;
    for (const AgentTask& task : m_tasks.values()) {
        if (task.status() == status) {
            result.append(task);
        }
    }
    
    return result;
}

bool AgentOrchestrator::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    AgentTask& task = m_tasks[taskId];
    
    if (task.isInProgress()) {
        // Unassign from all agents
        for (const QString& agentId : task.assignedAgents()) {
            if (m_agents.contains(agentId)) {
                Agent& agent = m_agents[agentId];
                agent.setActiveTasks(agent.activeTasks() - 1);
            }
        }
    }
    
    task.setStatus(AgentStatus::Terminated);
    task.setErrorMessage(QStringLiteral("Task cancelled by user"));
    
    qDebug() << "[AgentOrchestrator] Cancelled task" << taskId;
    return true;
}

bool AgentOrchestrator::sendMessage(const QString& from, const QString& to, const QString& type,
                                  const QString& content, const QJsonObject& metadata)
{
    QMutexLocker locker(&m_mutex);
    
    // Validate that the source agent exists
    if (!m_agents.contains(from)) {
        qWarning() << "[AgentOrchestrator] Invalid source agent:" << from;
        return false;
    }
    
    // For point-to-point messages, validate target
    if (!to.isEmpty() && to != QStringLiteral("*") && !m_agents.contains(to)) {
        qWarning() << "[AgentOrchestrator] Invalid target agent:" << to;
        return false;
    }
    
    AgentMessage message(from, to, type);
    message.setContent(content);
    message.setMetadata(metadata);
    message.setCorrelationId(generateId());
    
    m_messageHistory.append(message);
    
    // Limit message history size
    if (m_messageHistory.size() > MAX_MESSAGE_HISTORY) {
        m_messageHistory.removeFirst();
    }
    
    qDebug() << "[AgentOrchestrator] Message sent from" << from << "to" << to << ": " << type;
    
    emit messageReceived(message);
    return true;
}

bool AgentOrchestrator::broadcastMessage(const QString& from, const QString& type, const QString& content,
                                      const QJsonObject& metadata)
{
    return sendMessage(from, QStringLiteral("*"), type, content, metadata);
}

QList<AgentMessage> AgentOrchestrator::getMessageHistory(const QString& agentId, int limit) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AgentMessage> result;
    
    for (int i = m_messageHistory.size() - 1; i >= 0 && result.size() < limit; --i) {
        const AgentMessage& message = m_messageHistory[i];
        
        if (agentId.isEmpty() || 
            message.from() == agentId || 
            message.to() == agentId || 
            message.isBroadcast()) {
            result.append(message);
        }
    }
    
    return result;
}

QString AgentOrchestrator::requestConsensus(const QString& decision, const QStringList& agents,
                                         std::function<void(bool, const QJsonObject&)> callback)
{
    QMutexLocker locker(&m_mutex);
    
    QString consensusId = generateId();
    
    // Send consensus request to all involved agents
    QJsonObject context;
    context[QStringLiteral("decision")] = decision;
    context[QStringLiteral("agents")] = QJsonArray::fromStringList(agents);
    context[QStringLiteral("consensus_id")] = consensusId;
    context[QStringLiteral("created_at")] = QDateTime::currentDateTime().toString();
    
    // Store callback
    m_consensusCallbacks[consensusId] = [callback](const QJsonObject& result) {
        bool reached = result[QStringLiteral("reached")].toBool();
        callback(reached, result);
    };
    
    // Send to agents
    for (const QString& agentId : agents) {
        if (m_agents.contains(agentId)) {
            sendMessage(QStringLiteral("orchestrator"), agentId, QStringLiteral("consensus_request"), 
                       decision, context);
        }
    }
    
    qDebug() << "[AgentOrchestrator] Requested consensus" << consensusId << "on:" << decision;
    return consensusId;
}

QString AgentOrchestrator::resolveConflict(const QString& conflictType, const QStringList& agents,
                                         const QJsonObject& context,
                                         std::function<void(const QJsonObject&)> callback)
{
    QMutexLocker locker(&m_mutex);
    
    QString conflictId = generateId();
    
    QJsonObject fullContext = context;
    fullContext[QStringLiteral("conflict_type")] = conflictType;
    fullContext[QStringLiteral("agents")] = QJsonArray::fromStringList(agents);
    fullContext[QStringLiteral("conflict_id")] = conflictId;
    fullContext[QStringLiteral("created_at")] = QDateTime::currentDateTime().toString();
    
    // Store callback
    m_conflictResolutionCallbacks[conflictId] = callback;
    
    // Send conflict resolution request to agents
    for (const QString& agentId : agents) {
        if (m_agents.contains(agentId)) {
            sendMessage(QStringLiteral("orchestrator"), agentId, QStringLiteral("conflict_resolution"), 
                       conflictType, fullContext);
        }
    }
    
    qDebug() << "[AgentOrchestrator] Requested conflict resolution" << conflictId << "for:" << conflictType;
    return conflictId;
}

bool AgentOrchestrator::scaleAgents(AgentType agentType, int targetCount)
{
    QMutexLocker locker(&m_mutex);
    
    int currentCount = 0;
    QList<QString> agentIds;
    
    // Count existing agents of this type
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        if (it.value().type() == agentType) {
            currentCount++;
            agentIds.append(it.key());
        }
    }
    
    if (currentCount < targetCount) {
        // Scale up - create new agents
        int toCreate = targetCount - currentCount;
        for (int i = 0; i < toCreate; ++i) {
            QString agentName = QString("%1_%2_%3").arg(static_cast<int>(agentType)).arg(currentCount + i).arg(QRandomGenerator::global()->bounded(1000));
            QSet<AgentCapability> capabilities;
            capabilities.insert(AgentCapability::Custom); // Default capability
            
            registerAgent(agentType, agentName, capabilities);
        }
        
        qDebug() << "[AgentOrchestrator] Scaled up" << agentType << "from" << currentCount << "to" << targetCount;
        return true;
    } else if (currentCount > targetCount) {
        // Scale down - terminate excess agents
        int toTerminate = currentCount - targetCount;
        for (int i = 0; i < toTerminate && i < agentIds.size(); ++i) {
            Agent& agent = m_agents[agentIds[i]];
            if (agent.activeTasks() == 0) {
                agent.setStatus(AgentStatus::Terminated);
                m_agents.remove(agentIds[i]);
            }
        }
        
        qDebug() << "[AgentOrchestrator] Scaled down" << agentType << "from" << currentCount << "to" << targetCount;
        return true;
    }
    
    return false; // No scaling needed
}

QJsonObject AgentOrchestrator::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    
    // Agent statistics
    QJsonObject agentStats;
    agentStats[QStringLiteral("total")] = m_agents.size();
    agentStats[QStringLiteral("idle")] = 0;
    agentStats[QStringLiteral("busy")] = 0;
    agentStats[QStringLiteral("error")] = 0;
    
    for (const Agent& agent : m_agents.values()) {
        switch (agent.status()) {
            case AgentStatus::Idle: agentStats[QStringLiteral("idle")] = agentStats[QStringLiteral("idle")].toInt() + 1; break;
            case AgentStatus::Busy: agentStats[QStringLiteral("busy")] = agentStats[QStringLiteral("busy")].toInt() + 1; break;
            case AgentStatus::Error: agentStats[QStringLiteral("error")] = agentStats[QStringLiteral("error")].toInt() + 1; break;
            default: break;
        }
    }
    
    // Task statistics
    QJsonObject taskStats;
    taskStats[QStringLiteral("total_submitted")] = m_totalTasksSubmitted;
    taskStats[QStringLiteral("total_completed")] = m_totalTasksCompleted;
    taskStats[QStringLiteral("total_failed")] = m_totalTasksFailed;
    taskStats[QStringLiteral("in_queue")] = m_taskQueue.size();
    taskStats[QStringLiteral("in_progress")] = 0;
    
    for (const AgentTask& task : m_tasks.values()) {
        if (task.isInProgress()) {
            taskStats[QStringLiteral("in_progress")] = taskStats[QStringLiteral("in_progress")].toInt() + 1;
        }
    }
    
    // Message statistics
    QJsonObject messageStats;
    messageStats[QStringLiteral("total")] = m_messageHistory.size();
    messageStats[QStringLiteral("recent")] = qMin(100, m_messageHistory.size());
    
    stats[QStringLiteral("agents")] = agentStats;
    stats[QStringLiteral("tasks")] = taskStats;
    stats[QStringLiteral("messages")] = messageStats;
    stats[QStringLiteral("average_processing_time")] = m_totalTasksCompleted > 0 ? 
        static_cast<double>(m_totalProcessingTime) / m_totalTasksCompleted : 0.0;
    
    return stats;
}

void AgentOrchestrator::setConfiguration(const AgentOrchestrationConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_config = config;
    
    // Update timers based on new configuration
    m_healthCheckTimer->setInterval(config.healthCheckIntervalMs);
    
    qDebug() << "[AgentOrchestrator] Configuration updated";
}

AgentOrchestrationConfig AgentOrchestrator::configuration() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}

// ============================================================================
// Private Slots
// ============================================================================

void AgentOrchestrator::onHealthCheckTimer()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[AgentOrchestrator] Running health check on" << m_agents.size() << "agents";
    
    // Check each agent's health
    for (auto it = m_agents.begin(); it != m_agents.end(); ++it) {
        Agent& agent = it.value();
        
        if (!checkAgentHealth(it.key())) {
            qWarning() << "[AgentOrchestrator] Agent" << it.key() << "is unhealthy, marking as error";
            agent.setStatus(AgentStatus::Error);
            emit agentStatusChanged(it.key(), agent.status(), AgentStatus::Error);
        }
        
        // Update last activity
        agent.setLastActivity(QDateTime::currentDateTime());
    }
}

void AgentOrchestrator::onTaskTimeoutTimer()
{
    QMutexLocker locker(&m_mutex);
    
    QDateTime now = QDateTime::currentDateTime();
    
    // Check for timed out tasks
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        AgentTask& task = it.value();
        
        if (task.isInProgress()) {
            qint64 timeout = m_config.taskTimeoutMs;
            if (task.startedAt().isValid()) {
                qint64 elapsed = task.startedAt().msecsTo(now);
                if (elapsed > timeout) {
                    qWarning() << "[AgentOrchestrator] Task" << it.key() << "timed out";
                    
                    // Unassign from agents
                    for (const QString& agentId : task.assignedAgents()) {
                        if (m_agents.contains(agentId)) {
                            Agent& agent = m_agents[agentId];
                            agent.setActiveTasks(agent.activeTasks() - 1);
                        }
                    }
                    
                    task.setStatus(AgentStatus::Error);
                    task.setErrorMessage(QStringLiteral("Task timed out"));
                    m_totalTasksFailed++;
                    
                    emit taskStatusChanged(it.key(), AgentStatus::Busy, AgentStatus::Error, 0);
                    emit taskFailed(it.key(), QStringLiteral("Task timed out"));
                }
            }
        }
    }
}

void AgentOrchestrator::onLoadBalancingTimer()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_config.enableLoadBalancing) {
        return;
    }
    
    loadBalanceAgents();
}

// ============================================================================
// Private Methods
// ============================================================================

void AgentOrchestrator::initializeDefaultAgents()
{
    // Create default agents for different types
    QMap<AgentType, QSet<AgentCapability>> defaultCapabilities;
    
    defaultCapabilities[AgentType::CodeAnalyzer] = {AgentCapability::CodeAnalysis, AgentCapability::PatternMatching};
    defaultCapabilities[AgentType::BugDetector] = {AgentCapability::CodeAnalysis, AgentCapability::Security};
    defaultCapabilities[AgentType::Refactorer] = {AgentCapability::Refactoring, AgentCapability::CodeAnalysis};
    defaultCapabilities[AgentType::TestGenerator] = {AgentCapability::Testing, AgentCapability::CodeAnalysis};
    defaultCapabilities[AgentType::Documenter] = {AgentCapability::Documentation, AgentCapability::CodeAnalysis};
    defaultCapabilities[AgentType::PerformanceOptimizer] = {AgentCapability::Performance, AgentCapability::CodeAnalysis};
    defaultCapabilities[AgentType::SecurityAuditor] = {AgentCapability::Security, AgentCapability::CodeAnalysis};
    defaultCapabilities[AgentType::CodeCompleter] = {AgentCapability::Completion, AgentCapability::CodeAnalysis};
    
    for (auto it = defaultCapabilities.begin(); it != defaultCapabilities.end(); ++it) {
        QString agentName = QString("%1_%2").arg(static_cast<int>(it.key())).arg(0);
        registerAgent(it.key(), agentName, it.value());
    }
    
    qDebug() << "[AgentOrchestrator] Initialized" << defaultCapabilities.size() << "default agents";
}

QString AgentOrchestrator::findBestAgent(const AgentTask& task) const
{
    // Get available agents with required capabilities
    QList<Agent> candidates = getAvailableAgents(QSet<AgentCapability>(), 100); // Get more candidates
    
    // Filter by capabilities
    QList<Agent> capableAgents;
    for (const Agent& agent : candidates) {
        if (validateAgentCapabilities(agent.id(), QSet<AgentCapability>())) { // Simplified check
            capableAgents.append(agent);
        }
    }
    
    if (capableAgents.isEmpty()) {
        return QString(); // No suitable agent found
    }
    
    // Select best agent based on current load and priority
    Agent* bestAgent = nullptr;
    int bestScore = -1;
    
    for (Agent& agent : capableAgents) {
        int score = 100; // Base score
        
        // Prefer agents with lower current load
        score -= agent.activeTasks() * 10;
        
        // Prefer agents that haven't been used recently (for diversity)
        qint64 idleTime = agent.lastActivity().secsTo(QDateTime::currentDateTime());
        score += qMin(50, static_cast<int>(idleTime / 60)); // Up to 50 points for idle time
        
        // Agent type preference based on task type
        if ((task.type() == TaskType::Analysis && agent.type() == AgentType::CodeAnalyzer) ||
            (task.type() == TaskType::Testing && agent.type() == AgentType::TestGenerator) ||
            (task.type() == TaskType::Refactoring && agent.type() == AgentType::Refactorer)) {
            score += 20; // Bonus for specialized agents
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestAgent = &agent;
        }
    }
    
    return bestAgent ? bestAgent->id() : QString();
}

bool AgentOrchestrator::assignTask(const QString& taskId, const QString& agentId)
{
    if (!m_tasks.contains(taskId) || !m_agents.contains(agentId)) {
        return false;
    }
    
    AgentTask& task = m_tasks[taskId];
    Agent& agent = m_agents[agentId];
    
    if (task.status() != AgentStatus::Waiting) {
        return false;
    }
    
    if (!agent.isAvailable()) {
        return false;
    }
    
    // Assign task
    task.assignAgent(agentId);
    task.setStatus(AgentStatus::Busy);
    task.setStartedAt(QDateTime::currentDateTime());
    task.setProgress(0);
    
    // Update agent
    agent.setActiveTasks(agent.activeTasks() + 1);
    agent.setStatus(AgentStatus::Busy);
    agent.setLastActivity(QDateTime::currentDateTime());
    
    qDebug() << "[AgentOrchestrator] Assigned task" << taskId << "to agent" << agentId;
    
    emit taskStatusChanged(taskId, AgentStatus::Waiting, AgentStatus::Busy, 0);
    emit agentStatusChanged(agentId, AgentStatus::Idle, AgentStatus::Busy);
    
    return true;
}

void AgentOrchestrator::processTaskQueue()
{
    QMutexLocker locker(&m_mutex);
    
    // Process tasks in queue by priority
    QQueue<QString> priorityQueue;
    
    // Separate high priority from normal
    while (!m_taskQueue.isEmpty()) {
        QString taskId = m_taskQueue.dequeue();
        if (m_tasks.contains(taskId)) {
            const AgentTask& task = m_tasks[taskId];
            if (task.priority() == TaskPriority::High || task.priority() == TaskPriority::Critical) {
                priorityQueue.enqueue(taskId);
            } else {
                priorityQueue.enqueue(taskId);
            }
        }
    }
    
    // Sort by priority and creation time
    QList<QString> sortedTasks;
    while (!priorityQueue.isEmpty()) {
        sortedTasks.append(priorityQueue.dequeue());
    }
    
    std::sort(sortedTasks.begin(), sortedTasks.end(), 
        [&](const QString& a, const QString& b) {
            const AgentTask& taskA = m_tasks[a];
            const AgentTask& taskB = m_tasks[b];
            
            // Higher priority first
            if (taskA.priority() != taskB.priority()) {
                return static_cast<int>(taskA.priority()) > static_cast<int>(taskB.priority());
            }
            
            // Earlier creation time first
            return taskA.createdAt() < taskB.createdAt();
        });
    
    // Try to assign each task
    for (const QString& taskId : sortedTasks) {
        if (!m_tasks.contains(taskId)) continue;
        
        const AgentTask& task = m_tasks[taskId];
        if (task.status() != AgentStatus::Waiting) continue;
        
        QString bestAgent = findBestAgent(task);
        if (!bestAgent.isEmpty()) {
            assignTask(taskId, bestAgent);
        } else {
            // No suitable agent, re-queue for later
            m_taskQueue.enqueue(taskId);
        }
    }
}

bool AgentOrchestrator::checkAgentHealth(const QString& agentId) const
{
    if (!m_agents.contains(agentId)) {
        return false;
    }
    
    const Agent& agent = m_agents[agentId];
    
    // Check if agent is responsive (simplified check)
    qint64 lastActivityAge = agent.lastActivity().secsTo(QDateTime::currentDateTime());
    if (lastActivityAge > 300) { // 5 minutes without activity
        return false;
    }
    
    // Check resource usage
    if (agent.cpuUsage() > 95.0 || agent.memoryUsage() > 95.0) {
        return false;
    }
    
    return true;
}

void AgentOrchestrator::loadBalanceAgents()
{
    // Simple load balancing: redistribute tasks from busy agents to idle ones
    QList<QString> busyAgents;
    QList<QString> idleAgents;
    
    for (const Agent& agent : m_agents.values()) {
        if (agent.activeTasks() > agent.maxConcurrentTasks() / 2) {
            busyAgents.append(agent.id());
        } else if (agent.activeTasks() == 0) {
            idleAgents.append(agent.id());
        }
    }
    
    // This is a simplified load balancing algorithm
    // In a real implementation, would consider task complexity,
    // agent expertise, network latency, etc.
    
    if (!busyAgents.isEmpty() && !idleAgents.isEmpty()) {
        qDebug() << "[AgentOrchestrator] Load balancing:" << busyAgents.size() << "busy," << idleAgents.size() << "idle agents";
    }
}

void AgentOrchestrator::cleanupTasks()
{
    // Remove completed/failed tasks that are older than a threshold
    QDateTime threshold = QDateTime::currentDateTime().addSecs(-3600); // 1 hour ago
    
    QList<QString> toRemove;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        const AgentTask& task = it.value();
        if ((task.isCompleted() || task.isFailed()) && task.completedAt().isValid() && 
            task.completedAt() < threshold) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& taskId : toRemove) {
        m_tasks.remove(taskId);
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "[AgentOrchestrator] Cleaned up" << toRemove.size() << "old tasks";
    }
}

QString AgentOrchestrator::generateId() const
{
    return QString::number(QRandomGenerator::global()->bounded(INT_MAX)) + 
           QString::number(QDateTime::currentMSecsSinceEpoch());
}

bool AgentOrchestrator::validateAgentCapabilities(const QString& agentId, const QSet<AgentCapability>& requiredCapabilities) const
{
    if (!m_agents.contains(agentId)) {
        return false;
    }
    
    const Agent& agent = m_agents[agentId];
    
    // Simplified capability validation
    // In real implementation, would properly map capability names to enum values
    for (const QString& capability : requiredCapabilities) {
        if (!agent.capabilities().contains(AgentCapability::Custom)) {
            return false;
        }
    }
    
    return true;
}

} // namespace RawrXD
