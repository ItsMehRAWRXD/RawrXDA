#include "shard_router_metadata.hpp"
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

/**
 * @class SwarmLoadBalancer
 * @brief Capacity-Aware Load Balancer for 800B Model Shards.
 */
class SwarmLoadBalancer {
public:
    struct HealthReport {
        std::string nodeId;
        double loadScore;
        bool backpressureTriggered;
    };

    /**
     * @brief Calculates a normalized score where 100.0 is perfect availability.
     * Routes based on (availableVRAM / (queueDepth + activeShards)).
     */
    double calculateCapacityScore(const SwarmNodeStatus& node) {
        if (!node.isActive) return 0.0;
        
        double vramGB = (double)node.capacity.available_vram_bytes / (1024.0 * 1024.0 * 1024.0);
        double pressure = (double)node.capacity.queue_depth + (double)node.capacity.active_shard_count + 1.0;
        
        // Base score = GB per unit of pressure
        double score = (vramGB / pressure) * 10.0;
        
        // Latency penalty (exponential decay)
        double latencySeconds = (double)node.capacity.last_rdtsc_latency / 2.5e9; // Approx 2.5GHz
        score *= std::exp(-latencySeconds * 2.0);
        
        return std::clamp(score, 0.0, 100.0);
    }

    /**
     * @brief Selects the optimal node for a shard request.
     * Triggers backpressure if all candidates are below health threshold.
     */
    std::string routeShardRequest(const std::vector<SwarmNodeStatus>& candidates) {
        if (candidates.empty()) return "";

        std::string bestNode;
        double bestScore = -1.0;
        bool allSaturated = true;

        for (const auto& node : candidates) {
            double score = calculateCapacityScore(node);
            if (score > 5.0) allSaturated = false; // 5.0 = Saturation threshold

            if (score > bestScore) {
                bestScore = score;
                bestNode = node.nodeId;
            }
        }

        if (allSaturated) {
            triggerBackpressureSignal(bestNode, "NODE_SATURATED");
        }

        return bestNode;
    }

private:
    void triggerBackpressureSignal(const std::string& nodeId, const std::string& signal) {
        // Beaconism integration: Send szLBBackpressure signal
        // extern "C" void BeaconSend(uint32_t id, const char* msg, ...);
        // BeaconSend(8, "szLBBackpressure: %s on %s", signal.c_str(), nodeId.c_str());
    }
};
