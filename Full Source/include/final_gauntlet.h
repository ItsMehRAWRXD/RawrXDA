// ============================================================================
// final_gauntlet.h — Phase 32: The Final Gauntlet Runtime Verification
// ============================================================================
//
// Public API for the pre-packaging acceptance test suite.
//
// PATTERN:   No exceptions. PatchResult-compatible structured results.
// THREADING: All tests run on the caller's thread.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_FINAL_GAUNTLET_H
#define RAWRXD_FINAL_GAUNTLET_H

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>   // OutputDebugStringA

// ============================================================================
// Constants
// ============================================================================

static constexpr int GAUNTLET_TEST_COUNT = 10;

// ============================================================================
// GauntletResult — Single test outcome (PatchResult-compatible)
// ============================================================================

struct GauntletResult {
    bool        passed;
    const char* detail;         // Human-readable description
    int         errorCode;      // 0 on pass, >0 on fail
    double      elapsedMs;      // Wall-clock time for this test
    const char* testName;       // Set by the runner

    static GauntletResult pass(const char* msg = "PASS");
    static GauntletResult fail(const char* msg, int code = -1);
};

// ============================================================================
// GauntletSummary — Full run outcome
// ============================================================================

struct GauntletSummary {
    GauntletResult results[GAUNTLET_TEST_COUNT];
    int         totalTests;
    int         passed;
    int         failed;
    double      totalElapsedMs;
    bool        allPassed;
};

// ============================================================================
// Public API
// ============================================================================

// Run all 10 tests and return the summary.
GauntletSummary runFinalGauntlet();

// Get the name of test by index (0..GAUNTLET_TEST_COUNT-1).
const char* getGauntletTestName(int index);

#endif // RAWRXD_FINAL_GAUNTLET_H
