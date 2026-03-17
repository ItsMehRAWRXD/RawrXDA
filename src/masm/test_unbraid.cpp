/*
 * test_unbraid.cpp — Unbraid Pipeline Engine Test Harness
 *
 * Simulates a multi-stage pipeline under rising/falling memory pressure.
 * Verifies that stages unbraid (bypass) when model demands memory,
 * and rebraid (restore) when pressure drops.
 *
 * Build:
 *   cl /EHsc /I d:\rawrxd\include test_unbraid.cpp RawrXD_UnbraidPipeline.obj
 *       kernel32.lib /Fe:test_unbraid.exe
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "UnbraidPipeline.h"

/* ─── Fake pipeline stage functions ────────────────────────────────────── */
static int g_execCounts[8] = {0};
static int g_bypassCounts[8] = {0};

static void stage0_exec(void* d, uint64_t s) { g_execCounts[0]++; }
static void stage0_bypass(void* d, uint64_t s) { g_bypassCounts[0]++; }
static void stage1_exec(void* d, uint64_t s) { g_execCounts[1]++; }
static void stage1_bypass(void* d, uint64_t s) { g_bypassCounts[1]++; }
static void stage2_exec(void* d, uint64_t s) { g_execCounts[2]++; }
static void stage2_bypass(void* d, uint64_t s) { g_bypassCounts[2]++; }
static void stage3_exec(void* d, uint64_t s) { g_execCounts[3]++; }
static void stage3_bypass(void* d, uint64_t s) { g_bypassCounts[3]++; }
static void stage4_exec(void* d, uint64_t s) { g_execCounts[4]++; }
static void stage4_bypass(void* d, uint64_t s) { g_bypassCounts[4]++; }

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); tests_passed++; } \
    else { printf("  [FAIL] %s\n", msg); tests_failed++; } \
} while(0)

/* ==========================================================================
 * Test 1: Init + destroy
 * ========================================================================== */
void test_init_destroy() {
    printf("\n=== Test 1: Init/Destroy ===\n");

    uint64_t budget = 1024ULL * 1024 * 100;  /* 100 MB */
    UnbraidCtx ctx = Unbraid_Init(budget);
    CHECK(ctx != NULL, "Init returned non-null context");

    int pct = Unbraid_GetPressurePct(ctx);
    CHECK(pct == 0, "Initial pressure is 0%");

    UnbraidStats stats;
    memset(&stats, 0, sizeof(stats));
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.memBudget == budget, "Budget matches");
    CHECK(stats.memAllocated == 0, "Initial allocation is 0");
    CHECK(stats.stageCount == 0, "No stages registered");

    Unbraid_Destroy(ctx);
    printf("  Init/Destroy clean.\n");
}

/* ==========================================================================
 * Test 2: Register stages
 * ========================================================================== */
void test_register_stages() {
    printf("\n=== Test 2: Register Stages ===\n");

    uint64_t budget = 1024ULL * 1024 * 100;  /* 100 MB */
    UnbraidCtx ctx = Unbraid_Init(budget);
    CHECK(ctx != NULL, "Init OK");

    /* Register 5 stages with different priorities and buffer sizes */
    /* Stage 0: priority=10 (low), buf=1MB — unbraided first */
    int s0 = Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass,
        1024ULL * 1024, 10);
    CHECK(s0 == 0, "Stage 0 registered");

    /* Stage 1: priority=20, buf=2MB */
    int s1 = Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass,
        1024ULL * 1024 * 2, 20);
    CHECK(s1 == 1, "Stage 1 registered");

    /* Stage 2: priority=30, buf=4MB */
    int s2 = Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage2_exec, (UnbraidStageFn)stage2_bypass,
        1024ULL * 1024 * 4, 30);
    CHECK(s2 == 2, "Stage 2 registered");

    /* Stage 3: priority=50 (high), buf=8MB — unbraided last */
    int s3 = Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage3_exec, (UnbraidStageFn)stage3_bypass,
        1024ULL * 1024 * 8, 50);
    CHECK(s3 == 3, "Stage 3 registered");

    /* Stage 4: priority=5 (lowest), buf=512KB — unbraided very first */
    int s4 = Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage4_exec, (UnbraidStageFn)stage4_bypass,
        1024ULL * 512, 5);
    CHECK(s4 == 4, "Stage 4 registered");

    UnbraidStats stats;
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.stageCount == 5, "5 stages registered");
    CHECK(stats.braidedCount == 5, "All 5 braided initially");

    /* Check that working buffer allocation is ~15.5MB */
    uint64_t expected = (1 + 2 + 4 + 8) * 1024 * 1024 + 512 * 1024;
    CHECK(stats.memAllocated == expected,
          "Working buffer allocation matches 15.5MB");

    Unbraid_Destroy(ctx);
}

/* ==========================================================================
 * Test 3: Pipeline execution (fully braided)
 * ========================================================================== */
void test_execute_braided() {
    printf("\n=== Test 3: Execute (Fully Braided) ===\n");

    memset(g_execCounts, 0, sizeof(g_execCounts));
    memset(g_bypassCounts, 0, sizeof(g_bypassCounts));

    uint64_t budget = 1024ULL * 1024 * 100;
    UnbraidCtx ctx = Unbraid_Init(budget);

    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass, 0, 10);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass, 0, 20);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage2_exec, (UnbraidStageFn)stage2_bypass, 0, 30);

    int dummy = 42;
    int ran = Unbraid_Execute(ctx, &dummy, sizeof(dummy));
    CHECK(ran == 3, "All 3 stages executed");
    CHECK(g_execCounts[0] == 1, "Stage 0 exec called");
    CHECK(g_execCounts[1] == 1, "Stage 1 exec called");
    CHECK(g_execCounts[2] == 1, "Stage 2 exec called");
    CHECK(g_bypassCounts[0] == 0, "Stage 0 NOT bypassed");

    Unbraid_Destroy(ctx);
}

/* ==========================================================================
 * Test 4: Force unbraid / rebraid
 * ========================================================================== */
void test_force_unbraid() {
    printf("\n=== Test 4: Force Unbraid/Rebraid ===\n");

    memset(g_execCounts, 0, sizeof(g_execCounts));
    memset(g_bypassCounts, 0, sizeof(g_bypassCounts));

    uint64_t budget = 1024ULL * 1024 * 100;
    UnbraidCtx ctx = Unbraid_Init(budget);

    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass, 0, 10);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass, 0, 20);

    /* Force-unbraid stage 0 */
    int ok = Unbraid_ForceUnbraid(ctx, 0);
    CHECK(ok == 1, "Force unbraid stage 0 succeeded");

    UnbraidStats stats;
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.braidedCount == 1, "1 braided after force-unbraid");
    CHECK(stats.unbraidedCount == 1, "1 unbraided after force-unbraid");

    /* Execute — stage 0 should bypass, stage 1 should exec */
    int dummy = 0;
    Unbraid_Execute(ctx, &dummy, 0);
    CHECK(g_bypassCounts[0] == 1, "Stage 0 bypassed via trampoline");
    CHECK(g_execCounts[1] == 1, "Stage 1 normally executed");

    /* Force-rebraid stage 0 */
    ok = Unbraid_ForceRebraid(ctx, 0);
    CHECK(ok == 1, "Force rebraid stage 0 succeeded");
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.braidedCount == 2, "Both braided after rebraid");

    /* Execute again — both should exec */
    Unbraid_Execute(ctx, &dummy, 0);
    CHECK(g_execCounts[0] == 1, "Stage 0 exec after rebraid");

    Unbraid_Destroy(ctx);
}

/* ==========================================================================
 * Test 5: Memory pressure → automatic unbraiding
 * ========================================================================== */
void test_pressure_unbraid() {
    printf("\n=== Test 5: Memory Pressure Auto-Unbraid ===\n");

    /* Budget = 100 units (nominal) */
    uint64_t budget = 1000;
    UnbraidCtx ctx = Unbraid_Init(budget);

    /* Register 4 stages with no working buffers (so we control pressure) */
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass, 0, 5);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass, 0, 10);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage2_exec, (UnbraidStageFn)stage2_bypass, 0, 20);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage3_exec, (UnbraidStageFn)stage3_bypass, 0, 50);

    UnbraidStats stats;
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.braidedCount == 4, "All 4 braided at start");

    /* Simulate model loading: push to 80% pressure */
    Unbraid_AddPressure(ctx, 800);
    int pct = Unbraid_Tick(ctx);
    printf("  Pressure after 800/1000 alloc: %d%%\n", pct);
    CHECK(pct <= 80, "Pressure is ~80% (some freed by unbraid)");

    /* Tick a few more times to let unbraid cascade */
    for (int i = 0; i < 5; i++) {
        pct = Unbraid_Tick(ctx);
    }
    printf("  Pressure after cascading ticks: %d%%\n", pct);

    Unbraid_GetStats(ctx, &stats);
    printf("  Braided: %u  Unbraided: %u\n",
           stats.braidedCount, stats.unbraidedCount);
    CHECK(stats.unbraidedCount > 0, "At least 1 stage unbraided");
    CHECK(stats.totalUnbraids > 0, "Unbraid operations recorded");

    /* Now release pressure heavily */
    Unbraid_ReleasePressure(ctx, 600);
    for (int i = 0; i < 10; i++) {
        pct = Unbraid_Tick(ctx);
    }
    printf("  Pressure after releasing 600: %d%%\n", pct);

    Unbraid_GetStats(ctx, &stats);
    printf("  Braided: %u  Unbraided: %u  Rebraids: %llu\n",
           stats.braidedCount, stats.unbraidedCount,
           (unsigned long long)stats.totalRebraids);
    CHECK(stats.totalRebraids > 0, "Rebraids occurred");

    Unbraid_Destroy(ctx);
}

/* ==========================================================================
 * Test 6: Aggressive unbraid at 90%+
 * ========================================================================== */
void test_aggressive_unbraid() {
    printf("\n=== Test 6: Aggressive Unbraid (90%+) ===\n");

    uint64_t budget = 1000;
    UnbraidCtx ctx = Unbraid_Init(budget);

    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass, 0, 5);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass, 0, 10);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage2_exec, (UnbraidStageFn)stage2_bypass, 0, 20);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage3_exec, (UnbraidStageFn)stage3_bypass, 0, 50);

    /* Push to 95% */
    Unbraid_AddPressure(ctx, 950);

    /* Single tick at aggressive → should unbraid 2 stages */
    int pct = Unbraid_Tick(ctx);
    printf("  After aggressive tick: %d%%\n", pct);

    UnbraidStats stats;
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.totalUnbraids >= 2, "Aggressive unbraid: >= 2 stages in one tick");

    Unbraid_Destroy(ctx);
}

/* ==========================================================================
 * Test 7: Stats tracking
 * ========================================================================== */
void test_stats() {
    printf("\n=== Test 7: Statistics ===\n");

    uint64_t budget = 1024ULL * 1024 * 50;
    UnbraidCtx ctx = Unbraid_Init(budget);

    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage0_exec, (UnbraidStageFn)stage0_bypass,
        1024 * 1024, 10);
    Unbraid_RegisterStage(ctx,
        (UnbraidStageFn)stage1_exec, (UnbraidStageFn)stage1_bypass,
        1024 * 1024 * 2, 20);

    /* Run pipeline */
    int dummy = 0;
    Unbraid_Execute(ctx, &dummy, 0);
    Unbraid_Execute(ctx, &dummy, 0);

    UnbraidStats stats;
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.pipeRuns == 2, "2 pipeline runs recorded");
    CHECK(stats.fullRuns > 0, "Full runs tracked");
    CHECK(stats.patchSwaps >= 2, "At least 2 patch swaps (from register)");

    /* Force unbraid and run */
    Unbraid_ForceUnbraid(ctx, 0);
    Unbraid_Execute(ctx, &dummy, 0);
    Unbraid_GetStats(ctx, &stats);
    CHECK(stats.bypassRuns > 0, "Bypass runs tracked");
    CHECK(stats.memFreed > 0, "Memory freed tracked");

    printf("  Stats: ticks=%llu  pipeRuns=%llu  swaps=%llu  freed=%llu\n",
           (unsigned long long)stats.tickCount,
           (unsigned long long)stats.pipeRuns,
           (unsigned long long)stats.patchSwaps,
           (unsigned long long)stats.memFreed);

    Unbraid_Destroy(ctx);
}

/* ─── Main ────────────────────────────────────────────────────────────────── */
int main() {
    printf("=== RawrXD Unbraid Pipeline Engine — Test Suite ===\n");

    test_init_destroy();
    test_register_stages();
    test_execute_braided();
    test_force_unbraid();
    test_pressure_unbraid();
    test_aggressive_unbraid();
    test_stats();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
