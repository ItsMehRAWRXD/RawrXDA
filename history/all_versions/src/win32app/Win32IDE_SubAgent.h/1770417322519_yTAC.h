#pragma once
// ============================================================================
// Win32IDE_SubAgent.h — SubAgent, Chaining & HexMag Swarm System
// ============================================================================
// Provides:
//   - SubAgent spawning: models can launch child agents for subtasks
//   - Chaining: sequential multi-step pipelines (output_n → input_n+1)
//   - HexMag Swarm: parallel fan-out of subtasks with merge/reduce
//   - TodoList: structured task tracking across agent iterations
//
// Integration points:
//   - AgenticBridge::ExecuteAgentCommand detects tool calls in LLM output
//   - NativeAgent::Ask prompt includes tool descriptions
//   - AutonomyManager uses SubAgentManager for complex task decomposition
//   - StreamingUX displays swarm progress
// ============================================================================

#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

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
#include "IDELogger.h"

// Forward declarations
class AgenticBridge;
class AgenticEngine;
namespace RawrXD { class CPUInferenceEngine; }

// ============================================================================
// Todo Item — structured task tracking for agent iterations
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
    std::string id;                 // Unique agent ID (UUID)
    std::string parentId;           // Parent agent/session that spawned it
    std::string description;        // Short description of the task
    std::string prompt;             // The full prompt sent to the sub-model
    std::string result;             // Accumulated output
    
    enum class State {
        Pending,        // Queued, not yet started
        Running,        // Currently generating
        Completed,      // Finished successfully
        Failed,         // Error during generation
        Cancelled       // User or parent cancelled
    } state;

    float progress;                 // 0.0 – 1.0
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
    int index;                      // 0-based step index
    std::string promptTemplate;     // May contain {{input}} placeholder
    std::string result;             // Output of this step
    SubAgent::State state;
    std::string subAgentId;         // SubAgent assigned to this step
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
    float weight;                   // Importance weight for merge (default 1.0)
};

// ============================================================================
// SwarmConfig — configuration for a HexMag swarm operation
// ============================================================================
struct SwarmConfig {
    int maxParallel;                // Max concurrent sub-agents (default 4)
    int timeoutMs;                  // Per-task timeout (default 60000)
    std::string mergeStrategy;      // "concatenate", "vote", "summarize"
    std::string mergePrompt;        // Custom merge prompt (for "summarize")
    bool failFast;                  // Cancel remaining on first failure
};

// ============================================================================
// SubAgentManager — orchestrates subagent lifecycle, chains, and swarms
// ============================================================================
class SubAgentManager {
public:
    using OutputCallback = std::function<void(const std::string& agentId,
                                              const std::string& token)>;
    using CompletionCallback = std::function<void(const std::string& agentId,
                                                   const std::string& result,
                                                   bool success)>;
    using SwarmCompleteCallback = std::function<void(const std::string& mergedResult,
                                                      bool allSucceeded)>;

    explicit SubAgentManager(AgenticEngine* engine);
    ~SubAgentManager();

    // ---- Core SubAgent Operations ----

    /// Spawn a new sub-agent with a prompt. Returns agent ID.
    std::string spawnSubAgent(const std::string& parentId,
                               const std::string& description,
                               const std::string& prompt);

    /// Cancel a running sub-agent
    void cancelSubAgent(const std::string& agentId);

    /// Cancel all running sub-agents
    void cancelAll();

    /// Get a sub-agent's current state
    const SubAgent* getSubAgent(const std::string& agentId) const;

    /// Get all sub-agents (snapshot)
    std::vector<SubAgent> getAllSubAgents() const;

    /// Get sub-agents spawned by a specific parent
    std::vector<SubAgent> getSubAgentsForParent(const std::string& parentId) const;

    /// Wait for a sub-agent to complete (blocking, with timeout)
    bool waitForSubAgent(const std::string& agentId, int timeoutMs = 60000);

    /// Get sub-agent result (empty if not completed)
    std::string getSubAgentResult(const std::string& agentId) const;

    // ---- Chaining (Sequential Pipeline) ----

    /// Execute a chain of prompts sequentially. Each step's output feeds the next.
    /// promptTemplates may contain {{input}} to be replaced with prior output.
    /// Returns the final output.
    std::string executeChain(const std::string& parentId,
                              const std::vector<std::string>& promptTemplates,
                              const std::string& initialInput = "");

    /// Get chain steps (for UI progress display)
    std::vector<ChainStep> getChainSteps() const;

    // ---- HexMag Swarm (Parallel Fan-out + Merge) ----

    /// Launch a HexMag swarm: fan out prompts in parallel, merge results.
    /// Returns merged result.
    std::string executeSwarm(const std::string& parentId,
                              const std::vector<std::string>& prompts,
                              const SwarmConfig& config = SwarmConfig{4, 60000, "concatenate", "", false});

    /// Launch swarm asynchronously (non-blocking).
    /// swarmId is returned; use callbacks or poll for results.
    std::string executeSwarmAsync(const std::string& parentId,
                                   const std::vector<std::string>& prompts,
                                   const SwarmConfig& config,
                                   SwarmCompleteCallback onComplete);

    /// Get swarm tasks for a running swarm
    std::vector<SwarmTask> getSwarmTasks(const std::string& swarmId) const;

    /// Get overall swarm progress (0.0 – 1.0)
    float getSwarmProgress(const std::string& swarmId) const;

    // ---- Todo List Management ----

    /// Set the full todo list (replaces existing)
    void setTodoList(const std::vector<TodoItem>& items);

    /// Update a single todo item's status
    void updateTodoStatus(int id, TodoItem::Status status);

    /// Get current todo list
    std::vector<TodoItem> getTodoList() const;

    /// Serialize todo list to JSON string
    std::string todoListToJSON() const;

    // ---- Callbacks ----
    void setOutputCallback(OutputCallback cb) { m_outputCallback = cb; }
    void setCompletionCallback(CompletionCallback cb) { m_completionCallback = cb; }

    // ---- Status ----
    int activeSubAgentCount() const;
    int totalSpawned() const { return m_totalSpawned.load(); }
    std::string getStatusSummary() const;

    // ---- Tool Dispatch (parses LLM output for tool calls) ----

    /// Detect and execute tool calls in model output.
    /// Returns true if a tool call was found and executed.
    /// `toolResult` receives the tool's output for re-injection.
    bool dispatchToolCall(const std::string& parentId,
                           const std::string& modelOutput,
                           std::string& toolResult);

private:
    // Internal execution
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

    AgenticEngine* m_engine;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<SubAgent>> m_agents;
    std::vector<std::thread> m_threads;

    // Chain state
    std::vector<ChainStep> m_chainSteps;
    mutable std::mutex m_chainMutex;

    // Swarm state
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

    // Todo list
    std::vector<TodoItem> m_todoList;
    mutable std::mutex m_todoMutex;

    // Callbacks
    OutputCallback m_outputCallback;
    CompletionCallback m_completionCallback;

    // Counters
    std::atomic<int> m_totalSpawned{0};
};
