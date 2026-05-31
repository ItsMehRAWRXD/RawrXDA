#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <chrono>

namespace rawrxd {
namespace agents {

enum class AgentRole {
    Planner,
    Coder,
    Reviewer,
    Tester,
    Architect,
    Security,
    Optimizer,
    Documenter
};

enum class ExecutionMode {
    Parallel,      // All agents run concurrently within VRAM budget
    Sequential,    // One at a time
    Pipeline,      // Ordered by dependencies
    Collaborative  // Two-pass with shared context
};

enum class AggregationMode {
    Union,       // All outputs
    Refinement,  // Last output wins
    Voting,      // Majority on suggestions
    Synthesis    // Combined coherent output
};

struct AgentConfig {
    std::string id;
    std::string name;
    AgentRole role;
    std::string systemPrompt;
    std::vector<std::string> tools;
    std::vector<std::string> dependencies;
    uint32_t priority; // 0 = highest
};

struct AgentTask {
    std::string type;        // "ask", "plan", "code", "review", "test", "document"
    std::string input;
    std::string contextFile;
    std::string contextSelection;
};

struct AgentArtifact {
    std::string type;        // "code", "test", "documentation", "plan"
    std::string name;
    std::string content;
    std::string language;
};

struct AgentResult {
    std::string agentId;
    std::string agentName;
    std::string output;
    std::vector<AgentArtifact> artifacts;
    std::vector<std::string> suggestions;
    bool success;
    std::string error;
    uint64_t executionTimeMs;
    uint32_t tokenUsage;
};

struct HardwareProfile {
    uint64_t totalVRAM;      // bytes
    uint64_t availableVRAM;  // bytes
    uint32_t gpuCount;
    bool hasVulkan;
    bool hasHIP;
    bool hasCUDA;
};

struct ExecutionPlan {
    std::vector<std::vector<std::string>> stages; // Each stage = agents that can run in parallel
    uint32_t estimatedVRAMPerAgent;
    uint32_t maxConcurrentAgents;
};

// Forward declarations
class ModelPool;
class VulkanDevice;

class AgentExecutor {
public:
    AgentExecutor(std::shared_ptr<ModelPool> modelPool);
    ~AgentExecutor();

    // Hardware detection
    HardwareProfile detectHardware();
    
    // Execution
    std::vector<AgentResult> execute(
        const std::vector<AgentConfig>& agents,
        const AgentTask& task,
        ExecutionMode mode,
        std::function<void(const std::string&, float)> onProgress = nullptr
    );

    // Cancellation
    void cancelAll();
    void cancelAgent(const std::string& agentId);

    // Status
    bool isRunning() const;
    std::vector<std::string> getActiveAgents() const;

private:
    std::shared_ptr<ModelPool> modelPool_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> running_{false};
    mutable std::mutex activeMutex_;
    std::unordered_map<std::string, std::future<AgentResult>> activeFutures_;
    std::condition_variable completionCV_;

    // Execution modes
    std::vector<AgentResult> executeParallel(
        const std::vector<AgentConfig>& agents,
        const AgentTask& task,
        std::function<void(const std::string&, float)> onProgress
    );
    
    std::vector<AgentResult> executeSequential(
        const std::vector<AgentConfig>& agents,
        const AgentTask& task,
        std::function<void(const std::string&, float)> onProgress
    );
    
    std::vector<AgentResult> executePipeline(
        const std::vector<AgentConfig>& agents,
        const AgentTask& task,
        std::function<void(const std::string&, float)> onProgress
    );
    
    std::vector<AgentResult> executeCollaborative(
        const std::vector<AgentConfig>& agents,
        const AgentTask& task,
        std::function<void(const std::string&, float)> onProgress
    );

    // Core agent execution
    AgentResult executeAgent(
        const AgentConfig& agent,
        const AgentTask& task,
        const std::unordered_map<std::string, std::string>& previousOutputs
    );

    // Planning
    ExecutionPlan createExecutionPlan(
        const std::vector<AgentConfig>& agents,
        const HardwareProfile& hardware
    );
    
    std::vector<AgentConfig> resolveDependencies(
        const std::vector<AgentConfig>& agents
    );

    // Helpers
    std::string buildPrompt(
        const AgentConfig& agent,
        const AgentTask& task,
        const std::unordered_map<std::string, std::string>& previousOutputs
    );
    
    AgentResult parseResponse(const std::string& response, const AgentConfig& agent);
    
    bool isCancelled() const { return cancelled_.load(); }
};

} // namespace agents
} // namespace rawrxd
