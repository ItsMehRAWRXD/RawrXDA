#include "cot_fallback_system.hpp"

// ============================================================================
// CoTFallbackSystem — Singleton & core controls
// Provides the 4 symbols required by the Win32IDE link target.
// ============================================================================

CoTFallbackSystem& CoTFallbackSystem::instance() {
    static CoTFallbackSystem inst;
    return inst;
}

PatchResult CoTFallbackSystem::disableCoT(const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    CoTBackendState prev = m_state.load(std::memory_order_acquire);
    if (prev == CoTBackendState::Disabled) {
        return PatchResult::ok("CoT already disabled");
    }

    m_state.store(CoTBackendState::Disabled, std::memory_order_release);
    m_stats.manualDisables.fetch_add(1, std::memory_order_relaxed);

    CoTFallbackEvent evt{};
    evt.eventId           = nextEventId();
    evt.timestampMs       = currentTimeMs();
    evt.triggerState      = CoTBackendState::Disabled;
    evt.modeUsed          = m_config.fallbackMode;
    evt.reason            = "Manual disable: " + reason;
    evt.cotLatencyMs      = -1;
    evt.fallbackLatencyMs = 0;
    evt.estimatedQualityLoss = 0.0f;
    evt.userNotified      = false;
    recordFallbackEvent(evt);

    if (m_stateChangeCb) {
        m_stateChangeCb(prev, CoTBackendState::Disabled, reason);
    }

    return PatchResult::ok("CoT disabled");
}

PatchResult CoTFallbackSystem::enableCoT() {
    std::lock_guard<std::mutex> lock(m_mutex);

    CoTBackendState prev = m_state.load(std::memory_order_acquire);
    if (prev == CoTBackendState::Healthy) {
        return PatchResult::ok("CoT already healthy");
    }

    m_cbState = CircuitBreakerState{};
    m_rollingWindow.clear();

    m_state.store(CoTBackendState::Healthy, std::memory_order_release);

    if (m_stateChangeCb) {
        m_stateChangeCb(prev, CoTBackendState::Healthy, "Manual re-enable");
    }

    return PatchResult::ok("CoT re-enabled");
}

bool CoTFallbackSystem::isCoTAvailable() const {
    CoTBackendState s = m_state.load(std::memory_order_acquire);
    return s == CoTBackendState::Healthy || s == CoTBackendState::Degraded;
}
