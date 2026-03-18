#pragma once
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <stdexcept>

namespace RawrXD::Agentic {
    // Production-grade error recovery with exponential backoff
    class ErrorRecoveryManager {
    public:
        enum class RecoveryStrategy {
            ImmediateRetry,
            ExponentialBackoff,
            CircuitBreaker,
            FallbackMode
        };

        struct RecoveryConfig {
            int maxRetries = 3;
            std::chrono::milliseconds baseDelay{1000};
            double backoffMultiplier = 2.0;
            std::chrono::milliseconds maxDelay{30000};
            int circuitBreakerThreshold = 5;
            std::chrono::milliseconds circuitBreakerTimeout{60000};
        };

        static ErrorRecoveryManager& instance();

        template<typename Func>
        auto executeWithRecovery(Func&& func, const RecoveryConfig& config = {}) {
            return executeWithRecoveryImpl(std::forward<Func>(func), config);
        }

        void recordFailure(const std::string& operation);
        void recordSuccess(const std::string& operation);
        bool isCircuitOpen(const std::string& operation) const;

    private:
        struct CircuitState {
            std::atomic<int> failureCount{0};
            std::chrono::steady_clock::time_point lastFailure;
            bool open = false;
        };

        std::unordered_map<std::string, CircuitState> circuitStates_;
        mutable std::mutex mutex_;

        template<typename Func>
        auto executeWithRecoveryImpl(Func&& func, const RecoveryConfig& config);
    };
}

// ---------------------------------------------------------------------------
// Inline template implementation
// ---------------------------------------------------------------------------
namespace RawrXD::Agentic {
    template<typename Func>
    auto ErrorRecoveryManager::executeWithRecoveryImpl(Func&& func, const RecoveryConfig& config) {
        int attempt = 0;
        auto delay = config.baseDelay;

        while (attempt < config.maxRetries) {
            try {
                return func();
            } catch (const std::exception&) {
                attempt++;
                if (attempt >= config.maxRetries) {
                    throw;
                }

                std::this_thread::sleep_for(delay);
                delay = std::chrono::milliseconds(
                    static_cast<long long>(delay.count() * config.backoffMultiplier));
                if (delay > config.maxDelay) {
                    delay = config.maxDelay;
                }
            }
        }

        throw std::runtime_error("ErrorRecoveryManager exhausted retries");
    }
} // namespace RawrXD::Agentic
