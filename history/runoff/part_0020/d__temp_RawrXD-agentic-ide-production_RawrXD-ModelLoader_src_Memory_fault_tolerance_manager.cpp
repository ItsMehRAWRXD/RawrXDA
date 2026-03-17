#include "fault_tolerance_manager.hpp"
#include <QDebug>
#include <thread>
#include <random>

FaultToleranceManager::FaultToleranceManager(QObject* parent) : QObject(parent) {}

FaultToleranceManager::~FaultToleranceManager() {}

bool FaultToleranceManager::executeWithCircuitBreaker(const QString& component_id, 
                                                     std::function<bool()> operation,
                                                     std::function<void()> fallback) {
    QMutexLocker locker(&mutex);
    
    // Get or create circuit state
    if (!circuits.contains(component_id)) {
        circuits[component_id] = CircuitBreakerState();
    }
    
    CircuitBreakerState& state = circuits[component_id];
    
    // Check if request should be allowed
    if (!shouldAllowRequest(state)) {
        locker.unlock();
        if (fallback) {
            fallback();
        } else if (fallbacks.contains(component_id)) {
            fallbacks[component_id]();
        }
        emit fallbackTriggered(component_id);
        return false;
    }
    
    // Execute operation
    locker.unlock();
    bool success = false;
    try {
        success = operation();
    } catch (...) {
        success = false;
    }
    
    if (success) {
        recordSuccess(component_id);
        return true;
    } else {
        recordFailure(component_id, "execution_failed", "Operation returned false or threw exception");
        if (fallback) {
            fallback();
        } else {
            QMutexLocker fallbackLocker(&mutex);
            if (fallbacks.contains(component_id)) {
                fallbacks[component_id]();
            }
        }
        return false;
    }
}

void FaultToleranceManager::recordSuccess(const QString& component_id) {
    QMutexLocker locker(&mutex);
    if (!circuits.contains(component_id)) return;
    
    CircuitBreakerState& state = circuits[component_id];
    state.success_count++;
    state.last_success_time = std::chrono::steady_clock::now();
    
    if (state.state == CircuitState::HALF_OPEN) {
        state.state = CircuitState::CLOSED;
        state.failure_count = 0;
        state.state_change_time = std::chrono::steady_clock::now();
        emit circuitClosed(component_id);
        qInfo() << "Circuit closed for component:" << component_id;
    }
}

void FaultToleranceManager::recordFailure(const QString& component_id, const std::string& error_type, const std::string& error_message) {
    QMutexLocker locker(&mutex);
    if (!circuits.contains(component_id)) {
        circuits[component_id] = CircuitBreakerState();
    }
    
    CircuitBreakerState& state = circuits[component_id];
    state.failure_count++;
    state.last_failure_time = std::chrono::steady_clock::now();
    
    if (state.state == CircuitState::CLOSED) {
        if (state.failure_count >= state.config.failure_threshold) {
            state.state = CircuitState::OPEN;
            state.state_change_time = std::chrono::steady_clock::now();
            emit circuitOpened(component_id, QString::fromStdString(error_message));
            qWarning() << "Circuit opened for component:" << component_id << "Reason:" << QString::fromStdString(error_message);
        }
    } else if (state.state == CircuitState::HALF_OPEN) {
        state.state = CircuitState::OPEN;
        state.state_change_time = std::chrono::steady_clock::now();
        emit circuitOpened(component_id, "Failed during half-open state");
    }
}

bool FaultToleranceManager::shouldAllowRequest(CircuitBreakerState& state) {
    if (!state.config.enabled) return true;
    
    if (state.state == CircuitState::CLOSED) return true;
    
    if (state.state == CircuitState::OPEN) {
        auto now = std::chrono::steady_clock::now();
        if (now - state.state_change_time > state.config.reset_timeout) {
            state.state = CircuitState::HALF_OPEN;
            state.state_change_time = now;
            // emit circuitHalfOpened(component_id); // Need component_id here, but not passed
            return true; // Allow one request to test
        }
        return false;
    }
    
    if (state.state == CircuitState::HALF_OPEN) {
        // Only allow one request at a time in half-open state
        // This is a simplified implementation
        return true; 
    }
    
    return true;
}

FaultToleranceManager::CircuitState FaultToleranceManager::getCircuitState(const QString& component_id) const {
    QMutexLocker locker(&mutex);
    if (!circuits.contains(component_id)) return CircuitState::CLOSED;
    return circuits[component_id].state;
}

bool FaultToleranceManager::executeWithRetry(std::function<bool()> operation, const RetryPolicy& policy) {
    int attempts = 0;
    std::chrono::milliseconds current_backoff = policy.initial_backoff;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> jitter_dist(0.8, 1.2);
    
    while (attempts < policy.max_retries) {
        attempts++;
        
        try {
            if (operation()) {
                return true;
            }
        } catch (...) {
            // Exception treated as failure
        }
        
        if (attempts >= policy.max_retries) break;
        
        // emit retryAttempt("unknown", attempts, policy.max_retries);
        
        // Calculate backoff
        auto sleep_duration = current_backoff;
        if (policy.jitter) {
            sleep_duration = std::chrono::milliseconds((long long)(sleep_duration.count() * jitter_dist(gen)));
        }
        
        std::this_thread::sleep_for(sleep_duration);
        
        current_backoff = std::chrono::milliseconds((long long)(current_backoff.count() * policy.backoff_multiplier));
        if (current_backoff > policy.max_backoff) {
            current_backoff = policy.max_backoff;
        }
    }
    
    return false;
}

void FaultToleranceManager::registerFallback(const QString& component_id, std::function<void()> fallback_func) {
    QMutexLocker locker(&mutex);
    fallbacks[component_id] = fallback_func;
}

FaultToleranceManager::ComponentHealth FaultToleranceManager::getComponentHealth(const QString& component_id) const {
    QMutexLocker locker(&mutex);
    ComponentHealth health;
    health.component_id = component_id;
    
    if (circuits.contains(component_id)) {
        const auto& state = circuits[component_id];
        health.state = state.state;
        health.failure_count = state.failure_count;
        health.success_count = state.success_count;
        // Convert steady_clock to QDateTime (approximate)
        health.last_failure = QDateTime::currentDateTime(); // Placeholder
        health.last_success = QDateTime::currentDateTime(); // Placeholder
        
        int total = state.success_count + state.failure_count;
        health.error_rate = total > 0 ? (double)state.failure_count / total : 0.0;
    } else {
        health.state = CircuitState::CLOSED;
        health.failure_count = 0;
        health.success_count = 0;
        health.error_rate = 0.0;
    }
    
    return health;
}

std::vector<FaultToleranceManager::ComponentHealth> FaultToleranceManager::getAllComponentHealth() const {
    QMutexLocker locker(&mutex);
    std::vector<ComponentHealth> result;
    
    for (auto it = circuits.begin(); it != circuits.end(); ++it) {
        ComponentHealth health;
        health.component_id = it.key();
        health.state = it.value().state;
        health.failure_count = it.value().failure_count;
        health.success_count = it.value().success_count;
        
        int total = it.value().success_count + it.value().failure_count;
        health.error_rate = total > 0 ? (double)it.value().failure_count / total : 0.0;
        
        result.push_back(health);
    }
    
    return result;
}
