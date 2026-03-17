/**
 * @file test_thermal_tools.cpp
 * @brief Test program for thermal tools integration
 */

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

#include "logging/logger.h"
static Logger s_logger("test_thermal_tools");

// MMF structures (matching the service)
constexpr uint32_t kSidecarSignature = 0x534F5645;
constexpr uint32_t kMaxDrives = 16;

#pragma pack(push, 1)
struct SovereignThermalMMF {
    uint32_t signature;
    uint32_t version;
    uint32_t driveCount;
    uint32_t reserved;
    int32_t temps[kMaxDrives];
    int32_t wear[kMaxDrives];
    uint64_t timestampMs;
};
#pragma pack(pop)

int main() {
    s_logger.info("╔════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║        SOVEREIGN THERMAL TOOLS - INTEGRATION TEST          ║\n");
    s_logger.info("╚════════════════════════════════════════════════════════════╝\n\n");

    // Try to open the MMF
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
    if (!mapping) {
        mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\SOVEREIGN_NVME_TEMPS");
        if (!mapping) {
            s_logger.info("[ERROR] NVMe Oracle service not running!\n");
            s_logger.info("        Run: sc.exe start SovereignNVMeOracle\n");
            return 1;
        }
        s_logger.info("[INFO] Using Local namespace MMF\n");
    } else {
        s_logger.info("[INFO] Using Global namespace MMF\n");
    }

    auto* view = static_cast<SovereignThermalMMF*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SovereignThermalMMF))
    );
    
    if (!view) {
        s_logger.info("[ERROR] Failed to map view\n");
        CloseHandle(mapping);
        return 1;
    }

    // Validate signature
    if (view->signature != kSidecarSignature) {
        s_logger.info("[ERROR] Invalid signature: 0x");
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        return 1;
    }

    s_logger.info("[OK] Signature valid: 0x");
    s_logger.info("[OK] Version: ");
    s_logger.info("[OK] Drive count: ");
    s_logger.info("[OK] Last update: ");

    // Display drive rankings
    s_logger.info("╔════════════════════════════════════════════════════════════╗\n");
    s_logger.info("║                    DRIVE RANKING                           ║\n");
    s_logger.info("╠════════╦══════════╦══════════╦══════════╦═════════════════╣\n");
    s_logger.info("║ Drive  ║ Temp (C) ║ Wear (%) ║ Score    ║ Status          ║\n");
    s_logger.info("╠════════╬══════════╬══════════╬══════════╬═════════════════╣\n");

    struct DriveInfo {
        int id;
        int temp;
        int wear;
        double score;
        std::string status;
    };
    std::vector<DriveInfo> drives;

    for (uint32_t i = 0; i < view->driveCount && i < kMaxDrives; i++) {
        DriveInfo d;
        d.id = i;
        d.temp = view->temps[i];
        d.wear = view->wear[i];
        
        // Calculate score and status
        if (d.temp < -1) {
            d.status = "BLACKLISTED[invalid]";
            d.score = INFINITY;
        } else if (d.temp >= 70) {
            d.status = "BLACKLISTED[overheat]";
            d.score = INFINITY;
        } else if (d.wear >= 0 && d.wear > 95) {
            d.status = "BLACKLISTED[wear]";
            d.score = INFINITY;
        } else {
            double effectiveTemp = (d.temp >= 0) ? d.temp : 50.0;
            double effectiveWear = (d.wear >= 0) ? d.wear : 50.0;
            d.score = effectiveTemp * 0.7 + effectiveWear * 0.3;
            d.status = "AVAILABLE";
        }
        
        drives.push_back(d);
    }

    // Sort by score
    std::sort(drives.begin(), drives.end(), 
        [](const DriveInfo& a, const DriveInfo& b) { return a.score < b.score; });

    for (const auto& d : drives) {
        s_logger.info("║ ");
    }

    s_logger.info("╚════════╩══════════╩══════════╩══════════╩═════════════════╝\n\n");

    // Find best drive
    int bestDrive = -1;
    int coolestTemp = 999;
    for (const auto& d : drives) {
        if (d.status == "AVAILABLE" && d.temp >= 0 && d.temp < coolestTemp) {
            coolestTemp = d.temp;
            bestDrive = d.id;
        }
    }

    if (bestDrive >= 0) {
        s_logger.info("[RESULT] Best drive for I/O: Drive ");
        s_logger.info("         Path: \\\\.\\PhysicalDrive");
    } else {
        s_logger.info("[WARNING] No valid drives available!\n");
    }

    // Thermal headroom check
    int maxValidTemp = -999;
    for (const auto& d : drives) {
        if (d.status == "AVAILABLE" && d.temp >= 0 && d.temp > maxValidTemp) {
            maxValidTemp = d.temp;
        }
    }

    s_logger.info("\n[THERMAL CHECK]\n");
    s_logger.info("  Hottest valid drive: ");
    s_logger.info("  Throttle threshold:  65C\n");
    s_logger.info("  Headroom:            ");
    s_logger.info("  Recommendation:      ");

    UnmapViewOfFile(view);
    CloseHandle(mapping);

    s_logger.info("\n[SUCCESS] Thermal tools integration test passed!\n");
    return 0;
}
