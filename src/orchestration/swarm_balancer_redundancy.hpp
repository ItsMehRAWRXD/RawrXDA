#include "shard_router_metadata.hpp"
#include <algorithm>
#include <numeric>

/**
 * @class SwarmLoadBalancer
 * @brief Manages traffic distribution across shards based on real-time node pressure.
 */
class SwarmLoadBalancer {
public:
    /**
     * @brief Normalizes node load using a combination of VRAM pressure and network latency.
     * Returns a weight where higher = more available capacity.
     */
    double calculateNodeScore(const SwarmNodeStatus& node) {
        if (!node.isActive) return 0.0;
        
        // Pressure-aware scoring: Inverse of latency squared * VRAM ratio
        double latencyFactor = 1.0 / (node.lastLatencyMs * node.lastLatencyMs + 1.0);
        double vramFactor = (double)node.availableVRAM / 1024.0; // Units of GB
        
        return latencyFactor * vramFactor * 100.0;
    }

    /**
     * @brief Selects the best node for a shard using weighted round-robin or 
     * weighted-random selection based on calculated scores.
     */
    std::string balanceShardTraffic(const std::vector<SwarmNodeStatus>& candidates) {
        if (candidates.empty()) return "";
        
        std::vector<double> scores;
        for (const auto& node : candidates) {
            scores.push_back(calculateNodeScore(node));
        }

        double totalScore = std::accumulate(scores.begin(), scores.end(), 0.0);
        if (totalScore <= 0.0) return candidates[0].nodeId;

        // Simple roulette wheel selection
        double target = (double)rand() / RAND_MAX * totalScore;
        double current = 0.0;
        for (size_t i = 0; i < candidates.size(); ++i) {
            current += scores[i];
            if (current >= target) return candidates[i].nodeId;
        }

        return candidates.back().nodeId;
    }
};

/**
 * @class RedundancyManager
 * @brief Ensures K-Replica availability for every critical model layer.
 */
class RedundancyManager {
public:
    static const int MIN_REPLICAS = 3;

    /**
     * @brief Audits the swarm registry and triggers shard replication if 
     * layer availability drops below MIN_REPLICAS.
     */
    void auditLayerAvailability(uint32_t layerId, const std::vector<std::string>& activeNodes) {
        if (activeNodes.size() < MIN_REPLICAS) {
            // Trigger background P2P shard transfer
            requestShardReplication(layerId, MIN_REPLICAS - activeNodes.size());
        }
    }

private:
    struct TransferJob {
        uint32_t layerId;
        std::string targetNode;
        int priority;
        enum class TransferStatus { Queued, InProgress, Completed, Failed };
        TransferStatus status;
    };
    
    struct PeerInfo {
        std::string nodeId;
        std::string address;
        uint16_t port;
    };
    
    std::mutex mutex_;
    std::vector<PeerInfo> peers_;
    std::vector<TransferJob> transferQueue_;
    
    void requestShardReplication(uint32_t layerId, size_t needed) {
        // Production: enqueue P2P layer transfer jobs to reachable peers
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < needed && i < peers_.size(); ++i) {
            TransferJob job;
            job.layerId = layerId;
            job.targetNode = peers_[i].nodeId;
            job.priority = 1;
            job.status = TransferJob::TransferStatus::Queued;
            transferQueue_.push_back(job);
        }
    }
};
