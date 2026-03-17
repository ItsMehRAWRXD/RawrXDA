#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Telemetry snapshot structure
struct TelemetrySnapshot {
    uint64_t timeMs = 0;
    
    // CPU metrics
    bool cpuTempValid = false;
    double cpuTempC = 0.0;
    double cpuUsagePercent = 0.0;
    
    // GPU metrics
    bool gpuTempValid = false;
    double gpuTempC = 0.0;
    double gpuUsagePercent = 0.0;
    std::string gpuVendor;
};

using TelemetryMetadata = std::unordered_map<std::string, std::string>;

struct TelemetryEvent {
    std::string name;
    uint64_t timestampMs = 0;
    TelemetryMetadata metadata;
};

namespace telemetry {
    bool Initialize();
    bool InitializeHardware();  // Two-phase init: call after QApplication exists
    bool Poll(TelemetrySnapshot& out);
    void Shutdown();
}

class Telemetry  {public:
    Telemetry();
    ~Telemetry();
    
    // Two-phase initialization: call this after QApplication is running
    void initializeHardware();
    
    void recordEvent(const std::string& event_name, const TelemetryMetadata& metadata = {});
    bool saveTelemetry(const std::string& filepath);
    void enableTelemetry(bool enable);
    
private:
    bool is_enabled_;
    std::vector<TelemetryEvent> events_;
};

