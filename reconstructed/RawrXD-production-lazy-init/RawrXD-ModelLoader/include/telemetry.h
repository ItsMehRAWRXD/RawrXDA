#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Telemetry snapshot structure
struct TelemetrySnapshot {
    uint64_t timeMs = 0;

    bool cpuTempValid = false;
    double cpuTempC = 0.0;
    double cpuUsagePercent = 0.0;
    bool cpuPowerValid = false;
    double cpuPowerW = 0.0;

    bool gpuTempValid = false;
    double gpuTempC = 0.0;
    double gpuUsagePercent = 0.0;
    bool gpuPowerValid = false;
    double gpuPowerW = 0.0;

    double vramUsedMB = 0.0;
    std::string gpuVendor;
};

namespace telemetry {
    using TelemetrySnapshot = ::TelemetrySnapshot;
    bool Initialize();
    bool Poll(TelemetrySnapshot& out);
    void Shutdown();
}

namespace RawrXD {

// Forward declaration
struct AppState;

using TelemetrySnapshot = ::TelemetrySnapshot;

// Telemetry system for monitoring and metrics
class Telemetry {
public:
    Telemetry();
    ~Telemetry();

    // Initialize telemetry
    bool Initialize(const std::string& endpoint);

    // Shutdown telemetry
    void Shutdown();

    // Record a metric
    void RecordMetric(const std::string& name, double value);

    // Record an event
    void RecordEvent(const std::string& event_name, const std::string& details);

    // Flush pending telemetry data
    void Flush();

    // Get current telemetry snapshot
    TelemetrySnapshot GetSnapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace RawrXD