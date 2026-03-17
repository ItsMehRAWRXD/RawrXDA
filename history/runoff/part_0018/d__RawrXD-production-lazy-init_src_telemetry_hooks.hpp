/**
 * @file telemetry_hooks.hpp
 * @brief Telemetry hooks for observability and metrics collection.
 */

#ifndef TELEMETRY_HOOKS_HPP
#define TELEMETRY_HOOKS_HPP

#include <QString>

namespace RawrXD {

class CircuitBreakerMetrics {
public:
    struct Event {
        QString backend;
        QString eventType;
        int failureCount;
    };

    static void recordEvent(const Event& event);
};

} // namespace RawrXD

#endif // TELEMETRY_HOOKS_HPP