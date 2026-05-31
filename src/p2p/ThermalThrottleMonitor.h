#pragma once
#include <windows.h>
#include <stdint.h>
#include <vector>
#include <mutex>

/**
 * @file ThermalThrottleMonitor.h
 * @brief Detects AVX-512 frequency throttling by measuring cycle-to-wallclock drift.
 */

struct ThrottleState {
    double freq_mhz;
    double drift_score;
    bool is_throttled;
};

class ThermalThrottleMonitor {
public:
    static ThermalThrottleMonitor& Instance() {
        static ThermalThrottleMonitor instance;
        return instance;
    }

    ThrottleState CheckThrottle() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 1. Measure TSC (Cycles) vs QPC (Wallclock)
        uint64_t tsc_start = __rdtsc();
        LARGE_INTEGER qpc_start;
        QueryPerformanceCounter(&qpc_start);

        // Heavy work simulation (if needed) or just passive sampling
        // We use a known sleep to measure drift
        Sleep(10); 

        uint64_t tsc_end = __rdtsc();
        LARGE_INTEGER qpc_end;
        QueryPerformanceCounter(&qpc_end);

        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);

        double elapsed_sec = (double)(qpc_end.QuadPart - qpc_start.QuadPart) / (double)freq.QuadPart;
        uint64_t elapsed_cycles = tsc_end - tsc_start;
        
        double current_mhz = (double)elapsed_cycles / (elapsed_sec * 1000000.0);

        // 2. Identify Throttling
        // If current_mhz is significantly below base clock (e.g. 500MHz below nominal), flag it
        // Note: Real implementation would query base clock from registry or CPUID leaf 0x16
        bool throttled = (current_mhz < m_nominalMhz * 0.85); 

        ThrottleState state = { current_mhz, (m_nominalMhz - current_mhz), throttled };
        return state;
    }

    void SetNominalMhz(double mhz) { m_nominalMhz = mhz; }

private:
    ThermalThrottleMonitor() : m_nominalMhz(3500.0) {} // Default nominal
    std::mutex m_mutex;
    double m_nominalMhz;
};
