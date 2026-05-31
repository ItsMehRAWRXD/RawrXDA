// src/direct_io/nvme_thermal_stressor.cpp
// ════════════════════════════════════════════════════════════════════════════════
// SovereignControlBlock NVMe Thermal Poller & I/O Stressor
// C++ Implementation of wrapper classes
// ════════════════════════════════════════════════════════════════════════════════

#include "nvme_thermal_stressor.h"
#include <chrono>
#include <algorithm>

namespace sovereign {

// ════════════════════════════════════════════════════════════════════════════════
// NVMeThermalMonitor Implementation
// ════════════════════════════════════════════════════════════════════════════════

std::vector<ThermalSample> NVMeThermalMonitor::pollAll() const {
    std::vector<ThermalSample> results;
    results.reserve(driveIds_.size());
    
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::vector<int32_t> temps(driveIds_.size(), -1);
    NVMe_PollAllDrives(temps.data(), driveIds_.data(), 
                       static_cast<uint32_t>(driveIds_.size()));
    
    for (size_t i = 0; i < driveIds_.size(); ++i) {
        ThermalSample sample;
        sample.driveId = driveIds_[i];
        sample.temperature = temps[i];
        sample.timestampMs = static_cast<uint64_t>(ms);
        results.push_back(sample);
    }
    
    return results;
}

// ════════════════════════════════════════════════════════════════════════════════
// NVMeStressor Implementation
// ════════════════════════════════════════════════════════════════════════════════

IOResult NVMeStressor::stressWrite(uint32_t driveId, void* buffer, size_t size, uint64_t offset) {
    IOResult result{};
    
    // Get temperature before
    result.tempBefore = NVMe_GetTemperature(driveId);
    
    // Perform write
    uint32_t offsetLow = static_cast<uint32_t>(offset & 0xFFFFFFFF);
    uint32_t offsetHigh = static_cast<uint32_t>(offset >> 32);
    
    result.bytesTransferred = NVMe_StressWrite(driveId, buffer, size, offsetLow, offsetHigh);
    
    if (result.bytesTransferred == 0) {
        result.errorCode = NVMe_GetLastError();
    }
    
    // Get temperature after
    result.tempAfter = NVMe_GetTemperature(driveId);
    
    return result;
}

IOResult NVMeStressor::stressRead(uint32_t driveId, void* buffer, size_t size, uint64_t offset) {
    IOResult result{};
    
    result.tempBefore = NVMe_GetTemperature(driveId);
    
    uint32_t offsetLow = static_cast<uint32_t>(offset & 0xFFFFFFFF);
    uint32_t offsetHigh = static_cast<uint32_t>(offset >> 32);
    
    result.bytesTransferred = NVMe_StressRead(driveId, buffer, size, offsetLow, offsetHigh);
    
    if (result.bytesTransferred == 0) {
        result.errorCode = NVMe_GetLastError();
    }
    
    result.tempAfter = NVMe_GetTemperature(driveId);
    
    return result;
}

int32_t NVMeStressor::burstIO(uint32_t driveId, void* buffer, size_t totalBytes, bool write) {
    return NVMe_StressBurst(driveId, buffer, totalBytes, write ? 1 : 0);
}

// ════════════════════════════════════════════════════════════════════════════════
// SovereignThermalGovernor Implementation
// ════════════════════════════════════════════════════════════════════════════════

uint32_t SovereignThermalGovernor::selectDrive() {
    auto samples = monitor_.pollAll();
    
    if (samples.empty()) {
        return static_cast<uint32_t>(-1);
    }
    
    // Find coolest drive that's under the limit
    int32_t bestTemp = 32767;
    uint32_t bestDrive = samples[0].driveId;
    
    for (const auto& s : samples) {
        if (s.temperature >= 0 && s.temperature < bestTemp) {
            bestTemp = s.temperature;
            bestDrive = s.driveId;
        }
    }
    
    // If all drives are too hot, still return the coolest one
    // (caller should check isDriveCool() separately if they need to throttle)
    lastSelectedDrive_ = bestDrive;
    return bestDrive;
}

} // namespace sovereign
