#include "ThermalThrottleMonitor.h"
#include "EvolutionEventBus.h"
#include <sstream>
#include <iostream>

void MonitorThermalPulse() {
    auto& monitor = ThermalThrottleMonitor::Instance();
    ThrottleState state = monitor.CheckThrottle();

    if (state.is_throttled) {
        std::stringstream ss;
        ss << "{\"freq_mhz\": " << state.freq_mhz << ", \"drift\": " << state.drift_score << ", \"status\": \"THROTTLED\"}";
        EvolutionEventBus::Instance().Emit("ThermalThrottleDetected", "LocalNode", ss.str().c_str());
        std::cout << "[ThermalGuard] WARNING: AVX-512 Throttling detected! Frequency dropped to " << state.freq_mhz << " MHz." << std::endl;
    }
}
