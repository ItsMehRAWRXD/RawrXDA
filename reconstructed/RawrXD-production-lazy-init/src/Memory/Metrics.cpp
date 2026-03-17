#include "Metrics.hpp"
#include <chrono>
#include <mutex>
#include <QString>
#include <QJsonObject>
#include "../telemetry_singleton.h"

namespace mem {

int64_t Metrics::startTimer() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

int64_t Metrics::elapsedMs(int64_t startTime) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto elapsed = now - startTime;
    // Convert to milliseconds (assuming nanoseconds)
    return elapsed / 1'000'000;
}

void Metrics::increment(const char* name, int64_t value) {
    // Structured telemetry event for counters
    QJsonObject meta;
    meta["type"] = QString("counter");
    meta["name"] = QString::fromUtf8(name);
    meta["value"] = static_cast<double>(value);
    GetTelemetry().recordEvent("metrics_increment", meta);
}

void Metrics::record(const char* name, double value) {
    // Structured telemetry event for gauges/histograms
    QJsonObject meta;
    meta["type"] = QString("gauge");
    meta["name"] = QString::fromUtf8(name);
    meta["value"] = value;
    GetTelemetry().recordEvent("metrics_record", meta);
}

void Metrics::recordLatency(const char* name, int64_t ms) {
    // Forward as histogram
    QJsonObject meta;
    meta["type"] = QString("latency_ms");
    meta["name"] = QString::fromUtf8(name);
    meta["value"] = static_cast<double>(ms);
    GetTelemetry().recordEvent("metrics_latency", meta);
}

void Metrics::recordBytes(const char* name, int64_t bytes) {
    // Forward as gauge
    QJsonObject meta;
    meta["type"] = QString("bytes");
    meta["name"] = QString::fromUtf8(name);
    meta["value"] = static_cast<double>(bytes);
    GetTelemetry().recordEvent("metrics_bytes", meta);
}

} // namespace mem

// --- RawrXD::LLMMetrics and CircuitBreakerMetrics linker stubs ---
namespace RawrXD {

// Forward declarations if not present
class LLMMetrics {
public:
    struct Request;
    static void recordRequest(const Request&);
};
class CircuitBreakerMetrics {
public:
    struct Event;
    static void recordEvent(const Event&);
};

// Stub implementations
void LLMMetrics::recordRequest(const LLMMetrics::Request&) {}
void CircuitBreakerMetrics::recordEvent(const CircuitBreakerMetrics::Event&) {}

} // namespace RawrXD
