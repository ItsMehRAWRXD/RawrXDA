#pragma once
#include <string>
#include <atomic>

struct AppState {
    std::string governor_status;
    std::atomic<float> current_cpu_temp{0.0f};
    std::atomic<float> current_gpu_temp{0.0f};
    std::atomic<float> current_cpu_power{0.0f};
    std::atomic<float> current_gpu_power{0.0f};
    
    std::string status_message;
    uint32_t boost_step_mhz = 25;
    
    // Feature flags
    bool ryzen_master_detected = false;
    bool adrenalin_cli_detected = false;
};
