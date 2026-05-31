// ==================================================
// SovereignEvolutionDashboard.h — Visualizing the Swarm's Fitness
// ==================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace RawrXD {
namespace UI {

struct EvolutionPoint {
    long long timestamp;
    uint64_t cycles;
    std::string kernelName;
    std::string nodeId;
};

/**
 * @brief Telemetry aggregator for the Sovereign Evolution Dashboard.
 * Collects cycle counts, ZK-Handshake results, and TKV lineage snapshots.
 */
class EvolutionDashboard {
public:
    static EvolutionDashboard& Instance();

    void PushEvent(const std::string& nodeId, const std::string& kernel, uint64_t cycles);
    void ReportRollback(const std::string& nodeId, const std::string& failedKernel, const std::string& restoredKernel);
    
    // Returns a Markdown-formatted summary of the Swarm's current fitness state
    std::string GenerateSovereignReport();

private:
    EvolutionDashboard() = default;
    std::mutex m_dataMutex;
    std::map<std::string, std::vector<EvolutionPoint>> m_nodeHistory;
    std::vector<std::string> m_globalAlerts;
};

} // namespace UI
} // namespace RawrXD
