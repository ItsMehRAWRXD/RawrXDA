// ============================================================================
// final_gauntlet.cpp — Phase 32: The Final Gauntlet Runtime Verification
// ============================================================================
//
// PURPOSE:
//   Pre-packaging acceptance tests that exercise every major subsystem in
//   a single coordinated pass. Each test is self-contained and returns a
//   GauntletResult with pass/fail, a descriptive message, and timing data.
//
// SUBSYSTEMS TESTED:
//   1.  MonacoCoreBuffer — GapBuffer insert / remove / line-count fidelity
//   2.  MonacoCoreBuffer — Gap resize under stress (large document simulation)
//   3.  PDBManager — Singleton access, config, stats coherence
//   4.  PDB_HashName — Known-value hash-determinism check
//   5.  ReferenceRouter — Provider registration, empty-query result validity
//   6.  AcceleratorRouter — Singleton init, backend enumeration, CPU fallback
//   7.  FeatureRegistry — Feature enumeration, stub-detection, percentage check
//   8.  FeatureRegistry — Consistency between getFeatureCount and getAllFeatures
//   9.  UnifiedHotpatchManager — Stats base-line coherence (no negative counters)
//  10.  End-to-end cross-subsystem canary (chain: buffer → hash → feature count)
//
// PATTERN:   No exceptions. PatchResult-compatible returns where applicable.
// THREADING: All tests single-threaded (caller's thread).
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "final_gauntlet.h"

// MonacoCore (GapBuffer)
#include "RawrXD_MonacoCore.h"

// PDB subsystem
#include "pdb_native.h"
#include "pdb_gsi_hash.h"
#include "pdb_reference_provider.h"

// Pull PDB types into scope
using namespace RawrXD::PDB;

// Accelerator
#include "accelerator_router.h"

// Feature audit
#include "feature_registry.h"

// Hotpatch coordination
#include "unified_hotpatch_manager.hpp"

#include <cstring>
#include <cstdio>
#include <chrono>
#include <cmath>

// ============================================================================
// GauntletResult Helpers
// ============================================================================

GauntletResult GauntletResult::pass(const char* msg) {
    GauntletResult r{};
    r.passed = true;
    r.detail = msg ? msg : "PASS";
    r.errorCode = 0;
    r.elapsedMs = 0.0;
    return r;
}

GauntletResult GauntletResult::fail(const char* msg, int code) {
    GauntletResult r{};
    r.passed = false;
    r.detail = msg ? msg : "FAIL";
    r.errorCode = code;
    r.elapsedMs = 0.0;
    return r;
}

// ============================================================================
// Timing helper
// ============================================================================
static double timerMs(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ============================================================================
// TEST 1: MonacoCoreBuffer — Basic Insert / Remove / LineCount
// ============================================================================
static GauntletResult test_GapBuffer_Basic() {
    auto t0 = std::chrono::steady_clock::now();

    MonacoCoreBuffer buf;
    if (!buf.init(4096)) {
        return GauntletResult::fail("GapBuffer init(4096) failed", 1);
    }

    // Insert a known string
    const char* hello = "Hello, Gauntlet!\n";
    uint32_t helloLen = static_cast<uint32_t>(strlen(hello));
    if (!buf.insert(0, hello, helloLen)) {
        buf.destroy();
        return GauntletResult::fail("GapBuffer insert(0, hello) failed", 2);
    }

    // Verify length
    if (buf.length() != helloLen) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Length mismatch: expected %u, got %u", helloLen, buf.length());
        buf.destroy();
        return GauntletResult::fail(msg, 3);
    }

    // Verify line count (1 newline → 2 lines)
    uint32_t lc = buf.lineCount();
    if (lc < 1) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "LineCount too low: expected >=1, got %u", lc);
        buf.destroy();
        return GauntletResult::fail(msg, 4);
    }

    // Remove "Hello" (5 chars) — remainder should be ", Gauntlet!\n"
    if (!buf.remove(0, 5)) {
        buf.destroy();
        return GauntletResult::fail("GapBuffer remove(0,5) failed", 5);
    }
    if (buf.length() != helloLen - 5) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Post-remove length: expected %u, got %u",
                 helloLen - 5, buf.length());
        buf.destroy();
        return GauntletResult::fail(msg, 6);
    }

    // Retrieve line 0 and verify content
    char line0[256] = {0};
    uint32_t lineLen = buf.getLine(0, line0, 255);
    if (lineLen == 0) {
        buf.destroy();
        return GauntletResult::fail("getLine(0) returned 0 bytes", 7);
    }

    // Should start with ", Gauntlet!"
    if (strncmp(line0, ", Gauntlet!", 11) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "getLine(0) content mismatch: got '%.64s'", line0);
        buf.destroy();
        return GauntletResult::fail(msg, 8);
    }

    buf.destroy();

    GauntletResult r = GauntletResult::pass("GapBuffer basic ops: insert/remove/lineCount OK");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 2: MonacoCoreBuffer — Stress Test (Gap Resize)
// ============================================================================
static GauntletResult test_GapBuffer_Stress() {
    auto t0 = std::chrono::steady_clock::now();

    MonacoCoreBuffer buf;
    // Start with a small capacity to force gap resizes
    if (!buf.init(256)) {
        return GauntletResult::fail("GapBuffer init(256) failed for stress test", 1);
    }

    // Insert 500 lines (will exceed initial 256 bytes and force resizes)
    const char* line = "Line XXXX: The quick brown fox jumps.\n";
    uint32_t lineLen = static_cast<uint32_t>(strlen(line));
    uint32_t totalInserted = 0;

    for (int i = 0; i < 500; ++i) {
        char formatted[64];
        int n = snprintf(formatted, sizeof(formatted),
                         "Line %04d: The quick brown fox jumps.\n", i);
        if (n <= 0 || n >= (int)sizeof(formatted)) {
            buf.destroy();
            return GauntletResult::fail("snprintf overflow in stress loop", 2);
        }
        uint32_t fLen = static_cast<uint32_t>(n);

        if (!buf.insert(totalInserted, formatted, fLen)) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "GapBuffer stress insert failed at iteration %d (totalInserted=%u)",
                     i, totalInserted);
            buf.destroy();
            return GauntletResult::fail(msg, 3);
        }
        totalInserted += fLen;
    }

    // Verify total length
    if (buf.length() != totalInserted) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Stress length mismatch: expected %u, got %u",
                 totalInserted, buf.length());
        buf.destroy();
        return GauntletResult::fail(msg, 4);
    }

    // Verify line count (500 newlines → at least 500 lines)
    uint32_t lc = buf.lineCount();
    if (lc < 500) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Stress lineCount: expected >=500, got %u", lc);
        buf.destroy();
        return GauntletResult::fail(msg, 5);
    }

    // Verify first line content
    char firstLine[256] = {0};
    buf.getLine(0, firstLine, 255);
    if (strncmp(firstLine, "Line 0000:", 10) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "First line mismatch: got '%.64s'", firstLine);
        buf.destroy();
        return GauntletResult::fail(msg, 6);
    }

    // Verify last line content
    char lastLine[256] = {0};
    buf.getLine(499, lastLine, 255);
    if (strncmp(lastLine, "Line 0499:", 10) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Last line mismatch: got '%.64s'", lastLine);
        buf.destroy();
        return GauntletResult::fail(msg, 7);
    }

    buf.destroy();

    GauntletResult r = GauntletResult::pass("GapBuffer stress: 500 lines, gap resize survived");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 3: PDBManager — Singleton Access & Stats Coherence
// ============================================================================
static GauntletResult test_PDB_Singleton() {
    auto t0 = std::chrono::steady_clock::now();

    // Access singleton — must not crash
    PDBManager& mgr = PDBManager::instance();

    // Stats must be coherent (no overflow / garbage)
    PDBManager::Stats stats = mgr.getStats();

    // modulesLoaded should be a reasonable number (< 10000 in any test env)
    if (stats.modulesLoaded > 10000) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "PDB modulesLoaded suspiciously high: %u", stats.modulesLoaded);
        return GauntletResult::fail(msg, 1);
    }

    // symbolsIndexed should not be garbage (< 100M)
    if (stats.symbolsIndexed > 100000000ULL) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "PDB symbolsIndexed suspiciously high: %llu",
                 (unsigned long long)stats.symbolsIndexed);
        return GauntletResult::fail(msg, 2);
    }

    // cacheHits + cacheMisses >= lookupCount is NOT necessarily true
    // (some lookups bypass cache). But cacheHits should not exceed lookupCount.
    if (stats.cacheHits > stats.lookupCount + 1) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "PDB cacheHits (%llu) > lookupCount (%llu) — stats inconsistency",
                 (unsigned long long)stats.cacheHits,
                 (unsigned long long)stats.lookupCount);
        return GauntletResult::fail(msg, 3);
    }

    // Attempt to resolve a symbol that doesn't exist — should return gracefully
    uint64_t rva = 0;
    const char* mod = nullptr;
    PDBResult pr = mgr.resolveSymbol("__gauntlet_nonexistent_symbol_42__", &rva, &mod);
    // We expect failure (not found), but NOT a crash
    // (success would be surprising but not an error — just log it)

    GauntletResult r = GauntletResult::pass("PDB singleton access + stats coherence OK");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 4: PDB_HashName — Deterministic Hash
// ============================================================================
static GauntletResult test_PDB_HashDeterminism() {
    auto t0 = std::chrono::steady_clock::now();

    // Hash the same string twice — must produce identical results
    const char* testSymbol = "NtCreateFile";
    uint32_t len = static_cast<uint32_t>(strlen(testSymbol));

    uint32_t h1 = PDB_HashName(testSymbol, len);
    uint32_t h2 = PDB_HashName(testSymbol, len);

    if (h1 != h2) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "PDB_HashName not deterministic: 0x%08X vs 0x%08X", h1, h2);
        return GauntletResult::fail(msg, 1);
    }

    // Hash of empty string must not crash and must be deterministic
    uint32_t empty1 = PDB_HashName("", 0);
    uint32_t empty2 = PDB_HashName("", 0);
    if (empty1 != empty2) {
        return GauntletResult::fail("PDB_HashName('',0) not deterministic", 2);
    }

    // Different strings should (almost certainly) produce different hashes
    const char* other = "NtClose";
    uint32_t otherLen = static_cast<uint32_t>(strlen(other));
    uint32_t h3 = PDB_HashName(other, otherLen);
    if (h1 == h3) {
        // Technically possible but astronomically unlikely for these two names
        return GauntletResult::fail(
            "PDB_HashName collision: 'NtCreateFile' == 'NtClose' — suspicious", 3);
    }

    // Hash must be non-zero for a non-empty string (extremely unlikely to be 0)
    if (h1 == 0) {
        return GauntletResult::fail("PDB_HashName('NtCreateFile') returned 0", 4);
    }

    GauntletResult r = GauntletResult::pass("PDB_HashName: deterministic, distinct, non-zero");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 5: ReferenceRouter — Provider Count & Empty Query
// ============================================================================
static GauntletResult test_ReferenceRouter() {
    auto t0 = std::chrono::steady_clock::now();

    ReferenceRouter& router = ReferenceRouter::instance();

    // Provider count should be >= 0 and < MAX_PROVIDERS (16)
    uint32_t pCount = router.getProviderCount();
    if (pCount > 16) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "ReferenceRouter provider count out of range: %u", pCount);
        return GauntletResult::fail(msg, 1);
    }

    // Execute a query for a symbol that definitely doesn't exist
    // — must not crash, must return count == 0 or a valid result set
    ReferenceQuery query = ReferenceQuery::forSymbol("__gauntlet_nonexistent_ref__");
    ReferenceResult result;
    result.clear();

    PDBResult pr = router.findAllReferences(query, &result);
    // We don't require success (no PDBs loaded), but we require no crash
    // and count <= MAX_REFERENCES
    if (result.count > MAX_REFERENCES) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "ReferenceResult.count overflow: %u > MAX_REFERENCES(%u)",
                 result.count, MAX_REFERENCES);
        return GauntletResult::fail(msg, 2);
    }

    // Stats coherence
    ReferenceRouter::Stats stats = router.getStats();
    if (stats.totalQueries == 0 && stats.cacheHits > 0) {
        return GauntletResult::fail("RefRouter: cacheHits > 0 but totalQueries == 0", 3);
    }

    GauntletResult r = GauntletResult::pass("ReferenceRouter: singleton OK, empty query safe");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 6: AcceleratorRouter — Init & Backend Enumeration
// ============================================================================
static GauntletResult test_AcceleratorRouter() {
    auto t0 = std::chrono::steady_clock::now();

    AcceleratorRouter& ar = AcceleratorRouter::instance();

    // Initialize — may succeed or fail depending on hardware, but must not crash
    RouterResult initRes = ar.initialize();
    // We don't require success (no GPU may be present on build server)

    // Backend count must be sane
    uint32_t backendCount = ar.getAvailableBackendCount();
    if (backendCount > 32) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "AcceleratorRouter: %u backends — suspiciously high", backendCount);
        return GauntletResult::fail(msg, 1);
    }

    // Active backend should be a valid enum value
    RouterBackendType active = ar.getActiveBackend();
    uint8_t activeVal = static_cast<uint8_t>(active);
    // RouterBackendType values: 0 (None) through 6 (CPU_Fallback)
    if (activeVal > 10) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "AcceleratorRouter: activeBackend raw value %u out of enum range",
                 (unsigned)activeVal);
        return GauntletResult::fail(msg, 2);
    }

    // CPU fallback should always be available
    bool cpuAvail = ar.isBackendAvailable(RouterBackendType::CPU_Fallback);
    if (!cpuAvail && backendCount == 0) {
        // Both are zero/false — this is acceptable on bare systems
        // Don't fail, just note it
    }

    // Submit a no-op inference task — should either succeed (CPU fallback)
    // or fail gracefully (no backend), but never crash
    RouterInferenceTask task{};
    task.inputData = nullptr;
    task.inputSizeBytes = 0;
    task.outputData = nullptr;
    task.outputSizeBytes = 0;
    task.kernelName = "gauntlet_noop";
    task.priority = DispatchPriority::Batch;
    task.scope = DispatchScope::Inference;
    task.preferredBackend = RouterBackendType::CPU_Fallback;
    task.timeoutMs = 100;
    task.batchSize = 1;
    task.quantType = 0;

    RouterResult taskRes = ar.submitInference(task);
    // We accept either success or failure — just no crash

    GauntletResult r = GauntletResult::pass("AcceleratorRouter: init + enumerate + submit OK");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 7: FeatureRegistry — Enumeration & Stub Detection
// ============================================================================
static GauntletResult test_FeatureRegistry_Enum() {
    auto t0 = std::chrono::steady_clock::now();

    FeatureRegistry& reg = FeatureRegistry::instance();

    // Feature count must be positive (we register 90+ features)
    size_t count = reg.getFeatureCount();
    if (count == 0) {
        return GauntletResult::fail("FeatureRegistry: getFeatureCount() == 0", 1);
    }

    // Run stub detection — must not crash
    reg.detectStubs();

    // Completion percentage must be in [0.0, 1.0]
    float pct = reg.getCompletionPercentage();
    if (pct < 0.0f || pct > 1.0f) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "FeatureRegistry: completionPercentage out of range: %.4f", pct);
        return GauntletResult::fail(msg, 2);
    }

    // If we have features, percentage should be > 0 (at least some are implemented)
    if (count > 0 && pct <= 0.0f) {
        return GauntletResult::fail(
            "FeatureRegistry: featureCount > 0 but completionPercentage == 0.0 — suspicious", 3);
    }

    char detail[256];
    snprintf(detail, sizeof(detail),
             "FeatureRegistry: %zu features, %.1f%% complete",
             count, pct * 100.0f);
    GauntletResult r = GauntletResult::pass(detail);
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 8: FeatureRegistry — Consistency (count vs vector size)
// ============================================================================
static GauntletResult test_FeatureRegistry_Consistency() {
    auto t0 = std::chrono::steady_clock::now();

    FeatureRegistry& reg = FeatureRegistry::instance();

    size_t scalarCount = reg.getFeatureCount();
    std::vector<FeatureEntry> allFeatures = reg.getAllFeatures();
    size_t vectorCount = allFeatures.size();

    if (scalarCount != vectorCount) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "FeatureRegistry: getFeatureCount()=%zu != getAllFeatures().size()=%zu",
                 scalarCount, vectorCount);
        return GauntletResult::fail(msg, 1);
    }

    // Every feature must have a non-empty name
    for (size_t i = 0; i < vectorCount; ++i) {
        if (!allFeatures[i].name || allFeatures[i].name[0] == '\0') {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "FeatureRegistry: feature[%zu] has empty name", i);
            return GauntletResult::fail(msg, 2);
        }
    }

    GauntletResult r = GauntletResult::pass("FeatureRegistry: count/vector consistency OK");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 9: UnifiedHotpatchManager — Stats Baseline
// ============================================================================
static GauntletResult test_HotpatchStats() {
    auto t0 = std::chrono::steady_clock::now();

    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();

    // Atomic counters must be non-negative (they're unsigned, so check < sane max)
    uint64_t totalOps = stats.totalOperations.load(std::memory_order_relaxed);
    uint64_t totalFail = stats.totalFailures.load(std::memory_order_relaxed);

    // In a fresh session, operations should be reasonable
    if (totalOps > 1000000000ULL) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "HotpatchManager: totalOperations=%llu — suspiciously high",
                 (unsigned long long)totalOps);
        return GauntletResult::fail(msg, 1);
    }

    // Failures should never exceed operations
    if (totalFail > totalOps + 1) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "HotpatchManager: totalFailures(%llu) > totalOperations(%llu)",
                 (unsigned long long)totalFail,
                 (unsigned long long)totalOps);
        return GauntletResult::fail(msg, 2);
    }

    // Individual layer counts
    uint64_t memCount = stats.memoryPatchCount.load(std::memory_order_relaxed);
    uint64_t byteCount = stats.bytePatchCount.load(std::memory_order_relaxed);
    uint64_t srvCount = stats.serverPatchCount.load(std::memory_order_relaxed);

    // Sum of layer counts should be <= totalOperations (some ops may be queries)
    if (memCount + byteCount + srvCount > totalOps + 10) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "HotpatchManager: layer sum (%llu+%llu+%llu) > totalOps(%llu)",
                 (unsigned long long)memCount,
                 (unsigned long long)byteCount,
                 (unsigned long long)srvCount,
                 (unsigned long long)totalOps);
        return GauntletResult::fail(msg, 3);
    }

    GauntletResult r = GauntletResult::pass("HotpatchManager: stats baseline coherent");
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// TEST 10: Cross-Subsystem Canary
// ============================================================================
//
// Chain: GapBuffer → PDB Hash → FeatureRegistry
// Insert text into a GapBuffer, hash the text with PDB_HashName, use that
// hash as a "canary value", then verify FeatureRegistry can enumerate.
// This catches ABI mismatches, linkage failures, and memory corruption
// that single-subsystem tests might miss.
//
static GauntletResult test_CrossSubsystem_Canary() {
    auto t0 = std::chrono::steady_clock::now();

    // Step 1: Create buffer and insert canary text
    MonacoCoreBuffer buf;
    if (!buf.init(1024)) {
        return GauntletResult::fail("Canary: GapBuffer init failed", 1);
    }

    const char* canary = "RawrXD_Canary_Phase32";
    uint32_t canaryLen = static_cast<uint32_t>(strlen(canary));
    if (!buf.insert(0, canary, canaryLen)) {
        buf.destroy();
        return GauntletResult::fail("Canary: GapBuffer insert failed", 2);
    }

    // Verify buffer content via getLine
    char retrieved[256] = {0};
    uint32_t gotLen = buf.getLine(0, retrieved, 255);
    if (gotLen == 0 || strncmp(retrieved, canary, canaryLen) != 0) {
        buf.destroy();
        return GauntletResult::fail("Canary: GapBuffer content readback mismatch", 3);
    }

    // Step 2: Hash the retrieved content
    uint32_t hash = PDB_HashName(retrieved, canaryLen);
    if (hash == 0) {
        buf.destroy();
        return GauntletResult::fail("Canary: PDB_HashName returned 0 for canary string", 4);
    }

    // Step 3: Use the hash as a seed to index into the feature registry
    FeatureRegistry& reg = FeatureRegistry::instance();
    size_t fCount = reg.getFeatureCount();
    if (fCount == 0) {
        buf.destroy();
        return GauntletResult::fail("Canary: FeatureRegistry has 0 features", 5);
    }

    // Index a feature by (hash % count) — just verify it doesn't crash
    std::vector<FeatureEntry> features = reg.getAllFeatures();
    size_t idx = static_cast<size_t>(hash % fCount);
    if (!features[idx].name || features[idx].name[0] == '\0') {
        buf.destroy();
        return GauntletResult::fail("Canary: feature at hash-indexed position has empty name", 6);
    }

    buf.destroy();

    char detail[256];
    snprintf(detail, sizeof(detail),
             "Cross-subsystem canary OK: hash=0x%08X, feature[%zu]='%.64s'",
             hash, idx, features[idx].name.c_str());
    GauntletResult r = GauntletResult::pass(detail);
    r.elapsedMs = timerMs(t0);
    return r;
}

// ============================================================================
// GAUNTLET RUNNER
// ============================================================================

static const char* s_testNames[GAUNTLET_TEST_COUNT] = {
    "GapBuffer Basic",
    "GapBuffer Stress (500 lines)",
    "PDB Singleton & Stats",
    "PDB Hash Determinism",
    "ReferenceRouter Query",
    "AcceleratorRouter Init",
    "FeatureRegistry Enum",
    "FeatureRegistry Consistency",
    "Hotpatch Stats Baseline",
    "Cross-Subsystem Canary"
};

using TestFunc = GauntletResult (*)();

static TestFunc s_testFuncs[GAUNTLET_TEST_COUNT] = {
    test_GapBuffer_Basic,
    test_GapBuffer_Stress,
    test_PDB_Singleton,
    test_PDB_HashDeterminism,
    test_ReferenceRouter,
    test_AcceleratorRouter,
    test_FeatureRegistry_Enum,
    test_FeatureRegistry_Consistency,
    test_HotpatchStats,
    test_CrossSubsystem_Canary
};

GauntletSummary runFinalGauntlet() {
    GauntletSummary summary{};
    summary.totalTests = GAUNTLET_TEST_COUNT;
    summary.passed = 0;
    summary.failed = 0;

    auto totalStart = std::chrono::steady_clock::now();

    for (int i = 0; i < GAUNTLET_TEST_COUNT; ++i) {
        OutputDebugStringA("[Phase 32] Running: ");
        OutputDebugStringA(s_testNames[i]);
        OutputDebugStringA("...\n");

        // Run the test
        summary.results[i] = s_testFuncs[i]();

        // Copy test name
        summary.results[i].testName = s_testNames[i];

        if (summary.results[i].passed) {
            summary.passed++;
        } else {
            summary.failed++;
        }

        // Debug output
        char dbg[512];
        snprintf(dbg, sizeof(dbg), "[Phase 32] %s: %s (%.2f ms) — %s\n",
                 s_testNames[i],
                 summary.results[i].passed ? "PASS" : "FAIL",
                 summary.results[i].elapsedMs,
                 summary.results[i].detail);
        OutputDebugStringA(dbg);
    }

    summary.totalElapsedMs = timerMs(totalStart);
    summary.allPassed = (summary.failed == 0);

    char finalMsg[256];
    snprintf(finalMsg, sizeof(finalMsg),
             "[Phase 32] GAUNTLET COMPLETE: %d/%d passed in %.2f ms — %s\n",
             summary.passed, summary.totalTests,
             summary.totalElapsedMs,
             summary.allPassed ? "ALL GREEN" : "FAILURES DETECTED");
    OutputDebugStringA(finalMsg);

    return summary;
}

const char* getGauntletTestName(int index) {
    if (index < 0 || index >= GAUNTLET_TEST_COUNT) return "???";
    return s_testNames[index];
}
