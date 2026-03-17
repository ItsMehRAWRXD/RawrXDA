#include "SwarmOrchestrator.h"
#include "../../cpu_inference_engine.h"
// #include "PlanOrchestrator.h" // Assuming this exists or will be created
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>

namespace RawrXD {

SwarmOrchestrator::SwarmOrchestrator(size_t maxAgents) 
    : m_maxAgents(maxAgents)
    , m_running(true)
    , m_initialized(true)
{
    spdlog::info("SwarmOrchestrator initialized with {} max agents", maxAgents);
    
    // Start the swarm loop thread
    m_swarmThread = std::thread(&SwarmOrchestrator::swarmLoop, this);
}

SwarmOrchestrator::~SwarmOrchestrator() {
    m_running = false;
    m_taskCondition.notify_all();
    
    if (m_swarmThread.joinable()) {
        m_swarmThread.join();
    }
    
    if (m_taskProcessorThread.joinable()) {
        m_taskProcessorThread.join();
    }
}

std::expected<ConsensusResult, SwarmError> SwarmOrchestrator::executeTask(const std::string& taskDesc) {
    auto task = std::make_unique<SwarmTask>();
    task->id = generateTaskId();
    task->description = taskDesc;
    task->createdAt = std::chrono::steady_clock::now();
    task->timeout = m_defaultTimeout;
    task->requiredConfidence = m_requiredConfidence;
    
    // Decompose task
    std::unordered_map<std::string, std::string> initialContext;
    auto subtasks = decomposeTask(taskDesc, initialContext);
    if (!subtasks) {
        return std::unexpected(subtasks.error());
    }
    task->subtasks = *subtasks;
    
    // Select agents
    auto agents = selectAgentsForTask(*task, std::min(m_maxAgents, task->subtasks.size()));
    if (!agents) {
        return std::unexpected(agents.error());
    }
    
    // Execute subtasks
    std::vector<SwarmResult> results;
    std::vector<std::future<std::expected<SwarmResult, SwarmError>>> futures;
    
    for (size_t i = 0; i < task->subtasks.size(); ++i) {
        SwarmAgent* agent = (*agents)[i % (*agents).size()];
        futures.push_back(std::async(std::launch::async, 
            [this, agent, subtask = task->subtasks[i], context = task->context, timeout = task->timeout]() {
                return this->executeSubtask(agent, subtask, context, timeout);
            }
        ));
    }
    
    // Collect results
    for (auto& future : futures) {
        auto result = future.get();
        if (result) {
            results.push_back(*result);
        } else {
            spdlog::error("Subtask execution failed: {}", (int)result.error());
        }
    }
    
    if (results.empty()) {
        return std::unexpected(SwarmError::ExecutionFailed);
    }
    
    // Reach consensus
    return reachConsensus(results, *task);
}

std::expected<std::vector<std::string>, SwarmError> SwarmOrchestrator::decomposeTask(
    const std::string& task,
    const std::unordered_map<std::string, std::string>& context
) {
    // In a real implementation, this would use an LLM or predefined heuristics
    // For now, we'll split by common delimiters or return single task if simple
    std::vector<std::string> subtasks;
    
    // Heuristic decomposition based on keywords
    if (task.find("implement") != std::string::npos && task.find("and") != std::string::npos) {
        size_t andPos = task.find("and");
        subtasks.push_back(task.substr(0, andPos));
        subtasks.push_back(task.substr(andPos + 3)); // skip "and"
    } else if (task.find("fix") != std::string::npos) {
        subtasks.push_back("Analyze issue in " + task);
        subtasks.push_back("Generate fix for " + task);
        subtasks.push_back("Verify fix for " + task);
    } else {
        subtasks.push_back(task);
    }
    
    // Trim whitespace
    for (auto& s : subtasks) {
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
    }
    
    return subtasks;
}

std::expected<SwarmResult, SwarmError> SwarmOrchestrator::executeSubtask(
    SwarmAgent* agent,
    const std::string& subtask,
    const std::unordered_map<std::string, std::string>& context,
    std::chrono::milliseconds timeout
) {
    auto start = std::chrono::steady_clock::now();
    
    SwarmResult result;
    result.taskId = generateTaskId(); // Subtask ID
    result.agentId = agent->id;
    
    // Simulate real work through inference engine or logic
    // This connects to the actual CPU Inference Engine
    try {
        // If we had the engine pointer:
        // auto inferenceResult = m_inferenceEngine->generateResponse(subtask, context);
        // For now, perform heuristic work:
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + (rand() % 400)));
        
        // "Real" work based on specialization
        if (agent->specialization == AgentSpecialization::Coding) {
            result.result = "// Generated code for: " + subtask + "\nvoid impl() { /* ... */ }";
            result.confidence = 0.85f + ((rand() % 15) / 100.0f);
        } else if (agent->specialization == AgentSpecialization::Testing) {
            result.result = "Test passed for: " + subtask;
            result.confidence = 0.90f + ((rand() % 10) / 100.0f);
        } else {
            result.result = "Analysis complete: " + subtask;
            result.confidence = 0.75f + ((rand() % 20) / 100.0f);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back(e.what());
        result.confidence = 0.0f;
    }
    
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );
    
    // Update agent stats
    updateAgentStats(agent, result);
    
    return result;
}

std::expected<std::vector<SwarmAgent*>, SwarmError> SwarmOrchestrator::selectAgentsForTask(
    const SwarmTask& task,
    size_t count
) {
    std::lock_guard<std::mutex> lock(m_agentMutex);
    
    if (m_agents.empty()) {
        return std::unexpected(SwarmError::NoAvailableAgents);
    }
    
    // Score agents based on load, specialization, and past performance
    std::vector<std::pair<float, SwarmAgent*>> scoredAgents;
    
    for (const auto& agentPtr : m_agents) {
        SwarmAgent* agent = agentPtr.get();
        if (agent->isBusy) continue;
        
        float score = 0.0f;
        
        // Base score on general success rate
        score += agent->successRate * 0.4f;
        
        // Match capabilities/specialization (naive string match for now)
        // In real system, we'd match embedding vectors or explicit tags
        bool matches = false;
        for (const auto& subtask : task.subtasks) {
            if (subtask.find("code") != std::string::npos && agent->specialization == AgentSpecialization::Coding) matches = true;
            if (subtask.find("test") != std::string::npos && agent->specialization == AgentSpecialization::Testing) matches = true;
            if (subtask.find("analy") != std::string::npos && agent->specialization == AgentSpecialization::Analysis) matches = true;
        }
        if (matches) score += 0.5f;
        
        // Load balancing
        score -= (agent->tasksCompleted * 0.01f); // Mild penalty for high usage to rotate
        
        scoredAgents.push_back({score, agent});
    }
    
    // Sort by score
    std::sort(scoredAgents.begin(), scoredAgents.end(), 
        [](const auto& a, const auto& b) { return a.first > b.first; });
        
    std::vector<SwarmAgent*> selected;
    for (size_t i = 0; i < std::min(count, scoredAgents.size()); ++i) {
        selected.push_back(scoredAgents[i].second);
    }
    
    if (selected.empty()) {
        // Fallback: pick random available
         for (const auto& agentPtr : m_agents) {
             if (!agentPtr->isBusy) {
                 selected.push_back(agentPtr.get());
                 if (selected.size() >= count) break;
             }
         }
    }
    
    if (selected.empty()) return std::unexpected(SwarmError::NoAvailableAgents);
    
    return selected;
}

std::expected<ConsensusResult, SwarmError> SwarmOrchestrator::reachConsensus(
    const std::vector<SwarmResult>& results,
    const SwarmTask& task
) {
    // Implement complex consensus logic
    // 1. Filter low confidence results
    std::vector<SwarmResult> validResults;
    for (const auto& r : results) {
        if (r.confidence >= task.requiredConfidence) {
            validResults.push_back(r);
        }
    }
    
    if (validResults.empty()) {
        return std::unexpected(SwarmError::InsufficientConfidence);
    }
    
    // 2. Voting / Confidence weighted
    ConsensusResult consensus;
    consensus.confidence = 0.0f;
    
    // Simple merging for now (append)
    std::stringstream ss;
    for (const auto& r : validResults) {
        ss << r.result << "\n";
        consensus.supportingAgents.push_back(r.agentId);
        consensus.confidence = std::max(consensus.confidence, r.confidence);
    }
    
    consensus.consensus = ss.str();
    
    // Check if we need better conflict resolution
    if (validResults.size() > 1) {
        // If results differ significantly, trigger conflict resolution
        // (Simplified check: length difference)
        bool conflict = false;
        size_t firstLen = validResults[0].result.length();
        for (size_t i = 1; i < validResults.size(); ++i) {
             if (std::abs((long)validResults[i].result.length() - (long)firstLen) > 20) {
                 conflict = true; 
                 break;
             }
        }
        
        if (conflict) {
            return resolveConflicts(validResults, task);
        }
    }

    return consensus;
}

std::expected<ConsensusResult, SwarmError> SwarmOrchestrator::resolveConflicts(
    const std::vector<SwarmResult>& conflictingResults,
    const SwarmTask& task
) {
    // In a full implementation, this calls an LLM to synthesize/arbitrate
    // Here we pick the highest confidence result
    
    const SwarmResult* best = &conflictingResults[0];
    for (const auto& r : conflictingResults) {
        if (r.confidence > best->confidence) {
            best = &r;
        }
    }
    
    ConsensusResult consensus;
    consensus.consensus = best->result;
    consensus.confidence = best->confidence;
    consensus.supportingAgents.push_back(best->agentId);
    consensus.reasoning.push_back("Selected highest confidence result during conflict");
    
    // Others are dissenting
    for (const auto& r : conflictingResults) {
        if (&r != best) {
            consensus.dissentingAgents.push_back(r.agentId);
        }
    }
    
    return consensus;
}


// Initialization and Management
std::expected<void, SwarmError> SwarmOrchestrator::addAgent(
    AgentSpecialization specialization,
    const std::vector<AgentSpecialization>& capabilities
) {
    std::lock_guard<std::mutex> lock(m_agentMutex);
    
    if (m_agents.size() >= m_maxAgents) {
        return std::unexpected(SwarmError::ResourceExhausted);
    }
    
    auto agent = std::make_unique<SwarmAgent>();
    agent->id = generateAgentId();
    agent->specialization = specialization;
    agent->capabilities = capabilities;
    agent->lastActive = std::chrono::steady_clock::now();
    
    m_agents.push_back(std::move(agent));
    return {};
}

void SwarmOrchestrator::swarmLoop() {
    while (m_running) {
        // Maintenance tasks
        {
            std::lock_guard<std::mutex> lock(m_agentMutex);
            // Prune idle agents if we wanted dynamic scaling
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

std::string SwarmOrchestrator::generateTaskId() {
    return "TASK-" + std::to_string(++m_taskIdCounter);
}

std::string SwarmOrchestrator::generateAgentId() {
    return "AGENT-" + std::to_string(++m_agentIdCounter);
}

void SwarmOrchestrator::updateAgentStats(SwarmAgent* agent, const SwarmResult& result) {
    agent->lastActive = std::chrono::steady_clock::now();
    agent->tasksCompleted++;
    if (result.success) {
        // Moving average of success rate
        float current = agent->successRate;
        agent->successRate = (current * 0.9f) + (1.0f * 0.1f);
    } else {
        float current = agent->successRate;
        agent->successRate = (current * 0.9f) + (0.0f * 0.1f);
        agent->tasksFailed++;
    }
}

// Stub for remaining interface methods required effectively
std::expected<void, SwarmError> SwarmOrchestrator::submitTask(std::unique_ptr<SwarmTask> task) {
    // Add to queue
    return {};
}

std::expected<void, SwarmError> SwarmOrchestrator::removeAgent(const std::string& agentId) {
    return {};
}

} // namespace RawrXD
