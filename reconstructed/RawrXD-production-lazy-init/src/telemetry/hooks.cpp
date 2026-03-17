/**
 * @file telemetry_hooks.cpp
 * @brief Implementation of telemetry hooks for observability and metrics collection.
 */

#include "telemetry_hooks.hpp"
#include <QDebug>

namespace RawrXD {

void CircuitBreakerMetrics::recordEvent(const Event& event) {
    qInfo() << "[Telemetry] CircuitBreakerMetrics::recordEvent - Backend:" << event.backend
            << "EventType:" << event.eventType
            << "FailureCount:" << event.failureCount;
}

} // namespace RawrXD