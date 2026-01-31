// overclock_governor_stub.cpp
// Stub implementation for OverclockGovernor
// Replace with real implementation when hardware integration is ready

#include "overclock_governor.h"
#include "../qtapp/settings.h"

OverclockGovernor::OverclockGovernor() = default;

OverclockGovernor::~OverclockGovernor() {
    Stop();
}

bool OverclockGovernor::Start(AppState& state) {
    (void)state;
    // Stub - no actual overclocking
    running_ = true;
    return true;
}

void OverclockGovernor::Stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

int OverclockGovernor::ComputePidDelta(float pidOutput, uint32_t boostStepMhz) {
    (void)pidOutput;
    (void)boostStepMhz;
    return 0;
}

int OverclockGovernor::ComputeCpuDesiredDelta(float pidOutput, const AppState& state) {
    (void)pidOutput;
    (void)state;
    return 0;
}

int OverclockGovernor::ComputeGpuDesiredDelta(float gpuPidOutput, const AppState& state) {
    (void)gpuPidOutput;
    (void)state;
    return 0;
}

void OverclockGovernor::RunLoop(AppState* state) {
    (void)state;
    // Stub - no actual governor loop
}
