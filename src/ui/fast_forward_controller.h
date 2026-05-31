// ============================================================================
// fast_forward_controller.h — Fast Forward with TLS (Time Limit Stop)
// ============================================================================
// Rejects current TPS and forces completion within a hard deadline.
// Monitors tokens-per-second, auto-initiates FF on slow generation,
// accelerates by skipping tokens, and enforces a TLS countdown.
//
// Pattern: Event-driven state machine with timer-based enforcement.
// ============================================================================

#pragma once

#include "async_message_queue.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace RawrXD {
namespace UI {

// ============================================================================
// Fast Forward Configuration
// ============================================================================

struct FastForwardConfig {
    uint32_t tlsTimeoutMs = 30000;          // Hard deadline (30s)
    uint32_t minProgressThreshold = 10;     // Min % before FF allowed
    uint32_t accelerationFactor = 3;          // Keep 1 of every N tokens
    std::string fallbackMessage =
        "\u26A0 Generation exceeded time limit. Forced completion.";
    bool enableAutoFF = true;               // Auto-FF on slow TPS
    float slowTPSThreshold = 5.0f;          // TPS below this = slow
    uint32_t slowTPSDurationMs = 5000;      // Slow for this long → auto-FF
    uint32_t warningAtRemainingMs = 10000;  // Warn at 10s remaining
};

// ============================================================================
// Fast Forward State
// ============================================================================

struct FastForwardState {
    std::string originalMessageId;
    std::string ffMessageId;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point deadline;
    float currentTPS = 0.0f;
    uint32_t tokensGenerated = 0;
    bool forcedCompletion = false;

    enum class AbortReason : uint8_t {
        NONE = 0,
        USER_CANCEL,
        SLOW_TPS,
        TLS_EXCEEDED,
        ERROR
    } abortReason = AbortReason::NONE;

    float remainingMs() const {
        auto now = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now).count();
        return static_cast<float>(std::max(0LL, remaining));
    }

    bool isExpired() const {
        return std::chrono::steady_clock::now() >= deadline;
    }
};

// ============================================================================
// Fast Forward Controller
// ============================================================================

class FastForwardController {
public:
    using FFStartedCallback     = std::function<void(const FastForwardState&)>;
    using FFProgressCallback    = std::function<void(const FastForwardState&)>;
    using FFAcceleratingCallback = std::function<void(const FastForwardState&, uint32_t skipped)>;
    using FFCompletedCallback   = std::function<void(const FastForwardState&)>;
    using FFTimeoutCallback     = std::function<void(const FastForwardState&)>;
    using FFAbortedCallback     = std::function<void(const FastForwardState&, const std::string&)>;
    using TLSWarningCallback    = std::function<void(const FastForwardState&, float remainingMs)>;
    using TLSExceededCallback   = std::function<void(const FastForwardState&)>;

    explicit FastForwardController(const FastForwardConfig& config = {});
    ~FastForwardController();

    // ---- Initiate / Cancel / Complete ----

    FastForwardState initiateFF(
        const QueuedMessage& message,
        const std::string& reason = "manual"
    );

    bool cancelFF(const std::string& messageId, const std::string& reason = "user_cancel");
    bool completeFF(const std::string& messageId);

    // ---- Acceleration ----

    // Returns true if the token should be KEPT (not skipped)
    bool shouldKeepToken(const std::string& messageId, uint32_t tokenIndex);

    // ---- Progress Updates ----

    void updateProgress(const std::string& messageId, uint32_t newTokens);

    // ---- Queries ----

    FastForwardState* getFFState(const std::string& messageId);
    bool isFFActive(const std::string& messageId) const;
    std::vector<FastForwardState> getActiveFFs() const;

    // ---- Event Subscriptions ----

    void onFFStarted(FFStartedCallback cb);
    void onFFProgress(FFProgressCallback cb);
    void onFFAccelerating(FFAcceleratingCallback cb);
    void onFFCompleted(FFCompletedCallback cb);
    void onFFTimeout(FFTimeoutCallback cb);
    void onFFAborted(FFAbortedCallback cb);
    void onTLSWarning(TLSWarningCallback cb);
    void onTLSExceeded(TLSExceededCallback cb);

    // ---- Control ----

    void shutdown();

private:
    FastForwardConfig config_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, FastForwardState> activeFFs_;

    // Timers
    std::unordered_map<std::string, std::thread> tlsTimers_;
    std::unordered_map<std::string, std::thread> tpsMonitors_;
    std::atomic<bool> shutdown_{false};

    // Callbacks
    mutable std::mutex callbackMutex_;
    std::vector<FFStartedCallback>     onStartedCbs_;
    std::vector<FFProgressCallback>    onProgressCbs_;
    std::vector<FFAcceleratingCallback> onAcceleratingCbs_;
    std::vector<FFCompletedCallback>   onCompletedCbs_;
    std::vector<FFTimeoutCallback>     onTimeoutCbs_;
    std::vector<FFAbortedCallback>     onAbortedCbs_;
    std::vector<TLSWarningCallback>    onTLSWarningCbs_;
    std::vector<TLSExceededCallback>   onTLSExceededCbs_;

    // Internal
    void startTLSTimer(const std::string& messageId);
    void startTPSMonitor(const std::string& messageId);
    void handleTLSExceeded(const std::string& messageId);
    void forceComplete(const std::string& messageId);
    void cleanupFF(const std::string& messageId);

    // Event dispatch
    void dispatchStarted(const FastForwardState& state);
    void dispatchProgress(const FastForwardState& state);
    void dispatchAccelerating(const FastForwardState& state, uint32_t skipped);
    void dispatchCompleted(const FastForwardState& state);
    void dispatchTimeout(const FastForwardState& state);
    void dispatchAborted(const FastForwardState& state, const std::string& reason);
    void dispatchTLSWarning(const FastForwardState& state, float remainingMs);
    void dispatchTLSExceeded(const FastForwardState& state);
};

} // namespace UI
} // namespace RawrXD
