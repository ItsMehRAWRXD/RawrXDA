#pragma once
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <mutex>
#include <map>

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

class SubAgentManager {
public:
    static SubAgentManager& instance();
    
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

private:
    SubAgentManager() = default;
    ~SubAgentManager() = default;
    SubAgentManager(const SubAgentManager&) = delete;
    SubAgentManager& operator=(const SubAgentManager&) = delete;

    bool m_initialized{false};
    SwarmTopology m_topology;
    std::string m_coordinatorModel;
    std::vector<std::string> m_activeShards;
    std::map<std::string, ShardStatus> m_shardMap;
    mutable std::mutex m_mtx;

    // Internal methods for properly wired orchestration
    void internal_rebalanceLoad();
    void internal_verifyShardIntegrity();
