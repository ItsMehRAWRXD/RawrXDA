#pragma once

#include <vector>
#include <string>

struct TelemetrySnapshot {
    bool cpuTempValid;
    double cpuTempC;
    // Add other fields as necessary
};

namespace telemetry {
    bool Initialize();
    bool InitializeHardware();
    void Shutdown();
    // bool Poll(TelemetrySnapshot& out); // Uncomment if needed
}
