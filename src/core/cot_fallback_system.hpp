// ============================================================================
// cot_fallback_system.hpp — Chain-of-Thought Fallback & Disable System
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Provides seamless CoT backend disable/fallback capability:
//   1. CoT can be disabled at runtime without restart
//   2. If CoT backend fails, transparent fallback to direct inference
//   3. Graceful degradation with quality-loss telemetry
//   4. A/B testing support between CoT and direct modes
//   5. Circuit-breaker pattern for CoT backend health
//
// This makes the reasoning system safe to deploy:
//   - CoT failures don't crash the system
//   - Users get answers even when reasoning is broken
//   - Operators can disable CoT instantly in emergencies
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <deque>
#include "../core/model_memory_hotpatch.hpp"

// ============================================================================
// CoTBackendState — Health state of the CoT subsystem
// ============================================================================
enum class CoTBackendState : uint8_t {
    Healthy     = 0,    // Fully operational
    Degraded    = 1,    // Working but with elevated errors/latency
    CircuitOpen = 2,    // Circuit breaker tripped — using fallback
    Disabled    = 3,    // Manually disabled by operator
    Count       = 4
};

// ============================================================================
// CoTFallbackMode — What happens when CoT is unavailable
// ============================================================================
enum class CoTFallbackMode : uint8_t {
    DirectInference     = 0,    // Skip CoT, go straight to model
    CachedResponse      = 1,    // Use cached response if available
    SimplifiedChain     = 2,    // Use a minimal 1-step chain
    ErrorResponse       = 3,    // Return error to client
    Count               = 4
};

// ============================================================================
// CircuitBreakerConfig — Controls the circuit breaker behavior
// ============================================================================
struct CircuitBreakerConfig {
    int         failureThreshold;       // Failures before opening circuit
    int         successThreshold;       // Successes to close circuit (half-open)
    int         halfOpenTimeout;        // ms before trying to close (half-open state)
    int         windowSizeMs;           // Rolling window for failure counting
    float       failureRateThreshold;   // % failure rate to trip (0.0-1.0)
    int         minRequestsInWindow;    // Min requests before rate applies

    CircuitBreakerConfig()
        : failureThreshold(5),
          successThreshold(3),
          halfOpenTimeout(30000),
          windowSizeMs(60000),
          failureRateThreshold(0.5f),
          minRequestsInWindow(10) {}
};

// ============================================================================
// CoTFallbackConfig — Overall fallback configuration
// ============================================================================
struct CoTFallbackConfig {
    bool                    enabled;            // Master switch for fallback system
    CoTFallbackMode         fallbackMode;       // What to do when CoT fails
    CircuitBreakerConfig    circuitBreaker;     // Circuit breaker settings
    int                     maxFallbackLatencyMs; // Max time for fallback path
    bool                    logFallbackEvents;  // Log every fallback to telemetry
    bool                    trackQualityDelta;  // Track quality difference vs CoT
    float                   qualityAlertThreshold; // Alert if quality drops below this

    CoTFallbackConfig()
        : enabled(true),
          fallbackMode(CoTFallbackMode::DirectInference),
          maxFallbackLatencyMs(5000),
          logFallbackEvents(true),
          trackQualityDelta(true),
          qualityAlertThreshold(0.3f) {}
};

// ============================================================================
// CoTFallbackEvent — Telemetry for a fallback occurrence
// ============================================================================
struct CoTFallbackEvent {
    uint64_t        eventId;
    uint64_t        timestampMs;
    CoTBackendState triggerState;        // What state triggered fallback
    CoTFallbackMode modeUsed;            // Which fallback mode was used
    std::string     reason;              // Why fallback occurred
    double          cotLatencyMs;        // How long CoT took before failing (-1 if N/A)
    double          fallbackLatencyMs;   // How long the fallback took
    float           estimatedQualityLoss;// Estimated quality loss (0.0-1.0)
    bool            userNotified;        // Whether the user was told about degradation
};

// ============================================================================
// CoTHealthMetrics — Rolling health metrics for the CoT backend
// ============================================================================
struct CoTHealthMetrics {
    int             totalRequests;
    int             successCount;
    int             failureCount;
    int             timeoutCount;
    float           successRate;     // 0.0-1.0
    double          avgLatencyMs;
    double          p99LatencyMs;
    CoTBackendState currentState;
    uint64_t        lastFailureTimestamp;
    uint64_t        lastSuccessTimestamp;
    int             consecutiveFailures;
    int             consecutiveSuccesses;
};

// ============================================================================
// Callbacks
// ============================================================================
using CoTStateChangeCb  = std::function<void(CoTBackendState oldState,
                                              CoTBackendState newState,
                                              const std::string& reason)>;
using CoTFallbackCb     = std::function<void(const CoTFallbackEvent& event)>;

// ============================================================================
// CoTFallbackSystem — Singleton
// ============================================================================
class CoTFallbackSystem {
public:
    static CoTFallbackSystem& instance();

    // ---- Configuration ----
    void setConfig(const CoTFallbackConfig& config);
    CoTFallbackConfig getConfig() const;
    void setCircuitBreakerConfig(const CircuitBreakerConfig& config);

    // ---- Manual Controls ----

    /// Disable CoT immediately (emergency kill switch)
    PatchResult disableCoT(const std::string& reason);

    /// Re-enable CoT
    PatchResult enableCoT();

    /// Check if CoT is currently available
    bool isCoTAvailable() const;

    /// Get current backend state
    CoTBackendState getState() const;

    // ---- Request Routing ----

    /// Determine whether a request should use CoT or fallback
    /// Returns true if CoT should be used, false for fallback
    bool shouldUseCoT() const;

    /// Report success of a CoT request (for circuit breaker)
    void reportCoTSuccess(double latencyMs);

    /// Report failure of a CoT request (for circuit breaker)
    void reportCoTFailure(const std::string& reason, double latencyMs = -1);

    /// Report timeout of a CoT request
    void reportCoTTimeout(double latencyMs);

    // ---- Fallback Execution ----

    /// Execute a request through the fallback path
    /// The `directInferenceFn` is called if mode is DirectInference
    /// Returns the response string
    std::string executeFallback(
        const std::string& input,
        std::function<std::string(const std::string&)> directInferenceFn);

    // ---- Health Monitoring ----
    CoTHealthMetrics getHealthMetrics() const;
    std::string getHealthJSON() const;

    // ---- Telemetry ----
    std::vector<CoTFallbackEvent> getRecentEvents(int count = 50) const;
    int getTotalFallbackCount() const;

    // ---- Callbacks ----
    void setStateChangeCallback(CoTStateChangeCb cb) { m_stateChangeCb = cb; }
    void setFallbackCallback(CoTFallbackCb cb) { m_fallbackCb = cb; }

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalRequests{0};
        std::atomic<uint64_t> cotRequests{0};
        std::atomic<uint64_t> fallbackRequests{0};
        std::atomic<uint64_t> circuitBreakerTrips{0};
        std::atomic<uint64_t> manualDisables{0};
    };
    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    CoTFallbackSystem();
    ~CoTFallbackSystem() = default;
    CoTFallbackSystem(const CoTFallbackSystem&) = delete;
    CoTFallbackSystem& operator=(const CoTFallbackSystem&) = delete;

    // ---- Circuit Breaker Logic ----
    void updateCircuitBreaker();
    bool checkCircuitBreakerTrip();
    void tryHalfOpen();
    void transitionState(CoTBackendState newState, const std::string& reason);

    // ---- Event recording ----
    void recordFallbackEvent(const CoTFallbackEvent& event);
    uint64_t nextEventId();

    mutable std::mutex m_mutex;
    CoTFallbackConfig m_config;
    std::atomic<CoTBackendState> m_state{CoTBackendState::Healthy};

    // Circuit breaker state
    struct CircuitBreakerState {
        int     consecutiveFailures = 0;
        int     consecutiveSuccesses = 0;
        int     windowFailures = 0;
        int     windowSuccesses = 0;
        uint64_t lastFailureMs = 0;
        uint64_t circuitOpenedMs = 0;
        bool    halfOpen = false;
    } m_cbState;

    // Rolling window for failure rate calculation
    struct RequestOutcome {
        bool        success;
        double      latencyMs;
        uint64_t    timestampMs;
    };
    std::deque<RequestOutcome> m_rollingWindow;

    // Latency tracking
    std::deque<double> m_recentLatencies;
    static constexpr size_t kMaxLatencies = 1000;

    // Event log
    std::deque<CoTFallbackEvent> m_events;
    static constexpr size_t kMaxEvents = 500;
    std::atomic<uint64_t> m_eventCounter{0};

    Stats m_stats;

    // Callbacks
    CoTStateChangeCb m_stateChangeCb;
    CoTFallbackCb    m_fallbackCb;
};
