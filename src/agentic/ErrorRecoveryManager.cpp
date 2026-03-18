#include "ErrorRecoveryManager.h"
#include <thread>
#include <iostream>
#include <mutex>

namespace RawrXD::Agentic {

ErrorRecoveryManager& ErrorRecoveryManager::instance() {
    static ErrorRecoveryManager instance;
    return instance;
}

void ErrorRecoveryManager::recordFailure(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = circuitStates_[operation];
    state.failureCount++;
    state.lastFailure = std::chrono::steady_clock::now();

    if (state.failureCount >= 5) { // Default threshold
        state.open = true;
        std::cerr << "[ErrorRecovery] Circuit breaker opened for: " << operation << std::endl;
    }
}

void ErrorRecoveryManager::recordSuccess(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = circuitStates_[operation];
    state.failureCount = 0;
    state.open = false;
}

bool ErrorRecoveryManager::isCircuitOpen(const std::string& operation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = circuitStates_.find(operation);
    if (it == circuitStates_.end()) return false;

    const auto& state = it->second;
    if (!state.open) return false;

    // Check if circuit should reset
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state.lastFailure).count();

    if (elapsed > 60000) { // Default timeout
        const_cast<CircuitState&>(state).open = false;
        const_cast<CircuitState&>(state).failureCount = 0;
        return false;
    }

    return true;
}

} // namespace RawrXD::Agentic
