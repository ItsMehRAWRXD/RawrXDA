#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

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

namespace telemetry {
    bool Initialize();
    bool InitializeHardware();  // Two-phase init
    bool Poll(TelemetrySnapshot& out);
    void Shutdown();
}

// Telemetry event entry
struct TelemetryEvent {
    std::string name;
    uint64_t timestampMs = 0;
    // Metadata stored as key-value pairs
    std::vector<std::pair<std::string, std::string>> metadata;
};

class Telemetry {
public:
    Telemetry();
    ~Telemetry();
    
    // Two-phase initialization
    void initializeHardware();
    
    void recordEvent(const std::string& event_name);
    bool saveTelemetry(const std::string& filepath);
    void enableTelemetry(bool enable);
    
private:
    bool is_enabled_ = false;
    std::vector<TelemetryEvent> events_;
    std::mutex m_mutex;
};
