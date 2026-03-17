// telemetry_stub.cpp
// Stub implementation for telemetry functions

#include "telemetry.h"

namespace telemetry {

bool Initialize() {
    return true;
}

bool InitializeHardware() {
    return true;
}

bool Poll(TelemetrySnapshot& out) {
    out.timeMs = 0;
    out.cpuTempValid = false;
    out.cpuTempC = 0.0;
    out.cpuUsagePercent = 0.0;
    out.gpuTempValid = false;
    out.gpuTempC = 0.0;
    out.gpuUsagePercent = 0.0;
    out.gpuVendor = "Unknown";
    return true;
}

void Shutdown() {
    // Stub - nothing to clean up
}

} // namespace telemetry
