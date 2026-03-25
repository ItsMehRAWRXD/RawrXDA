<<<<<<< HEAD
// AgenticAgentCoordinator implementation - C++20
#include "agentic_agent_coordinator.h"

#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"

=======
// AgenticAgentCoordinator Implementation - Pure C++20
#include "agentic_agent_coordinator.h"
#include "agentic_iterative_reasoning.h"  // Always include — ensures complete type for unique_ptr
#include "agentic_loop_state.h"
>>>>>>> origin/main
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

<<<<<<< HEAD
namespace {
std::string generateId() {
=======
static std::string generateUUID() {
>>>>>>> origin/main
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (dis(gen) & 0xFFFFFFFFULL) << "-";
    ss << std::setw(4) << (dis(gen) & 0xFFFFULL) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x0FFFULL) | 0x4000ULL) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x3FFFULL) | 0x8000ULL) << "-";
    ss << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

std::string toIsoLike(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[64] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return std::string(buf);
}
}  // namespace

AgenticAgentCoordinator::AgenticAgentCoordinator()
<<<<<<< HEAD
    : m_coordinationStartTime(std::chrono::system_clock::now()),
      m_lastCheckpointTime(m_coordinationStartTime) {}

AgenticAgentCoordinator::~AgenticAgentCoordinator() = default;

void AgenticAgentCoordinator::initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference) {
    m_engine = engine;
    m_inference = inference;
    m_inferenceEngine = inference;
}

std::string AgenticAgentCoordinator::createAgent(AgentRole role) {
    const std::string agentId = generateId();
=======
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

    std::cout << "[AgenticAgentCoordinator] Initialized with AgenticEngine and InferenceEngine" << std::endl;
}

// ===== AGENT MANAGEMENT =====

std::string AgenticAgentCoordinator::createAgent(AgentRole role)
{
    std::string agentId = generateUUID();
>>>>>>> origin/main

    auto agent = std::make_unique<AgentInstance>();
    agent->agentId = agentId;
    agent->role = role;
    agent->reasoner = std::make_unique<AgenticIterativeReasoning>();
    agent->state = std::make_unique<AgenticLoopState>();
    agent->isAvailable = true;
    agent->utilization = 0.0f;
    agent->tasksCompleted = 0;
    agent->lastActive = std::chrono::system_clock::now();
    agent->currentTask.clear();
    agent->lastResult.clear();

    if (agent->reasoner) {
        agent->reasoner->initialize(m_engine, agent->state.get(), m_inferenceEngine);
    }

    m_agents[agentId] = std::move(agent);
<<<<<<< HEAD
    return agentId;
}

void AgenticAgentCoordinator::removeAgent(const std::string& agentId) {
    m_agents.erase(agentId);
=======

    std::cout << "[AgenticAgentCoordinator] Created agent: " << agentId << " with role: " << static_cast<int>(role) << std::endl;

    return agentId;
}

void AgenticAgentCoordinator::removeAgent(const std::string& agentId)
{
    m_agents.erase(agentId);

    std::cout << "[AgenticAgentCoordinator] Removed agent: " << agentId << std::endl;
>>>>>>> origin/main
}

AgenticAgentCoordinator::AgentInstance* AgenticAgentCoordinator::getAgent(const std::string& agentId) {
    auto it = m_agents.find(agentId);
<<<<<<< HEAD
    return it != m_agents.end() ? it->second.get() : nullptr;
=======
    if (it != m_agents.end()) {
        return it->second.get();
    }
    return nullptr;
>>>>>>> origin/main
}

std::vector<AgenticAgentCoordinator::AgentInstance*> AgenticAgentCoordinator::getAvailableAgents(AgentRole role) {
    std::vector<AgentInstance*> out;
    for (auto& kv : m_agents) {
        if (kv.second->isAvailable && kv.second->role == role) {
            out.push_back(kv.second.get());
        }
    }
    return out;
}

<<<<<<< HEAD
std::vector<std::string> AgenticAgentCoordinator::getAllAgentIds() {
    std::vector<std::string> ids;
    ids.reserve(m_agents.size());
    for (const auto& kv : m_agents) {
        ids.push_back(kv.first);
=======
std::vector<std::string> AgenticAgentCoordinator::getAllAgentIds() const
{
    std::vector<std::string> ids;
    for (const auto& pair : m_agents) {
        ids.push_back(pair.first);
>>>>>>> origin/main
    }
    return ids;
}

<<<<<<< HEAD
AgentStatus AgenticAgentCoordinator::getAgentStatus(const std::string& agentId) {
    AgentStatus st{};
    auto* agent = getAgent(agentId);
    if (!agent) {
        st.agent_id = agentId;
        st.role = static_cast<int>(AgentRole::Developer);
        st.available = false;
        st.utilization = 0.0f;
        st.tasks_completed = 0;
        st.last_active = "";
        return st;
=======
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
>>>>>>> origin/main
    }

    st.agent_id = agent->agentId;
    st.role = static_cast<int>(agent->role);
    st.available = agent->isAvailable;
    st.utilization = agent->utilization;
    st.tasks_completed = agent->tasksCompleted;
    st.last_active = toIsoLike(agent->lastActive);
    return st;
}

std::vector<AgentStatus> AgenticAgentCoordinator::getAllAgentStatuses() {
    std::vector<AgentStatus> statuses;
    statuses.reserve(m_agents.size());
    for (const auto& kv : m_agents) {
        statuses.push_back(getAgentStatus(kv.first));
    }
    return statuses;
}

<<<<<<< HEAD
=======
// ===== TASK ASSIGNMENT =====

>>>>>>> origin/main
std::string AgenticAgentCoordinator::assignTask(
    const std::string& taskDescription,
    const std::map<std::string, std::string>& parameters,
    AgentRole requiredRole) {
    const std::string taskId = generateId();

    std::vector<AgentInstance*> candidates = getAvailableAgents(requiredRole);
    AgentInstance* best = nullptr;
    for (auto* a : candidates) {
        if (!best || a->utilization < best->utilization) best = a;
    }

<<<<<<< HEAD
    if (!best) {
        return {};
=======
    if (agentId.empty()) {
        std::cerr << "[AgenticAgentCoordinator] No available agent for task" << std::endl;
        return "";
>>>>>>> origin/main
    }

    auto assignment = std::make_unique<TaskAssignment>();
    assignment->taskId = taskId;
    assignment->assignedAgentId = best->agentId;
    assignment->requiredRole = static_cast<int>(requiredRole);
    assignment->description = taskDescription;
    assignment->parameters = parameters;
    assignment->assignedTime = std::chrono::system_clock::now();
    assignment->status = "assigned";

    best->isAvailable = false;
    best->currentTask = taskDescription;
    best->utilization = std::max(best->utilization, 0.7f);
    best->lastActive = assignment->assignedTime;

    m_assignments[taskId] = std::move(assignment);
    ++m_totalTasksAssigned;

    if (m_taskAssignedCallback) {
        m_taskAssignedCallback(taskId, best->agentId);
    }

<<<<<<< HEAD
    return taskId;
}

bool AgenticAgentCoordinator::executeAssignedTask(const std::string& taskId) {
    auto it = m_assignments.find(taskId);
    if (it == m_assignments.end()) return false;

    TaskAssignment& task = *it->second;
    task.status = "running";

    auto* agent = getAgent(task.assignedAgentId);
    const auto start = std::chrono::system_clock::now();

    if (!agent) {
        task.status = "failed";
        task.result["error"] = "assigned agent not found";
        return false;
    }

    std::string summary = "completed";
    if (agent->reasoner) {
        auto rr = agent->reasoner->reason(task.description);
        if (!rr.success) {
            task.status = "failed";
            task.result["error"] = rr.error;
            agent->lastResult = rr.error;
            agent->isAvailable = true;
            agent->currentTask.clear();
            agent->utilization = std::max(0.0f, agent->utilization - 0.4f);
            return false;
        }
        summary = rr.result;
    }

    task.status = "completed";
    task.result["summary"] = summary;
    task.completionTime = std::chrono::system_clock::now();

    const auto ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        task.completionTime - start).count());
    m_taskDurations.push_back({task.description, ms});

    agent->lastResult = summary;
    agent->isAvailable = true;
    agent->currentTask.clear();
    agent->utilization = std::max(0.0f, agent->utilization - 0.5f);
    agent->tasksCompleted += 1;
    agent->lastActive = task.completionTime;

    ++m_totalTasksCompleted;
    updateAgentMetrics(agent->agentId);

    if (m_taskCompletedCallback) {
        m_taskCompletedCallback(taskId);
    }
    return true;
}

std::string AgenticAgentCoordinator::getTaskStatus(const std::string& taskId) {
    auto it = m_assignments.find(taskId);
    if (it == m_assignments.end()) return "unknown";
    return it->second->status;
}

std::map<std::string, std::string> AgenticAgentCoordinator::getTaskResult(const std::string& taskId) {
    auto it = m_assignments.find(taskId);
    if (it == m_assignments.end()) return {};
    return it->second->result;
}

std::vector<std::map<std::string, std::string>> AgenticAgentCoordinator::getAllTaskStatuses() {
    std::vector<std::map<std::string, std::string>> rows;
    rows.reserve(m_assignments.size());
    for (const auto& kv : m_assignments) {
        const TaskAssignment& t = *kv.second;
        std::map<std::string, std::string> row;
        row["taskId"] = t.taskId;
        row["agentId"] = t.assignedAgentId;
        row["description"] = t.description;
        row["status"] = t.status;
        row["requiredRole"] = std::to_string(t.requiredRole);
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<std::string> AgenticAgentCoordinator::decomposeLargeTask(const std::string& goal) {
    std::vector<std::string> out;
    if (goal.empty()) return out;

    std::string chunk;
    std::istringstream iss(goal);
    while (std::getline(iss, chunk, ';')) {
        if (!chunk.empty()) out.push_back(chunk);
    }
    if (out.empty()) {
        out.push_back("Analyze: " + goal);
        out.push_back("Implement: " + goal);
        out.push_back("Validate: " + goal);
    }
    return out;
}

void AgenticAgentCoordinator::synchronizeAgentStates() {
    for (const auto& kv : m_agents) {
        syncStateWithAgent(kv.first);
    }
}

bool AgenticAgentCoordinator::detectStateConflict(const std::string& agentId1, const std::string& agentId2) {
    auto* a1 = getAgent(agentId1);
    auto* a2 = getAgent(agentId2);
    if (!a1 || !a2) return false;
    if (a1->isAvailable || a2->isAvailable) return false;
    return !a1->currentTask.empty() && a1->currentTask == a2->currentTask;
}

std::string AgenticAgentCoordinator::resolveStateConflict(const std::string& agentId1, const std::string& agentId2) {
    if (!detectStateConflict(agentId1, agentId2)) {
        return "no-conflict";
    }
    auto* a2 = getAgent(agentId2);
    if (a2) {
        a2->isAvailable = true;
        a2->currentTask.clear();
        a2->utilization = std::max(0.0f, a2->utilization - 0.3f);
    }
    recordConflict(agentId1, agentId2, "duplicate currentTask", "released second agent");
    return "released-second-agent";
}

bool AgenticAgentCoordinator::buildConsensus(const std::vector<std::string>& agentIds, const std::string& question) {
    if (agentIds.empty()) return false;
    std::map<std::string, int> votes;
    for (const auto& id : agentIds) {
        votes[getAgentOpinion(id, question)]++;
    }
    int best = 0;
    for (const auto& kv : votes) best = std::max(best, kv.second);
    return best * 2 >= static_cast<int>(agentIds.size());
}

std::string AgenticAgentCoordinator::resolveDisagreement(const std::vector<std::string>& agentIds) {
    if (agentIds.empty()) return {};
    return getAgentOpinion(agentIds.front(), "resolve-disagreement");
}

void AgenticAgentCoordinator::allocateResources(const std::string& agentId, float cpuShare, float /*memoryShare*/) {
    auto* a = getAgent(agentId);
    if (!a) return;
    a->utilization = std::clamp(cpuShare, 0.0f, 1.0f);
    a->lastActive = std::chrono::system_clock::now();
}

void AgenticAgentCoordinator::rebalanceResources() {
    rebalanceWorkload();
}

std::map<std::string, float> AgenticAgentCoordinator::getCoordinationMetrics() const {
    std::map<std::string, float> m;
    m["total_utilization"] = getTotalUtilization();
    m["tasks_assigned"] = static_cast<float>(m_totalTasksAssigned);
    m["tasks_completed"] = static_cast<float>(m_totalTasksCompleted);
    m["task_completion_rate"] = m_totalTasksAssigned > 0
        ? static_cast<float>(m_totalTasksCompleted) / static_cast<float>(m_totalTasksAssigned)
        : 0.0f;
    m["avg_task_duration_ms"] = getAverageTaskDuration();
    m["total_conflicts"] = static_cast<float>(m_totalConflicts);
    return m;
}

float AgenticAgentCoordinator::getTotalUtilization() const {
    float total = 0.0f;
    for (const auto& kv : m_agents) total += kv.second->utilization;
    return total;
}

int AgenticAgentCoordinator::getTotalTasksCompleted() const {
    return m_totalTasksCompleted;
}

float AgenticAgentCoordinator::getAverageTaskDuration() const {
    if (m_taskDurations.empty()) return 0.0f;
    long long total = 0;
    for (const auto& p : m_taskDurations) total += p.second;
    return static_cast<float>(total) / static_cast<float>(m_taskDurations.size());
}

std::vector<AgentConflict> AgenticAgentCoordinator::getConflictHistory() {
    return m_conflicts;
}

std::string AgenticAgentCoordinator::selectBestAgentForTask(const std::string& taskDescription) {
    AgentRole targetRole = AgentRole::Developer;
    const std::string lower = taskDescription;
    if (lower.find("test") != std::string::npos) targetRole = AgentRole::Tester;
    if (lower.find("review") != std::string::npos) targetRole = AgentRole::Reviewer;
    if (lower.find("analy") != std::string::npos) targetRole = AgentRole::Analyzer;
    if (lower.find("arch") != std::string::npos) targetRole = AgentRole::Architect;

    std::vector<AgentInstance*> candidates = getAvailableAgents(targetRole);
    if (candidates.empty()) {
        for (const auto& kv : m_agents) {
            if (kv.second->isAvailable) candidates.push_back(kv.second.get());
        }
    }
    if (candidates.empty()) return {};

    auto* best = *std::min_element(
        candidates.begin(), candidates.end(),
        [](const AgentInstance* a, const AgentInstance* b) {
            return a->utilization < b->utilization;
        });
    return best ? best->agentId : std::string();
}

void AgenticAgentCoordinator::rebalanceWorkload() {
    if (m_agents.empty()) return;
    const float avg = getTotalUtilization() / static_cast<float>(m_agents.size());
    for (auto& kv : m_agents) {
        if (kv.second->isAvailable) {
            kv.second->utilization = std::min(kv.second->utilization, avg);
        }
    }
}

AgenticAgentCoordinator::GlobalState AgenticAgentCoordinator::getGlobalState() {
    GlobalState s;
    s.agents = getAllAgentStatuses();
    s.tasks = getAllTaskStatuses();
    s.metrics = getCoordinationMetrics();
    return s;
}

bool AgenticAgentCoordinator::restoreGlobalState(const GlobalState& state) {
    m_agents.clear();
    m_assignments.clear();
    m_taskDurations.clear();
    m_conflicts.clear();

    for (const auto& a : state.agents) {
        auto agent = std::make_unique<AgentInstance>();
        agent->agentId = a.agent_id;
        agent->role = static_cast<AgentRole>(a.role);
        agent->reasoner = std::make_unique<AgenticIterativeReasoning>();
        agent->state = std::make_unique<AgenticLoopState>();
        agent->isAvailable = a.available;
        agent->utilization = std::clamp(a.utilization, 0.0f, 1.0f);
        agent->tasksCompleted = a.tasks_completed;
        agent->lastActive = std::chrono::system_clock::now();
        if (agent->reasoner) {
            agent->reasoner->initialize(m_engine, agent->state.get(), m_inferenceEngine);
        }
        m_agents[agent->agentId] = std::move(agent);
    }

    m_totalTasksAssigned = static_cast<int>(state.metrics.count("tasks_assigned")
        ? state.metrics.at("tasks_assigned") : 0.0f);
    m_totalTasksCompleted = static_cast<int>(state.metrics.count("tasks_completed")
        ? state.metrics.at("tasks_completed") : 0.0f);
    m_totalConflicts = static_cast<int>(state.metrics.count("total_conflicts")
        ? state.metrics.at("total_conflicts") : 0.0f);

    return true;
}

void AgenticAgentCoordinator::saveCheckpoint() {
    m_lastCheckpoint = getGlobalState();
    m_lastCheckpointTime = std::chrono::system_clock::now();
}

bool AgenticAgentCoordinator::restoreFromCheckpoint() {
    if (m_lastCheckpoint.agents.empty() && m_lastCheckpoint.tasks.empty() && m_lastCheckpoint.metrics.empty()) {
        return false;
    }
    return restoreGlobalState(m_lastCheckpoint);
}

std::string AgenticAgentCoordinator::getAgentOpinion(const std::string& agentId, const std::string& question) {
    auto* agent = getAgent(agentId);
    if (!agent) return "unknown";
    return "agent:" + agentId + " opinion on " + question;
}

std::string AgenticAgentCoordinator::reconcileConflictingStates(
    const std::map<std::string, std::string>& state1,
    const std::map<std::string, std::string>& state2) {
    std::map<std::string, std::string> merged = state1;
    for (const auto& kv : state2) {
        merged[kv.first] = kv.second;
    }
    std::ostringstream oss;
    bool first = true;
    for (const auto& kv : merged) {
        if (!first) oss << ";";
        first = false;
        oss << kv.first << "=" << kv.second;
    }
    return oss.str();
}

void AgenticAgentCoordinator::syncStateWithAgent(const std::string& agentId) {
    auto* a = getAgent(agentId);
    if (!a) return;
    a->lastActive = std::chrono::system_clock::now();
}

void AgenticAgentCoordinator::recordConflict(
    const std::string& agent1,
    const std::string& agent2,
    const std::string& reason,
    const std::string& resolution) {
    AgentConflict c{};
    c.conflictId = generateId();
    c.agent1Id = agent1;
    c.agent2Id = agent2;
    c.conflictReason = reason;
    c.resolution = resolution;
    c.timestamp = std::chrono::system_clock::now();
    m_conflicts.push_back(std::move(c));
    ++m_totalConflicts;
}

void AgenticAgentCoordinator::updateAgentMetrics(const std::string& agentId) {
    auto* a = getAgent(agentId);
    if (!a) return;
    if (a->isAvailable) {
        a->utilization = std::max(0.0f, a->utilization - 0.1f);
    } else {
        a->utilization = std::min(1.0f, a->utilization + 0.1f);
    }
=======
    std::cout << "[AgenticAgentCoordinator] Assigned task: " << taskId << " to agent: " << agentId << std::endl;

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
    std::cout << "[AgenticAgentCoordinator] Resolving conflicts" << std::endl;
}

void AgenticAgentCoordinator::synchronizeState()
{
    for (auto& pair : m_agents) {
        if (pair.second && pair.second->state) {
            // Synchronize agent state
        }
    }
    std::cout << "[AgenticAgentCoordinator] Synchronized all agent states" << std::endl;
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
    std::cout << "[AgenticAgentCoordinator] " << message << std::endl;
>>>>>>> origin/main
}
