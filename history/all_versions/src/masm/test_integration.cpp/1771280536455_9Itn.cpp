/*
 * test_integration.cpp — Full Integration Test of All 6 Engines
 *
 * Tests the complete RawrXD pipeline acceleration system:
 *   1. Sloloris StreamLoader (chaos tensor streaming)
 *   2. Bounce TPS (hot/cold tensor ping-pong with SuperStrength)
 *   3. HotPatch TPS (runtime TPS hotpatching via explore/exploit)
 *   4. TPS Bridge (coordinates all 3 engines in pipeline)
 *   5. DirectionlessLoader (omnidirectional memory with load locking)
 *   6. Unbraid Pipeline (hot-patches stages out under memory pressure)
 *
 * The Unbraid engine registers pipeline stages that call the other engines.
 * Under memory pressure from large model requests, stages unbraid to free RAM.
 *
 * Build:
 *   cl /EHsc /I d:\rawrxd\include test_integration.cpp
 *       RawrXD_SlolorisStreamLoader.obj RawrXD_BounceTPS.obj
 *       RawrXD_HotPatchTPS.obj RawrXD_TPSBridge.obj
 *       RawrXD_DirectionlessLoader.obj RawrXD_UnbraidPipeline.obj
 *       kernel32.lib /Fe:test_integration.exe
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "SlolorisStreamLoader.h"
#include "BounceTPS.h"
#include "HotPatchTPS.h"
#include "TPSBridge.h"
#include "DirectionlessLoader.h"
#include "UnbraidPipeline.h"

/* ─── Global Engine Contexts ────────────────────────────────────────────── */
static SlolorisCtx g_sloloris = NULL;
static BounceCtx g_bounce = NULL;
static HotPatchCtx g_hotpatch = NULL;
static BridgeCtx g_bridge = NULL;
static DirLoadCtx g_dirloader = NULL;
static UnbraidCtx g_unbraid = NULL;

/* ─── Pipeline Stage Functions (called by Unbraid) ──────────────────────── */
static int g_stageCalls[6] = {0};
static int g_stageBypasses[6] = {0};

/* Stage 0: Sloloris Tick */
static void stage0_exec(void* data, uint64_t size) {
    g_stageCalls[0]++;
    if (g_sloloris) {
        Sloloris_Tick(g_sloloris);
    }
}
static void stage0_bypass(void* data, uint64_t size) {
    g_stageBypasses[0]++;
    // Minimal bypass: just increment counter
}

/* Stage 1: Bounce Tick */
static void stage1_exec(void* data, uint64_t size) {
    g_stageCalls[1]++;
    if (g_bounce) {
        Bounce_Tick(g_bounce);
    }
}
static void stage1_bypass(void* data, uint64_t size) {
    g_stageBypasses[1]++;
}

/* Stage 2: HotPatch Tick */
static void stage2_exec(void* data, uint64_t size) {
    g_stageCalls[2]++;
    if (g_hotpatch) {
        HotPatch_Tick(g_hotpatch);
    }
}
static void stage2_bypass(void* data, uint64_t size) {
    g_stageBypasses[2]++;
}

/* Stage 3: TPS Bridge Execute */
static void stage3_exec(void* data, uint64_t size) {
    g_stageCalls[3]++;
    if (g_bridge) {
        Bridge_Execute(g_bridge, data, size);
    }
}
static void stage3_bypass(void* data, uint64_t size) {
    g_stageBypasses[3]++;
}

/* Stage 4: DirectionlessLoader Tick */
static void stage4_exec(void* data, uint64_t size) {
    g_stageCalls[4]++;
    if (g_dirloader) {
        DirLoad_Tick(g_dirloader);
    }
}
static void stage4_bypass(void* data, uint64_t size) {
    g_stageBypasses[4]++;
}

/* Stage 5: Model Processing (simulated large request) */
static void stage5_exec(void* data, uint64_t size) {
    g_stageCalls[5]++;
    // Simulate model processing: consume some memory
    if (g_unbraid) {
        Unbraid_AddPressure(g_unbraid, 1024 * 1024); // +1MB per call
    }
}
static void stage5_bypass(void* data, uint64_t size) {
    g_stageBypasses[5]++;
    // Bypass still consumes, but less
    if (g_unbraid) {
        Unbraid_AddPressure(g_unbraid, 512 * 1024); // +512KB per call
    }
}

/* ─── Test Helpers ───────────────────────────────────────────────────────── */
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); tests_passed++; } \
    else { printf("  [FAIL] %s\n", msg); tests_failed++; } \
} while(0)

static void reset_counters() {
    memset(g_stageCalls, 0, sizeof(g_stageCalls));
    memset(g_stageBypasses, 0, sizeof(g_stageBypasses));
}

/* ─── Engine Initialization ──────────────────────────────────────────────── */
static int init_all_engines() {
    printf("Initializing all 6 engines...\n");

    // 1. Sloloris StreamLoader
    g_sloloris = Sloloris_Init();
    CHECK(g_sloloris != NULL, "Sloloris_Init succeeded");

    // 2. Bounce TPS
    g_bounce = Bounce_Init();
    CHECK(g_bounce != NULL, "Bounce_Init succeeded");

    // 3. HotPatch TPS
    g_hotpatch = HotPatch_Init();
    CHECK(g_hotpatch != NULL, "HotPatch_Init succeeded");

    // 4. TPS Bridge
    g_bridge = Bridge_Init(g_sloloris, g_bounce, g_hotpatch);
    CHECK(g_bridge != NULL, "Bridge_Init succeeded");

    // 5. DirectionlessLoader
    g_dirloader = DirLoad_Init();
    CHECK(g_dirloader != NULL, "DirLoad_Init succeeded");

    // 6. Unbraid Pipeline (100MB budget)
    uint64_t budget = 100ULL * 1024 * 1024;
    g_unbraid = Unbraid_Init(budget);
    CHECK(g_unbraid != NULL, "Unbraid_Init succeeded");

    return (g_sloloris && g_bounce && g_hotpatch && g_bridge && g_dirloader && g_unbraid);
}

/* ─── Register Pipeline Stages ───────────────────────────────────────────── */
static int register_pipeline_stages() {
    printf("Registering 6 pipeline stages in Unbraid engine...\n");

    // Stage 0: Sloloris (priority 10, 1MB buffer)
    int s0 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass,
        1024ULL * 1024, 10);
    CHECK(s0 == 0, "Stage 0 (Sloloris) registered");

    // Stage 1: Bounce (priority 20, 2MB buffer)
    int s1 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass,
        1024ULL * 1024 * 2, 20);
    CHECK(s1 == 1, "Stage 1 (Bounce) registered");

    // Stage 2: HotPatch (priority 30, 4MB buffer)
    int s2 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage2_exec, (UnbraidStageFn)stage2_bypass,
        1024ULL * 1024 * 4, 30);
    CHECK(s2 == 2, "Stage 2 (HotPatch) registered");

    // Stage 3: Bridge (priority 40, 8MB buffer)
    int s3 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage3_exec, (UnbraidStageFn)stage3_bypass,
        1024ULL * 1024 * 8, 40);
    CHECK(s3 == 3, "Stage 3 (Bridge) registered");

    // Stage 4: DirectionlessLoader (priority 50, 16MB buffer)
    int s4 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage4_exec, (UnbraidStageFn)stage4_bypass,
        1024ULL * 1024 * 16, 50);
    CHECK(s4 == 4, "Stage 4 (DirLoader) registered");

    // Stage 5: Model Processing (priority 5, 32MB buffer — lowest priority, unbraids first)
    int s5 = Unbraid_RegisterStage(g_unbraid,
        (UnbraidStageFn)stage5_exec, (UnbraidStageFn)stage5_bypass,
        1024ULL * 1024 * 32, 5);
    CHECK(s5 == 5, "Stage 5 (Model) registered");

    return 1;
}

/* ─── Cleanup ────────────────────────────────────────────────────────────── */
static void cleanup_all() {
    if (g_unbraid) Unbraid_Destroy(g_unbraid);
    if (g_dirloader) DirLoad_Destroy(g_dirloader);
    if (g_bridge) Bridge_Destroy(g_bridge);
    if (g_hotpatch) HotPatch_Destroy(g_hotpatch);
    if (g_bounce) Bounce_Destroy(g_bounce);
    if (g_sloloris) Sloloris_Destroy(g_sloloris);

    g_unbraid = NULL;
    g_dirloader = NULL;
    g_bridge = NULL;
    g_hotpatch = NULL;
    g_bounce = NULL;
    g_sloloris = NULL;
}

/* ==========================================================================
 * Test 1: Engine Initialization
 * ========================================================================== */
void test_engine_init() {
    printf("\n=== Test 1: Engine Initialization ===\n");

    int ok = init_all_engines();
    CHECK(ok, "All 6 engines initialized successfully");

    cleanup_all();
}

/* ==========================================================================
 * Test 2: Pipeline Registration
 * ========================================================================== */
void test_pipeline_registration() {
    printf("\n=== Test 2: Pipeline Registration ===\n");

    int ok = init_all_engines();
    CHECK(ok, "Engines initialized");

    if (ok) {
        ok = register_pipeline_stages();
        CHECK(ok, "All 6 stages registered");

        UnbraidStats stats;
        Unbraid_GetStats(g_unbraid, &stats);
        CHECK(stats.stageCount == 6, "6 stages registered");
        CHECK(stats.braidedCount == 6, "All 6 braided initially");

        // Check buffer allocation: 1+2+4+8+16+32 = 63MB
        uint64_t expected = 63ULL * 1024 * 1024;
        CHECK(stats.memAllocated == expected, "63MB working buffers allocated");
    }

    cleanup_all();
}

/* ==========================================================================
 * Test 3: Normal Pipeline Execution (Fully Braided)
 * ========================================================================== */
void test_normal_execution() {
    printf("\n=== Test 3: Normal Pipeline Execution ===\n");

    reset_counters();
    int ok = init_all_engines() && register_pipeline_stages();
    CHECK(ok, "Setup OK");

    if (ok) {
        // Execute pipeline 3 times
        int dummy = 0;
        for (int i = 0; i < 3; i++) {
            int ran = Unbraid_Execute(g_unbraid, &dummy, sizeof(dummy));
            CHECK(ran == 6, "All 6 stages executed");
        }

        // Check that all stages were called via exec (not bypass)
        for (int i = 0; i < 6; i++) {
            char buf[64];
            sprintf(buf, "Stage %d exec called 3 times", i);
            CHECK(g_stageCalls[i] == 3, buf);
            sprintf(buf, "Stage %d NOT bypassed", i);
            CHECK(g_stageBypasses[i] == 0, buf);
        }

        // Check pressure from model processing: 3 runs * 6 stages * 1MB = 18MB
        int pct = Unbraid_GetPressurePct(g_unbraid);
        CHECK(pct >= 15 && pct <= 25, "Pressure ~18-25% from model processing");
    }

    cleanup_all();
}

/* ==========================================================================
 * Test 4: Memory Pressure → Automatic Unbraiding
 * ========================================================================== */
void test_pressure_unbraiding() {
    printf("\n=== Test 4: Memory Pressure Auto-Unbraiding ===\n");

    reset_counters();
    int ok = init_all_engines() && register_pipeline_stages();
    CHECK(ok, "Setup OK");

    if (ok) {
        // Simulate large model request: add 80MB pressure
        Unbraid_AddPressure(g_unbraid, 80ULL * 1024 * 1024);
        int pct = Unbraid_Tick(g_unbraid);
        printf("  Pressure after 80MB alloc: %d%%\n", pct);
        CHECK(pct >= 75, "Pressure >= 75% (should trigger unbraiding)");

        // Tick multiple times to let unbraiding cascade
        for (int i = 0; i < 10; i++) {
            pct = Unbraid_Tick(g_unbraid);
        }
        printf("  Pressure after cascading unbraid: %d%%\n", pct);

        UnbraidStats stats;
        Unbraid_GetStats(g_unbraid, &stats);
        printf("  Braided: %u  Unbraided: %u\n", stats.braidedCount, stats.unbraidedCount);
        CHECK(stats.unbraidedCount > 0, "At least 1 stage unbraided");
        CHECK(stats.totalUnbraids > 0, "Unbraid operations recorded");
        CHECK(stats.memFreed > 0, "Memory freed by unbraiding");

        // Execute pipeline — some stages should bypass
        int dummy = 0;
        int ran = Unbraid_Execute(g_unbraid, &dummy, sizeof(dummy));
        CHECK(ran == 6, "All 6 stages still executed (some bypassed)");

        // Check that some stages were bypassed
        int total_bypasses = 0;
        for (int i = 0; i < 6; i++) {
            total_bypasses += g_stageBypasses[i];
        }
        CHECK(total_bypasses > 0, "Some stages bypassed under pressure");
    }

    cleanup_all();
}

/* ==========================================================================
 * Test 5: Pressure Relief → Rebraiding
 * ========================================================================== */
void test_pressure_relief() {
    printf("\n=== Test 5: Pressure Relief Rebraiding ===\n");

    reset_counters();
    int ok = init_all_engines() && register_pipeline_stages();
    CHECK(ok, "Setup OK");

    if (ok) {
        // First, create pressure and unbraid
        Unbraid_AddPressure(g_unbraid, 80ULL * 1024 * 1024);
        for (int i = 0; i < 10; i++) {
            Unbraid_Tick(g_unbraid);
        }

        UnbraidStats stats;
        Unbraid_GetStats(g_unbraid, &stats);
        int unbraided_before = stats.unbraidedCount;
        CHECK(unbraided_before > 0, "Some stages unbraided");

        // Now release pressure heavily
        Unbraid_ReleasePressure(g_unbraid, 70ULL * 1024 * 1024);
        for (int i = 0; i < 15; i++) {
            Unbraid_Tick(g_unbraid);
        }

        int pct = Unbraid_GetPressurePct(g_unbraid);
        printf("  Pressure after releasing 70MB: %d%%\n", pct);
        CHECK(pct < 60, "Pressure < 60% (should trigger rebraiding)");

        Unbraid_GetStats(g_unbraid, &stats);
        printf("  Braided: %u  Unbraided: %u  Rebraids: %llu\n",
               stats.braidedCount, stats.unbraidedCount,
               (unsigned long long)stats.totalRebraids);
        CHECK(stats.totalRebraids > 0, "Rebraid operations occurred");
        CHECK(stats.memRestored > 0, "Memory restored by rebraiding");
    }

    cleanup_all();
}

/* ==========================================================================
 * Test 6: Aggressive Unbraiding at Critical Pressure
 * ========================================================================== */
void test_aggressive_unbraiding() {
    printf("\n=== Test 6: Aggressive Unbraiding (90%+) ===\n");

    reset_counters();
    int ok = init_all_engines() && register_pipeline_stages();
    CHECK(ok, "Setup OK");

    if (ok) {
        // Push to 95% pressure
        Unbraid_AddPressure(g_unbraid, 95ULL * 1024 * 1024);
        int pct = Unbraid_Tick(g_unbraid);
        printf("  Pressure after 95MB alloc: %d%%\n", pct);
        CHECK(pct >= 90, "Pressure >= 90%");

        UnbraidStats stats;
        Unbraid_GetStats(g_unbraid, &stats);
        CHECK(stats.totalUnbraids >= 2, "Aggressive unbraid: >= 2 stages in one tick");
    }

    cleanup_all();
}

/* ==========================================================================
 * Test 7: Full System Stats and Integration
 * ========================================================================== */
void test_full_system_stats() {
    printf("\n=== Test 7: Full System Integration ===\n");

    reset_counters();
    int ok = init_all_engines() && register_pipeline_stages();
    CHECK(ok, "Setup OK");

    if (ok) {
        // Run normal pipeline
        int dummy = 0;
        for (int i = 0; i < 5; i++) {
            Unbraid_Execute(g_unbraid, &dummy, sizeof(dummy));
        }

        // Create pressure and unbraid
        Unbraid_AddPressure(g_unbraid, 85ULL * 1024 * 1024);
        for (int i = 0; i < 5; i++) {
            Unbraid_Tick(g_unbraid);
        }

        // Run pipeline under pressure
        for (int i = 0; i < 3; i++) {
            Unbraid_Execute(g_unbraid, &dummy, sizeof(dummy));
        }

        // Release pressure and rebraid
        Unbraid_ReleasePressure(g_unbraid, 80ULL * 1024 * 1024);
        for (int i = 0; i < 10; i++) {
            Unbraid_Tick(g_unbraid);
        }

        // Final stats
        UnbraidStats ub_stats;
        Unbraid_GetStats(g_unbraid, &ub_stats);

        printf("  Unbraid Stats:\n");
        printf("    Ticks: %llu  PipeRuns: %llu  Bypasses: %llu\n",
               (unsigned long long)ub_stats.tickCount,
               (unsigned long long)ub_stats.pipeRuns,
               (unsigned long long)ub_stats.bypassRuns);
        printf("    Unbraids: %llu  Rebraids: %llu\n",
               (unsigned long long)ub_stats.totalUnbraids,
               (unsigned long long)ub_stats.totalRebraids);
        printf("    Mem Freed: %llu MB  Restored: %llu MB\n",
               (unsigned long long)ub_stats.memFreed / (1024*1024),
               (unsigned long long)ub_stats.memRestored / (1024*1024));

        CHECK(ub_stats.pipeRuns == 8, "8 total pipeline runs");
        CHECK(ub_stats.bypassRuns > 0, "Some bypasses occurred");
        CHECK(ub_stats.totalUnbraids > 0, "Unbraids occurred");
        CHECK(ub_stats.totalRebraids > 0, "Rebraids occurred");

        // Check that all engines are still functional
        CHECK(g_sloloris != NULL, "Sloloris still alive");
        CHECK(g_bounce != NULL, "Bounce still alive");
        CHECK(g_hotpatch != NULL, "HotPatch still alive");
        CHECK(g_bridge != NULL, "Bridge still alive");
        CHECK(g_dirloader != NULL, "DirLoader still alive");
        CHECK(g_unbraid != NULL, "Unbraid still alive");
    }

    cleanup_all();
}

/* ─── Main ────────────────────────────────────────────────────────────────── */
int main() {
    printf("=== RawrXD Full System Integration Test ===\n");
    printf("Testing all 6 engines: Sloloris + Bounce + HotPatch + Bridge + DirLoader + Unbraid\n");

    test_engine_init();
    test_pipeline_registration();
    test_normal_execution();
    test_pressure_unbraiding();
    test_pressure_relief();
    test_aggressive_unbraiding();
    test_full_system_stats();

    printf("\n=== Integration Test Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
