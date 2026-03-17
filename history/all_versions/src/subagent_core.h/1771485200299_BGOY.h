#pragma once
// ============================================================================
// subagent_core.h — Portable SubAgent, Chaining & HexMag Swarm System
// ============================================================================
// Platform-independent core. Used by Win32IDE, CLI, and React/HTTP server.
// No windows.h, no IDELogger, no IDEConfig — pure C++20.
// ============================================================================

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>

// Forward declarations
class AgenticEngine;
class AgentHistoryRecorder;
class PolicyEngine;

// ============================================================================
// Pluggable logger — callers inject via setLogCallback; default = no-op
// ============================================================================
using SubAgentLogCallback = std::function<void(int /*level 0=debug,1=info,2=warn,3=err*/,
                                                const std::string& msg)>;

// ============================================================================
// Pluggable metrics — callers inject via setMetricsCallback; default = no-op
// ============================================================================
using SubAgentMetricsCallback = std::function<void(const std::string& key)>;

// ============================================================================
// TodoItem — structured task tracking for agent iterations
// ============================================================================
struct TodoItem {
    int id;
    std::string title;
    std::string description;
    enum class Status { NotStarted, InProgress, Completed, Failed } status;

    std::string statusString() const {
        switch (status) {
            case Status::NotStarted: return "not-started";
            case Status::InProgress: return "in-progress";
            case Status::Completed:  return "completed";
            case Status::Failed:     return "failed";
        }
        return "unknown";
    }

    std::string toJSON() const {
        std::ostringstream oss;
        oss << "{\"id\":" << id
            << ",\"title\":\"" << title
            << "\",\"description\":\"" << description
            << "\",\"status\":\"" << statusString() << "\"}";
        return oss.str();
    }
};

// ============================================================================
// SubAgent — a child agent instance spawned by a parent model
// ============================================================================
struct SubAgent {
    std::string id;
    std::string parentId;
    std::string description;
    std::string prompt;
    std::string result;

    enum class State {
        Pending, Running, Completed, Failed, Cancelled
    } state;

    float progress;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    int tokensGenerated;

    std::string stateString() const {
        switch (state) {
            case State::Pending:    return "pending";
            case State::Running:    return "running";
            case State::Completed:  return "completed";
            case State::Failed:     return "failed";
            case State::Cancelled:  return "cancelled";
        }
        return "unknown";
    }

    int elapsedMs() const {
        auto end = (state == State::Running)
            ? std::chrono::steady_clock::now() : endTime;
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            end - startTime).count();
    }
};

// ============================================================================
// ChainStep — one step in a sequential chain pipeline
// ============================================================================
struct ChainStep {
    int index;
    std::string promptTemplate;
    std::string result;
    SubAgent::State state;
    std::string subAgentId;
};

// ============================================================================
// SwarmTask — one parallel task in a HexMag swarm fan-out
// ============================================================================
struct SwarmTask {
    std::string id;
    std::string prompt;
    std::string result;
    SubAgent::State state;
    std::string subAgentId;
    float weight;
};

// ============================================================================
// SwarmConfig — configuration for a HexMag swarm operation
// ============================================================================
struct SwarmConfig {
    int maxParallel  = 4;
    int timeoutMs    = 60000;
    std::string mergeStrategy = "concatenate";
    std::string mergePrompt;
    bool failFast    = false;
};

// ============================================================================
// SubAgentManager — orchestrates subagent lifecycle, chains, and swarms
// ============================================================================
class SubAgentManager {
public:
    using OutputCallback      = std::function<void(const std::string& agentId, const std::string& token)>;
    using CompletionCallback  = std::function<void(const std::string& agentId, const std::string& result, bool success)>;
    using SwarmCompleteCallback = std::function<void(const std::string& mergedResult, bool allSucceeded)>;

    explicit SubAgentManager(AgenticEngine* engine);
    ~SubAgentManager();

    // ---- Logging / Metrics injection ----
    void setLogCallback(SubAgentLogCallback cb)       { m_logCb = cb; }
    void setMetricsCallback(SubAgentMetricsCallback cb){ m_metricsCb = cb; }
    void setHistoryRecorder(AgentHistoryRecorder* rec) { m_historyRecorder = rec; }
    void setPolicyEngine(PolicyEngine* engine)          { m_policyEngine = engine; }

    // ---- Core SubAgent Operations ----
    std::string spawnSubAgent(const std::string& parentId,
                               const std::string& description,
                               const std::string& prompt);
    void cancelSubAgent(const std::string& agentId);
    void cancelAll();
    const SubAgent* getSubAgent(const std::string& agentId) const;
    std::vector<SubAgent> getAllSubAgents() const;
    std::vector<SubAgent> getSubAgentsForParent(const std::string& parentId) const;
    bool waitForSubAgent(const std::string& agentId, int timeoutMs = 60000);
    std::string getSubAgentResult(const std::string& agentId) const;

    // ---- Chaining (Sequential Pipeline) ----
    std::string executeChain(const std::string& parentId,
                              const std::vector<std::string>& promptTemplates,
                              const std::string& initialInput = "");
    std::vector<ChainStep> getChainSteps() const;

    // ---- HexMag Swarm (Parallel Fan-out + Merge) ----
    std::string executeSwarm(const std::string& parentId,
                              const std::vector<std::string>& prompts,
                              const SwarmConfig& config = SwarmConfig{});
    std::string executeSwarmAsync(const std::string& parentId,
                                   const std::vector<std::string>& prompts,
                                   const SwarmConfig& config,
                                   SwarmCompleteCallback onComplete);
    std::vector<SwarmTask> getSwarmTasks(const std::string& swarmId) const;
    float getSwarmProgress(const std::string& swarmId) const;

    // ---- Todo List Management ----
    void setTodoList(const std::vector<TodoItem>& items);
    void updateTodoStatus(int id, TodoItem::Status status);
    std::vector<TodoItem> getTodoList() const;
    std::string todoListToJSON() const;

    // ---- Callbacks ----
    void setOutputCallback(OutputCallback cb)         { m_outputCallback = cb; }
    void setCompletionCallback(CompletionCallback cb) { m_completionCallback = cb; }

    // ---- IDE Tool Callbacks (for open_file, terminal_send) ----
    using OpenFileCallback = std::function<void(const std::string& path)>;
    using SendToTerminalCallback = std::function<void(const std::string& cmd)>;
    void setOpenFileCallback(OpenFileCallback cb)     { m_openFileCallback = cb; }
    void setSendToTerminalCallback(SendToTerminalCallback cb) { m_sendToTerminalCallback = cb; }
    void setWorkspaceRoot(const std::string& root)    { m_workspaceRoot = root; }

    // ---- Status ----
    int activeSubAgentCount() const;
    int totalSpawned() const { return m_totalSpawned.load(); }
    std::string getStatusSummary() const;

    // ---- Tool Dispatch ----
    bool dispatchToolCall(const std::string& parentId,
                           const std::string& modelOutput,
                           std::string& toolResult);

private:
    void runSubAgentThread(const std::string& agentId);
    std::string generateUUID() const;
    std::string mergeSwarmResults(const std::vector<SwarmTask>& tasks,
                                   const SwarmConfig& config);
    std::string replaceTemplate(const std::string& tmpl,
                                 const std::string& input) const;

    // Tool parsing helpers
    bool parseRunSubagent(const std::string& text, std::string& description,
                           std::string& prompt) const;
    bool parseTodoList(const std::string& text, std::vector<TodoItem>& items) const;
    bool parseChainCall(const std::string& text, std::vector<std::string>& steps,
                         std::string& initialInput) const;
    bool parseSwarmCall(const std::string& text, std::vector<std::string>& prompts,
                         SwarmConfig& config) const;
    bool parseBulkFixCall(const std::string& text, std::string& strategyName,
                           std::vector<std::string>& targetPaths, std::string& context) const;
    bool parseGenericToolCall(const std::string& text, std::string& toolName,
                               std::string& toolJson) const;
    std::string executeBulkFix(const std::string& parentId, const std::string& strategy,
                                const std::vector<std::string>& targets, const std::string& context);
    std::string executeBulkFixAsync(const std::string& parentId, const std::string& strategy,
                                     const std::vector<std::string>& targets);
    std::string executeGenericTool(const std::string& parentId,
                                    const std::string& toolName,
                                    const std::string& toolJson);

    // Logging/metrics helpers
    void logInfo(const std::string& msg) const  { if (m_logCb) m_logCb(1, msg); }
    void logError(const std::string& msg) const { if (m_logCb) m_logCb(3, msg); }
    void logDebug(const std::string& msg) const { if (m_logCb) m_logCb(0, msg); }
    void metric(const std::string& key) const   { if (m_metricsCb) m_metricsCb(key); }

    AgenticEngine* m_engine;
    AgentHistoryRecorder* m_historyRecorder = nullptr;
    PolicyEngine* m_policyEngine = nullptr;
    SubAgentLogCallback m_logCb;
    SubAgentMetricsCallback m_metricsCb;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<SubAgent>> m_agents;
    std::vector<std::thread> m_threads;

    std::vector<ChainStep> m_chainSteps;
    mutable std::mutex m_chainMutex;

    struct SwarmState {
        std::string swarmId;
        std::vector<SwarmTask> tasks;
        SwarmConfig config;
        SwarmCompleteCallback onComplete;
        std::atomic<int> completedCount{0};
        std::atomic<bool> cancelled{false};
        std::string mergedResult;
    };
    std::unordered_map<std::string, std::shared_ptr<SwarmState>> m_swarms;
    mutable std::mutex m_swarmMutex;

    std::vector<TodoItem> m_todoList;
    mutable std::mutex m_todoMutex;

    OutputCallback m_outputCallback;
    CompletionCallback m_completionCallback;
    OpenFileCallback m_openFileCallback;
    SendToTerminalCallback m_sendToTerminalCallback;
    std::string m_workspaceRoot;

    std::atomic<int> m_totalSpawned{0};
};
