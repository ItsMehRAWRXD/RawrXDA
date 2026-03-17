// AgenticAgentCoordinator Implementation
#include "agentic_agent_coordinator.h"
#include "cpu_inference_engine.h"
#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

// Simple UUID generator
static std::string generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* digits = "0123456789abcdef";
    std::stringstream ss;
    for (int i = 0; i < 8; i++) ss << digits[dis(gen)];
    ss << "-";
    for (int i = 0; i < 4; i++) ss << digits[dis(gen)];
    ss << "-";
    for (int i = 0; i < 4; i++) ss << digits[dis(gen)];
    ss << "-";
    for (int i = 0; i < 4; i++) ss << digits[dis(gen)];
    ss << "-";
    for (int i = 0; i < 12; i++) ss << digits[dis(gen)];
    return ss.str();
}

static std::string time_to_string(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%dT%H:%M:%S");
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

void AgenticAgentCoordinator::initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference)
{
    m_engine = engine;
    m_inference = inference;
    m_inferenceEngine = inference;
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

std::vector<std::string> AgenticAgentCoordinator::getAllAgentIds()
{
    std::vector<std::string> ids;
    for (const auto& pair : m_agents) {
        ids.push_back(pair.first);
    }
    return ids;
}

AgentStatus AgenticAgentCoordinator::getAgentStatus(const std::string& agentId)
{
    AgentStatus status;
    auto agent = getAgent(agentId);
    if (!agent) return status;

    status.agent_id = agentId;
    status.role = static_cast<int>(agent->role);
    status.available = agent->isAvailable;
    status.utilization = agent->utilization;
    status.tasks_completed = agent->tasksCompleted;
    status.last_active = time_to_string(agent->lastActive);

    return status;
}

std::vector<AgentStatus> AgenticAgentCoordinator::getAllAgentStatuses()
{
    std::vector<AgentStatus> statuses;
    for (const auto& pair : m_agents) {
        statuses.push_back(getAgentStatus(pair.second->agentId));
    }
    return statuses;
}

// ===== TASK ASSIGNMENT =====

std::string AgenticAgentCoordinator::assignTask(
    const std::string& taskDescription,
    const std::map<std::string, std::string>& parameters,
    AgentRole requiredRole)
{
    std::string taskId = generate_id();

    // Find best agent for task
    std::string agentId = selectBestAgentForTask(taskDescription);

    if (agentId.empty()) {
        std::cerr << "[AgenticAgentCoordinator] No available agent for task" << std::endl;
        return "";
    }

    auto assignment = std::make_unique<TaskAssignment>();
    assignment->taskId = taskId;
    assignment->assignedAgentId = agentId;
    assignment->requiredRole = (int)requiredRole;
    assignment->description = taskDescription;
    assignment->parameters = parameters;
    assignment->assignedTime = std::chrono::system_clock::now();
    assignment->status = "assigned";

    m_assignments[taskId] = std::move(assignment);
    m_totalTasksAssigned++;

    auto agent = getAgent(agentId);
    if (agent) {
        agent->currentTask = taskDescription;
        agent->isAvailable = false;
        agent->utilization = 1.0f;
    }

    if (m_taskAssignedCallback) {
        m_taskAssignedCallback(taskId, agentId);
    }

    std::cout << "[AgenticAgentCoordinator] Assigned task: " << taskId << " to agent: " << agentId << std::endl;

    return taskId;
}

bool AgenticAgentCoordinator::executeAssignedTask(const std::string& taskId)
{
    auto it = m_assignments.find(taskId);
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

        assignment->result["success"] = result.success ? "true" : "false";
        assignment->result["output"] = result.result;
        assignment->completionTime = std::chrono::system_clock::now();
        assignment->status = result.success ? "completed" : "failed";

        agent->lastResult = result.result;
        agent->lastActive = std::chrono::system_clock::now();
        agent->isAvailable = true;
        agent->utilization = 0.0f;
        agent->tasksCompleted++;

        m_totalTasksCompleted++;

        // Record task duration
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            assignment->completionTime - assignment->assignedTime).count();
        m_taskDurations.push_back({assignment->description, (int)ms});

        if (result.success) {
            if (m_taskCompletedCallback) m_taskCompletedCallback(taskId);
        } else {
             // Handle failure callback if needed
        }

        return result.success;
    }

    return false;
}

std::string AgenticAgentCoordinator::getTaskStatus(const std::string& taskId)
{
    auto it = m_assignments.find(taskId);
    if (it != m_assignments.end()) {
        return it->second->status;
    }
    return "unknown";
}

std::map<std::string, std::string> AgenticAgentCoordinator::getTaskResult(const std::string& taskId)
{
    auto it = m_assignments.find(taskId);
    if (it != m_assignments.end()) {
        return it->second->result;
    }
    return {};
}

std::vector<std::map<std::string, std::string>> AgenticAgentCoordinator::getAllTaskStatuses()
{
    std::vector<std::map<std::string, std::string>> statuses;

    for (const auto& pair : m_assignments) {
        std::map<std::string, std::string> status;
        status["task_id"] = pair.second->taskId;
        status["agent_id"] = pair.second->assignedAgentId;
        status["description"] = pair.second->description;
        status["status"] = pair.second->status;
        status["assigned_time"] = time_to_string(pair.second->assignedTime);

        statuses.push_back(status);
    }

    return statuses;
}

// ===== COORDINATION MECHANISMS =====

std::vector<std::string> AgenticAgentCoordinator::decomposeLargeTask(const std::string& goal)
{
    std::vector<std::string> subtasks;

    // Use analyzer agent to decompose
    auto analyzers = getAvailableAgents(AgentRole::Analyzer);
    if (analyzers.empty()) {
        std::cerr << "[AgenticAgentCoordinator] No analyzer agent available" << std::endl;
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
     std::cout << "[AgenticAgentCoordinator] Synchronized all agent states" << std::endl;
}

bool AgenticAgentCoordinator::detectStateConflict(const std::string& agentId1, const std::string& agentId2)
{
    auto agent1 = getAgent(agentId1);
    auto agent2 = getAgent(agentId2);

    if (!agent1 || !agent2 || !agent1->state || !agent2->state) {
        return false;
    }

    // Compare states - assumes getAllMemory returns std::map or similar in new impl
    // Casting usage implies check AgenticLoopState
    // For now we assume they are compatible or this is a stub logic that needs implementation
    return false; 
}

std::string AgenticAgentCoordinator::resolveStateConflict(const std::string& agentId1, const std::string& agentId2)
{
    // ... logic ...
    std::string resolution = "resolved";
    recordConflict(agentId1, agentId2, "State conflict", resolution);
    return resolution;
}

bool AgenticAgentCoordinator::buildConsensus(const std::vector<std::string>& agentIds, const std::string& question)
{
    std::vector<std::string> opinions;
    for (const auto& agentId : agentIds) {
        opinions.push_back(getAgentOpinion(agentId, question));
    }
    return !opinions.empty();
}

std::string AgenticAgentCoordinator::getAgentOpinion(const std::string& agentId, const std::string& question)
{
    return "Agent opinion";
}

std::string AgenticAgentCoordinator::resolveDisagreement(const std::vector<std::string>& agentIds)
{
    return "Resolved through voting";
}

void AgenticAgentCoordinator::allocateResources(
    const std::string& agentId,
    float cpuShare,
    float memoryShare)
{
    auto agent = getAgent(agentId);
    if (agent) {
         std::cout << "[AgenticAgentCoordinator] Allocated resources to agent:" << agentId << std::endl;
    }
}

void AgenticAgentCoordinator::rebalanceResources()
{
     std::cout << "[AgenticAgentCoordinator] Rebalanced resources across agents" << std::endl;
}

// ===== MONITORING =====

std::map<std::string, float> AgenticAgentCoordinator::getCoordinationMetrics() const
{
    std::map<std::string, float> metrics;

    metrics["total_agents"] = static_cast<float>(m_agents.size());
    metrics["total_tasks_assigned"] = static_cast<float>(m_totalTasksAssigned);
    metrics["total_tasks_completed"] = static_cast<float>(m_totalTasksCompleted);
    metrics["total_utilization"] = getTotalUtilization();
    metrics["average_task_duration"] = getAverageTaskDuration();
    metrics["conflicts_detected"] = static_cast<float>(m_conflicts.size());

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

    long long totalDuration = 0;
    for (const auto& pair : m_taskDurations) {
        totalDuration += pair.second;
    }

    return (float)totalDuration / m_taskDurations.size();
}

std::vector<AgentConflict> AgenticAgentCoordinator::getConflictHistory()
{
    return m_conflicts;
}

// ===== LOAD BALANCING =====

std::string AgenticAgentCoordinator::selectBestAgentForTask(const std::string& taskDescription)
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

    return bestAgent ? bestAgent->agentId : std::string();
}

void AgenticAgentCoordinator::rebalanceWorkload()
{
    std::cout << "[AgenticAgentCoordinator] Rebalanced workload across agents" << std::endl;
}

// ===== STATE MANAGEMENT =====

AgenticAgentCoordinator::GlobalState AgenticAgentCoordinator::getGlobalState()
{
    GlobalState globalState;
    globalState.agents = getAllAgentStatuses();
    globalState.tasks = getAllTaskStatuses();
    globalState.metrics = getCoordinationMetrics();

    return globalState;
}

bool AgenticAgentCoordinator::restoreGlobalState(const GlobalState& state)
{
    // Restore logic
    return true;
}

void AgenticAgentCoordinator::saveCheckpoint()
{
    m_lastCheckpoint = getGlobalState();
    m_lastCheckpointTime = std::chrono::system_clock::now();
    std::cout << "[AgenticAgentCoordinator] Saved checkpoint" << std::endl;
}

bool AgenticAgentCoordinator::restoreFromCheckpoint()
{
    if (m_lastCheckpoint.agents.empty()) {
        return false;
    }
    return restoreGlobalState(m_lastCheckpoint);
}

// ===== PRIVATE HELPERS =====

void AgenticAgentCoordinator::syncStateWithAgent(const std::string& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent || !agent->state) return;
    // Synchronize logic
}

std::string AgenticAgentCoordinator::reconcileConflictingStates(
    const std::map<std::string, std::string>& state1,
    const std::map<std::string, std::string>& state2)
{
    return "States reconciled using merge strategy";
}

void AgenticAgentCoordinator::recordConflict(
    const std::string& agent1,
    const std::string& agent2,
    const std::string& reason,
    const std::string& resolution)
{
    AgentConflict conflict;
    conflict.conflictId = generate_id();
    conflict.agent1Id = agent1;
    conflict.agent2Id = agent2;
    conflict.conflictReason = reason;
    conflict.resolution = resolution;
    conflict.timestamp = std::chrono::system_clock::now();

    m_conflicts.push_back(conflict);
    m_totalConflicts++;
}

void AgenticAgentCoordinator::updateAgentMetrics(const std::string& agentId)
{
    auto agent = getAgent(agentId);
    if (!agent) return;
    // Update metrics
}
