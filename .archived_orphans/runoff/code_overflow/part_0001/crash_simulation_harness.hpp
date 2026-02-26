// ============================================================================
// crash_simulation_harness.hpp — Crash Simulation & Fault Injection Harness
// ============================================================================
// Deterministic crash/fault injection framework for validating recovery paths
// across all three hotpatch layers, the inference state machine, scheduler,
// and journal. Injects controlled faults to prove that:
//   - The recovery journal correctly rolls back uncommitted patches
//   - The state machine transitions to Faulted and recovers
//   - Memory patches are safely reverted under page-fault
//   - Thread pool drains cleanly under OOM simulation
//
// Fault types:
//   - Simulated process crash (controlled unwind)
//   - OOM injection (allocation failure at specific callsite)
//   - Page-fault injection (VirtualProtect → NO_ACCESS)
//   - Timeout injection (sleep injection in critical path)
//   - Corruption injection (bit-flip in specified buffer)
//   - Network failure (socket error injection for server patches)
//   - Disk I/O failure (journal write returns error)
//
// Architecture:
//   - FaultPoint: named injection site in production code
//   - FaultRule: triggers fault at a FaultPoint after N hits
//   - CrashScenario: sequence of FaultRules that models a failure mode
//   - HarnessRunner: executes scenario, collects results
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"   // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace RawrXD {
namespace Testing {

// ============================================================================
// Fault Types
// ============================================================================
enum class FaultType : uint8_t {
    None            = 0,
    ProcessCrash    = 1,    // Simulated abnormal termination
    OOM             = 2,    // malloc/new returns nullptr
    PageFault       = 3,    // Access violation on target memory
    Timeout         = 4,    // Artificial delay exceeding threshold
    Corruption      = 5,    // Bit-flip in target buffer
    NetworkFailure  = 6,    // Socket/HTTP error
    DiskIOFailure   = 7,    // Journal/file write failure
    LockDeadlock    = 8,    // Simulated lock timeout
    GPUHang         = 9,    // Vulkan device lost
    StackOverflow   = 10,   // Deep recursion simulation
};

const char* faultTypeName(FaultType type);

// ============================================================================
// Fault Severity
// ============================================================================
enum class FaultSeverity : uint8_t {
    Transient   = 0,    // Recoverable, single occurrence
    Persistent  = 1,    // Keeps failing until cleared
    Cascading   = 2,    // Triggers secondary faults
};

// ============================================================================
// FaultPoint — named injection site
// ============================================================================
struct FaultPoint {
    const char*     name;           // e.g., "memory_patch_apply"
    const char*     subsystem;      // e.g., "MemoryHotpatch"
    const char*     file;           // Source file (optional)
    uint32_t        line;           // Source line (optional)
    std::atomic<uint64_t> hitCount{0};
    bool            armed;          // If true, fault will fire
};

// ============================================================================
// FaultRule — when to inject a fault
// ============================================================================
struct FaultRule {
    uint64_t        ruleId;
    const char*     faultPointName;     // Which FaultPoint to target
    FaultType       type;
    FaultSeverity   severity;
    uint64_t        triggerAfterHits;   // Fire after this many hits (0 = first hit)
    uint64_t        repeatCount;        // How many times to fire (0 = infinite)
    uint32_t        delayMs;            // For Timeout faults
    uint32_t        corruptionBits;     // For Corruption faults (bit count to flip)
    bool            enabled;
};

// ============================================================================
// FaultInjectionResult
// ============================================================================
struct FaultInjectionResult {
    bool        faultFired;
    FaultType   type;
    const char* faultPointName;
    const char* detail;
    uint64_t    hitNumber;   // Which hit triggered the fault
};

// ============================================================================
// CrashScenario — a named set of fault rules modeling a failure mode
// ============================================================================
struct CrashScenario {
    uint64_t        scenarioId;
    const char*     name;
    const char*     description;
    std::vector<FaultRule> rules;

    // Expected outcomes
    bool            expectJournalRollback;
    bool            expectStateMachineFault;
    bool            expectMemoryRevert;
    bool            expectCleanRecovery;
};

// ============================================================================
// Scenario Execution Result
// ============================================================================
struct ScenarioResult {
    bool            passed;
    const char*     scenarioName;
    double          executionMs;
    uint64_t        faultsInjected;
    uint64_t        faultsRecovered;
    uint64_t        unrecoveredFaults;

    // Validation checks
    bool            journalRollbackCorrect;
    bool            stateMachineRecovered;
    bool            memoryReverted;
    bool            noResourceLeaks;
    bool            noDeadlocks;

    std::vector<std::string> failureDetails;

    bool isClean() const {
        return passed && journalRollbackCorrect && stateMachineRecovered
            && memoryReverted && noResourceLeaks && noDeadlocks;
    }
};

// ============================================================================
// Harness Stats
// ============================================================================
struct HarnessStats {
    std::atomic<uint64_t> scenariosRun{0};
    std::atomic<uint64_t> scenariosPassed{0};
    std::atomic<uint64_t> scenariosFailed{0};
    std::atomic<uint64_t> totalFaultsInjected{0};
    std::atomic<uint64_t> totalFaultsRecovered{0};
    double                totalExecutionMs = 0;
};

// ============================================================================
// CrashSimulationHarness — the test runner
// ============================================================================
class CrashSimulationHarness {
public:
    CrashSimulationHarness();
    ~CrashSimulationHarness();

    // Non-copyable
    CrashSimulationHarness(const CrashSimulationHarness&) = delete;
    CrashSimulationHarness& operator=(const CrashSimulationHarness&) = delete;

    // ---- Fault Point Registration ----
    PatchResult registerFaultPoint(const char* name,
                                    const char* subsystem,
                                    const char* file = nullptr,
                                    uint32_t line = 0);
    PatchResult unregisterFaultPoint(const char* name);
    bool isFaultPointRegistered(const char* name) const;

    // ---- Fault Check (called from production code) ----
    // Returns a FaultInjectionResult; if faultFired is true, caller must
    // simulate the fault (e.g., return error, throw simulated exception, etc.)
    FaultInjectionResult checkFaultPoint(const char* name);

    // ---- Fault Rules ----
    PatchResult addFaultRule(const FaultRule& rule);
    PatchResult removeFaultRule(uint64_t ruleId);
    PatchResult clearAllRules();
    PatchResult armAllRules();
    PatchResult disarmAllRules();

    // ---- Scenario Management ----
    PatchResult addScenario(const CrashScenario& scenario);
    PatchResult removeScenario(uint64_t scenarioId);

    // Built-in scenarios
    PatchResult installBuiltinScenarios();

    // ---- Execution ----
    ScenarioResult runScenario(uint64_t scenarioId);
    std::vector<ScenarioResult> runAllScenarios();

    // Custom scenario with inline validation
    ScenarioResult runCustomScenario(
        const char* name,
        const std::vector<FaultRule>& rules,
        std::function<bool()> workload,           // The code to test
        std::function<bool()> validator);          // Post-fault validation

    // ---- Stats & Reporting ----
    HarnessStats getStats() const;
    void resetStats();
    std::string exportResultsJson() const;
    std::string exportSummary() const;

    // ---- Convenience Helpers ----
    // Quick one-shot fault injection at a named point
    PatchResult injectOnce(const char* faultPointName, FaultType type);

    // Simulate OOM for N allocations
    PatchResult simulateOOM(uint32_t allocationCount);

    // Simulate disk failure for journal writes
    PatchResult simulateDiskFailure(uint32_t writeCount);

private:
    uint64_t nextRuleId();
    uint64_t nextScenarioId();
    FaultRule* findRuleForPoint(const char* name);

    mutable std::mutex m_mutex;

    std::unordered_map<std::string, FaultPoint> m_faultPoints;
    std::vector<FaultRule> m_rules;
    std::vector<CrashScenario> m_scenarios;
    std::vector<ScenarioResult> m_results;

    std::atomic<uint64_t> m_nextRuleId{1};
    std::atomic<uint64_t> m_nextScenarioId{1};
    std::atomic<bool>     m_globalArmed{false};

    HarnessStats m_stats;
};

// ============================================================================
// Macro for embedding fault points in production code
// ============================================================================
#ifdef RAWRXD_FAULT_INJECTION_ENABLED
    #define RAWRXD_FAULT_POINT(harness, name) \
        do { \
            auto __fir = (harness).checkFaultPoint(name); \
            if (__fir.faultFired) { \
                /* Handle the simulated fault */ \
            } \
        } while(0)
#else
    #define RAWRXD_FAULT_POINT(harness, name) ((void)0)
#endif

} // namespace Testing
} // namespace RawrXD
