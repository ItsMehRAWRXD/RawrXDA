#pragma once
namespace RawrXD {
namespace Integration {

struct Config {
    static bool loggingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_LOGGING_ENABLED") && qgetenv("RAWRXD_LOGGING_ENABLED") != "0";
    }
    static bool stubLoggingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_LOG_STUBS") && qgetenv("RAWRXD_LOG_STUBS") != "0";
    }
    static bool metricsEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_ENABLE_METRICS") && qgetenv("RAWRXD_ENABLE_METRICS") != "0";
    }
    static bool tracingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_ENABLE_TRACING") && qgetenv("RAWRXD_ENABLE_TRACING") != "0";
    }
};

#define RAWRXD_INIT_TIMED(name) RawrXD::Integration::ScopedTimer __rawr_timer__("main", name, "init")
#define RAWRXD_TIMED_FUNC() RawrXD::Integration::ScopedTimer __rawr_timer__(__FUNCTION__, __FUNCTION__, "execution")
#define RAWRXD_TIMED_NAMED(name) RawrXD::Integration::ScopedTimer __rawr_timer__(name, name, "execution")

inline void logWithLevel(const std::string &level, const std::string &component, const std::string &event, const std::string &message, const void* &data = void*()) {
    if (!Config::loggingEnabled()) return;
    void* logEntry{
        {"timestamp", // DateTime::currentDateTimeUtc().toString(ISODate)},
        {"level", level},
        {"component", component},
        {"event", event},
        {"message", message}
    };
    if (!data.empty()) {
        logEntry.insert("data", data);
    }
    // // qInfo().noquote() << void*(logEntry).toJson(void*::Compact);
}

inline void logInfo(const std::string &component, const std::string &event, const std::string &message, const void* &data = void*()) {
    logWithLevel(std::stringLiteral("INFO"), component, event, message, data);
}

inline void logDebug(const std::string &component, const std::string &event, const std::string &message, const void* &data = void*()) {
    logWithLevel(std::stringLiteral("DEBUG"), component, event, message, data);
}

inline void logWarn(const std::string &component, const std::string &event, const std::string &message, const void* &data = void*()) {
    logWithLevel(std::stringLiteral("WARN"), component, event, message, data);
}

inline void logError(const std::string &component, const std::string &event, const std::string &message, const void* &data = void*()) {
    logWithLevel(std::stringLiteral("ERROR"), component, event, message, data);
}

struct ScopedTimer {
    std::string component;
    std::string name;
    std::string event;
    std::chrono::steady_clock::time_point timer;
    explicit ScopedTimer(const char *componentName, const char *objectName, const char *eventName)
        : component(std::string::fromUtf8(componentName)),
          name(std::string::fromUtf8(objectName)),
          event(std::string::fromUtf8(eventName)) {
        if (Config::loggingEnabled()) {
            timer.start();
        }
    }
    ~ScopedTimer() {
        if (Config::loggingEnabled()) {
            void* logEntry{
                {"timestamp", // DateTime::currentDateTimeUtc().toString(ISODate)},
                {"level", "INFO"},
                {"component", component},
                {"event", event},
                {"name", name},
                {"latency_ms", timer.elapsed()}
            };
            // // qInfo().noquote() << void*(logEntry).toJson(void*::Compact);
        }
    }
};

inline void recordMetric(const char *metricName, int64_t value = 1) {
    if (!Config::metricsEnabled()) return;
    void* metricEntry{
        {"timestamp", // DateTime::currentDateTimeUtc().toString(ISODate)},
        {"type", "metric"},
        {"name", std::string::fromUtf8(metricName)},
        {"value", value}
    };
    // // qInfo().noquote() << void*(metricEntry).toJson(void*::Compact);
}

inline void traceEvent(const char *spanName, const char *eventName) {
    if (!Config::tracingEnabled()) return;
    void* traceEntry{
        {"timestamp", // DateTime::currentDateTimeUtc().toString(ISODate)},
        {"type", "trace"},
        {"span", std::string::fromUtf8(spanName)},
        {"event", std::string::fromUtf8(eventName)}
    };
    // // qInfo().noquote() << void*(traceEntry).toJson(void*::Compact);
}

template <typename Fn>
auto retryWithBackoff(Fn fn, int maxRetries = 3, int64_t delayMs = 100) {
    int attempt = 0;
    while (true) {
        try {
            return fn();
        } catch (const std::exception &ex) {
            ++attempt;
            if (attempt >= maxRetries) {
                logError(std::stringLiteral("Retry"), std::stringLiteral("exhausted"),
                         std::stringLiteral("Max retries reached: %1").arg(std::string::fromUtf8(ex.what())));
                throw;
            }
            logWarn(std::stringLiteral("Retry"), std::stringLiteral("attempt"),
                    std::stringLiteral("Attempt %1 failed, retrying in %2ms").arg(attempt).arg(delayMs));
            std::thread::msleep(static_cast<unsigned long>(delayMs));
            delayMs *= 2;
        }
    }
}

class CircuitBreaker {
public:
    enum class State { Closed, Open, HalfOpen };

    explicit CircuitBreaker(int failureThreshold = 5, int64_t resetTimeoutMs = 30000)
        : m_failureThreshold(failureThreshold), m_resetTimeoutMs(resetTimeoutMs) {}

    bool allowRequest() {
        if (m_state == State::Closed) return true;
        if (m_state == State::Open) {
            if (m_timer.elapsed() >= m_resetTimeoutMs) {
                m_state = State::HalfOpen;
                logInfo(std::stringLiteral("CircuitBreaker"), std::stringLiteral("half_open"), std::stringLiteral("Testing circuit"));
                return true;
            }
            return false;
        }
        return true;
    }

    void recordSuccess() {
        if (m_state == State::HalfOpen) {
            m_state = State::Closed;
            m_failures = 0;
            logInfo(std::stringLiteral("CircuitBreaker"), std::stringLiteral("closed"), std::stringLiteral("Circuit recovered"));
        }
    }

    void recordFailure() {
        ++m_failures;
        if (m_state == State::HalfOpen || m_failures >= m_failureThreshold) {
            m_state = State::Open;
            m_timer.start();
            logWarn(std::stringLiteral("CircuitBreaker"), std::stringLiteral("open"),
                    std::stringLiteral("Circuit opened after %1 failures").arg(m_failures));
        }
    }

    State state() const { return m_state; }

private:
    State m_state = State::Closed;
    int m_failures = 0;
    int m_failureThreshold;
    int64_t m_resetTimeoutMs;
    std::chrono::steady_clock::time_point m_timer;
};

} // namespace Integration
} // namespace RawrXD





