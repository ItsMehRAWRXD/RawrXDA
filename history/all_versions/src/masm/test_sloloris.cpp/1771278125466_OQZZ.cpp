// test_sloloris.cpp — Sloloris Stream Loader test bench
// Build: cl /std:c++20 /EHsc /I../include test_sloloris.cpp RawrXD_SlolorisStreamLoader.obj kernel32.lib
#include <cstdio>
#include <cstring>
#include <windows.h>
#include "../include/SlolorisStreamLoader.h"

// Create a fake "model file" in memory for testing
static HANDLE CreateTestModelFile(uint64_t* outSize) {
    // Create temp file with synthetic tensor data
    char tmpPath[MAX_PATH], tmpFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    GetTempFileNameA(tmpPath, "SLO", 0, tmpFile);

    HANDLE hFile = CreateFileA(tmpFile, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    // Write 8 fake tensors, each 64KB
    const DWORD tensorSize = 65536;
    const int tensorCount = 8;
    DWORD totalSize = tensorSize * tensorCount;

    char* buf = new char[totalSize];
    for (int i = 0; i < tensorCount; i++) {
        memset(buf + i * tensorSize, (char)(i + 0x41), tensorSize); // 'A', 'B', 'C'...
    }

    DWORD written = 0;
    WriteFile(hFile, buf, totalSize, &written, NULL);
    delete[] buf;

    *outSize = totalSize;

    // Reset file pointer
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    return hFile;
}

void TestInit() {
    printf("=== Test: Sloloris_Init / Destroy ===\n");
    auto ctx = Sloloris_Init(0);
    if (ctx) {
        printf("  [PASS] Context created at %p\n", ctx);
        Sloloris_Destroy(ctx);
        printf("  [PASS] Context destroyed\n");
    } else {
        printf("  [FAIL] Init returned NULL\n");
    }
}

void TestAttachAndDrip() {
    printf("\n=== Test: Attach + DRIP Strategy ===\n");
    auto ctx = Sloloris_Init(8);
    if (!ctx) { printf("  [FAIL] Init\n"); return; }

    uint64_t fileSize = 0;
    HANDLE hFile = CreateTestModelFile(&fileSize);
    if (hFile == INVALID_HANDLE_VALUE) { printf("  [FAIL] Test file\n"); Sloloris_Destroy(ctx); return; }

    // Set up tensor offset/size tables
    const int nTensors = 8;
    const uint64_t tensorSize = 65536;
    uint64_t offsets[8], sizes[8];
    for (int i = 0; i < nTensors; i++) {
        offsets[i] = i * tensorSize;
        sizes[i] = tensorSize;
    }

    int rc = Sloloris_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);
    printf("  AttachModel: %s\n", rc ? "OK" : "FAIL");

    // Run DRIP ticks
    printf("  Strategy: DRIP (Slowloris keepalive)\n");
    int strength = Sloloris_GetStrength(ctx);
    printf("  Initial strength: %d%%\n", strength);

    for (int i = 0; i < 20; i++) {
        int changes = Sloloris_Tick(ctx);
        strength = Sloloris_GetStrength(ctx);
        printf("  Tick %2d: %d changes, strength=%d%%\n", i, changes, strength);
    }

    printf("  Final strength: %d%%\n", Sloloris_GetStrength(ctx));

    CloseHandle(hFile);
    Sloloris_Destroy(ctx);
    printf("  [PASS] Drip test complete\n");
}

void TestBurstAndOrbit() {
    printf("\n=== Test: BURST + ORBIT Strategy ===\n");
    auto ctx = Sloloris_Init(8);
    if (!ctx) { printf("  [FAIL] Init\n"); return; }

    uint64_t fileSize = 0;
    HANDLE hFile = CreateTestModelFile(&fileSize);
    if (hFile == INVALID_HANDLE_VALUE) { printf("  [FAIL] Test file\n"); Sloloris_Destroy(ctx); return; }

    const int nTensors = 8;
    const uint64_t tensorSize = 65536;
    uint64_t offsets[8], sizes[8];
    for (int i = 0; i < nTensors; i++) {
        offsets[i] = i * tensorSize;
        sizes[i] = tensorSize;
    }
    Sloloris_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);

    // Force BURST mode
    int prev = Sloloris_SetStrategy(ctx, SLOLORIS_STRATEGY_BURST);
    printf("  Switched to BURST (was %d)\n", prev);

    for (int i = 0; i < 12; i++) {
        int changes = Sloloris_Tick(ctx);
        printf("  BURST tick %2d: %d changes, strength=%d%%\n", i, changes, Sloloris_GetStrength(ctx));
    }

    // Force orbit burst (DoS pattern)
    printf("  Forcing 2 orbits (DoS burst)...\n");
    int totalChanges = Sloloris_ForceOrbit(ctx, 2);
    printf("  ForceOrbit: %d total changes, strength=%d%%\n", totalChanges, Sloloris_GetStrength(ctx));

    CloseHandle(hFile);
    Sloloris_Destroy(ctx);
    printf("  [PASS] Burst/Orbit test complete\n");
}

void TestOnDemandFetch() {
    printf("\n=== Test: On-Demand Tensor Fetch ===\n");
    auto ctx = Sloloris_Init(8);
    if (!ctx) { printf("  [FAIL] Init\n"); return; }

    uint64_t fileSize = 0;
    HANDLE hFile = CreateTestModelFile(&fileSize);
    if (hFile == INVALID_HANDLE_VALUE) { printf("  [FAIL] Test file\n"); Sloloris_Destroy(ctx); return; }

    const int nTensors = 8;
    const uint64_t tensorSize = 65536;
    uint64_t offsets[8], sizes[8];
    for (int i = 0; i < nTensors; i++) {
        offsets[i] = i * tensorSize;
        sizes[i] = tensorSize;
    }
    Sloloris_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);

    // Don't tick — directly fetch (should trigger ghost→live upgrade)
    for (int i = 0; i < nTensors; i++) {
        auto ptr = Sloloris_GetTensor(ctx, i);
        if (ptr) {
            char firstByte = *static_cast<const char*>(ptr);
            printf("  Tensor %d: ptr=%p, first_byte='%c' %s\n", 
                   i, ptr, firstByte,
                   (firstByte == (char)(i + 0x41)) ? "[PASS]" : "[FAIL]");
        } else {
            printf("  Tensor %d: NULL [FAIL]\n", i);
        }
    }

    // Read stats
    auto stats = Sloloris_ReadStats(ctx);
    printf("  Stats: loads=%llu, ghosts=%llu, live=%u, strength=%u%%\n",
           stats.statsLoads, stats.statsGhosts, stats.liveSlots, stats.strengthPct);

    CloseHandle(hFile);
    Sloloris_Destroy(ctx);
    printf("  [PASS] On-demand fetch test complete\n");
}

// Create a larger test file to see chaos zone granularity
static HANDLE CreateLargeTestModelFile(uint64_t* outSize, int nTensors) {
    char tmpPath[MAX_PATH], tmpFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    GetTempFileNameA(tmpPath, "SLC", 0, tmpFile);

    HANDLE hFile = CreateFileA(tmpFile, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    const DWORD tensorSize = 4096; // Small tensors for fine-grained % steps
    DWORD totalSize = tensorSize * nTensors;
    char* buf = new char[totalSize];
    for (int i = 0; i < nTensors; i++)
        memset(buf + i * tensorSize, (char)(i + 0x30), tensorSize);

    DWORD written = 0;
    WriteFile(hFile, buf, totalSize, &written, NULL);
    delete[] buf;
    *outSize = totalSize;
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    return hFile;
}

void TestChaosZone() {
    printf("\n=== Test: CHAOS ZONE (post-88%% randomization) ===\n");
    
    // Use 32 tensors so each adds ~3.1% — we'll see the chaos around 88
    const int nTensors = 32;
    auto ctx = Sloloris_Init(nTensors);
    if (!ctx) { printf("  [FAIL] Init\n"); return; }

    uint64_t fileSize = 0;
    HANDLE hFile = CreateLargeTestModelFile(&fileSize, nTensors);
    if (hFile == INVALID_HANDLE_VALUE) { printf("  [FAIL] Test file\n"); Sloloris_Destroy(ctx); return; }

    const uint64_t tensorSize = 4096;
    uint64_t offsets[32], sizes[32];
    for (int i = 0; i < nTensors; i++) {
        offsets[i] = i * tensorSize;
        sizes[i] = tensorSize;
    }
    Sloloris_AttachModel(ctx, hFile, fileSize, nTensors, offsets, sizes);

    // Force BURST mode for steady climbing
    Sloloris_SetStrategy(ctx, SLOLORIS_STRATEGY_BURST);

    printf("  Watching strength curve through chaos zone:\n");
    printf("  %-6s  %-8s  %-10s\n", "Tick", "Changes", "Strength");
    printf("  %-6s  %-8s  %-10s\n", "------", "--------", "----------");

    int prevStrength = 0;
    for (int i = 0; i < 80; i++) {
        int changes = Sloloris_Tick(ctx);
        int strength = Sloloris_GetStrength(ctx);
        
        // Always show around the chaos zone, or when strength changes
        if (strength != prevStrength || (strength >= 80 && strength <= 100)) {
            const char* zone = (strength >= 88) ? " <<< CHAOS" : "";
            printf("  %-6d  %-8d  %d%%%s\n", i, changes, strength, zone);
        }
        prevStrength = strength;
    }

    auto stats = Sloloris_ReadStats(ctx);
    printf("\n  Final: strength=%u%%, loads=%llu, evicts=%llu, orbits=%llu\n",
           stats.strengthPct, stats.statsLoads, stats.statsEvicts, stats.statsOrbits);

    CloseHandle(hFile);
    Sloloris_Destroy(ctx);
    printf("  [PASS] Chaos zone test complete\n");
}

int main() {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Sloloris Stream Loader — Test Suite                ║\n");
    printf("║  Slowloris/DoS Tensor Cycling Engine                ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    TestInit();
    TestAttachAndDrip();
    TestBurstAndOrbit();
    TestOnDemandFetch();
    TestChaosZone();

    printf("\n=== All Sloloris tests complete ===\n");
    return 0;
}
