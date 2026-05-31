// ============================================================================
// fast_forward_controller.cpp — Fast Forward with TLS Implementation
// ============================================================================

#include "fast_forward_controller.h"
#include <algorithm>
#include <iostream>

namespace RawrXD {
namespace UI {

// ============================================================================
// Construction / Destruction
// ============================================================================

FastForwardController::FastForwardController(const FastForwardConfig& config)
    : config_(config)
{
}

FastForwardController::~FastForwardController() {
    shutdown();
}

// ============================================================================
// Initiate Fast Forward
// ============================================================================

FastForwardState FastForwardController::initiateFF(
    const QueuedMessage& message,
    const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string& messageId = message.id;

    // Check if already in FF
    if (activeFFs_.find(messageId) != activeFFs_.end()) {
        return activeFFs_[messageId];
    }

    // Check progress threshold
    if (message.progress < config_.minProgressThreshold) {
        throw std::runtime_error(
            "Cannot fast forward: progress below " +
            std::to_string(config_.minProgressThreshold) + "%"
        );
    }

    // Create FF state
    auto now = std::chrono::steady_clock::now();
    FastForwardState state;
    state.originalMessageId = messageId;
    state.ffMessageId = "ff_" + messageId;
    state.startTime = now;
    state.deadline = now + std::chrono::milliseconds(config_.tlsTimeoutMs);
    state.tokensGenerated = message.completionTokens;

    activeFFs_[messageId] = state;

    // Start TLS timer
    startTLSTimer(messageId);

    // Start TPS monitor
    startTPSMonitor(messageId);

    dispatchStarted(state);

    return state;
}

// ============================================================================
// Cancel / Complete
// ============================================================================

bool FastForwardController::cancelFF(const std::string& messageId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return false;

    it->second.abortReason = FastForwardState::AbortReason::USER_CANCEL;

    dispatchAborted(it->second, reason);
    cleanupFF(messageId);

    return true;
}

bool FastForwardController::completeFF(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return false;

    dispatchCompleted(it->second);
    cleanupFF(messageId);

    return true;
}

// ============================================================================
// Acceleration
// ============================================================================

bool FastForwardController::shouldKeepToken(const std::string& messageId, uint32_t tokenIndex) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return true; // No FF active, keep all

    // Keep 1 of every accelerationFactor tokens
    bool keep = (tokenIndex % config_.accelerationFactor) == 0;

    if (!keep) {
        // Count skipped tokens for this message
        static std::unordered_map<std::string, uint32_t> skippedCounts;
        uint32_t skipped = ++skippedCounts[messageId];

        if (skipped % 10 == 0) {
            dispatchAccelerating(it->second, skipped);
        }
    }

    return keep;
}

// ============================================================================
// Progress Updates
// ============================================================================

void FastForwardController::updateProgress(const std::string& messageId, uint32_t newTokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return;

    auto& state = it->second;
    state.tokensGenerated += newTokens;

    // Calculate TPS
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - state.startTime).count();

    if (elapsed > 0) {
        state.currentTPS = static_cast<float>(state.tokensGenerated) /
                          (static_cast<float>(elapsed) / 1000.0f);
    }

    dispatchProgress(state);
}

// ============================================================================
// Queries
// ============================================================================

FastForwardState* FastForwardController::getFFState(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it != activeFFs_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool FastForwardController::isFFActive(const std::string& messageId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeFFs_.find(messageId) != activeFFs_.end();
}

std::vector<FastForwardState> FastForwardController::getActiveFFs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<FastForwardState> result;
    result.reserve(activeFFs_.size());

    for (const auto& [_, state] : activeFFs_) {
        result.push_back(state);
    }

    return result;
}

// ============================================================================
// Event Subscriptions
// ============================================================================

void FastForwardController::onFFStarted(FFStartedCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onStartedCbs_.push_back(std::move(cb));
}

void FastForwardController::onFFProgress(FFProgressCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onProgressCbs_.push_back(std::move(cb));
}

void FastForwardController::onFFAccelerating(FFAcceleratingCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onAcceleratingCbs_.push_back(std::move(cb));
}

void FastForwardController::onFFCompleted(FFCompletedCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onCompletedCbs_.push_back(std::move(cb));
}

void FastForwardController::onFFTimeout(FFTimeoutCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onTimeoutCbs_.push_back(std::move(cb));
}

void FastForwardController::onFFAborted(FFAbortedCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onAbortedCbs_.push_back(std::move(cb));
}

void FastForwardController::onTLSWarning(TLSWarningCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onTLSWarningCbs_.push_back(std::move(cb));
}

void FastForwardController::onTLSExceeded(TLSExceededCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onTLSExceededCbs_.push_back(std::move(cb));
}

// ============================================================================
// Shutdown
// ============================================================================

void FastForwardController::shutdown() {
    shutdown_.store(true);

    // Cancel all active FFs
    std::vector<std::string> toCancel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : activeFFs_) {
            toCancel.push_back(id);
        }
    }

    for (const auto& id : toCancel) {
        cancelFF(id, "shutdown");
    }

    // Join all timer threads
    for (auto& [_, thread] : tlsTimers_) {
        if (thread.joinable()) thread.join();
    }
    tlsTimers_.clear();

    for (auto& [_, thread] : tpsMonitors_) {
        if (thread.joinable()) thread.join();
    }
    tpsMonitors_.clear();
}

// ============================================================================
// TLS Timer
// ============================================================================

void FastForwardController::startTLSTimer(const std::string& messageId) {
    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return;

    auto& state = it->second;
    auto now = std::chrono::steady_clock::now();

    // Warning timer
    auto warningDelay = state.deadline -
        std::chrono::milliseconds(config_.warningAtRemainingMs) - now;

    if (warningDelay > std::chrono::milliseconds(0)) {
        std::thread warningThread([this, messageId, warningDelay]() {
            std::this_thread::sleep_for(warningDelay);

            if (shutdown_.load()) return;

            std::lock_guard<std::mutex> lock(mutex_);
            auto it = activeFFs_.find(messageId);
            if (it != activeFFs_.end()) {
                float remaining = it->second.remainingMs();
                dispatchTLSWarning(it->second, remaining);
            }
        });
        tlsTimers_[messageId + "_warning"] = std::move(warningThread);
    }

    // Deadline timer
    auto deadlineDelay = state.deadline - now;
    std::thread deadlineThread([this, messageId, deadlineDelay]() {
        std::this_thread::sleep_for(deadlineDelay);

        if (shutdown_.load()) return;

        handleTLSExceeded(messageId);
    });
    tlsTimers_[messageId] = std::move(deadlineThread);
}

// ============================================================================
// TPS Monitor
// ============================================================================

void FastForwardController::startTPSMonitor(const std::string& messageId) {
    std::thread monitorThread([this, messageId]() {
        uint32_t lastTokenCount = 0;
        auto lastCheckTime = std::chrono::steady_clock::now();
        auto slowStartTime = std::chrono::steady_clock::time_point{};
        bool isSlow = false;

        while (!shutdown_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::lock_guard<std::mutex> lock(mutex_);

            auto it = activeFFs_.find(messageId);
            if (it == activeFFs_.end()) break;

            auto& state = it->second;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastCheckTime).count();

            if (elapsed <= 0) continue;

            uint32_t tokensDelta = state.tokensGenerated - lastTokenCount;
            float tps = static_cast<float>(tokensDelta) /
                       (static_cast<float>(elapsed) / 1000.0f);

            state.currentTPS = tps;

            // Check slow TPS
            if (config_.enableAutoFF && tps < config_.slowTPSThreshold) {
                if (!isSlow) {
                    slowStartTime = now;
                    isSlow = true;
                } else {
                    auto slowDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - slowStartTime).count();

                    if (slowDuration >= static_cast<long long>(config_.slowTPSDurationMs)) {
                        // Auto-initiate FF
                        state.abortReason = FastForwardState::AbortReason::SLOW_TPS;
                        dispatchAborted(state, "Slow TPS threshold exceeded");
                        cleanupFF(messageId);
                        break;
                    }
                }
            } else {
                isSlow = false;
            }

            lastTokenCount = state.tokensGenerated;
            lastCheckTime = now;
        }
    });

    tpsMonitors_[messageId] = std::move(monitorThread);
}

// ============================================================================
// TLS Exceeded Handler
// ============================================================================

void FastForwardController::handleTLSExceeded(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return;

    auto& state = it->second;
    state.abortReason = FastForwardState::AbortReason::TLS_EXCEEDED;
    state.forcedCompletion = true;

    dispatchTLSExceeded(state);
    dispatchTimeout(state);

    forceComplete(messageId);
}

// ============================================================================
// Force Complete
// ============================================================================

void FastForwardController::forceComplete(const std::string& messageId) {
    auto it = activeFFs_.find(messageId);
    if (it == activeFFs_.end()) return;

    dispatchCompleted(it->second);
    cleanupFF(messageId);
}

// ============================================================================
// Cleanup
// ============================================================================

void FastForwardController::cleanupFF(const std::string& messageId) {
    // Stop timers
    auto tlsIt = tlsTimers_.find(messageId);
    if (tlsIt != tlsTimers_.end()) {
        if (tlsIt->second.joinable()) {
            tlsIt->second.detach(); // Let it finish naturally
        }
        tlsTimers_.erase(tlsIt);
    }

    auto warningIt = tlsTimers_.find(messageId + "_warning");
    if (warningIt != tlsTimers_.end()) {
        if (warningIt->second.joinable()) {
            warningIt->second.detach();
        }
        tlsTimers_.erase(warningIt);
    }

    auto monitorIt = tpsMonitors_.find(messageId);
    if (monitorIt != tpsMonitors_.end()) {
        if (monitorIt->second.joinable()) {
            monitorIt->second.detach();
        }
        tpsMonitors_.erase(monitorIt);
    }

    // Remove from active
    activeFFs_.erase(messageId);
}

// ============================================================================
// Event Dispatch
// ============================================================================

void FastForwardController::dispatchStarted(const FastForwardState& state) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onStartedCbs_) cb(state);
}

void FastForwardController::dispatchProgress(const FastForwardState& state) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onProgressCbs_) cb(state);
}

void FastForwardController::dispatchAccelerating(const FastForwardState& state, uint32_t skipped) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onAcceleratingCbs_) cb(state, skipped);
}

void FastForwardController::dispatchCompleted(const FastForwardState& state) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onCompletedCbs_) cb(state);
}

void FastForwardController::dispatchTimeout(const FastForwardState& state) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onTimeoutCbs_) cb(state);
}

void FastForwardController::dispatchAborted(const FastForwardState& state, const std::string& reason) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onAbortedCbs_) cb(state, reason);
}

void FastForwardController::dispatchTLSWarning(const FastForwardState& state, float remainingMs) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onTLSWarningCbs_) cb(state, remainingMs);
}

void FastForwardController::dispatchTLSExceeded(const FastForwardState& state) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : onTLSExceededCbs_) cb(state);
}

} // namespace UI
} // namespace RawrXD
