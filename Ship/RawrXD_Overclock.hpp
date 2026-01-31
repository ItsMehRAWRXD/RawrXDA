// RawrXD_Overclock.hpp - Autonomous HW Overclock Governor
// Pure C++20 - No Qt Dependencies
// Real-time thermal-aware frequency management

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <windows.h>

namespace RawrXD {

struct OverclockState {
    float current_cpu_temp_c = 45.0f;
    float current_gpu_hotspot_c = 50.0f;
    uint32_t target_all_core_mhz = 5000;
    uint32_t boost_step_mhz = 25;
    std::string governor_status = "idle";
};

class OverclockGovernor {
public:
    OverclockGovernor() : running_(false) {}
    ~OverclockGovernor() { Stop(); }

    bool Start(OverclockState& state) {
        if (running_) return true;
        running_ = true;
        worker_ = std::thread(&OverclockGovernor::RunLoop, this, &state);
        return true;
    }

    void Stop() {
        if (running_) {
            running_ = false;
            if (worker_.joinable()) worker_.join();
        }
    }

private:
    std::atomic<bool> running_;
    std::thread worker_;

    void RunLoop(OverclockState* state) {
        state->governor_status = "running";
        int appliedOffset = 0;

        while (running_) {
            // Thermal throttle check
            if (state->current_cpu_temp_c > 90.0f) {
                appliedOffset -= (int)state->boost_step_mhz;
                state->governor_status = "throttling";
            } else if (state->current_cpu_temp_c < 75.0f) {
                appliedOffset += (int)state->boost_step_mhz;
                state->governor_status = "boosting";
            }

            // Apply overclock via hypothetical driver/vender API
            // In production, this would call RyzenMaster or Adrenalin CLI
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        state->governor_status = "stopped";
    }
};

} // namespace RawrXD
