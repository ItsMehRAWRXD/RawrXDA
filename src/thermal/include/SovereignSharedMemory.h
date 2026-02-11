#pragma once
#include <windows.h>
#include <cstdint>

// MMF Names
#define MMF_NAME_TEMPS      "Global\\SOVEREIGN_NVME_TEMPS"
#define MMF_NAME_GOVERNOR   "Global\\SOVEREIGN_GOVERNOR_STATUS"

// Constants
#define MAX_DRIVES          5
#define SIGNATURE_SOVE      0x534F5645 // "SOVE"

// ----------------------------------------------------------------
// 1. Telemetry Layout (Produced by MASM Service)
// ----------------------------------------------------------------
#pragma pack(push, 1)
struct SovereignTelemetryHeader {
    uint32_t signature;     // 0x00
    uint32_t version;       // 0x04
    uint32_t count;         // 0x08
    uint32_t reserved;      // 0x0C
    int32_t  temps[MAX_DRIVES]; // 0x10 - Temperature in Celsius
    int32_t  wear[MAX_DRIVES];  // 0x50 - Wear Level (Percentage or Raw)
    uint64_t timestamp;     // 0x90 - GetTickCount64
};
// Size check: 16 + (5*4) + (5*4) + 8 = 16 + 20 + 20 + 8 = 64 bytes... 
// Wait, the MASM code used offsets: OFF_TIMESTAMP = 144 (0x90).
// Let's re-verify the MASM layout:
// OFF_SIGNATURE=0, OFF_VERSION=4, OFF_COUNT=8, OFF_RESERVED=12
// OFF_TEMPS=16. (16 + 5*4 = 36).
// OFF_WEAR=80. (Padding here? 80 - 36 = 44 bytes gap).
// OFF_TIMESTAMP=144. (144 - (80 + 20) = 44 bytes gap).
// Total Size 152.
// We must match the MASM padding exactly.

struct SovereignTelemetryMMF {
    uint32_t signature;         // 0
    uint32_t version;           // 4
    uint32_t count;             // 8
    uint32_t reserved;          // 12
    int32_t  temps[16];         // 16 (Allocating 16 slots to match offset 80 for wear)
                                // 16 + 16*4 = 80. Perfect.
    int32_t  wear[16];          // 80
                                // 80 + 16*4 = 144. Perfect.
    uint64_t timestamp;         // 144
};
#pragma pack(pop)

// ----------------------------------------------------------------
// 2. Governor Status Layout (Produced by Governor, Consumed by Harness)
// ----------------------------------------------------------------
struct SovereignGovernorStatus {
    uint32_t allowedMask;       // Bitmask of drives allowed to run (1=Run, 0=Stop)
    uint32_t throttledMask;     // Bitmask of drives currently throttled (for UI)
    int32_t  maxTempSeen;       // Highest temp in current session
    uint32_t state;             // 0=Normal, 1=Warning, 2=Critical
    uint64_t lastUpdate;
};
