// ============================================================================
// enterprise_stress_tests.h — Soak, Fuzz, and Fragmentation Test Framework
// ============================================================================
//
// PURPOSE:
//   Comprehensive enterprise stress testing:
//   - Long-duration soak tests (all MASM modules under sustained load)
//   - JSON-RPC 2.0 fuzzing (malformed packets, oversized payloads)
//   - LSP bridge fuzzing (random symbol queries, concurrent invalidation)
//   - Memory fragmentation under rapid patch apply/rollback cycles
//   - Swarm consistency under partition and node churn
//
// PATTERN:  PatchResult-compatible, no exceptions, no std::function
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Stress {

// ============================================================================
// Constants
// ============================================================================
static constexpr uint32_t SOAK_DEFAULT_DURATION_SEC   = 300;   // 5 minutes
static constexpr uint32_t SOAK_EXTENDED_DURATION_SEC  = 3600;  // 1 hour
static constexpr uint32_t FUZZ_DEFAULT_ITERATIONS     = 10000;
static constexpr uint32_t FRAG_DEFAULT_CYCLES         = 5000;
static constexpr uint32_t FUZZ_MAX_PAYLOAD_SIZE       = 65536; // 64KB
static constexpr uint32_t FRAG_PATCH_SIZE_MIN         = 16;
static constexpr uint32_t FRAG_PATCH_SIZE_MAX         = 4096;

// ============================================================================
// Test Result — individual test outcome
// ============================================================================
struct StressTestResult {
    bool        passed;
    const char* testName;
    const char* detail;
    int         errorCode;
    double      elapsedMs;
    uint64_t    iterationsCompleted;
    uint64_t    errorsDetected;
    uint64_t    peakMemoryBytes;
    double      avgLatencyUs;
    double      p99LatencyUs;

    static StressTestResult pass(const char* name, double ms, uint64_t iters) {
        StressTestResult r{};
        r.passed = true;
        r.testName = name;
        r.detail = "PASS";
        r.errorCode = 0;
        r.elapsedMs = ms;
        r.iterationsCompleted = iters;
        r.errorsDetected = 0;
        return r;
    }

    static StressTestResult fail(const char* name, const char* msg, int code = -1) {
        StressTestResult r{};
        r.passed = false;
        r.testName = name;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Soak Test Configuration
// ============================================================================
struct SoakConfig {
    uint32_t durationSec;           // Total soak duration
    uint32_t checkpointIntervalSec; // Print checkpoint every N seconds
    bool     testMASMModules;       // Exercise SelfPatch, GGUF loader, LSP, Orchestrator, QuadBuf
    bool     testJsonRpc;           // Exercise JSON-RPC parser
    bool     testSwarm;             // Exercise swarm protocol
    bool     testHotpatch;          // Exercise memory/byte-level hotpatcher
    bool     abortOnFirstFailure;   // Stop immediately on failure
    uint32_t threadCount;           // Concurrent threads (default 4)
};

// ============================================================================
// Fuzz Test Configuration
// ============================================================================
enum class FuzzTarget : uint32_t {
    JsonRpc         = 0x01,   // JSON-RPC 2.0 parser
    LspBridge       = 0x02,   // LSP AI bridge
    SwarmProtocol   = 0x04,   // Swarm wire protocol
    GGUFHeader      = 0x08,   // GGUF file header parsing
    LicenseBlob     = 0x10,   // Enterprise license blob
    All             = 0xFF,
};

inline FuzzTarget operator|(FuzzTarget a, FuzzTarget b) {
    return static_cast<FuzzTarget>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool HasFuzzTarget(FuzzTarget set, FuzzTarget flag) {
    return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

struct FuzzConfig {
    FuzzTarget target;              // Which subsystems to fuzz
    uint32_t   iterations;          // Number of fuzz iterations per target
    uint32_t   maxPayloadSize;      // Maximum generated payload size
    uint32_t   seed;                // PRNG seed (0 = time-based)
    bool       nullTerminator;      // Include/exclude null terminators
    bool       oversized;           // Generate oversized payloads
    bool       malformedUtf8;       // Include invalid UTF-8 sequences
    bool       truncated;           // Send truncated packets
};

// ============================================================================
// Memory Fragmentation Test Configuration
// ============================================================================
struct FragmentationConfig {
    uint32_t totalCycles;           // Number of patch apply/rollback cycles
    uint32_t minPatchSize;          // Minimum patch size in bytes
    uint32_t maxPatchSize;          // Maximum patch size in bytes
    uint32_t concurrentPatches;     // Simultaneous outstanding patches
    bool     useVirtualProtect;     // Use real VirtualProtect calls
    bool     measureHeapStats;      // Call HeapWalk for fragmentation metrics
    uint32_t checkpointInterval;    // Report every N cycles
};

// ============================================================================
// Fragmentation Metrics
// ============================================================================
struct FragmentationMetrics {
    uint64_t totalAllocations;
    uint64_t totalFrees;
    uint64_t peakWorkingSetBytes;
    uint64_t currentWorkingSetBytes;
    uint64_t heapBlockCount;
    uint64_t heapFreeBlockCount;
    uint64_t largestFreeBlock;
    double   fragmentationRatio;    // freeBlocks / totalBlocks (lower is better)
    double   avgBlockSizeBytes;
};

// ============================================================================
// Stress Test Summary
// ============================================================================
static constexpr int STRESS_MAX_TESTS = 16;

struct StressTestSummary {
    StressTestResult results[STRESS_MAX_TESTS];
    int              totalTests;
    int              passed;
    int              failed;
    double           totalElapsedMs;
    bool             allPassed;
    FragmentationMetrics fragMetrics;  // Filled if fragmentation test was run
};

// ============================================================================
// Progress Callback — function pointer, NOT std::function
// ============================================================================
typedef void (*StressProgressCallback)(const char* phaseName,
                                        uint32_t progressPct,
                                        uint64_t iterationsCompleted,
                                        void* userData);

// ============================================================================
// Public API — Enterprise Stress Testing
// ============================================================================

/// Run the full soak test suite
StressTestResult runSoakTest(const SoakConfig& config,
                              StressProgressCallback callback = nullptr,
                              void* userData = nullptr);

/// Run JSON-RPC fuzzer
StressTestResult runJsonRpcFuzzer(const FuzzConfig& config,
                                   StressProgressCallback callback = nullptr,
                                   void* userData = nullptr);

/// Run LSP bridge fuzzer
StressTestResult runLspBridgeFuzzer(const FuzzConfig& config,
                                      StressProgressCallback callback = nullptr,
                                      void* userData = nullptr);

/// Run swarm protocol fuzzer
StressTestResult runSwarmFuzzer(const FuzzConfig& config,
                                  StressProgressCallback callback = nullptr,
                                  void* userData = nullptr);

/// Run memory fragmentation under patch churn
StressTestResult runFragmentationTest(const FragmentationConfig& config,
                                       StressProgressCallback callback = nullptr,
                                       void* userData = nullptr);

/// Run everything: soak + fuzz + fragmentation
StressTestSummary runFullStressGauntlet(StressProgressCallback callback = nullptr,
                                         void* userData = nullptr);

/// Get default configurations
SoakConfig getDefaultSoakConfig();
FuzzConfig getDefaultFuzzConfig(FuzzTarget target);
FragmentationConfig getDefaultFragmentationConfig();

} // namespace Stress
} // namespace RawrXD
