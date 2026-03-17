// ============================================================================
// Win32IDE_SubAgent.cpp — SubAgent, Chaining & HexMag Swarm Implementation
// ============================================================================

#include "Win32IDE_SubAgent.h"
#include "../config/IDEConfig.h"
#include "../agentic_engine.h"
#include "../native_agent.hpp"
#include <random>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <condition_variable>

// ============================================================================
// UUID Generator
// ============================================================================
std::string SubAgentManager::generateUUID() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << "sa-";
    ss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << (dis(gen) & 0xFFFF);
    return ss.str();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SubAgentManager::SubAgentManager(AgenticEngine* engine)
    : m_engine(engine)
{
    LOG_INFO("SubAgentManager initialized");
}

SubAgentManager::~SubAgentManager() {
    cancelAll();

    // Join all threads
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
    LOG_INFO("SubAgentManager destroyed — spawned " + std::to_string(m_totalSpawned.load()) + " total");
}

// ============================================================================
// Core SubAgent Operations
// ============================================================================

std::string SubAgentManager::spawnSubAgent(
    const std::string& parentId,
    const std::string& description,
    const std::string& prompt)
{
    SCOPED_METRIC("subagent.spawn");
    METRICS.increment("subagent.spawns_total");

    std::string agentId = generateUUID();

    auto agent = std::make_shared<SubAgent>();
    agent->id = agentId;
    agent->parentId = parentId;
    agent->description = description;
    agent->prompt = prompt;
    agent->state = SubAgent::State::Pending;
    agent->progress = 0.0f;
    agent->tokensGenerated = 0;
    agent->startTime = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_agents[agentId] = agent;
    }

    m_totalSpawned++;

    LOG_INFO("SubAgent spawned: " + agentId + " desc='" + description + "' parent=" + parentId);

    // Launch in background thread
    m_threads.emplace_back(&SubAgentManager::runSubAgentThread, this, agentId);

    return agentId;
}

void SubAgentManager::cancelSubAgent(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_agents.find(agentId);
    if (it != m_agents.end() &&
        (it->second->state == SubAgent::State::Pending ||
         it->second->state == SubAgent::State::Running)) {
        it->second->state = SubAgent::State::Cancelled;
        it->second->endTime = std::chrono::steady_clock::now();
        LOG_INFO("SubAgent cancelled: " + agentId);
        METRICS.increment("subagent.cancelled");
    }
}

void SubAgentManager::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, agent] : m_agents) {
        if (agent->state == SubAgent::State::Pending ||
            agent->state == SubAgent::State::Running) {
            agent->state = SubAgent::State::Cancelled;
            agent->endTime = std::chrono::steady_clock::now();
        }
    }
    // Also cancel all active swarms
    {
        std::lock_guard<std::mutex> slock(m_swarmMutex);
        for (auto& [sid, sw] : m_swarms) {
            sw->cancelled.store(true);
        }
    }
    LOG_INFO("All sub-agents cancelled");
}

const SubAgent* SubAgentManager::getSubAgent(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) return it->second.get();
    return nullptr;
}

std::vector<SubAgent> SubAgentManager::getAllSubAgents() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SubAgent> result;
    result.reserve(m_agents.size());
    for (const auto& [id, agent] : m_agents) {
        result.push_back(*agent);
    }
    return result;
}

std::vector<SubAgent> SubAgentManager::getSubAgentsForParent(const std::string& parentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SubAgent> result;
    for (const auto& [id, agent] : m_agents) {
        if (agent->parentId == parentId) {
            result.push_back(*agent);
        }
    }
    return result;
}

bool SubAgentManager::waitForSubAgent(const std::string& agentId, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_agents.find(agentId);
            if (it == m_agents.end()) return false;
            auto state = it->second->state;
            if (state == SubAgent::State::Completed ||
                state == SubAgent::State::Failed ||
                state == SubAgent::State::Cancelled) {
                return state == SubAgent::State::Completed;
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            cancelSubAgent(agentId);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::string SubAgentManager::getSubAgentResult(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) return it->second->result;
    return "";
}

// ============================================================================
// SubAgent Thread Execution
// ============================================================================

void SubAgentManager::runSubAgentThread(const std::string& agentId) {
    SCOPED_METRIC("subagent.run");

    std::string prompt;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_agents.find(agentId);
        if (it == m_agents.end()) return;
        it->second->state = SubAgent::State::Running;
        it->second->startTime = std::chrono::steady_clock::now();
        prompt = it->second->prompt;
    }

    LOG_INFO("SubAgent running: " + agentId);

    try {
        if (!m_engine) {
            throw std::runtime_error("No AgenticEngine available");
        }

        // Use the engine's chat method for inference
        std::string response = m_engine->chat(prompt);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_agents.find(agentId);
            if (it != m_agents.end()) {
                // Check if cancelled during execution
                if (it->second->state == SubAgent::State::Cancelled) {
                    LOG_INFO("SubAgent was cancelled during run: " + agentId);
                    return;
                }
                it->second->result = response;
                it->second->state = SubAgent::State::Completed;
                it->second->progress = 1.0f;
                it->second->endTime = std::chrono::steady_clock::now();
                it->second->tokensGenerated = (int)(response.size() / 4); // rough estimate
            }
        }

        METRICS.increment("subagent.completed");
        LOG_INFO("SubAgent completed: " + agentId +
                 " (" + std::to_string(response.size()) + " chars)");

        if (m_completionCallback) {
            m_completionCallback(agentId, response, true);
        }

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_agents.find(agentId);
        if (it != m_agents.end()) {
            it->second->state = SubAgent::State::Failed;
            it->second->result = std::string("[SubAgent Error] ") + e.what();
            it->second->endTime = std::chrono::steady_clock::now();
        }
        METRICS.increment("subagent.failed");
        LOG_ERROR("SubAgent failed: " + agentId + " — " + e.what());

        if (m_completionCallback) {
            m_completionCallback(agentId, e.what(), false);
        }
    }
}

// ============================================================================
// Chaining — Sequential Pipeline
// ============================================================================

std::string SubAgentManager::executeChain(
    const std::string& parentId,
    const std::vector<std::string>& promptTemplates,
    const std::string& initialInput)
{
    SCOPED_METRIC("subagent.chain");
    METRICS.increment("subagent.chains_total");

    LOG_INFO("Chain started: " + std::to_string(promptTemplates.size()) +
             " steps, parent=" + parentId);

    {
        std::lock_guard<std::mutex> lock(m_chainMutex);
        m_chainSteps.clear();
        for (int i = 0; i < (int)promptTemplates.size(); i++) {
            ChainStep step;
            step.index = i;
            step.promptTemplate = promptTemplates[i];
            step.state = SubAgent::State::Pending;
            m_chainSteps.push_back(step);
        }
    }

    std::string currentInput = initialInput;

    for (int i = 0; i < (int)promptTemplates.size(); i++) {
        // Build prompt by replacing {{input}} placeholder
        std::string resolvedPrompt = replaceTemplate(promptTemplates[i], currentInput);

        // Spawn sub-agent for this step
        std::string stepDesc = "Chain step " + std::to_string(i + 1) +
                                "/" + std::to_string(promptTemplates.size());
        std::string agentId = spawnSubAgent(parentId, stepDesc, resolvedPrompt);

        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            if (i < (int)m_chainSteps.size()) {
                m_chainSteps[i].subAgentId = agentId;
                m_chainSteps[i].state = SubAgent::State::Running;
            }
        }

        // Wait for completion
        bool success = waitForSubAgent(agentId, 120000); // 2 min per step

        std::string result = getSubAgentResult(agentId);

        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            if (i < (int)m_chainSteps.size()) {
                m_chainSteps[i].result = result;
                m_chainSteps[i].state = success
                    ? SubAgent::State::Completed
                    : SubAgent::State::Failed;
            }
        }

        if (!success) {
            LOG_ERROR("Chain step " + std::to_string(i) + " failed, aborting chain");
            METRICS.increment("subagent.chains_failed");
            return "[Chain Error] Step " + std::to_string(i + 1) + " failed: " + result;
        }

        currentInput = result;

        LOG_INFO("Chain step " + std::to_string(i + 1) + " completed (" +
                 std::to_string(result.size()) + " chars)");
    }

    METRICS.increment("subagent.chains_completed");
    LOG_INFO("Chain completed: " + std::to_string(promptTemplates.size()) + " steps");
    return currentInput;
}

std::vector<ChainStep> SubAgentManager::getChainSteps() const {
    std::lock_guard<std::mutex> lock(m_chainMutex);
    return m_chainSteps;
}

std::string SubAgentManager::replaceTemplate(const std::string& tmpl,
                                              const std::string& input) const {
    std::string result = tmpl;
    // Replace all occurrences of {{input}}
    const std::string placeholder = "{{input}}";
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.size(), input);
        pos += input.size();
    }
    // If no placeholder was found and there's input, append it
    if (result == tmpl && !input.empty()) {
        result += "\n\nContext from previous step:\n" + input;
    }
    return result;
}

// ============================================================================
// HexMag Swarm — Parallel Fan-out with Merge
// ============================================================================

std::string SubAgentManager::executeSwarm(
    const std::string& parentId,
    const std::vector<std::string>& prompts,
    const SwarmConfig& config)
{
    SCOPED_METRIC("subagent.swarm");
    METRICS.increment("subagent.swarms_total");

    LOG_INFO("HexMag Swarm started: " + std::to_string(prompts.size()) +
             " tasks, maxParallel=" + std::to_string(config.maxParallel) +
             " merge=" + config.mergeStrategy);

    std::string swarmId = generateUUID();

    auto swarmState = std::make_shared<SwarmState>();
    swarmState->swarmId = swarmId;
    swarmState->config = config;

    // Build task list
    for (int i = 0; i < (int)prompts.size(); i++) {
        SwarmTask task;
        task.id = swarmId + "-t" + std::to_string(i);
        task.prompt = prompts[i];
        task.state = SubAgent::State::Pending;
        task.weight = 1.0f;
        swarmState->tasks.push_back(task);
    }

    {
        std::lock_guard<std::mutex> lock(m_swarmMutex);
        m_swarms[swarmId] = swarmState;
    }

    // Execute tasks with parallelism limit
    std::vector<std::thread> swarmThreads;
    std::mutex batchMutex;
    std::condition_variable batchCV;
    std::atomic<int> running{0};
    std::atomic<bool> anyFailed{false};

    for (int i = 0; i < (int)swarmState->tasks.size(); i++) {
        // Wait if we've hit the parallelism limit
        {
            std::unique_lock<std::mutex> lock(batchMutex);
            batchCV.wait(lock, [&]() {
                return running.load() < config.maxParallel || swarmState->cancelled.load();
            });
        }

        if (swarmState->cancelled.load()) break;
        if (config.failFast && anyFailed.load()) break;

        running++;

        swarmThreads.emplace_back([this, &swarmState, i, &running, &batchMutex,
                                   &batchCV, &anyFailed, parentId]() {
            SwarmTask& task = swarmState->tasks[i];
            task.state = SubAgent::State::Running;

            std::string desc = "Swarm task " + std::to_string(i + 1) +
                               "/" + std::to_string(swarmState->tasks.size());
            std::string agentId = spawnSubAgent(parentId, desc, task.prompt);
            task.subAgentId = agentId;

            bool success = waitForSubAgent(agentId, swarmState->config.timeoutMs);
            std::string result = getSubAgentResult(agentId);

            task.result = result;
            task.state = success ? SubAgent::State::Completed : SubAgent::State::Failed;

            if (!success) {
                anyFailed.store(true);
                METRICS.increment("subagent.swarm_tasks_failed");
            } else {
                METRICS.increment("subagent.swarm_tasks_completed");
            }

            swarmState->completedCount++;
            running--;

            {
                std::lock_guard<std::mutex> lock(batchMutex);
            }
            batchCV.notify_one();
        });
    }

    // Wait for all swarm threads
    for (auto& t : swarmThreads) {
        if (t.joinable()) t.join();
    }

    // Merge results
    std::string merged = mergeSwarmResults(swarmState->tasks, config);
    swarmState->mergedResult = merged;

    LOG_INFO("HexMag Swarm completed: " + swarmId +
             " completed=" + std::to_string(swarmState->completedCount.load()) +
             "/" + std::to_string(swarmState->tasks.size()));
    METRICS.increment("subagent.swarms_completed");

    return merged;
}

std::string SubAgentManager::executeSwarmAsync(
    const std::string& parentId,
    const std::vector<std::string>& prompts,
    const SwarmConfig& config,
    SwarmCompleteCallback onComplete)
{
    std::string swarmId = generateUUID();

    // Launch swarm in background
    m_threads.emplace_back([this, parentId, prompts, config, onComplete, swarmId]() {
        std::string result = executeSwarm(parentId, prompts, config);

        bool allOk = true;
        {
            std::lock_guard<std::mutex> lock(m_swarmMutex);
            auto it = m_swarms.find(swarmId);
            if (it != m_swarms.end()) {
                for (const auto& task : it->second->tasks) {
                    if (task.state != SubAgent::State::Completed) {
                        allOk = false;
                        break;
                    }
                }
            }
        }

        if (onComplete) {
            onComplete(result, allOk);
        }
    });

    return swarmId;
}

std::vector<SwarmTask> SubAgentManager::getSwarmTasks(const std::string& swarmId) const {
    std::lock_guard<std::mutex> lock(m_swarmMutex);
    auto it = m_swarms.find(swarmId);
    if (it != m_swarms.end()) {
        return it->second->tasks;
    }
    return {};
}

float SubAgentManager::getSwarmProgress(const std::string& swarmId) const {
    std::lock_guard<std::mutex> lock(m_swarmMutex);
    auto it = m_swarms.find(swarmId);
    if (it != m_swarms.end()) {
        int total = (int)it->second->tasks.size();
        if (total == 0) return 1.0f;
        return (float)it->second->completedCount.load() / (float)total;
    }
    return 0.0f;
}

std::string SubAgentManager::mergeSwarmResults(
    const std::vector<SwarmTask>& tasks,
    const SwarmConfig& config)
{
    SCOPED_METRIC("subagent.merge");

    if (config.mergeStrategy == "concatenate") {
        std::ostringstream merged;
        for (int i = 0; i < (int)tasks.size(); i++) {
            if (tasks[i].state == SubAgent::State::Completed) {
                merged << "=== Task " << (i + 1) << " ===\n"
                       << tasks[i].result << "\n\n";
            }
        }
        return merged.str();
    }
    else if (config.mergeStrategy == "vote") {
        // Simple majority vote: pick the most common result
        std::unordered_map<std::string, int> votes;
        for (const auto& task : tasks) {
            if (task.state == SubAgent::State::Completed) {
                votes[task.result]++;
            }
        }
        std::string best;
        int bestCount = 0;
        for (const auto& [result, count] : votes) {
            if (count > bestCount) {
                bestCount = count;
                best = result;
            }
        }
        return best;
    }
    else if (config.mergeStrategy == "summarize") {
        // Concatenate all results and ask the model to summarize
        std::ostringstream all;
        all << "You are given the outputs of " << tasks.size()
            << " parallel sub-agents working on related subtasks.\n"
            << "Merge and synthesize these into a single coherent response.\n\n";

        if (!config.mergePrompt.empty()) {
            all << "Merge instructions: " << config.mergePrompt << "\n\n";
        }

        for (int i = 0; i < (int)tasks.size(); i++) {
            if (tasks[i].state == SubAgent::State::Completed) {
                all << "--- Sub-agent " << (i + 1) << " output ---\n"
                    << tasks[i].result << "\n\n";
            }
        }

        if (m_engine) {
            return m_engine->chat(all.str());
        }
        // Fallback: just concatenate
        return all.str();
    }

    // Default fallback: concatenate
    std::ostringstream merged;
    for (const auto& task : tasks) {
        if (task.state == SubAgent::State::Completed) {
            merged << task.result << "\n";
        }
    }
    return merged.str();
}

// ============================================================================
// Todo List Management
// ============================================================================

void SubAgentManager::setTodoList(const std::vector<TodoItem>& items) {
    std::lock_guard<std::mutex> lock(m_todoMutex);
    m_todoList = items;
    LOG_INFO("Todo list updated: " + std::to_string(items.size()) + " items");
}

void SubAgentManager::updateTodoStatus(int id, TodoItem::Status status) {
    std::lock_guard<std::mutex> lock(m_todoMutex);
    for (auto& item : m_todoList) {
        if (item.id == id) {
            item.status = status;
            LOG_DEBUG("Todo " + std::to_string(id) + " → " + item.statusString());
            return;
        }
    }
}

std::vector<TodoItem> SubAgentManager::getTodoList() const {
    std::lock_guard<std::mutex> lock(m_todoMutex);
    return m_todoList;
}

std::string SubAgentManager::todoListToJSON() const {
    std::lock_guard<std::mutex> lock(m_todoMutex);
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < m_todoList.size(); i++) {
        if (i > 0) oss << ",";
        oss << m_todoList[i].toJSON();
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// Status
// ============================================================================

int SubAgentManager::activeSubAgentCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& [id, agent] : m_agents) {
        if (agent->state == SubAgent::State::Pending ||
            agent->state == SubAgent::State::Running) {
            count++;
        }
    }
    return count;
}

std::string SubAgentManager::getStatusSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int pending = 0, running = 0, completed = 0, failed = 0, cancelled = 0;
    for (const auto& [id, agent] : m_agents) {
        switch (agent->state) {
            case SubAgent::State::Pending:   pending++;   break;
            case SubAgent::State::Running:   running++;   break;
            case SubAgent::State::Completed: completed++; break;
            case SubAgent::State::Failed:    failed++;    break;
            case SubAgent::State::Cancelled: cancelled++; break;
        }
    }
    std::ostringstream oss;
    oss << "SubAgents: total=" << m_agents.size()
        << " pending=" << pending << " running=" << running
        << " completed=" << completed << " failed=" << failed
        << " cancelled=" << cancelled;
    return oss.str();
}

// ============================================================================
// Tool Call Dispatch — Parse LLM Output for Tool Invocations
// ============================================================================

bool SubAgentManager::dispatchToolCall(
    const std::string& parentId,
    const std::string& modelOutput,
    std::string& toolResult)
{
    // Check for runSubagent tool call
    std::string description, prompt;
    if (parseRunSubagent(modelOutput, description, prompt)) {
        LOG_INFO("Detected runSubagent tool call: " + description);
        METRICS.increment("subagent.tool_dispatch.runSubagent");

        std::string agentId = spawnSubAgent(parentId, description, prompt);
        bool success = waitForSubAgent(agentId, 120000);
        std::string result = getSubAgentResult(agentId);

        toolResult = "SubAgent '" + description + "' " +
                     (success ? "completed" : "failed") + ":\n" + result;
        return true;
    }

    // Check for manage_todo_list tool call
    std::vector<TodoItem> items;
    if (parseTodoList(modelOutput, items)) {
        LOG_INFO("Detected manage_todo_list tool call: " + std::to_string(items.size()) + " items");
        METRICS.increment("subagent.tool_dispatch.todo_list");
        setTodoList(items);
        toolResult = "Todo list updated with " + std::to_string(items.size()) + " items:\n" + todoListToJSON();
        return true;
    }

    // Check for chain call
    std::vector<std::string> steps;
    std::string initialInput;
    if (parseChainCall(modelOutput, steps, initialInput)) {
        LOG_INFO("Detected chain tool call: " + std::to_string(steps.size()) + " steps");
        METRICS.increment("subagent.tool_dispatch.chain");
        std::string result = executeChain(parentId, steps, initialInput);
        toolResult = "Chain completed (" + std::to_string(steps.size()) + " steps):\n" + result;
        return true;
    }

    // Check for swarm call
    std::vector<std::string> prompts;
    SwarmConfig config;
    if (parseSwarmCall(modelOutput, prompts, config)) {
        LOG_INFO("Detected HexMag swarm tool call: " + std::to_string(prompts.size()) + " tasks");
        METRICS.increment("subagent.tool_dispatch.swarm");
        std::string result = executeSwarm(parentId, prompts, config);
        toolResult = "HexMag Swarm completed (" + std::to_string(prompts.size()) + " tasks):\n" + result;
        return true;
    }

    return false;
}

// ============================================================================
// Tool Call Parsers
// ============================================================================

bool SubAgentManager::parseRunSubagent(const std::string& text,
                                        std::string& description,
                                        std::string& prompt) const {
    // Pattern: TOOL:runSubagent:{"description":"...","prompt":"..."}
    // Also supports: <tool>runSubagent</tool> <description>...</description> <prompt>...</prompt>
    // Also supports: tool:run_subagent description="..." prompt="..."

    // JSON-style pattern
    {
        size_t pos = text.find("runSubagent");
        if (pos == std::string::npos) pos = text.find("run_subagent");
        if (pos == std::string::npos) pos = text.find("runSubAgent");
        if (pos != std::string::npos) {
            // Try to find JSON block after it
            size_t jsonStart = text.find('{', pos);
            if (jsonStart != std::string::npos) {
                // Find matching closing brace (simple nesting)
                int depth = 0;
                size_t jsonEnd = jsonStart;
                for (size_t i = jsonStart; i < text.size(); i++) {
                    if (text[i] == '{') depth++;
                    else if (text[i] == '}') {
                        depth--;
                        if (depth == 0) { jsonEnd = i; break; }
                    }
                }
                if (jsonEnd > jsonStart) {
                    std::string json = text.substr(jsonStart, jsonEnd - jsonStart + 1);

                    // Simple JSON extraction (without pulling in a full JSON parser dependency)
                    auto extractField = [&](const std::string& fieldName) -> std::string {
                        std::string key = "\"" + fieldName + "\"";
                        size_t kpos = json.find(key);
                        if (kpos == std::string::npos) return "";
                        size_t colon = json.find(':', kpos + key.size());
                        if (colon == std::string::npos) return "";
                        size_t valStart = json.find('"', colon + 1);
                        if (valStart == std::string::npos) return "";
                        valStart++;
                        // Handle escaped quotes
                        std::string value;
                        for (size_t i = valStart; i < json.size(); i++) {
                            if (json[i] == '\\' && i + 1 < json.size()) {
                                value += json[++i];
                            } else if (json[i] == '"') {
                                break;
                            } else {
                                value += json[i];
                            }
                        }
                        return value;
                    };

                    description = extractField("description");
                    prompt = extractField("prompt");

                    if (!prompt.empty()) return true;
                }
            }

            // Fallback: line-based parsing
            // tool:run_subagent description="..." prompt="..."
            auto extractQuoted = [&](const std::string& key) -> std::string {
                std::string pattern = key + "=\"";
                size_t kpos = text.find(pattern, pos);
                if (kpos == std::string::npos) {
                    pattern = key + "='";
                    kpos = text.find(pattern, pos);
                }
                if (kpos == std::string::npos) return "";
                char quote = text[kpos + key.size() + 1];
                size_t vstart = kpos + pattern.size();
                size_t vend = text.find(quote, vstart);
                if (vend == std::string::npos) return "";
                return text.substr(vstart, vend - vstart);
            };

            if (description.empty()) description = extractQuoted("description");
            if (prompt.empty()) prompt = extractQuoted("prompt");

            if (!prompt.empty()) return true;
        }
    }

    return false;
}

bool SubAgentManager::parseTodoList(const std::string& text,
                                     std::vector<TodoItem>& items) const {
    // Pattern: TOOL:manage_todo_list:[{"id":1,"title":"...","status":"..."}]
    // Also: manage_todo_list with JSON array

    size_t pos = text.find("manage_todo_list");
    if (pos == std::string::npos) pos = text.find("manageTodoList");
    if (pos == std::string::npos) return false;

    // Find array start
    size_t arrStart = text.find('[', pos);
    if (arrStart == std::string::npos) return false;

    // Find matching closing bracket
    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < text.size(); i++) {
        if (text[i] == '[') depth++;
        else if (text[i] == ']') {
            depth--;
            if (depth == 0) { arrEnd = i; break; }
        }
    }

    if (arrEnd <= arrStart) return false;

    std::string arr = text.substr(arrStart, arrEnd - arrStart + 1);

    // Parse each object in the array (simple parser)
    size_t objStart = 0;
    while ((objStart = arr.find('{', objStart)) != std::string::npos) {
        size_t objEnd = arr.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = arr.substr(objStart, objEnd - objStart + 1);

        TodoItem item;

        // Extract id
        auto extractInt = [&](const std::string& key) -> int {
            std::string pattern = "\"" + key + "\":";
            size_t p = obj.find(pattern);
            if (p == std::string::npos) pattern = "\"" + key + "\": ";
            p = obj.find(pattern);
            if (p == std::string::npos) return -1;
            size_t numStart = p + pattern.size();
            while (numStart < obj.size() && (obj[numStart] == ' ' || obj[numStart] == '\t')) numStart++;
            std::string num;
            while (numStart < obj.size() && std::isdigit(obj[numStart])) {
                num += obj[numStart++];
            }
            return num.empty() ? -1 : std::stoi(num);
        };

        auto extractStr = [&](const std::string& key) -> std::string {
            std::string pattern = "\"" + key + "\":\"";
            size_t p = obj.find(pattern);
            if (p == std::string::npos) {
                pattern = "\"" + key + "\": \"";
                p = obj.find(pattern);
            }
            if (p == std::string::npos) return "";
            size_t vstart = p + pattern.size();
            std::string value;
            for (size_t i = vstart; i < obj.size(); i++) {
                if (obj[i] == '\\' && i + 1 < obj.size()) {
                    value += obj[++i];
                } else if (obj[i] == '"') {
                    break;
                } else {
                    value += obj[i];
                }
            }
            return value;
        };

        item.id = extractInt("id");
        item.title = extractStr("title");
        item.description = extractStr("description");
        std::string statusStr = extractStr("status");

        if (statusStr == "in-progress" || statusStr == "in_progress")
            item.status = TodoItem::Status::InProgress;
        else if (statusStr == "completed")
            item.status = TodoItem::Status::Completed;
        else if (statusStr == "failed")
            item.status = TodoItem::Status::Failed;
        else
            item.status = TodoItem::Status::NotStarted;

        if (item.id >= 0 && !item.title.empty()) {
            items.push_back(item);
        }

        objStart = objEnd + 1;
    }

    return !items.empty();
}

bool SubAgentManager::parseChainCall(const std::string& text,
                                      std::vector<std::string>& steps,
                                      std::string& initialInput) const {
    // Pattern: TOOL:chain:{"steps":["...","..."],"input":"..."}
    // Also: tool:chain steps=["...","..."]

    size_t pos = text.find("TOOL:chain");
    if (pos == std::string::npos) pos = text.find("tool:chain");
    if (pos == std::string::npos) pos = text.find("executeChain");
    if (pos == std::string::npos) return false;

    // Find steps array
    size_t stepsPos = text.find("\"steps\"", pos);
    if (stepsPos == std::string::npos) stepsPos = text.find("steps=", pos);
    if (stepsPos == std::string::npos) return false;

    size_t arrStart = text.find('[', stepsPos);
    if (arrStart == std::string::npos) return false;

    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < text.size(); i++) {
        if (text[i] == '[') depth++;
        else if (text[i] == ']') {
            depth--;
            if (depth == 0) { arrEnd = i; break; }
        }
    }

    if (arrEnd <= arrStart) return false;

    // Extract string elements from array
    std::string arr = text.substr(arrStart + 1, arrEnd - arrStart - 1);
    size_t strStart = 0;
    while ((strStart = arr.find('"', strStart)) != std::string::npos) {
        strStart++;
        std::string value;
        for (size_t i = strStart; i < arr.size(); i++) {
            if (arr[i] == '\\' && i + 1 < arr.size()) {
                value += arr[++i];
            } else if (arr[i] == '"') {
                strStart = i + 1;
                break;
            } else {
                value += arr[i];
            }
        }
        if (!value.empty()) steps.push_back(value);
    }

    // Extract initial input
    size_t inputPos = text.find("\"input\"", pos);
    if (inputPos != std::string::npos) {
        size_t valStart = text.find('"', text.find(':', inputPos) + 1);
        if (valStart != std::string::npos) {
            valStart++;
            for (size_t i = valStart; i < text.size(); i++) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    initialInput += text[++i];
                } else if (text[i] == '"') {
                    break;
                } else {
                    initialInput += text[i];
                }
            }
        }
    }

    return !steps.empty();
}

bool SubAgentManager::parseSwarmCall(const std::string& text,
                                      std::vector<std::string>& prompts,
                                      SwarmConfig& config) const {
    // Pattern: TOOL:hexmag_swarm:{"prompts":["..."],"strategy":"...","maxParallel":4}
    // Also: tool:swarm prompts=[...] strategy=summarize

    size_t pos = text.find("hexmag_swarm");
    if (pos == std::string::npos) pos = text.find("TOOL:swarm");
    if (pos == std::string::npos) pos = text.find("tool:swarm");
    if (pos == std::string::npos) pos = text.find("executeSwarm");
    if (pos == std::string::npos) pos = text.find("hexmagSwarm");
    if (pos == std::string::npos) return false;

    // Find prompts array
    size_t promptsPos = text.find("\"prompts\"", pos);
    if (promptsPos == std::string::npos) promptsPos = text.find("prompts=", pos);
    if (promptsPos == std::string::npos) return false;

    size_t arrStart = text.find('[', promptsPos);
    if (arrStart == std::string::npos) return false;

    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < text.size(); i++) {
        if (text[i] == '[') depth++;
        else if (text[i] == ']') {
            depth--;
            if (depth == 0) { arrEnd = i; break; }
        }
    }

    if (arrEnd <= arrStart) return false;

    // Extract string elements
    std::string arr = text.substr(arrStart + 1, arrEnd - arrStart - 1);
    size_t strStart = 0;
    while ((strStart = arr.find('"', strStart)) != std::string::npos) {
        strStart++;
        std::string value;
        for (size_t i = strStart; i < arr.size(); i++) {
            if (arr[i] == '\\' && i + 1 < arr.size()) {
                value += arr[++i];
            } else if (arr[i] == '"') {
                strStart = i + 1;
                break;
            } else {
                value += arr[i];
            }
        }
        if (!value.empty()) prompts.push_back(value);
    }

    // Parse config fields
    config.maxParallel = 4;
    config.timeoutMs = 60000;
    config.mergeStrategy = "concatenate";
    config.failFast = false;

    // Extract strategy
    auto extractJsonStr = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":\"";
        size_t p = text.find(pattern, pos);
        if (p == std::string::npos) {
            pattern = "\"" + key + "\": \"";
            p = text.find(pattern, pos);
        }
        if (p == std::string::npos) return "";
        size_t vstart = p + pattern.size();
        std::string value;
        for (size_t i = vstart; i < text.size(); i++) {
            if (text[i] == '"') break;
            value += text[i];
        }
        return value;
    };

    auto extractJsonInt = [&](const std::string& key) -> int {
        std::string pattern = "\"" + key + "\":";
        size_t p = text.find(pattern, pos);
        if (p == std::string::npos) {
            pattern = "\"" + key + "\": ";
            p = text.find(pattern, pos);
        }
        if (p == std::string::npos) return -1;
        size_t numStart = p + pattern.size();
        while (numStart < text.size() && !std::isdigit(text[numStart]) && text[numStart] != '-')
            numStart++;
        std::string num;
        while (numStart < text.size() && (std::isdigit(text[numStart]) || text[numStart] == '-'))
            num += text[numStart++];
        return num.empty() ? -1 : std::stoi(num);
    };

    std::string strategy = extractJsonStr("strategy");
    if (strategy.empty()) strategy = extractJsonStr("mergeStrategy");
    if (!strategy.empty()) config.mergeStrategy = strategy;

    int maxP = extractJsonInt("maxParallel");
    if (maxP > 0) config.maxParallel = maxP;

    int timeout = extractJsonInt("timeoutMs");
    if (timeout > 0) config.timeoutMs = timeout;

    std::string mergePrompt = extractJsonStr("mergePrompt");
    if (!mergePrompt.empty()) config.mergePrompt = mergePrompt;

    return !prompts.empty();
}
