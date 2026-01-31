#include "ThermalGovernor.h"
#include <iostream>
#include <windows.h>

ThermalGovernor::ThermalGovernor() 
    : hTelemetryMap(NULL), pTelemetry(NULL), 
      hGovernorMap(NULL), pGovernor(NULL) {
    // Default safe config
    config.throttleTemp = 70;
    config.releaseTemp = 60;
    config.criticalTemp = 80;
    
    driveStates.resize(16, {false, false, -273});
}

ThermalGovernor::~ThermalGovernor() {
    Shutdown();
}

void ThermalGovernor::Shutdown() {
    if (pTelemetry) UnmapViewOfFile(pTelemetry);
    if (hTelemetryMap) CloseHandle(hTelemetryMap);
    if (pGovernor) UnmapViewOfFile(pGovernor);
    if (hGovernorMap) CloseHandle(hGovernorMap);
    
    pTelemetry = NULL;
    hTelemetryMap = NULL;
    pGovernor = NULL;
    hGovernorMap = NULL;
}

void ThermalGovernor::SetGlobalConfig(int32_t throttle, int32_t release, int32_t critical) {
    config.throttleTemp = throttle;
    config.releaseTemp = release;
    config.criticalTemp = critical;
}

bool ThermalGovernor::OpenTelemetry() {
    // Try Global first, fellback to Local
    hTelemetryMap = OpenFileMappingA(FILE_MAP_READ, FALSE, MMF_NAME_TEMPS);
    if (!hTelemetryMap) {
        hTelemetryMap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\SOVEREIGN_NVME_TEMPS");
    }
    
    if (!hTelemetryMap) return false;
    
    pTelemetry = (const SovereignTelemetryMMF*)MapViewOfFile(hTelemetryMap, FILE_MAP_READ, 0, 0, sizeof(SovereignTelemetryMMF));
    return (pTelemetry != NULL);
}

bool ThermalGovernor::CreateGovernorMMF() {
    // Create Global for harness visibility
    // Requires SeCreateGlobalPrivilege if running as Service, but as user app strictly reading user mapping is safer?
    // We will try Global, fall back to Local.
    
    hGovernorMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SovereignGovernorStatus), MMF_NAME_GOVERNOR);
    if (!hGovernorMap && GetLastError() == ERROR_ACCESS_DENIED) {
         hGovernorMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SovereignGovernorStatus), "Local\\SOVEREIGN_GOVERNOR_STATUS");
    }

    if (!hGovernorMap) return false;
    
    pGovernor = (SovereignGovernorStatus*)MapViewOfFile(hGovernorMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SovereignGovernorStatus));
    if (!pGovernor) return false;

    // Init Buffer
    memset(pGovernor, 0, sizeof(SovereignGovernorStatus));
    pGovernor->allowedMask = 0xFFFFFFFF; // Allow all by default
    return true;
}

bool ThermalGovernor::Initialize() {
    if (!CreateGovernorMMF()) {
        
        return false;
    }
    
    if (!OpenTelemetry()) {
        
        return false;
    }
    
    return true;
}

void ThermalGovernor::ProcessDrive(int index, int32_t temp) {
    if (index >= driveStates.size()) return;
    
    DriveState& state = driveStates[index];
    
    // Ignore invalid readings
    if (temp <= -100 || temp > 200) return; 

    // Peak tracking
    if (temp > state.peakTemp) state.peakTemp = temp;

    // Hysteresis Logic
    if (temp >= config.criticalTemp) {
        state.isCritical = true;
        state.isThrottled = true;
    } else if (temp >= config.throttleTemp) {
        state.isThrottled = true;
        // Critical stickiness: only clear critical if we drop below release? 
        // Or clear critical as soon as we drop below critical?
        // Let's hold critical until release to be safe.
    } else if (temp <= config.releaseTemp) {
        state.isThrottled = false;
        state.isCritical = false;
    }
}

void ThermalGovernor::Update() {
    if (!pTelemetry) {
        if (!OpenTelemetry()) return;
    }
    
    // Validate Signature
    if (pTelemetry->signature != SIGNATURE_SOVE) return;

    uint32_t allowed = 0;
    uint32_t throttled = 0;
    int32_t maxSession = -273;

    for (int i = 0; i < 5; i++) { // Or pTelemetry->count
        int32_t t = pTelemetry->temps[i];
        ProcessDrive(i, t);
        
        if (!driveStates[i].isThrottled) {
            allowed |= (1 << i);
        } else {
            throttled |= (1 << i);
        }
        
        if (t > maxSession) maxSession = t;
    }
    
    // Write Status
    pGovernor->allowedMask = allowed;
    pGovernor->throttledMask = throttled;
    pGovernor->maxTempSeen = maxSession;
    pGovernor->lastUpdate = GetTickCount64();
    
    if (throttled != 0) {
        pGovernor->state = 1; // Warning
        if (pGovernor->maxTempSeen >= config.criticalTemp) pGovernor->state = 2; // Critical
    } else {
        pGovernor->state = 0;
    }
}
