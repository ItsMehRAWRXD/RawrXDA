#pragma once

#include "SovereignKAIROSBridge.h"
#include <vector>
#include <string>
#include <deque>

namespace RawrXD::Runtime {

struct MetricSnapshot {
    long long timestamp;
    uint32_t activeOps;
    uint32_t diskUsage;
    uint32_t cacheHits;
    uint32_t memoryWorkingSet;
};

class SovereignTelemetryHub {
public:
    static SovereignTelemetryHub& instance();

    void recordInferenceEvent(uint32_t tokensPerSec, uint32_t latencyMs);
    void updateKairosMetrics(const KairosStats& stats);

    std::vector<MetricSnapshot> getRecentSnapshots(uint32_t count);

    // NeuralMeshSync: Aggregates across multiple local/remote instances
    void syncWithDistantNodes();

private:
    SovereignTelemetryHub();
    std::deque<MetricSnapshot> m_history;
    std::mutex m_mutex;
};

} // namespace RawrXD::Runtime
