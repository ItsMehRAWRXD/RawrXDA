#pragma once
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <mutex>
#include <map>
#include <functional>
#include "../todo_manager.h"

namespace RawrXD::Agentic {

struct SwarmTopology {
    uint32_t workerCount;
    std::chrono::milliseconds taskTimeout;
    bool gpuWorkStealing;
};

struct ShardStatus {
    std::string path;
    uint32_t gpuIndex;
    bool isLoaded;
    size_t sizeInBytes;
};

struct SubAgentInfo {
    std::string id;
    std::string description;
    std::string stateString() const { return "idle"; }
    int64_t elapsedMs() const { return 0; }
};

class SubAgentManager {
public:
    static SubAgentManager& instance();
    
    SubAgentManager(void* parent = nullptr);
    ~SubAgentManager();
    
    // Logging
    using LogCallback = std::function<void(int level, const std::string& msg)>;
    void setLogCallback(LogCallback cb) { m_logCb = std::move(cb); }
    
    // History and Policy
    void setHistoryRecorder(void* recorder) { m_historyRecorder = recorder; }
    void setPolicyEngine(void* engine) { m_policyEngine = engine; }
    
    // Slot 20: Called by Win32SwarmBridge
    bool initializeSwarm(const SwarmTopology& topology, 
                        const std::string& coordinatorModel);
    
    // Slot 55: Called by Win32SwarmBridge
    void shutdownSwarm();
    
    // Slot 54: Swarm Task Execution
    uint32_t executeSwarmTask(const std::string& taskDescription);
    
    bool isSwarmActive() const;

    // --- 800B Model Sharding & Enhancements ---
    bool loadModelShard(const std::string& shardPath, uint32_t gpuIndex);
    void synchronizeShards();
    
    // Production Enhancements (Properly Wired)
    size_t getActiveShardCount() const;
    void broadcastCommand(const std::string& command);
    float getSwarmLoadAverage() const;
    
    // --- Sub-agent operations ---
    bool dispatchToolCall(const std::string& tool, const std::string& input, std::string& output);
    std::string spawnSubAgent(const std::string& type, const std::string& name, const std::string& prompt);
    bool waitForSubAgent(const std::string& id, int timeoutMs);
    std::string getSubAgentResult(const std::string& id);
    std::string executeChain(const std::string& tool, const std::vector<std::string>& steps);
    std::string executeSwarm(const std::string& tool, const std::vector<std::string>& prompts);
    std::vector<SubAgentInfo> getAllSubAgents();
    std::string getStatusSummary();
    
    // --- Todo integration ---
    std::vector<TodoItem> getTodoList();

private:
    SubAgentManager(const SubAgentManager&) = delete;
    SubAgentManager& operator=(const SubAgentManager&) = delete;

    bool m_initialized{false};
    SwarmTopology m_topology;
    std::string m_coordinatorModel;
    std::vector<std::string> m_activeShards;
    std::map<std::string, ShardStatus> m_shardMap;
    mutable std::mutex m_mtx;
    
    LogCallback m_logCb;
    void* m_historyRecorder{nullptr};
    void* m_policyEngine{nullptr};
    std::vector<TodoItem> m_todos;

    // Internal methods for properly wired orchestration
    void internal_rebalanceLoad();
    void internal_verifyShardIntegrity();
};

} // namespace RawrXD::Agentic
