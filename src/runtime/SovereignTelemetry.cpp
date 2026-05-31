#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>
#include <atomic>

/**
 * Sovereign Telemetry & Monitoring (Phase 49.2).
 * Real-time mesh health, throughput, and consensus latency tracking.
 */

namespace RawrXD::Runtime {

struct MeshStats {
    std::atomic<uint64_t> totalTokens{0};
    std::atomic<uint64_t> consensusSuccess{0};
    std::atomic<uint64_t> consensusFail{0};
    std::atomic<uint32_t> activeNodes{0};
    std::atomic<double> averageLatencyMs{0.0};
};

class SovereignTelemetry {
public:
    static SovereignTelemetry& instance() {
        static SovereignTelemetry inst;
        return inst;
    }

    void recordTokenThroughput(uint64_t count) {
        m_stats.totalTokens += count;
    }

    void recordConsensus(bool success, double latencyMs) {
        if (success) m_stats.consensusSuccess++;
        else m_stats.consensusFail++;
        
        // Rolling average for latency
        double oldLat = m_stats.averageLatencyMs.load();
        m_stats.averageLatencyMs.store((oldLat * 0.9) + (latencyMs * 0.1));
    }

    void report() {
        std::cout << "\n--- Sovereign Mesh Telemetry ---" << std::endl;
        std::cout << "Active Nodes: " << m_stats.activeNodes.load() << std::endl;
        std::cout << "Total Throughput: " << m_stats.totalTokens.load() << " tokens" << std::endl;
        std::cout << "Consensus: " << m_stats.consensusSuccess.load() << " OK / " 
                  << m_stats.consensusFail.load() << " ERR" << std::endl;
        std::cout << "Avg Latency: " << m_stats.averageLatencyMs.load() << " ms" << std::endl;
        std::cout << "--------------------------------\n" << std::endl;
    }

    void updateNodeCount(uint32_t count) {
        m_stats.activeNodes = count;
    }

private:
    MeshStats m_stats;
    SovereignTelemetry() {}
};

} // namespace RawrXD::Runtime
