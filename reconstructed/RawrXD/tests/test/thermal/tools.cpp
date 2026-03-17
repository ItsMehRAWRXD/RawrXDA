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
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        SOVEREIGN THERMAL TOOLS - INTEGRATION TEST          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Try to open the MMF
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
    if (!mapping) {
        mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\SOVEREIGN_NVME_TEMPS");
        if (!mapping) {
            std::cout << "[ERROR] NVMe Oracle service not running!\n";
            std::cout << "        Run: sc.exe start SovereignNVMeOracle\n";
            return 1;
        }
        std::cout << "[INFO] Using Local namespace MMF\n";
    } else {
        std::cout << "[INFO] Using Global namespace MMF\n";
    }

    auto* view = static_cast<SovereignThermalMMF*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SovereignThermalMMF))
    );
    
    if (!view) {
        std::cout << "[ERROR] Failed to map view\n";
        CloseHandle(mapping);
        return 1;
    }

    // Validate signature
    if (view->signature != kSidecarSignature) {
        std::cout << "[ERROR] Invalid signature: 0x" << std::hex << view->signature << std::dec << "\n";
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        return 1;
    }

    std::cout << "[OK] Signature valid: 0x" << std::hex << view->signature << std::dec << "\n";
    std::cout << "[OK] Version: " << view->version << "\n";
    std::cout << "[OK] Drive count: " << view->driveCount << "\n";
    std::cout << "[OK] Last update: " << view->timestampMs << " ms\n\n";

    // Display drive rankings
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    DRIVE RANKING                           ║\n";
    std::cout << "╠════════╦══════════╦══════════╦══════════╦═════════════════╣\n";
    std::cout << "║ Drive  ║ Temp (C) ║ Wear (%) ║ Score    ║ Status          ║\n";
    std::cout << "╠════════╬══════════╬══════════╬══════════╬═════════════════╣\n";

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
        std::cout << "║ " << std::setw(6) << d.id << " ║ "
                  << std::setw(8) << d.temp << " ║ "
                  << std::setw(8) << d.wear << " ║ "
                  << std::setw(8) << (std::isinf(d.score) ? -1.0 : d.score) << " ║ "
                  << std::setw(15) << d.status << " ║\n";
    }

    std::cout << "╚════════╩══════════╩══════════╩══════════╩═════════════════╝\n\n";

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
        std::cout << "[RESULT] Best drive for I/O: Drive " << bestDrive << " @ " << coolestTemp << "C\n";
        std::cout << "         Path: \\\\.\\PhysicalDrive" << bestDrive << "\n";
    } else {
        std::cout << "[WARNING] No valid drives available!\n";
    }

    // Thermal headroom check
    int maxValidTemp = -999;
    for (const auto& d : drives) {
        if (d.status == "AVAILABLE" && d.temp >= 0 && d.temp > maxValidTemp) {
            maxValidTemp = d.temp;
        }
    }

    std::cout << "\n[THERMAL CHECK]\n";
    std::cout << "  Hottest valid drive: " << maxValidTemp << "C\n";
    std::cout << "  Throttle threshold:  65C\n";
    std::cout << "  Headroom:            " << (65 - maxValidTemp) << "C\n";
    std::cout << "  Recommendation:      " << (maxValidTemp < 65 ? "PROCEED with heavy I/O" : "THROTTLE or wait") << "\n";

    UnmapViewOfFile(view);
    CloseHandle(mapping);

    std::cout << "\n[SUCCESS] Thermal tools integration test passed!\n";
    return 0;
}
