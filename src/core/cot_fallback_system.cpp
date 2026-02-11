// ============================================================================
// cot_fallback_system.cpp — Chain-of-Thought Fallback & Disable System
// ============================================================================
// Implementation: Circuit breaker, fallback routing, health monitoring
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "cot_fallback_system.hpp"
#include <algorithm>
#include <numeric>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ============================================================================
// Utility
// ============================================================================
namespace {

uint64_t currentTimeMs() {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return static_cast<uint64_t>(uli.QuadPart / 10000ULL - 11644473600000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
           static_cast<uint64_t>(tv.tv_usec) / 1000ULL;
#endif
}

const char* stateToString(CoTBackendState state) {
    switch (state) {
        case CoTBackendState::Healthy:      return "Healthy";
        case CoTBackendState::Degraded:     return "Degraded";
        case CoTBackendState::CircuitOpen:  return "CircuitOpen";
        case CoTBackendState::Disabled:     return "Disabled";
        default:                            return "Unknown";
    }
}

const char* modeToString(CoTFallbackMode mode) {
    switch (mode) {
        case CoTFallbackMode::DirectInference:  return "DirectInference";
        case CoTFallbackMode::CachedResponse:   return "CachedResponse";
        case CoTFallbackMode::SimplifiedChain:  return "SimplifiedChain";
        case CoTFallbackMode::ErrorResponse:    return "ErrorResponse";
        default:                                return "Unknown";
    }
}

} // anonymous namespace

// ============================================================================
// Singleton
// ============================================================================
CoTFallbackSystem& CoTFallbackSystem::instance() {
    static CoTFallbackSystem inst;
    return inst;
}

CoTFallbackSystem::CoTFallbackSystem()
    : m_config()
{
    m_state.store(CoTBackendState::Healthy, std::memory_order_relaxed);
}

// ============================================================================
// Configuration
// ============================================================================
void CoTFallbackSystem::setConfig(const CoTFallbackConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

CoTFallbackConfig CoTFallbackSystem::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void CoTFallbackSystem::setCircuitBreakerConfig(const CircuitBreakerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.circuitBreaker = config;
}

// ============================================================================
// Manual Controls
// ============================================================================
PatchResult CoTFallbackSystem::disableCoT(const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    CoTBackendState prev = m_state.load(std::memory_order_acquire);
    if (prev == CoTBackendState::Disabled) {
        return PatchResult::ok("CoT already disabled");
    }

    m_state.store(CoTBackendState::Disabled, std::memory_order_release);
    m_stats.manualDisables.fetch_add(1, std::memory_order_relaxed);

    // Record event
    CoTFallbackEvent evt{};
    evt.eventId = nextEventId();
    evt.timestampMs = currentTimeMs();
    evt.triggerState = CoTBackendState::Disabled;
    evt.modeUsed = m_config.fallbackMode;
    evt.reason = "Manual disable: " + reason;
    evt.cotLatencyMs = -1;
    evt.fallbackLatencyMs = 0;
    evt.estimatedQualityLoss = 0.0f;
    evt.userNotified = false;
    recordFallbackEvent(evt);

    // Notify callback outside lock is ideal, but we keep it simple
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

    // Reset circuit breaker state
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

CoTBackendState CoTFallbackSystem::getState() const {
    return m_state.load(std::memory_order_acquire);
}

// ============================================================================
// Request Routing
// ============================================================================
bool CoTFallbackSystem::shouldUseCoT() const {
    CoTBackendState s = m_state.load(std::memory_order_acquire);
    switch (s) {
        case CoTBackendState::Healthy:
            return true;
        case CoTBackendState::Degraded:
            return true;  // Still try, but might fallback on failure
        case CoTBackendState::CircuitOpen:
            // Check if half-open timeout has elapsed
            // (read-only check — actual transition happens in updateCircuitBreaker)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_cbState.halfOpen) {
                    return true; // Allow one probe request
                }
                uint64_t now = currentTimeMs();
                uint64_t elapsed = now - m_cbState.circuitOpenedMs;
                if (elapsed >= static_cast<uint64_t>(m_config.circuitBreaker.halfOpenTimeout)) {
                    return true; // Time to probe
                }
            }
            return false;
        case CoTBackendState::Disabled:
            return false;
        default:
            return false;
    }
}

void CoTFallbackSystem::reportCoTSuccess(double latencyMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_stats.totalRequests.fetch_add(1, std::memory_order_relaxed);
    m_stats.cotRequests.fetch_add(1, std::memory_order_relaxed);

    uint64_t now = currentTimeMs();

    // Record in rolling window
    m_rollingWindow.push_back({true, latencyMs, now});

    // Track latency
    m_recentLatencies.push_back(latencyMs);
    if (m_recentLatencies.size() > kMaxLatencies) {
        m_recentLatencies.pop_front();
    }

    // Update circuit breaker counters
    m_cbState.consecutiveFailures = 0;
    m_cbState.consecutiveSuccesses++;
    m_cbState.lastFailureMs = 0; // Clear last failure tracking on success

    // If in half-open state, check if we can close the circuit
    if (m_cbState.halfOpen) {
        if (m_cbState.consecutiveSuccesses >= m_config.circuitBreaker.successThreshold) {
            m_cbState.halfOpen = false;
            transitionState(CoTBackendState::Healthy, "Circuit breaker closed after recovery");
        }
    } else if (m_state.load(std::memory_order_acquire) == CoTBackendState::Degraded) {
        // Check if we've recovered from degraded state
        if (m_cbState.consecutiveSuccesses >= m_config.circuitBreaker.successThreshold) {
            transitionState(CoTBackendState::Healthy, "Recovered from degraded state");
        }
    }

    // Prune old entries from rolling window
    uint64_t windowStart = now - static_cast<uint64_t>(m_config.circuitBreaker.windowSizeMs);
    while (!m_rollingWindow.empty() && m_rollingWindow.front().timestampMs < windowStart) {
        m_rollingWindow.pop_front();
    }
}

void CoTFallbackSystem::reportCoTFailure(const std::string& reason, double latencyMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_stats.totalRequests.fetch_add(1, std::memory_order_relaxed);

    uint64_t now = currentTimeMs();

    m_rollingWindow.push_back({false, latencyMs, now});

    m_cbState.consecutiveFailures++;
    m_cbState.consecutiveSuccesses = 0;
    m_cbState.lastFailureMs = now;

    // Prune old entries
    uint64_t windowStart = now - static_cast<uint64_t>(m_config.circuitBreaker.windowSizeMs);
    while (!m_rollingWindow.empty() && m_rollingWindow.front().timestampMs < windowStart) {
        m_rollingWindow.pop_front();
    }

    // Check if we should trip the circuit breaker
    if (checkCircuitBreakerTrip()) {
        if (m_cbState.halfOpen) {
            // Half-open probe failed — reopen circuit
            m_cbState.halfOpen = false;
            m_cbState.circuitOpenedMs = now;
            m_stats.circuitBreakerTrips.fetch_add(1, std::memory_order_relaxed);
            transitionState(CoTBackendState::CircuitOpen,
                "Half-open probe failed: " + reason);
        } else {
            CoTBackendState cur = m_state.load(std::memory_order_acquire);
            if (cur == CoTBackendState::Healthy) {
                // First degradation
                transitionState(CoTBackendState::Degraded,
                    "Elevated failures: " + reason);
            } else if (cur == CoTBackendState::Degraded) {
                // Trip to open
                m_cbState.circuitOpenedMs = now;
                m_stats.circuitBreakerTrips.fetch_add(1, std::memory_order_relaxed);
                transitionState(CoTBackendState::CircuitOpen,
                    "Circuit breaker tripped: " + reason);
            }
        }
    }

    // Record fallback event
    CoTFallbackEvent evt{};
    evt.eventId = nextEventId();
    evt.timestampMs = now;
    evt.triggerState = m_state.load(std::memory_order_acquire);
    evt.modeUsed = m_config.fallbackMode;
    evt.reason = reason;
    evt.cotLatencyMs = latencyMs;
    evt.fallbackLatencyMs = 0; // Not yet executed
    evt.estimatedQualityLoss = 0.15f; // Conservative estimate
    evt.userNotified = false;
    recordFallbackEvent(evt);
}

void CoTFallbackSystem::reportCoTTimeout(double latencyMs) {
    reportCoTFailure("Timeout after " + std::to_string(static_cast<int>(latencyMs)) + "ms", latencyMs);
}

// ============================================================================
// Fallback Execution
// ============================================================================
std::string CoTFallbackSystem::executeFallback(
    const std::string& input,
    std::function<std::string(const std::string&)> directInferenceFn)
{
    CoTFallbackMode mode;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        mode = m_config.fallbackMode;
    }

    m_stats.fallbackRequests.fetch_add(1, std::memory_order_relaxed);

    uint64_t startMs = currentTimeMs();
    std::string result;

    switch (mode) {
        case CoTFallbackMode::DirectInference: {
            if (directInferenceFn) {
                result = directInferenceFn(input);
            } else {
                result = "[Fallback] Direct inference unavailable";
            }
            break;
        }
        case CoTFallbackMode::CachedResponse: {
            // In a full implementation, this would check a response cache
            // For now, report the cache miss and fall through to direct
            if (directInferenceFn) {
                result = directInferenceFn(input);
            } else {
                result = "[Fallback] No cached response available";
            }
            break;
        }
        case CoTFallbackMode::SimplifiedChain: {
            // Simplified chain: wrap input in a minimal prompt and run direct
            std::string simplified =
                "Answer concisely. Do not explain your reasoning steps.\n\n" + input;
            if (directInferenceFn) {
                result = directInferenceFn(simplified);
            } else {
                result = "[Fallback] Simplified chain unavailable";
            }
            break;
        }
        case CoTFallbackMode::ErrorResponse: {
            result = "[Error] Chain-of-Thought reasoning is temporarily unavailable. "
                     "The system is operating in fallback mode.";
            break;
        }
        default: {
            result = "[Error] Unknown fallback mode";
            break;
        }
    }

    uint64_t endMs = currentTimeMs();
    double fallbackLatency = static_cast<double>(endMs - startMs);

    // Record telemetry
    if (m_config.logFallbackEvents) {
        CoTFallbackEvent evt{};
        evt.eventId = nextEventId();
        evt.timestampMs = startMs;
        evt.triggerState = m_state.load(std::memory_order_acquire);
        evt.modeUsed = mode;
        evt.reason = "Fallback execution";
        evt.cotLatencyMs = -1;
        evt.fallbackLatencyMs = fallbackLatency;
        evt.estimatedQualityLoss = (mode == CoTFallbackMode::DirectInference) ? 0.2f :
                                   (mode == CoTFallbackMode::SimplifiedChain) ? 0.1f : 0.3f;
        evt.userNotified = (mode == CoTFallbackMode::ErrorResponse);

        std::lock_guard<std::mutex> lock(m_mutex);
        recordFallbackEvent(evt);
    }

    return result;
}

// ============================================================================
// Health Monitoring
// ============================================================================
CoTHealthMetrics CoTFallbackSystem::getHealthMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    CoTHealthMetrics metrics{};
    metrics.currentState = m_state.load(std::memory_order_acquire);
    metrics.lastFailureTimestamp = m_cbState.lastFailureMs;
    metrics.consecutiveFailures = m_cbState.consecutiveFailures;
    metrics.consecutiveSuccesses = m_cbState.consecutiveSuccesses;

    // Calculate from rolling window
    metrics.totalRequests = static_cast<int>(m_rollingWindow.size());
    metrics.successCount = 0;
    metrics.failureCount = 0;
    metrics.timeoutCount = 0;

    for (const auto& outcome : m_rollingWindow) {
        if (outcome.success) {
            metrics.successCount++;
            metrics.lastSuccessTimestamp = outcome.timestampMs;
        } else {
            metrics.failureCount++;
        }
    }

    metrics.successRate = metrics.totalRequests > 0
        ? static_cast<float>(metrics.successCount) / static_cast<float>(metrics.totalRequests)
        : 1.0f;

    // Calculate latency stats
    if (!m_recentLatencies.empty()) {
        double sum = 0;
        for (double lat : m_recentLatencies) {
            sum += lat;
        }
        metrics.avgLatencyMs = sum / static_cast<double>(m_recentLatencies.size());

        // P99: sort a copy and take the 99th percentile
        std::vector<double> sorted(m_recentLatencies.begin(), m_recentLatencies.end());
        std::sort(sorted.begin(), sorted.end());
        size_t p99Index = static_cast<size_t>(static_cast<double>(sorted.size()) * 0.99);
        if (p99Index >= sorted.size()) p99Index = sorted.size() - 1;
        metrics.p99LatencyMs = sorted[p99Index];
    }

    return metrics;
}

std::string CoTFallbackSystem::getHealthJSON() const {
    CoTHealthMetrics m = getHealthMetrics();

    std::string json = "{\n";
    json += "  \"state\": \"" + std::string(stateToString(m.currentState)) + "\",\n";
    json += "  \"totalRequests\": " + std::to_string(m.totalRequests) + ",\n";
    json += "  \"successCount\": " + std::to_string(m.successCount) + ",\n";
    json += "  \"failureCount\": " + std::to_string(m.failureCount) + ",\n";
    json += "  \"successRate\": " + std::to_string(m.successRate) + ",\n";
    json += "  \"avgLatencyMs\": " + std::to_string(m.avgLatencyMs) + ",\n";
    json += "  \"p99LatencyMs\": " + std::to_string(m.p99LatencyMs) + ",\n";
    json += "  \"consecutiveFailures\": " + std::to_string(m.consecutiveFailures) + ",\n";
    json += "  \"consecutiveSuccesses\": " + std::to_string(m.consecutiveSuccesses) + ",\n";
    json += "  \"circuitBreakerTrips\": " +
        std::to_string(m_stats.circuitBreakerTrips.load(std::memory_order_relaxed)) + ",\n";
    json += "  \"totalFallbacks\": " +
        std::to_string(m_stats.fallbackRequests.load(std::memory_order_relaxed)) + "\n";
    json += "}";

    return json;
}

// ============================================================================
// Telemetry
// ============================================================================
std::vector<CoTFallbackEvent> CoTFallbackSystem::getRecentEvents(int count) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<CoTFallbackEvent> result;
    int n = std::min(count, static_cast<int>(m_events.size()));
    // Return most recent events
    auto it = m_events.end() - n;
    for (; it != m_events.end(); ++it) {
        result.push_back(*it);
    }
    return result;
}

int CoTFallbackSystem::getTotalFallbackCount() const {
    return static_cast<int>(
        m_stats.fallbackRequests.load(std::memory_order_relaxed));
}

// ============================================================================
// Statistics
// ============================================================================
void CoTFallbackSystem::resetStats() {
    m_stats.totalRequests.store(0, std::memory_order_relaxed);
    m_stats.cotRequests.store(0, std::memory_order_relaxed);
    m_stats.fallbackRequests.store(0, std::memory_order_relaxed);
    m_stats.circuitBreakerTrips.store(0, std::memory_order_relaxed);
    m_stats.manualDisables.store(0, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_rollingWindow.clear();
    m_recentLatencies.clear();
    m_events.clear();
}

// ============================================================================
// Circuit Breaker Logic
// ============================================================================
bool CoTFallbackSystem::checkCircuitBreakerTrip() {
    // Lock must be held by caller

    // Threshold-based check: too many consecutive failures
    if (m_cbState.consecutiveFailures >= m_config.circuitBreaker.failureThreshold) {
        return true;
    }

    // Rate-based check: failure rate too high within window
    int windowTotal = static_cast<int>(m_rollingWindow.size());
    if (windowTotal >= m_config.circuitBreaker.minRequestsInWindow) {
        int windowFailures = 0;
        for (const auto& o : m_rollingWindow) {
            if (!o.success) windowFailures++;
        }
        float rate = static_cast<float>(windowFailures) / static_cast<float>(windowTotal);
        if (rate >= m_config.circuitBreaker.failureRateThreshold) {
            return true;
        }
    }

    return false;
}

void CoTFallbackSystem::tryHalfOpen() {
    // Lock must be held by caller
    CoTBackendState cur = m_state.load(std::memory_order_acquire);
    if (cur != CoTBackendState::CircuitOpen) return;

    uint64_t now = currentTimeMs();
    uint64_t elapsed = now - m_cbState.circuitOpenedMs;
    if (elapsed >= static_cast<uint64_t>(m_config.circuitBreaker.halfOpenTimeout)) {
        m_cbState.halfOpen = true;
        m_cbState.consecutiveSuccesses = 0;
        // Don't transition state yet — wait for probe result
    }
}

void CoTFallbackSystem::transitionState(CoTBackendState newState, const std::string& reason) {
    // Lock must be held by caller

    CoTBackendState oldState = m_state.load(std::memory_order_acquire);
    if (oldState == newState) return;

    m_state.store(newState, std::memory_order_release);

    if (m_stateChangeCb) {
        m_stateChangeCb(oldState, newState, reason);
    }
}

void CoTFallbackSystem::updateCircuitBreaker() {
    // Lock must be held by caller
    // Called periodically or before routing decisions

    CoTBackendState cur = m_state.load(std::memory_order_acquire);
    if (cur == CoTBackendState::CircuitOpen && !m_cbState.halfOpen) {
        tryHalfOpen();
    }
}

// ============================================================================
// Event Recording
// ============================================================================
void CoTFallbackSystem::recordFallbackEvent(const CoTFallbackEvent& event) {
    // Lock must be held by caller
    m_events.push_back(event);
    if (m_events.size() > kMaxEvents) {
        m_events.pop_front();
    }

    if (m_fallbackCb) {
        m_fallbackCb(event);
    }
}

uint64_t CoTFallbackSystem::nextEventId() {
    return m_eventCounter.fetch_add(1, std::memory_order_relaxed);
}
