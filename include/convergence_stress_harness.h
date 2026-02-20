// ============================================================================
// convergence_stress_harness.h — Convergence-Phase Stress Testing
// ============================================================================
// Exercises all Tier-3 MASM modules (CamAuth, KQuant Kernel, KQuant Dequant,
// Watchdog, PerfCounters) plus VRAM spillover operations under:
//   - Init/shutdown cycles
//   - Boundary / edge-case inputs
//   - Fuzz inputs (randomized data)
//   - Thread contention (multi-core hammer)
//   - Throughput saturation
//   - Perf-instrumented runs (cycle histograms via PerfCounters)
//
// Extends the original masm_stress_harness pattern for the convergence phase.
// Tests modules NOT covered by the original Tier-2 harness.
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
// Module bitmask — Convergence targets
// ============================================================================

enum ConvergenceModule : int {
    CONV_CAMAUTH           = 0x01,   // Camellia-256 Authenticated Encryption
    CONV_KQUANT_KERNEL     = 0x02,   // K-Quant AVX2/AVX-512 dequant
    CONV_KQUANT_DEQUANT    = 0x04,   // K-Quant multi-type dispatcher
    CONV_WATCHDOG          = 0x08,   // Integrity verification
    CONV_PERF_COUNTERS     = 0x10,   // PerfCounters MASM kernel
    CONV_SPILLOVER         = 0x20,   // VRAM-to-host spillover
    CONV_ALL_MODULES       = 0x3F,
};

enum ConvergenceMode : int {
    CMODE_INIT_SHUTDOWN     = 1,    // Rapid init/shutdown cycling
    CMODE_BOUNDARY_INPUTS   = 2,    // Edge-case values, NULL, max
    CMODE_FUZZ_INPUTS       = 3,    // Randomized payloads
    CMODE_THREAD_CONTENTION = 4,    // Multi-thread hammer on same exports
    CMODE_THROUGHPUT        = 5,    // Max-rate saturation
    CMODE_PERF_INSTRUMENTED = 6,    // All calls wrapped in ScopedMeasurement
    CMODE_FULL_CONVERGENCE  = 99,   // All modes
};

// ============================================================================
// Configuration
// ============================================================================

struct ConvergenceConfig {
    int       modules;             // Bitmask of ConvergenceModule
    int       mode;                // ConvergenceMode
    uint32_t  iterations;          // Per-test iteration count
    uint32_t  threadCount;         // For contention mode (capped at 64)
    uint32_t  timeoutMs;           // Max time per test (0 = unlimited)
    uint32_t  fuzzSeed;            // RNG seed for fuzz mode (0 = time-based)
    uint32_t  maxPayloadBytes;     // Max fuzz buffer size (default 64KB)
    bool      stopOnFirstFail;     // Abort on first failure
    bool      enableLogging;       // Verbose output
    bool      enablePerfCapture;   // Record PerfCounters during all tests
    bool      verifyDeterminism;   // Cross-check identical inputs → identical outputs
};

// ============================================================================
// Results
// ============================================================================

struct ConvergenceResult {
    bool        passed;
    uint32_t    totalTests;
    uint32_t    passedTests;
    uint32_t    failedTests;
    uint32_t    skippedTests;
    uint64_t    totalDurationMs;
    const char* summary;

    // Determinism tracking
    uint32_t    deterministicTests;     // Tests that verified identical output
    uint32_t    nonDeterministicTests;  // Tests that produced different output
};

struct ConvergenceTestResult {
    bool        passed;
    const char* testName;
    const char* moduleName;
    const char* detail;
    uint64_t    durationMs;
    uint32_t    iterations;
    uint64_t    medianCycles;       // From PerfCounters (0 if not instrumented)
    uint64_t    p99Cycles;          // From PerfCounters (0 if not instrumented)
};

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ API
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace Convergence {

static constexpr int MAX_CONV_RESULTS = 256;

class ConvergenceStressHarness {
public:
    ConvergenceStressHarness();
    ~ConvergenceStressHarness() = default;

    // ---- Primary API ----
    ConvergenceResult run(const ConvergenceConfig& config);
    ConvergenceResult runDefault();

    // ---- Results ----
    const ConvergenceTestResult* getTestResults(int* count) const;

    // ---- Static defaults ----
    static ConvergenceConfig defaultConfig();

private:
    ConvergenceTestResult  m_results[MAX_CONV_RESULTS];
    int                    m_resultCount;
    ConvergenceConfig      m_config;

    // ---- Per-module test suites ----
    void testCamAuth();
    void testKQuantKernel();
    void testKQuantDequant();
    void testWatchdog();
    void testPerfCounters();
    void testSpillover();

    // ---- Per-mode harnesses ----
    void runInitShutdownCycles(int module);
    void runBoundaryInputs(int module);
    void runFuzzInputs(int module);
    void runThreadContention(int module);
    void runThroughput(int module);
    void runPerfInstrumented(int module);

    // ---- Determinism verification ----
    bool verifyDeterministicOutput(int module);

    // ---- Result recording ----
    void recordPass(const char* name, const char* moduleName,
                    const char* detail, uint64_t durationMs,
                    uint64_t medianCycles = 0, uint64_t p99Cycles = 0);
    void recordFail(const char* name, const char* moduleName,
                    const char* detail, uint64_t durationMs,
                    uint64_t medianCycles = 0, uint64_t p99Cycles = 0);

    // ---- Fuzz helpers ----
    void fillRandomBuffer(uint8_t* buf, uint32_t len);
    uint32_t nextRand();

    // ---- Timing ----
    uint64_t nowMs() const;

    // ---- RNG state ----
    uint32_t m_rngState;
};

} // namespace Convergence
} // namespace RawrXD

#endif // __cplusplus
