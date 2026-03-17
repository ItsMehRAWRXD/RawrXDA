#include <string>
#include <map>
#include <vector>
#include <atomic>

/**
 * @struct ModelShardMetadata
 * @brief Identifies a specific weight block (Layer Range) in the 800B model.
 */
struct ModelShardMetadata {
    uint32_t layerStart;
    uint32_t layerEnd;
    uint64_t totalParameters;
    char shardHash[32]; // SHA-256 for validation
};

/**
 * @struct NodeCapacity
 * @brief Detailed real-time capacity metrics for load balancing.
 */
struct NodeCapacity {
    uint64_t available_vram_bytes;
    uint32_t active_shard_count;
    uint32_t queue_depth;
    uint64_t last_rdtsc_latency;
};

/**
 * @struct SwarmNodeStatus
 * @brief Dynamic health and capacity of a node in the inference swarm.
 */
struct SwarmNodeStatus {
    std::string nodeId;
    NodeCapacity capacity;
    bool isActive;
};

/**
 * @class RawrXD_ShardRouter
 * @brief Distributed router for 800B Model Shards.
 */
class RawrXD_ShardRouter {
public:
    RawrXD_ShardRouter();
    
    /**
     * @brief Registers a shard available on a specific swarm node.
     */
    void registerShard(const std::string& nodeId, const ModelShardMetadata& shard);
    
    /**
     * @brief Routes a tensor activation to the optimal node for the next layer.
     */
    std::string selectOptimalNode(uint32_t nextLayerId);

private:
    std::map<uint32_t, std::vector<std::string>> m_layerToNodes; // Layer ID -> Node IDs
    std::map<std::string, SwarmNodeStatus> m_nodeHealth;
};
