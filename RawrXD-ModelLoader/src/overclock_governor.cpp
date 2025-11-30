#include "overclock_governor.h"
#include "gui.h"
#include "telemetry.h"
#include "overclock_vendor.h"
#include "baseline_profile.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <filesystem>

OverclockGovernor::OverclockGovernor() {}
OverclockGovernor::~OverclockGovernor() { Stop(); }

int OverclockGovernor::ComputePidDelta(float pidOutput, uint32_t boostStepMhz) {
    if (boostStepMhz == 0) return 0;
    if (pidOutput > 5.0f) return (int)boostStepMhz;
    if (pidOutput > 1.0f) return (int)(boostStepMhz / 2);
    if (pidOutput < -5.0f) return -(int)boostStepMhz;
    if (pidOutput < -1.0f) return -(int)(boostStepMhz / 2);
    return 0;
}

int OverclockGovernor::ComputeCpuDesiredDelta(float pidOutput, const AppState& state) {
    return ComputePidDelta(pidOutput, state.boost_step_mhz);
}

int OverclockGovernor::ComputeGpuDesiredDelta(float gpuPidOutput, const AppState& state) {
    // Use GPU boost step the same as CPU for now
    return ComputePidDelta(gpuPidOutput, state.boost_step_mhz);
}

bool OverclockGovernor::Start(AppState& state) {
    if (running_) return true;
    running_ = true;
    worker_ = std::thread(&OverclockGovernor::RunLoop, this, &state);
    return true;
}

void OverclockGovernor::Stop() {
    if (running_) {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }
}

void OverclockGovernor::RunLoop(AppState* state) {
    if (!state) return;
    state->governor_status = "initializing";
    overclock_vendor::DetectRyzenMaster(*state);
    overclock_vendor::DetectAdrenalinCLI(*state);
    baseline_profile::Load(*state); // load baseline if exists
    state->governor_status = "running";

    uint32_t baseDetectMhz = (state->target_all_core_mhz==0? 5000 : state->target_all_core_mhz);
    if (state->baseline_loaded && state->baseline_detected_mhz > 0) {
        baseDetectMhz = state->baseline_detected_mhz;
    } else {
        state->baseline_detected_mhz = baseDetectMhz;
    }
    int appliedOffset = 0;
    auto lastStepTime = std::chrono::steady_clock::now();
    const std::chrono::seconds stepInterval(10);

    std::filesystem::path logPath{"oc-session.log"};
    std::ofstream log(logPath, std::ios::app);
    auto logEvent = [&](const std::string& tag){
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        char buf[32]; std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&tt));
        log << buf << " " << tag << " offset=" << appliedOffset
            << " cpu_temp=" << state->current_cpu_temp_c
            << " gpu_temp=" << state->current_gpu_hotspot_c
            << " status=" << state->governor_status << "\n";
    };
    logEvent("start");

    int thermalFaults = 0;
    const int maxFaultsBeforeRollback = 3;

    while (running_) {
        telemetry::TelemetrySnapshot snap; telemetry::Poll(snap);
        if (snap.cpuTempValid) state->current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
        if (snap.gpuTempValid) state->current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
        // Simulated current freq = base + applied offset
        state->current_cpu_freq_mhz = baseDetectMhz + appliedOffset;
        state->current_gpu_freq_mhz = 0; // not managed yet

        // Thermal checks
        bool cpuHot = snap.cpuTempValid && snap.cpuTempC >= state->max_cpu_temp_c;
        bool gpuHot = snap.gpuTempValid && snap.gpuTempC >= state->max_gpu_hotspot_c;

        // PID error based on thermal headroom (positive error = we have headroom to boost)
        float cpuHeadroom = (float)state->max_cpu_temp_c - (float)state->current_cpu_temp_c;
        float targetHeadroom = 10.0f; // keep 10C buffer
        float error = cpuHeadroom - targetHeadroom; // want headroom ~= targetHeadroom
        state->pid_integral += error;
        // Clamp integral to avoid wind-up
        if (state->pid_integral > state->pid_integral_clamp) state->pid_integral = state->pid_integral_clamp;
        if (state->pid_integral < -state->pid_integral_clamp) state->pid_integral = -state->pid_integral_clamp;
        float derivative = error - state->pid_last_error;
        float pidOutput = state->pid_kp * error + state->pid_ki * state->pid_integral + state->pid_kd * derivative;
        state->pid_last_error = error;

        // --- GPU PID loop scaffold ---
        float gpuHeadroom = (float)state->max_gpu_hotspot_c - (float)state->current_gpu_hotspot_c;
        float gpuTargetHeadroom = 10.0f;
        float gpuError = gpuHeadroom - gpuTargetHeadroom;
        state->gpu_pid_integral += gpuError;
        if (state->gpu_pid_integral > state->gpu_pid_integral_clamp) state->gpu_pid_integral = state->gpu_pid_integral_clamp;
        if (state->gpu_pid_integral < -state->gpu_pid_integral_clamp) state->gpu_pid_integral = -state->gpu_pid_integral_clamp;
        float gpuDerivative = gpuError - state->gpu_pid_last_error;
        float gpuPidOutput = state->gpu_pid_kp * gpuError + state->gpu_pid_ki * state->gpu_pid_integral + state->gpu_pid_kd * gpuDerivative;
        state->gpu_pid_last_error = gpuError;

        if (cpuHot || gpuHot) {
            // Step down
            appliedOffset = std::max(0, appliedOffset - (int)state->boost_step_mhz);
            state->applied_core_offset_mhz = appliedOffset;
            overclock_vendor::ApplyCpuOffsetMhz(appliedOffset);
            state->governor_last_fault = cpuHot? "cpu_thermal" : "gpu_thermal";
            state->governor_status = "thermal-throttle";
            thermalFaults++;
            logEvent("thermal_fault");
            if (thermalFaults >= maxFaultsBeforeRollback && appliedOffset > 0) {
                appliedOffset = 0;
                state->applied_core_offset_mhz = appliedOffset;
                overclock_vendor::ApplyCpuOffsetMhz(appliedOffset);
                state->governor_status = "rollback";
                state->governor_last_fault = "rollback_after_faults";
                logEvent("rollback");
            }
            lastStepTime = std::chrono::steady_clock::now();
        } else {
            // PID-driven offset adjustment: map pidOutput to steps
            auto now = std::chrono::steady_clock::now();
            int desiredDelta = ComputeCpuDesiredDelta(pidOutput, *state);

            if (now - lastStepTime >= std::chrono::seconds(5) && desiredDelta != 0) {
                int newOffset = appliedOffset + desiredDelta;
                if (newOffset < 0) newOffset = 0;
                if (state->target_all_core_mhz > 0) {
                    int maxOffset = (int)state->target_all_core_mhz - (int)baseDetectMhz;
                    if (newOffset > maxOffset) newOffset = maxOffset;
                }
                if (newOffset != appliedOffset) {
                    appliedOffset = newOffset;
                    state->applied_core_offset_mhz = appliedOffset;
                    overclock_vendor::ApplyCpuOffsetMhz(appliedOffset);
                    state->governor_status = desiredDelta > 0 ? "pid-boost" : "pid-reduce";
                    logEvent(state->governor_status);
                }
                lastStepTime = now;
            } else {
                state->governor_status = "stable";
            }

            // Note: GPU overclocking not yet implemented in this version
            // Future enhancement: Add GPU offset control
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    state->governor_status = "stopped";
    logEvent("stop");
    // Persist baseline if improved stable offset achieved
    if (appliedOffset > state->baseline_stable_offset_mhz) {
        state->baseline_stable_offset_mhz = appliedOffset;
    }
    baseline_profile::Save(*state);
}
