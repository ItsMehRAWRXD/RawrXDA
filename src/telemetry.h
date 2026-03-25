<<<<<<< HEAD
void logEvent(const char* name, double v1, double v2);
=======
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

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
    bool InitializeHardware();  // Two-phase init: call after application exists
    bool Poll(TelemetrySnapshot& out);
    void Shutdown();
}

class Telemetry {
public:
    Telemetry();
    ~Telemetry();
    
    // Two-phase initialization: call this after application is running
    void initializeHardware();
    
    void recordEvent(const std::string& event_name, const nlohmann::json& metadata = nlohmann::json::object());
    bool saveTelemetry(const std::string& filepath);
    void enableTelemetry(bool enable);
    
private:
    bool is_enabled_;
    nlohmann::json events_;  // JSON array of events
};
>>>>>>> origin/main
