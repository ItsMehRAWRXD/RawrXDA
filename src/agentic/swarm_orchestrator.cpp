#include "swarm_orchestrator.h"
#include "../../include/agentic_engine.h"
#include <random>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <regex>

namespace RawrXD {

SwarmOrchestrator::SwarmOrchestrator(size_t maxAgents) : m_maxAgents(maxAgents) {
    // Initialize central brain (The Orchestrator itself)
    m_inferenceEngine = std::make_unique<AgenticEngine>();
    m_inferenceEngine->initialize();
    m_inferenceEngine->appendSystemPrompt("You are the Swarm Orchestrator. Your role is to decompose complex tasks into subtasks and coordinate agents.");

    // Default agents
    addAgent("coding");
    addAgent("debugging");
    addAgent("optimization");
    addAgent("analysis");
    
    m_running = true;
    m_swarmThread = std::thread(&SwarmOrchestrator::swarmLoop, this);
}

SwarmOrchestrator::~SwarmOrchestrator() {
    m_running = false;
    if (m_swarmThread.joinable()) {
        m_swarmThread.join();
    }
}

std::expected<void, SwarmError> SwarmOrchestrator::addAgent(const std::string& specialization) {
    try {
        auto agent = std::make_unique<SwarmAgent>();
        agent->id = "agent_" + std::to_string(m_agents.size());
        agent->specialization = specialization;
        
        agent->engine = std::make_unique<AgenticEngine>();
        agent->engine->initialize();
        
        // Configure agent specialization
        std::string prompt = "You are a specialized Swarm Agent. Your specialization is: " + specialization + ".\n"
                             "Focus solely on this aspect of the tasks provided.";
        agent->engine->appendSystemPrompt(prompt);

        m_agents.push_back(std::move(agent));
        return {};
    } catch (...) {
        return std::unexpected(SwarmError::AgentCreationFailed);
    }
}

std::vector<SwarmAgent*> SwarmOrchestrator::getAvailableAgents() const {
    std::vector<SwarmAgent*> available;
    for (const auto& agent : m_agents) {
        if (!agent->isBusy.load()) {
            available.push_back(agent.get());
        }
    }
    return available;
}

std::expected<std::string, SwarmError> SwarmOrchestrator::executeTask(
    const std::string& task
) {
    auto decompositionResult = decomposeTask(task);
    if (!decompositionResult) return std::unexpected(decompositionResult.error());
    
    auto subtasks = decompositionResult.value();
    if (subtasks.empty()) return "Task decomposition yielded no actionable items.";

    auto swarmTask = std::make_unique<SwarmTask>();
    swarmTask->id = generateTaskId();
    swarmTask->description = task;
    swarmTask->subtasks = subtasks;
    swarmTask->createdAt = std::chrono::steady_clock::now();
    
    // Check if we have agents
    if (m_agents.empty()) return std::unexpected(SwarmError::AgentCreationFailed);

    std::vector<std::future<std::expected<std::string, SwarmError>>> futures;

    // Distribute tasks round-robin for now
    // A better implementation would match specialization
    for (size_t i = 0; i < subtasks.size(); ++i) {
        // Find best agent
        SwarmAgent* bestAgent = m_agents[i % m_agents.size()].get();
        for (const auto& agent : m_agents) {
             if (subtasks[i].find(agent->specialization) != std::string::npos) {
                 bestAgent = agent.get();
                 break;
             }
        }
        
        futures.push_back(std::async(std::launch::async,
            [this, bestAgent, subtask = subtasks[i], context = swarmTask->context]() {
                return executeSubtask(bestAgent, subtask, context);
            }
        ));
    }
    
    std::vector<std::string> results;
    std::vector<float> confidences;
    
    for (auto& f : futures) {
        auto result = f.get();
        if (result) {
            results.push_back(result.value());
            confidences.push_back(calculateResultQuality(result.value()));
        } else {
             results.push_back("Subtask Execution Failed");
             confidences.push_back(0.0f);
        }
    }
    
    return reachConsensus(results, confidences);
}

std::expected<std::vector<std::string>, SwarmError> SwarmOrchestrator::decomposeTask(const std::string& task) {
    if (!m_inferenceEngine) return std::unexpected(SwarmError::ExecutionFailed);

    std::string prompt = "Break down the following task into 3 distinct implementation steps. "
                         "Return ONLY a JSON array of strings e.g. [\"Step 1\", \"Step 2\"]. "
                         "Task: " + task;
                         
    std::string response = m_inferenceEngine->processQuery(prompt);
    
    // Parse JSON
    try {
        size_t start = response.find('[');
        size_t end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos) {
            std::string arrayStr = response.substr(start, end - start + 1);
            auto jsonArray = json::parse(arrayStr);
            std::vector<std::string> steps;
            for (const auto& item : jsonArray) {
                if (item.is_string()) steps.push_back(item.get<std::string>());
            }
            if (!steps.empty()) return steps;
        }
    } catch (...) {
        // Fallthrough to fallback
    }

    // Fallback: If model fails to output valid JSON, act as single step
    return std::vector<std::string>{task};
}

std::expected<void, SwarmError> SwarmOrchestrator::distributeTask(SwarmTask& task, std::vector<SwarmAgent*>& agents) {
    // Logic moved to executeTask for now in this iteration
    return {};
}

std::expected<std::string, SwarmError> SwarmOrchestrator::reachConsensus(
    const std::vector<std::string>& proposals,
    const std::vector<float>& confidences
) {
    return weightedVotingConsensus(proposals, confidences);
}

std::expected<std::string, SwarmError> SwarmOrchestrator::weightedVotingConsensus(
    const std::vector<std::string>& proposals,
    const std::vector<float>& confidences
) {
    if (proposals.empty()) return std::unexpected(SwarmError::ConsensusFailed);
    
    // Synthesis: Concatenate results for now, or ask Leader to summarize
    std::stringstream synthesis;
    synthesis << "Swarm Task Results:\n";
    for (size_t i = 0; i < proposals.size(); ++i) {
        synthesis << "Agent Report " << (i+1) << " (Conf: " << confidences[i] << "):\n"
                  << proposals[i] << "\n---\n";
    }
    
    // Ideally, we feed this back to m_inferenceEngine to summarize
    if (m_inferenceEngine) {
        std::string summaryPrompt = "Synthesize the following agent reports into a single cohesive final answer:\n" + synthesis.str();
        return m_inferenceEngine->processQuery(summaryPrompt);
    }

    return synthesis.str();
}

std::expected<std::string, SwarmError> SwarmOrchestrator::executeSubtask(
    SwarmAgent* agent,
    const std::string& subtask,
    const std::unordered_map<std::string, std::string>& context
) {
    if (!agent || !agent->engine) return std::unexpected(SwarmError::AgentCreationFailed);
    
    // Mark agent busy
    bool wasBusy = agent->isBusy.exchange(true);
    // (In robust system, would wait or reject)
    
    std::string response = agent->engine->processQuery(subtask);
    
    agent->isBusy.store(false);
    agent->lastActive = std::chrono::steady_clock::now();
    
    if (response.empty()) return std::unexpected(SwarmError::ExecutionFailed);
    return response;
}

float SwarmOrchestrator::calculateResultQuality(const std::string& result) {
    // Basic heuristic: Length and keyword check
    if (result.length() < 10) return 0.1f;
    if (result.find("error") != std::string::npos) return 0.3f; // Lower confidence if error mentioned
    return 0.9f;
}

void SwarmOrchestrator::swarmLoop() {
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Reduced latency
        
        // Future: Check m_taskQueue and process background tasks
    }
}

} // namespace RawrXD

