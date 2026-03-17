// ============================================================================
// masm_stress_harness.h — MASM Module Fuzz & Stress Testing Harness
// ============================================================================
// Exercises all 5 Tier-2 MASM modules under stress: rapid init/shutdown,
// boundary inputs, concurrent access, fault injection.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Stress Test Configuration
// ============================================================================

enum StressTestModule : int {
    STRESS_SELFPATCH       = 0x01,
    STRESS_GGUF_LOADER     = 0x02,
    STRESS_ORCHESTRATOR    = 0x04,
    STRESS_QUADBUFFER      = 0x08,
    STRESS_LSP_BRIDGE      = 0x10,
    STRESS_ALL_MODULES     = 0x1F,
};

enum StressTestMode : int {
    SMODE_INIT_SHUTDOWN_CYCLE  = 1,  // Rapid init/shutdown
    SMODE_BOUNDARY_INPUTS      = 2,  // Edge-case / boundary values
    SMODE_CONCURRENT_ACCESS    = 3,  // Multi-thread stress
    SMODE_FAULT_INJECTION      = 4,  // Bad pointers, null contexts
    SMODE_THROUGHPUT            = 5,  // Max-rate calls
    SMODE_FULL_SUITE           = 99, // All modes
};

struct StressConfig {
    int       modules;           // Bitmask of StressTestModule
    int       mode;              // StressTestMode
    uint32_t  iterations;        // Per-test iteration count
    uint32_t  threadCount;       // For concurrent mode
    uint32_t  timeoutMs;         // Max time per test (0 = unlimited)
    bool      stopOnFirstFail;   // Abort on first failure
    bool      enableLogging;     // Verbose output
};

struct StressResult {
    bool     passed;
    uint32_t totalTests;
    uint32_t passedTests;
    uint32_t failedTests;
    uint32_t skippedTests;
    uint64_t totalDurationMs;
    const char* summary;
};

struct SingleTestResult {
    bool        passed;
    const char* testName;
    const char* detail;
    uint64_t    durationMs;
    uint32_t    iterations;
};

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ API
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace Stress {

// Maximum individual test results tracked
static constexpr int MAX_TEST_RESULTS = 128;

class MASMStressHarness {
public:
    MASMStressHarness();
    ~MASMStressHarness() = default;

    // Run the stress suite with given configuration
    StressResult run(const StressConfig& config);

    // Run default full suite
    StressResult runDefault();

    // Get individual test results after run()
    const SingleTestResult* getTestResults(int* count) const;

private:
    SingleTestResult m_results[MAX_TEST_RESULTS];
    int              m_resultCount;
    StressConfig     m_config;

    // Per-module test suites
    void testSelfPatch();
    void testGGUFLoader();
    void testOrchestrator();
    void testQuadBuffer();
    void testLSPBridge();

    // Per-mode harnesses
    void runInitShutdownCycles(int module);
    void runBoundaryInputs(int module);
    void runConcurrentAccess(int module);
    void runFaultInjection(int module);
    void runThroughput(int module);

    // Result recording
    void recordPass(const char* name, const char* detail, uint64_t durationMs);
    void recordFail(const char* name, const char* detail, uint64_t durationMs);

    uint64_t nowMs() const;
};

} // namespace Stress
} // namespace RawrXD

#endif // __cplusplus
