#pragma once
#include <string>
#include <chrono>

// SCALAR-ONLY: All threading removed - synchronous scalar operations

struct AppState;

class OverclockGovernor {
public:
    OverclockGovernor();
    ~OverclockGovernor();
    bool Start(AppState& state);
    void Stop();
        // Utility: map pidOutput + step size to desired delta in MHz
        static int ComputePidDelta(float pidOutput, uint32_t boostStepMhz);
    // Helper for unit testing: compute desired delta from pid output
    static int ComputeCpuDesiredDelta(float pidOutput, const AppState& state);
    static int ComputeGpuDesiredDelta(float gpuPidOutput, const AppState& state);
private:
    void RunLoop(AppState* state);
    bool running_;  // Scalar: no atomic needed
};
