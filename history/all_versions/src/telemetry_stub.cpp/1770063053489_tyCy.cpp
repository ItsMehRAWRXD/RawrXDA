#include "telemetry.h"

namespace telemetry {
    bool Initialize() { return true; }
    bool InitializeHardware() { return true; }
    // bool Poll(TelemetrySnapshot& out) { return false; } // Commented out in header
    void Shutdown() {}
}