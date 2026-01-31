#pragma once
#ifndef OVERCLOCK_GOVERNOR_H
#define OVERCLOCK_GOVERNOR_H

#include <atomic>
#include <cstdint>
#include <thread>

struct AppState;

class OverclockGovernor {
public:
    OverclockGovernor();
    ~OverclockGovernor();

    bool Start(AppState& state);
    void Stop();

private:
    static int ComputePidDelta(float pidOutput, uint32_t boostStepMhz);
    static int ComputeCpuDesiredDelta(float pidOutput, const AppState& state);
    static int ComputeGpuDesiredDelta(float gpuPidOutput, const AppState& state);

    void RunLoop(AppState* state);

private:
    std::atomic<bool> running_{false};
    std::thread worker_{};
};

#endif // OVERCLOCK_GOVERNOR_H
