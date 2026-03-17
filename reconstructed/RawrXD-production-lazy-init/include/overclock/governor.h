#pragma once

#include <atomic>
#include <thread>
#include <cstdint>
#include "settings.h"

class OverclockGovernor {
public:
    OverclockGovernor();
    ~OverclockGovernor();

    bool Start(AppState& state);
    void Stop();

private:
    int ComputePidDelta(float pidOutput, uint32_t boostStepMhz);
    int ComputeCpuDesiredDelta(float pidOutput, const AppState& state);
    int ComputeGpuDesiredDelta(float gpuPidOutput, const AppState& state);
    void RunLoop(AppState* state);

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
};
