// test_bridge.cpp — Combined 3-Engine TPS Bridge Test
//
// Loads a synthetic model, initializes Sloloris + Bounce + HotPatch,
// connects them through the TPSBridge, runs ticks, generates tokens,
// and reports composite strength (targeting 300%+).
//
// Build:
//   ml64 /c RawrXD_SlolorisStreamLoader.asm
//   ml64 /c RawrXD_BounceTPS.asm
//   ml64 /c RawrXD_HotPatchTPS.asm
//   ml64 /c RawrXD_TPSBridge.asm
//   cl /std:c++20 /EHsc /I../../include test_bridge.cpp ^
//      RawrXD_SlolorisStreamLoader.obj RawrXD_BounceTPS.obj ^
//      RawrXD_HotPatchTPS.obj RawrXD_TPSBridge.obj kernel32.lib

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include "../../include/SlolorisStreamLoader.h"
#include "../../include/BounceTPS.h"
#include "../../include/HotPatchTPS.h"
#include "../../include/TPSBridge.h"

// ─── Create synthetic model file ────────────────────────────────────────────
static constexpr int    NUM_TENSORS  = 16;
static constexpr DWORD  TENSOR_SIZE  = 65536;   // 64KB each = 1MB total

static HANDLE CreateTestModel(uint64_t* outSize,
                               uint64_t offsets[NUM_TENSORS],
                               uint64_t sizes[NUM_TENSORS]) {
    char tmpPath[MAX_PATH], tmpFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    GetTempFileNameA(tmpPath, "BRG", 0, tmpFile);

    HANDLE hFile = CreateFileA(tmpFile,
        GENERIC_READ | GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    DWORD totalSize = TENSOR_SIZE * NUM_TENSORS;
    char* buf = new char[totalSize];
    for (int i = 0; i < NUM_TENSORS; i++) {
        memset(buf + i * TENSOR_SIZE, (char)(i + 0x41), TENSOR_SIZE);
        offsets[i] = (uint64_t)i * TENSOR_SIZE;
        sizes[i]   = TENSOR_SIZE;
    }

    DWORD written = 0;
    WriteFile(hFile, buf, totalSize, &written, NULL);
    delete[] buf;

    *outSize = totalSize;
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    return hFile;
}

// ─── Status string ──────────────────────────────────────────────────────────
static const char* StatusString(int code) {
    switch (code) {
        case BRIDGE_COLD:         return "COLD";
        case BRIDGE_WARM:         return "WARM";
        case BRIDGE_HOT:          return "HOT";
        case BRIDGE_SUPERCHARGED: return "*** SUPERCHARGED ***";
        case BRIDGE_LEGENDARY:    return "$$$ LEGENDARY $$$";
        default:                  return "???";
    }
}

// ─── Main Test ──────────────────────────────────────────────────────────────
int main() {
    printf("========================================================\n");
    printf("  RawrXD 3-Engine TPS Bridge Test\n");
    printf("  Sloloris + Bounce + HotPatch → Bridge\n");
    printf("  Target: 300%%+ Composite Strength\n");
    printf("========================================================\n\n");

    // ── Create model file ──
    uint64_t modelSize = 0;
    uint64_t offsets[NUM_TENSORS];
    uint64_t sizes[NUM_TENSORS];
    HANDLE hFile = CreateTestModel(&modelSize, offsets, sizes);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[FAIL] Could not create test model\n");
        return 1;
    }
    printf("[OK] Test model: %u tensors, %llu bytes\n\n",
           NUM_TENSORS, (unsigned long long)modelSize);

    // ── Init Sloloris ──
    printf("--- Initializing Sloloris ---\n");
    auto sloCtx = Sloloris_Init(0);
    if (!sloCtx) { printf("[FAIL] Sloloris_Init\n"); return 1; }
    printf("[OK] Sloloris context: %p\n", sloCtx);

    // Attach model to Sloloris
    int ret = Sloloris_AttachModel(sloCtx, hFile, modelSize,
                                    NUM_TENSORS, offsets, sizes);
    printf("[OK] Sloloris_AttachModel: %d\n\n", ret);

    // ── Init Bounce ──
    printf("--- Initializing Bounce ---\n");
    auto bncCtx = Bounce_Init(0);
    if (!bncCtx) { printf("[FAIL] Bounce_Init\n"); return 1; }
    printf("[OK] Bounce context: %p\n", bncCtx);

    // Attach model to Bounce (reopen file handle for Bounce)
    // Bounce uses same file — create second handle
    char tmpPath2[MAX_PATH], tmpFile2[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath2);
    GetTempFileNameA(tmpPath2, "BN2", 0, tmpFile2);
    HANDLE hFile2 = CreateFileA(tmpFile2,
        GENERIC_READ | GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile2 != INVALID_HANDLE_VALUE) {
        DWORD totalSize2 = TENSOR_SIZE * NUM_TENSORS;
        char* buf2 = new char[totalSize2];
        for (int i = 0; i < NUM_TENSORS; i++)
            memset(buf2 + i * TENSOR_SIZE, (char)(i + 0x41), TENSOR_SIZE);
        DWORD written2 = 0;
        WriteFile(hFile2, buf2, totalSize2, &written2, NULL);
        delete[] buf2;
        SetFilePointer(hFile2, 0, NULL, FILE_BEGIN);

        ret = Bounce_AttachModel(bncCtx, hFile2, modelSize,
                                  NUM_TENSORS, offsets, sizes);
        printf("[OK] Bounce_AttachModel: %d\n", ret);
    }

    // Dynamic config
    Bounce_SetTargetTPS(bncCtx, 85);
    Bounce_SetPoolRatio(bncCtx, 80, 20);
    Bounce_SetPrefetchDepth(bncCtx, 8);
    printf("[OK] Bounce configured: target=85 TPS, 80/20 ratio, prefetch=8\n\n");

    // ── Init HotPatch ──
    printf("--- Initializing HotPatch ---\n");
    auto hpCtx = HotPatch_Init(0);
    if (!hpCtx) { printf("[FAIL] HotPatch_Init\n"); return 1; }
    printf("[OK] HotPatch context: %p\n", hpCtx);

    // Set aggressive patch params
    HotPatch_SetParam(hpCtx, HP_PATCH_BATCH_SIZE, 8);
    HotPatch_SetParam(hpCtx, HP_PATCH_PREFETCH_DEPTH, 16);
    HotPatch_SetParam(hpCtx, HP_PATCH_SPEC_DECODE, 4);
    HotPatch_SetParam(hpCtx, HP_PATCH_RING_AGGRESSION, 8);
    printf("[OK] HotPatch configured: batch=8, prefetch=16, spec=4, ring=8\n\n");

    // ── Init Bridge ──
    printf("--- Initializing Bridge ---\n");
    auto brCtx = Bridge_Init(sloCtx, bncCtx, hpCtx);
    if (!brCtx) { printf("[FAIL] Bridge_Init\n"); return 1; }
    printf("[OK] Bridge context: %p\n\n", brCtx);

    // ══════════════════════════════════════════════════════════════════════
    //  SIMULATION LOOP
    //  Simulate model inference: tick engines, generate tokens, measure TPS
    // ══════════════════════════════════════════════════════════════════════
    printf("========================================================\n");
    printf("  Starting simulation: 200 ticks with token generation\n");
    printf("========================================================\n\n");

    int peakStr = 0;
    int superchargedCount = 0;

    for (int tick = 1; tick <= 200; tick++) {
        // Tick the bridge (ticks all 3 engines internally)
        int strength = Bridge_Tick(brCtx);

        // Simulate token generation (batch of 4 tokens per tick)
        for (int t = 0; t < 4; t++) {
            Bridge_NotifyToken(brCtx);
            Bounce_NotifyTokenGen(bncCtx);
        }

        // Also read tensors to drive hot/cold activity
        if (tick % 5 == 0) {
            for (int i = 0; i < NUM_TENSORS && i < 4; i++) {
                Sloloris_GetTensor(sloCtx, i);
                Bounce_GetTensor(bncCtx, i);
            }
        }

        // Periodic status
        if (tick % 25 == 0) {
            int status = Bridge_GetStatus(brCtx);
            int tps    = Bridge_GetTPS(brCtx);
            uint32_t bncSuper = Bounce_GetSuperStrength(bncCtx);
            uint32_t bncDir   = Bounce_GetRotateDir(bncCtx);

            printf("\n--- Tick %d Summary ---\n", tick);
            printf("  Composite Strength: %d%%\n", strength);
            printf("  Bounce SuperStr:    %d%%\n", bncSuper);
            printf("  Bounce RotateDir:   %u\n", bncDir);
            printf("  Bridge TPS:         %d.%02d tok/sec\n", tps / 100, tps % 100);
            printf("  Status:             %s\n", StatusString(status));

            if (strength > peakStr) peakStr = strength;
            if (status >= BRIDGE_SUPERCHARGED) superchargedCount++;
        }
    }

    printf("\n========================================================\n");
    printf("  SIMULATION COMPLETE\n");
    printf("========================================================\n\n");

    // ── Final Stats ──
    BridgeStats brStats = {};
    Bridge_GetStats(brCtx, &brStats);
    printf("Bridge Stats:\n");
    printf("  Ticks:            %llu\n", (unsigned long long)brStats.tickCount);
    printf("  Tokens:           %llu\n", (unsigned long long)brStats.tokenCount);
    printf("  Composite:        %u%%\n", brStats.compositeStr);
    printf("  Peak Strength:    %u%%\n", brStats.peakStrength);
    printf("  Sloloris Str:     %u%%\n", brStats.sloStr);
    printf("  Bounce SuperStr:  %u%%\n", brStats.bncSuperStr);
    printf("  HotPatch Mult:    %u%%\n", brStats.hpMultiplier);
    printf("  Status:           %s\n", StatusString(brStats.statusCode));
    printf("  Bridge TPS:       %u.%02u tok/sec\n",
           brStats.measuredTPS / 100, brStats.measuredTPS % 100);
    printf("  Peak TPS:         %u.%02u tok/sec\n\n",
           brStats.peakTPS / 100, brStats.peakTPS % 100);

    HotPatchStats hpStats = {};
    HotPatch_GetStats(hpCtx, &hpStats);
    printf("HotPatch Stats:\n");
    printf("  Composite Mult:   %u%%\n", hpStats.compositeMult);
    printf("  Best Composite:   %u%%\n", hpStats.bestComposite);
    printf("  Tune Steps:       %llu\n", (unsigned long long)hpStats.tuneSteps);
    printf("  Explores:         %llu\n", (unsigned long long)hpStats.explores);
    printf("  Exploits:         %llu\n", (unsigned long long)hpStats.exploits);
    printf("  Improved:         %llu\n", (unsigned long long)hpStats.improved);
    printf("  Regressed:        %llu\n", (unsigned long long)hpStats.regressed);
    printf("  Patch Values:     [%u, %u, %u, %u, %u, %u, %u, %u]\n",
           hpStats.patchValues[0], hpStats.patchValues[1],
           hpStats.patchValues[2], hpStats.patchValues[3],
           hpStats.patchValues[4], hpStats.patchValues[5],
           hpStats.patchValues[6], hpStats.patchValues[7]);
    printf("  Patch Bonus:      %u\n", hpStats.patchBonus);
    printf("  Feedback Bonus:   %u\n\n", hpStats.feedbackBonus);

    // Check 300% target
    printf("========================================================\n");
    if (brStats.peakStrength >= 300) {
        printf("  >>> TARGET HIT: Peak %u%% >= 300%% <<<\n", brStats.peakStrength);
    } else {
        printf("  Peak %u%% — need more tuning for 300%%\n", brStats.peakStrength);
        printf("  (Sloloris saturation + Bounce super + HotPatch tune)\n");
    }
    printf("  Supercharged ticks: %d / 8 checkpoints\n", superchargedCount);
    printf("========================================================\n");

    // ── Cleanup ──
    Bridge_Destroy(brCtx);
    HotPatch_Destroy(hpCtx);
    Bounce_Destroy(bncCtx);
    Sloloris_Destroy(sloCtx);
    CloseHandle(hFile);
    if (hFile2 != INVALID_HANDLE_VALUE) CloseHandle(hFile2);

    printf("\n[DONE] All engines cleaned up.\n");
    return 0;
}
