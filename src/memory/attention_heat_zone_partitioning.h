#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

enum class AHZone {
    Hot,
    Warm,
    Cold
};

struct AHEntry {
    uint64_t kvId = 0;
    float attentionTemp = 0.0f;
    float emaAttentionTemp = 0.0f;
    size_t bytes = 0;
    AHZone zone = AHZone::Cold;
};

struct AHMigrateAction {
    uint64_t kvId = 0;
    AHZone from = AHZone::Cold;
    AHZone to = AHZone::Cold;
};

class AttentionHeatZonePartitioning {
public:
    void setThresholds(float hotMin, float warmMin);
    void setEmaDecay(float decay);
    void setHysteresis(float band);

    // Update token/span heat and assign zone.
    AHZone upsert(uint64_t kvId, float attentionTemp, size_t bytes);

    // Recompute all zones and return migration actions.
    std::vector<AHMigrateAction> rebalance();

    // Pressure-aware policy tweak (0.0..1.0). High pressure makes hot zone stricter.
    void onVramPressure(float pressureRatio);

private:
    AHZone classify(float temp) const;
    AHZone classifyWithHysteresis(AHZone current, float temp) const;

    mutable std::mutex m_mutex;
    float m_hotMin = 0.70f;
    float m_warmMin = 0.35f;
    float m_emaDecay = 0.95f;        // Increased from 0.90 for better smoothing
    float m_hysteresisBand = 0.12f;  // Increased from 0.05 to reduce thrashing
    std::unordered_map<uint64_t, AHEntry> m_entries;
};

} // namespace RawrXD::Memory
