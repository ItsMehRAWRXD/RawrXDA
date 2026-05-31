#pragma once

#include <cstdint>
#include <atomic>

namespace RawrXD {

/**
 * @brief ASM Thermal Sensor Bridge.
 * Allows the AVX-512 ASM kernels to observe clock-speed scaling or thermal events.
 * If the CPU begins to throttle due to intensive SIMD search, the bridge triggers
 * a throughput reduction to maintain stability.
 */
class ASMThermalBridge {
public:
    static ASMThermalBridge& instance() {
        static ASMThermalBridge i;
        return i;
    }

    // Called by ASM kernel via external symbol or C++ wrapper
    bool should_throttle() const {
        return m_thermal_trip.load(std::memory_order_relaxed);
    }

    void set_thermal_trip(bool trip) {
        m_thermal_trip.store(trip, std::memory_order_relaxed);
    }

    // Simulated telemetry for parity audit
    uint32_t get_current_throttle_depth() const {
        return should_throttle() ? 50 : 0; // 50% throughput reduction
    }

private:
    ASMThermalBridge() : m_thermal_trip(false) {}
    std::atomic<bool> m_thermal_trip;
};

} // namespace RawrXD
