#pragma once
#include "memory_oracle.h"
#include "hardware_telemetry_bridge.h"

namespace RawrXD::Memory {

class MemoryMorphController {
public:
    MemoryMorphController(MemoryOracle& oracle, HardwareTelemetryBridge& bridge)
        : m_oracle(oracle), m_bridge(bridge) {}

    void update() {
        auto metrics = m_bridge.pollAll();
        m_oracle.reportHardwareUsage(metrics.vramUsage, metrics.ramUsage, metrics.vramTotal, metrics.ramTotal);
    }

private:
    MemoryOracle& m_oracle;
    HardwareTelemetryBridge& m_bridge;
};

}
