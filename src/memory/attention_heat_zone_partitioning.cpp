#include "attention_heat_zone_partitioning.h"

#include <algorithm>

namespace RawrXD::Memory {

void AttentionHeatZonePartitioning::setThresholds(float hotMin, float warmMin) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (warmMin > hotMin) {
        std::swap(warmMin, hotMin);
    }
    m_hotMin = std::clamp(hotMin, 0.0f, 1.0f);
    m_warmMin = std::clamp(warmMin, 0.0f, m_hotMin);
}

void AttentionHeatZonePartitioning::setEmaDecay(float decay) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_emaDecay = std::clamp(decay, 0.50f, 0.999f);
}

void AttentionHeatZonePartitioning::setHysteresis(float band) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hysteresisBand = std::clamp(band, 0.0f, 0.50f);
}

AHZone AttentionHeatZonePartitioning::classify(float temp) const {
    if (temp >= m_hotMin) {
        return AHZone::Hot;
    }
    if (temp >= m_warmMin) {
        return AHZone::Warm;
    }
    return AHZone::Cold;
}

AHZone AttentionHeatZonePartitioning::classifyWithHysteresis(AHZone current, float temp) const {
    const float hotUp = m_hotMin + m_hysteresisBand;
    const float hotDown = m_hotMin - m_hysteresisBand;
    const float warmUp = m_warmMin + m_hysteresisBand;
    const float warmDown = m_warmMin - m_hysteresisBand;

    if (current == AHZone::Hot) {
        if (temp < hotDown) {
            return (temp >= warmDown) ? AHZone::Warm : AHZone::Cold;
        }
        return AHZone::Hot;
    }

    if (current == AHZone::Warm) {
        if (temp >= hotUp) return AHZone::Hot;
        if (temp < warmDown) return AHZone::Cold;
        return AHZone::Warm;
    }

    // Cold
    if (temp >= hotUp) return AHZone::Hot;
    if (temp >= warmUp) return AHZone::Warm;
    return AHZone::Cold;
}

AHZone AttentionHeatZonePartitioning::upsert(uint64_t kvId, float attentionTemp, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    AHEntry& e = m_entries[kvId];
    e.kvId = kvId;
    e.attentionTemp = std::clamp(attentionTemp, 0.0f, 1.0f);
    if (e.emaAttentionTemp == 0.0f && e.bytes == 0) {
        e.emaAttentionTemp = e.attentionTemp;
    } else {
        e.emaAttentionTemp = e.emaAttentionTemp * m_emaDecay + e.attentionTemp * (1.0f - m_emaDecay);
    }
    e.bytes = bytes;
    e.zone = classifyWithHysteresis(e.zone, e.emaAttentionTemp);
    return e.zone;
}

std::vector<AHMigrateAction> AttentionHeatZonePartitioning::rebalance() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<AHMigrateAction> actions;
    actions.reserve(m_entries.size());
    for (auto& [id, e] : m_entries) {
        const AHZone newZone = classifyWithHysteresis(e.zone, e.emaAttentionTemp);
        if (newZone != e.zone) {
            actions.push_back(AHMigrateAction{ id, e.zone, newZone });
            e.zone = newZone;
        }
    }
    return actions;
}

void AttentionHeatZonePartitioning::onVramPressure(float pressureRatio) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const float p = std::clamp(pressureRatio, 0.0f, 1.0f);
    // Under pressure, raise hot cutoff so fewer spans stay full-precision on GPU.
    m_hotMin = std::clamp(0.70f + 0.20f * p, 0.70f, 0.90f);
    // Keep warm threshold below hot and also slightly stricter under pressure.
    m_warmMin = std::clamp(0.30f + 0.15f * p, 0.30f, m_hotMin - 0.05f);
}

} // namespace RawrXD::Memory
