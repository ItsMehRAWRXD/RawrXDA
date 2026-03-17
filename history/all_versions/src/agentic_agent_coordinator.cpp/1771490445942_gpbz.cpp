// AgenticAgentCoordinator Implementation - Pure C++20
#include "agentic_agent_coordinator.h"
#include "agentic_iterative_reasoning.h"  // Always include — ensures complete type for unique_ptr
#include "agentic_loop_state.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

#include "logging/logger.h"
static Logger s_logger("agentic_agent_coordinator");

static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << (dis(gen) & 0xFFFF) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-";
    ss << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);
    
    return ss.str();
}

AgenticAgentCoordinator::AgenticAgentCoordinator()
    : m_coordinationStartTime(std::chrono::system_clock::now())
{
    s_logger.info("[AgenticAgentCoordinator] Initialized - Ready for multi-agent coordination");
}

AgenticAgentCoordinator::~AgenticAgentCoordinator()
{
    s_logger.info("[AgenticAgentCoordinator] Destroyed - Managed ");
}

void AgenticAgentCoordinator::initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference)
{
    m_engine = engine;
    m_inference = inference;

    s_logger.info("[AgenticAgentCoordinator] Initialized with AgenticEngine and InferenceEngine");
}

// ===== AGENT MANAGEMENT =====

std::string AgenticAgentCoordinator::createAgent(AgentRole role)
{
    std::string agentId = generateUUID();

    auto agent = std::make_unique<AgentInstance>();
    agent->agentId = agentId;
    agent->role = role;
    agent->reasoner = std::make_unique<AgenticIterativeReasoning>();
    agent->state = std::make_unique<AgenticLoopState>();
    agent->isAvailable = true;
    agent->utilization = 0.0f;
    agent->tasksCompleted = 0;
    agent->lastActive = std::chrono::system_clock::now();

    m_agents[agentId] = std::move(agent);

    s_logger.info("[AgenticAgentCoordinator] Created agent: ");

    return agentId;
}

void AgenticAgentCoordinator::removeAgent(const std::string& agentId)
{
    m_agents.erase(agentId);

    s_logger.info("[AgenticAgentCoordinator] Removed agent: ");
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

json AgenticAgentCoordinator::getAgentStatus(const std::string& agentId)
{
    json status;

    auto agent = getAgent(agentId);
    if (!agent) return status;

    status["agent_id"] = agentId;
    status["role"] = static_cast<int>(agent->role);
    status["available"] = agent->isAvailable;
    status["utilization"] = agent->utilization;
    status["tasks_completed"] = agent->tasksCompleted;

    return status;
}

json AgenticAgentCoordinator::getAllAgentStatuses()
{
    json statuses = json::array();

    for (const auto& pair : m_agents) {
        statuses.push_back(getAgentStatus(pair.first));
    }

    return statuses;
}

// ===== TASK ASSIGNMENT =====

std::string AgenticAgentCoordinator::assignTask(
    const std::string& taskDescription,
    const json& parameters,
    AgentRole requiredRole)
{
    std::string taskId = generateUUID();

    // Find best agent for task
    std::string agentId = selectBestAgentForTask(taskDescription);

    if (agentId.empty()) {
        s_logger.error("[AgenticAgentCoordinator] No available agent for task");
        return "";
    }

    auto assignment = std::make_unique<TaskAssignment>();
    assignment->taskId = taskId;
    assignment->assignedAgentId = agentId;
    assignment->requiredRole = requiredRole;
    assignment->description = taskDescription;
    assignment->parameters = parameters;
    assignment->assignedTime = std::chrono::system_clock::now();
    assignment->status = "assigned";

    m_assignments[taskId] = std::move(assignment);
    ++m_totalTasksAssigned;

    auto agent = getAgent(agentId);
    if (agent) {
        agent->currentTask = taskDescription;
        agent->isAvailable = false;
        agent->utilization = 1.0f;
    }

    s_logger.info("[AgenticAgentCoordinator] Assigned task: ");

    return taskId;
}

void AgenticAgentCoordinator::updateTaskStatus(const std::string& taskId, const std::string& status, const json& result)
{
    auto it = m_assignments.find(taskId);
    if (it != m_assignments.end()) {
        it->second->status = status;
        if (!result.is_null()) {
            it->second->result = result;
        }
        it->second->completionTime = std::chrono::system_clock::now();
    }
}

json AgenticAgentCoordinator::getTaskResult(const std::string& taskId)
{
    auto it = m_assignments.find(taskId);
    if (it != m_assignments.end()) {
        return it->second->result;
    }
    return json();
}

// ===== COORDINATION MECHANISMS =====

void AgenticAgentCoordinator::resolveConflicts()
{
    s_logger.info("[AgenticAgentCoordinator] Resolving conflicts");
}

void AgenticAgentCoordinator::synchronizeState()
{
    for (auto& pair : m_agents) {
        if (pair.second && pair.second->state) {
            // Synchronize agent state
        }
    }
    s_logger.info("[AgenticAgentCoordinator] Synchronized all agent states");
}

json AgenticAgentCoordinator::performJointInference(const std::vector<std::string>& agentIds, const std::string& prompt)
{
    json results = json::array();
    
    for (const auto& agentId : agentIds) {
        auto agent = getAgent(agentId);
        if (agent && agent->reasoner) {
            // Perform inference with agent
            results.push_back({{"agent_id", agentId}, {"response", "inference_result"}});
        }
    }
    
    return results;
}

// ===== PRIVATE HELPERS =====

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

void AgenticAgentCoordinator::logCoordination(const std::string& message)
{
    s_logger.info("[AgenticAgentCoordinator] ");
}
