#pragma once
#include <vector>
#include <cstdint>
#include "SovereignSharedMemory.h"

struct DriveConfig {
    int32_t throttleTemp;   // Temp to start throttling (e.g., 75 C)
    int32_t releaseTemp;    // Temp to stop throttling (e.g., 65 C) - Hysteresis
    int32_t criticalTemp;   // Hard stop temp (e.g., 85 C)
};

struct DriveState {
    bool isThrottled;
    bool isCritical;
    int32_t peakTemp;
};

class ThermalGovernor {
public:
    ThermalGovernor();
    ~ThermalGovernor();

    bool Initialize();
    void Update();
    void Shutdown();

    // Configuration
    void SetGlobalConfig(int32_t throttle, int32_t release, int32_t critical);

private:
    HANDLE hTelemetryMap;
    const SovereignTelemetryMMF* pTelemetry;

    HANDLE hGovernorMap;
    SovereignGovernorStatus* pGovernor;

    DriveConfig config;
    std::vector<DriveState> driveStates;

    void ProcessDrive(int index, int32_t temp);
    bool OpenTelemetry();
    bool CreateGovernorMMF();
};
