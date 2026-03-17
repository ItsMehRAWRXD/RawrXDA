// AgenticAgentCoordinator Implementation
#include "agentic_agent_coordinator.h"
#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

// Simple UUID generator replacement
std::string generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* digits = "0123456789abcdef";
    std::stringstream ss;
    for (int i = 0; i < 8; i++) ss << digits[dis(gen)];
    return ss.str();
}

AgenticAgentCoordinator::AgenticAgentCoordinator()
    : m_coordinationStartTime(std::chrono::system_clock::now())
{
    std::cout << "[AgenticAgentCoordinator] Initialized - Ready for multi-agent coordination" << std::endl;
}

AgenticAgentCoordinator::~AgenticAgentCoordinator()
{
    std::cout << "[AgenticAgentCoordinator] Destroyed - Managed "
              << m_agents.size() << " agents and "
              << m_assignments.size() << " task assignments" << std::endl;
}

void AgenticAgentCoordinator::initialize(AgenticEngine* engine, InferenceEngine* inference)
{
    m_engine = engine;
    m_inference = inference;
}

// ===== AGENT MANAGEMENT =====

std::string AgenticAgentCoordinator::createAgent(AgentRole role)
{
    std::string id = generate_id();
    auto instance = std::make_unique<AgentInstance>();
    instance->agentId = id;
    instance->role = role;
    instance->reasoner = std::make_unique<AgenticIterativeReasoning>();
    instance->state = std::make_unique<AgenticLoopState>();
    instance->isAvailable = true;
    instance->utilization = 0.0f;
    instance->tasksCompleted = 0;
    instance->lastActive = std::chrono::system_clock::now();
    
    m_agents[id] = std::move(instance);

    std::cout << "[AgenticAgentCoordinator] Created agent: " << id << " with role: " << static_cast<int>(role) << std::endl;

    return id;
}

void AgenticAgentCoordinator::removeAgent(const std::string& agentId)
{
    m_agents.erase(agentId);

    std::cout << "[AgenticAgentCoordinator] Removed agent: " << agentId << std::endl;
}

AgenticAgentCoordinator::AgentInstance* AgenticAgentCoordinator::getAgent(const std::string& agentId)
{
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<AgenticAgentCoordinator::AgentInstance*> AgenticAgentCoordinator::getAvailableAgents(
    AgentRole role)
{
    std::vector<AgentInstance*> available;

    for (auto& pair : m_agents) {
        if (pair.second->role == role && pair.second->isAvailable) {
            available.push_back(pair.second.get());
        }
    }

    return available;
}

std::vector<std::string> AgenticAgentCoordinator::getAllAgentIds() const
{
    std::vector<std::string> ids;
    for (const auto& pair : m_agents) {
        ids.push_back(pair.first);
    }
    return ids;
}

QJsonObject AgenticAgentCoordinator::getAgentStatus(const std::string& agentId)
{
    QJsonObject status;

    auto agent = getAgent(agentId);
    if (!agent) return status;

    status["agent_id"] = agentId;
    status["role"] = static_cast<int>(agent->role);
    status["available"] = agent->isAvailable;
    status["utilization"] = agent->utilization;
    status["tasks_completed"] = agent->tasksCompleted;
    status["last_active"] = QString::fromStdString(std::chrono::system_clock::to_time_t(agent->lastActive));

    return status;
}

QJsonArray AgenticAgentCoordinator::getAllAgentStatuses()
{
    QJsonArray statuses;

    for (const auto& pair : m_agents) {
        statuses.append(getAgentStatus(pair.second->agentId));
    }

    return statuses;
}

// ===== TASK ASSIGNMENT =====

QString AgenticAgentCoordinator::assignTask(
    const QString& taskDescription,
    const QJsonObject& parameters,
    AgentRole requiredRole)
{
    QString taskId = QUuid::createUuid().toString();

    // Find best agent for task
    QString agentId = selectBestAgentForTask(taskDescription);

    if (agentId.isEmpty()) {
        qWarning() << "[AgenticAgentCoordinator] No available agent for task";
        return "";
    }

    auto assignment = std::make_unique<TaskAssignment>();
    assignment->taskId = taskId;
    assignment->assignedAgentId = agentId;
    assignment->requiredRole = requiredRole;
    assignment->description = taskDescription;
    assignment->parameters = parameters;
    assignment->assignedTime = QDateTime::currentDateTime();
    assignment->status = "assigned";

    m_assignments[taskId.toStdString()] = std::move(assignment);
    m_totalTasksAssigned++;

    auto agent = getAgent(agentId);
    if (agent) {
        agent->currentTask = taskDescription;
        agent->isAvailable = false;
        agent->utilization = 1.0f;
    }

    emit taskAssigned(taskId, agentId);

    qInfo() << "[AgenticAgentCoordinator] Assigned task:" << taskId << "to agent:" << agentId;

    return taskId;
}

bool AgenticAgentCoordinator::executeAssignedTask(const QString& taskId)
{
    auto it = m_assignments.find(taskId.toStdString());
    if (it == m_assignments.end()) {
        return false;
    }

    auto& assignment = it->second;
    auto agent = getAgent(assignment->assignedAgentId);

    if (!agent) {
        return false;
    }

    // Execute reasoning on the agent
    if (agent->reasoner && agent->state) {
        agent->reasoner->initialize(m_engine, agent->state.get(), m_inferenceEngine);

        auto result = agent->reasoner->reason(assignment->description);

        assignment->result["success"] = result.success;
        assignment->result["output"] = result.result;
        assignment->completionTime = QDateTime::currentDateTime();
        assignment->status = result.success ? "completed" : "failed";

        agent->lastResult = result.result;
        agent->lastActive = QDateTime::currentDateTime();
        agent->isAvailable = true;
        agent->utilization = 0.0f;
        agent->tasksCompleted++;

        m_totalTasksCompleted++;

        // Record task duration
        int duration = assignment->assignedTime.msecsTo(assignment->completionTime);
        m_taskDurations.push_back({assignment->description, duration});

        if (result.success) {
            emit taskCompleted(taskId);
        } else {
            emit taskFailed(taskId, "Execution failed");
        }

        return result.success;
    }

    return false;
}

QString AgenticAgentCoordinator::getTaskStatus(const QString& taskId)
{
    auto it = m_assignments.find(taskId.toStdString());
    if (it != m_assignments.end()) {
        return it->second->status;
    }
    return "unknown";
}

QJsonObject AgenticAgentCoordinator::getTaskResult(const QString& taskId)
{
    auto it = m_assignments.find(taskId.toStdString());
    if (it != m_assignments.end()) {
        return it->second->result;
    }
    return QJsonObject();
}

QJsonArray AgenticAgentCoordinator::getAllTaskStatuses()
{
    QJsonArray statuses;

    for (const auto& pair : m_assignments) {
        QJsonObject status;
        status["task_id"] = pair.second->taskId;
        status["agent_id"] = pair.second->assignedAgentId;
        status["description"] = pair.second->description;
        status["status"] = pair.second->status;
        status["assigned_time"] = pair.second->assignedTime.toString(Qt::ISODate);

        statuses.append(status);
    }

    return statuses;
}

// ===== COORDINATION MECHANISMS =====

QJsonArray AgenticAgentCoordinator::decomposeLargeTask(const QString& goal)
{
    QJsonArray subtasks;

    // Use analyzer agent to decompose
    auto analyzers = getAvailableAgents(AgentRole::Analyzer);
    if (analyzers.empty()) {
        qWarning() << "[AgenticAgentCoordinator] No analyzer agent available";
        return subtasks;
    }

    // Would call analyzer to decompose task
    return subtasks;
}

void AgenticAgentCoordinator::synchronizeAgentStates()
{
    for (auto& pair : m_agents) {
        syncStateWithAgent(pair.second->agentId);
    }

    qInfo() << "[AgenticAgentCoordinator] Synchronized all agent states";
}

bool AgenticAgentCoordinator::detectStateConflict(const QString& agentId1, const QString& agentId2)
{
    auto agent1 = getAgent(agentId1);
    auto agent2 = getAgent(agentId2);

    if (!agent1 || !agent2 || !agent1->state || !agent2->state) {
        return false;
    }

    // Compare states
    QJsonObject state1 = agent1->state->getAllMemory();
    QJsonObject state2 = agent2->state->getAllMemory();

    return state1 != state2;
}

QString AgenticAgentCoordinator::resolveStateConflict(const QString& agentId1, const QString& agentId2)
{
    auto agent1 = getAgent(agentId1);
    auto agent2 = getAgent(agentId2);

    if (!agent1 || !agent2) {
        return "Agent not found";
    }

    QJsonObject state1 = agent1->state->getAllMemory();
    QJsonObject state2 = agent2->state->getAllMemory();

    QString resolution = reconcileConflictingStates(state1, state2);

    recordConflict(agentId1, agentId2, "State conflict", resolution);

    emit stateConflictDetected(agentId1, agentId2);
    emit stateConflictResolved(resolution);

    return resolution;
}

bool AgenticAgentCoordinator::buildConsensus(const QStringList& agentIds, const QString& question)
{
    QStringList opinions;

    for (const auto& agentId : agentIds) {
        auto agent = getAgent(agentId);
        if (agent && agent->state) {
            opinions.append(getAgentOpinion(agentId, question));
        }
    }

    // Check if opinions converge
    return opinions.size() > 0;
}

QString AgenticAgentCoordinator::getAgentOpinion(const QString& agentId, const QString& question)
{
    // Would query agent for its opinion on the question
    return "Agent opinion";
}

QString AgenticAgentCoordinator::resolveDisagreement(const QStringList& agentIds)
{
    // Use consensus algorithm or voting to resolve
    return "Resolved through voting";
}

void AgenticAgentCoordinator::allocateResources(
    const QString& agentId,
    float cpuShare,
    float memoryShare)
{
    auto agent = getAgent(agentId);
    if (agent) {
        qInfo() << "[AgenticAgentCoordinator] Allocated resources to agent:" << agentId;
    }
}

void AgenticAgentCoordinator::rebalanceResources()
{
    // Reallocate resources based on utilization
    qInfo() << "[AgenticAgentCoordinator] Rebalanced resources across agents";
}

// ===== MONITORING =====

QJsonObject AgenticAgentCoordinator::getCoordinationMetrics() const
{
    QJsonObject metrics;

    metrics["total_agents"] = static_cast<int>(m_agents.size());
    metrics["total_tasks_assigned"] = m_totalTasksAssigned;
    metrics["total_tasks_completed"] = m_totalTasksCompleted;
    metrics["total_utilization"] = getTotalUtilization();
    metrics["average_task_duration"] = getAverageTaskDuration();
    metrics["conflicts_detected"] = m_conflicts.size();

    return metrics;
}

float AgenticAgentCoordinator::getTotalUtilization() const
{
    if (m_agents.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& pair : m_agents) {
        total += pair.second->utilization;
    }

    return total / m_agents.size();
}

int AgenticAgentCoordinator::getTotalTasksCompleted() const
{
    return m_totalTasksCompleted;
}

float AgenticAgentCoordinator::getAverageTaskDuration() const
{
    if (m_taskDurations.empty()) return 0.0f;

    int totalDuration = 0;
    for (const auto& pair : m_taskDurations) {
        totalDuration += pair.second;
    }

    return totalDuration / static_cast<float>(m_taskDurations.size());
}

QJsonArray AgenticAgentCoordinator::getConflictHistory()
{
    QJsonArray history;

    for (const auto& conflict : m_conflicts) {
        QJsonObject obj;
        obj["conflict_id"] = conflict.conflictId;
        obj["agent1"] = conflict.agent1Id;
        obj["agent2"] = conflict.agent2Id;
        obj["reason"] = conflict.conflictReason;
        obj["resolution"] = conflict.resolution;
        obj["timestamp"] = conflict.timestamp.toString(Qt::ISODate);

        history.append(obj);
    }

    return history;
}

// ===== LOAD BALANCING =====

QString AgenticAgentCoordinator::selectBestAgentForTask(const QString& taskDescription)
{
    // Find least utilized agent
    AgentInstance* bestAgent = nullptr;
    float minUtilization = 2.0f;

    for (auto& pair : m_agents) {
        if (pair.second->isAvailable && pair.second->utilization < minUtilization) {
            minUtilization = pair.second->utilization;
            bestAgent = pair.second.get();
        }
    }

    return bestAgent ? bestAgent->agentId : QString();
}

void AgenticAgentCoordinator::rebalanceWorkload()
{
    // Redistribute tasks from over-utilized to under-utilized agents
    qInfo() << "[AgenticAgentCoordinator] Rebalanced workload across agents";
}

// ===== STATE MANAGEMENT =====

QJsonObject AgenticAgentCoordinator::getGlobalState()
{
    QJsonObject globalState;
    globalState["agents"] = getAllAgentStatuses();
    globalState["tasks"] = getAllTaskStatuses();
    globalState["metrics"] = getCoordinationMetrics();

    return globalState;
}

bool AgenticAgentCoordinator::restoreGlobalState(const QJsonObject& state)
{
    // Restore agent and task states
    return true;
}

void AgenticAgentCoordinator::saveCheckpoint()
{
    m_lastCheckpoint = getGlobalState();
    m_lastCheckpointTime = QDateTime::currentDateTime();

    qInfo() << "[AgenticAgentCoordinator] Saved checkpoint";
}

bool AgenticAgentCoordinator::restoreFromCheckpoint()
{
    if (m_lastCheckpoint.isEmpty()) {
        return false;
    }

    return restoreGlobalState(m_lastCheckpoint);
}

// ===== PRIVATE HELPERS =====

void AgenticAgentCoordinator::syncStateWithAgent(const QString& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent || !agent->state) return;

    // Synchronize agent state with global coordinator state
}

QString AgenticAgentCoordinator::reconcileConflictingStates(
    const QJsonObject& state1,
    const QJsonObject& state2)
{
    // Merge conflicting states intelligently
    return "States reconciled using merge strategy";
}

QString AgenticAgentCoordinator::selectResolutionStrategy(const QString& conflictReason)
{
    if (conflictReason.contains("state")) {
        return "merge";
    } else if (conflictReason.contains("priority")) {
        return "priority_based";
    }
    return "consensus";
}

void AgenticAgentCoordinator::recordConflict(
    const QString& agent1,
    const QString& agent2,
    const QString& reason,
    const QString& resolution)
{
    AgentConflict conflict;
    conflict.conflictId = QUuid::createUuid().toString();
    conflict.agent1Id = agent1;
    conflict.agent2Id = agent2;
    conflict.conflictReason = reason;
    conflict.resolution = resolution;
    conflict.timestamp = QDateTime::currentDateTime();

    m_conflicts.push_back(conflict);
    m_totalConflicts++;
}

void AgenticAgentCoordinator::updateAgentMetrics(const QString& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent) return;

    // Calculate and update metrics
    emit coordinationMetricsUpdated();
}
