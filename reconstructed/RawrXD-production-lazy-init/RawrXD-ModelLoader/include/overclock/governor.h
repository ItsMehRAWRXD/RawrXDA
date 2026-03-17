#pragma once

#include <string>
#include <memory>

namespace RawrXD {

// Forward declaration
struct AppState;

// Overclock governor for dynamic performance tuning
class OverclockGovernor {
public:
    OverclockGovernor();
    ~OverclockGovernor();
    
    // Initialize the governor
    bool Initialize(AppState& state);
    
    // Start the governor thread
    bool Start();
    
    // Stop the governor thread
    void Stop();
    
    // Update governor settings
    void UpdateSettings(const AppState& state);
    
    // Get current performance metrics
    struct Metrics {
        float cpu_temp_c;
        float gpu_temp_c;
        uint32_t cpu_freq_mhz;
        uint32_t gpu_freq_mhz;
        float cpu_voltage;
        float gpu_voltage;
    };
    Metrics GetCurrentMetrics();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace RawrXD