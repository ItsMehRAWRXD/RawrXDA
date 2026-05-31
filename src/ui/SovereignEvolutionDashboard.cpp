// ==================================================
// SovereignEvolutionDashboard.cpp — Real-time Telemetry for the Swarm
// ==================================================
#include "SovereignEvolutionDashboard.h"
#include <sstream>
#include <chrono>
#include <iomanip>

namespace RawrXD {
namespace UI {

EvolutionDashboard& EvolutionDashboard::Instance() {
    static EvolutionDashboard instance;
    return instance;
}

void EvolutionDashboard::PushEvent(const std::string& nodeId, const std::string& kernel, uint64_t cycles) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    EvolutionPoint p;
    p.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    p.cycles = cycles;
    p.kernelName = kernel;
    p.nodeId = nodeId;
    m_nodeHistory[nodeId].push_back(p);
}

void EvolutionDashboard::ReportRollback(const std::string& nodeId, const std::string& failedKernel, const std::string& restoredKernel) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::string alert = "[TKV_ROLLBACK] Node " + nodeId + " rejected " + failedKernel + " -> Restored " + restoredKernel;
    m_globalAlerts.push_back(alert);
}

std::string EvolutionDashboard::GenerateSovereignReport() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::stringstream ss;

    ss << "### 🧬 SOVEREIGN SWARM EVOLUTION REPORT\n\n";
    ss << "| Node ID | Active Kernel | Fitness (Cycles) | Status |\n";
    ss << "| :--- | :--- | :--- | :--- |\n";

    for (auto const& [nodeId, history] : m_nodeHistory) {
        if (history.empty()) continue;
        const auto& latest = history.back();
        ss << "| " << nodeId << " | " << latest.kernelName << " | " << latest.cycles << " | ✅ STABLE |\n";
    }

    if (!m_globalAlerts.empty()) {
        ss << "\n#### ⚠️ SYSTEM ALERTS\n";
        for (const auto& alert : m_globalAlerts) {
            ss << "- " << alert << "\n";
        }
    }

    return ss.str();
}

} // namespace UI
} // namespace RawrXD
