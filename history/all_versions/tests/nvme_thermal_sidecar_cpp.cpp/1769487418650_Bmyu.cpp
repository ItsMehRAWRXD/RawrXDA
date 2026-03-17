// nvme_thermal_sidecar_cpp.cpp - C++ Version of Sovereign NVMe Thermal Sidecar
// This version uses Local\ namespace which works without elevation
// Build: cmake --build ... --target nvme_thermal_sidecar_cpp

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Shared memory layout (must match reader)
#pragma pack(push, 1)
struct SovereignThermalData {
    uint32_t signature;     // "SOVE" = 0x45564F53
    uint32_t version;       // 1
    uint32_t driveCount;
    uint32_t reserved;
    int32_t  temps[16];     // Temperatures in Celsius
    int32_t  wear[16];      // Wear level percentage
    uint64_t timestampMs;
};
#pragma pack(pop)

static_assert(sizeof(SovereignThermalData) == 152, "Layout mismatch");

// Forward declarations for NVMe functions from nvme_thermal_stressor.asm
extern "C" {
    int NVMe_GetTemperature(int driveId);
    int NVMe_GetWearLevel(int driveId);
}

// Configuration
constexpr const char* kMapName = "Local\\SOVEREIGN_NVME_TEMPS";
constexpr uint32_t kSignature = 0x45564F53; // "SOVE"
constexpr uint32_t kVersion = 1;
constexpr int kDriveIds[] = {0, 1, 2, 4, 5};
constexpr int kDriveCount = sizeof(kDriveIds) / sizeof(kDriveIds[0]);
constexpr DWORD kPollIntervalMs = 500;

int main() {
    printf("[Sidecar] Starting Sovereign NVMe Thermal Sidecar (C++ Version)\n");
    printf("[Sidecar] Map Name: %s\n", kMapName);
    
    // Create the memory mapped file
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // Pagefile-backed
        NULL,                   // Default security
        PAGE_READWRITE,         // Read/Write access
        0,                      // Size high
        sizeof(SovereignThermalData),  // Size low
        kMapName                // Name
    );
    
    if (!hMapFile) {
        DWORD err = GetLastError();
        printf("[Sidecar] FATAL: CreateFileMappingA failed with error %lu\n", err);
        return 1;
    }
    
    printf("[Sidecar] MMF created successfully. Handle = 0x%p\n", hMapFile);
    
    // Map a view
    SovereignThermalData* pData = reinterpret_cast<SovereignThermalData*>(
        MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, sizeof(SovereignThermalData))
    );
    
    if (!pData) {
        DWORD err = GetLastError();
        printf("[Sidecar] FATAL: MapViewOfFile failed with error %lu\n", err);
        CloseHandle(hMapFile);
        return 1;
    }
    
    printf("[Sidecar] View mapped at 0x%p\n", pData);
    
    // Initialize header
    pData->signature = kSignature;
    pData->version = kVersion;
    pData->driveCount = kDriveCount;
    pData->reserved = 0;
    
    printf("[Sidecar] Entering main loop. Polling %d drives every %lu ms\n", kDriveCount, kPollIntervalMs);
    
    // Main loop - poll temperatures forever
    while (true) {
        // Poll each drive
        for (int i = 0; i < kDriveCount; i++) {
            int driveId = kDriveIds[i];
            
            // Get temperature from NVMe IOCTL (or neutral value if unavailable)
            int temp = NVMe_GetTemperature(driveId);
            int wear = NVMe_GetWearLevel(driveId);
            
            pData->temps[i] = temp;
            pData->wear[i] = wear;
        }
        
        // Update timestamp
        pData->timestampMs = GetTickCount64();
        
        // Sleep before next poll
        Sleep(kPollIntervalMs);
    }
    
    // Cleanup (never reached in normal operation)
    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);
    return 0;
}
