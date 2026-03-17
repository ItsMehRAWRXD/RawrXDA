// agent_self_healing_orchestrator.hpp — Full Self-Healing Pipeline
//
// Wires together:
//   1. AgenticHotpatchOrchestrator (failure detection)
//   2. AgentSelfRepair (MASM64 self-patch kernel bridge)
//   3. UnifiedHotpatchManager (three-layer coordination)
//
// Pipeline: detect bug → classify → select fix strategy → apply via MASM64
//           → CRC verify → log → (rollback if verification fails)
//
// This is the proof: the agent uses its own MASM64 to fix its own bugs.
//
// Architecture: C++20, no Qt, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "agent_self_repair.hpp"
#include "agentic_hotpatch_orchestrator.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include <cstdint>
#include <mutex>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// SelfHealAction — Describes what the orchestrator did to heal itself
// ---------------------------------------------------------------------------
struct SelfHealAction {
    enum Type : uint8_t {
        None              = 0,
        PatternFix        = 1,  // Fixed a known bug pattern
        TrampolineRedirect = 2, // Redirected a buggy function
        CASPointerFix     = 3,  // Atomically fixed a stale pointer
        NopSledCleanup    = 4,  // Cleaned up NOP sled artifacts
        OutputRewrite     = 5,  // Rewrote malformed output
        BiasInjection     = 6,  // Injected token bias to fix behavior
        FullRollback      = 7,  // Rolled back all patches (abort)
    };

    Type        type;
    uint64_t    patchId;
    uintptr_t   address;
    const char* description;
    bool        verified;       // CRC verified after apply
    uint64_t    timestamp;

    static SelfHealAction make(Type t, const char* desc) {
        SelfHealAction a{};
        a.type        = t;
        a.description = desc;
        a.verified    = false;
        a.timestamp   = 0;
        return a;
    }
};

// ---------------------------------------------------------------------------
// SelfHealReport — Summary of one self-healing cycle
// ---------------------------------------------------------------------------
struct SelfHealReport {
    uint64_t    cycleId;
    uint64_t    startTime;
    uint64_t    endTime;
    size_t      bugsDetected;
    size_t      bugsFixed;
    size_t      bugsFailed;
    size_t      patchesVerified;
    size_t      patchesCorrupted;
    bool        rollbackTriggered;

    static SelfHealReport begin(uint64_t id) {
        SelfHealReport r{};
        r.cycleId     = id;
        r.startTime   = GetTickCount64();
        return r;
    }
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------
typedef void (*SelfHealCycleCallback)(const SelfHealReport* report, void* userData);

// ---------------------------------------------------------------------------
// AgentSelfHealingOrchestrator — The full pipeline
// ---------------------------------------------------------------------------
class AgentSelfHealingOrchestrator {
public:
    static AgentSelfHealingOrchestrator& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;

    // ---- The Main Event ----
    // Run one complete self-healing cycle:
    //   1. Scan .text for all registered bug signatures (MASM64 kernel)
    //   2. For each bug found with autoFix: apply patch (MASM64 kernel)
    //   3. CRC32 verify each applied patch (MASM64 kernel)
    //   4. If any verification fails: rollback that patch (MASM64 kernel)
    //   5. Fire callbacks, update statistics
    SelfHealReport runHealingCycle();

    // ---- Targeted Healing ----
    // Heal a specific function by installing a trampoline to a fixed version.
    PatchResult healFunction(void* buggyFn, void* fixedFn);

    // Heal a callback table entry atomically.
    PatchResult healCallbackSlot(void** slot, void* expected, void* fixedHandler);

    // ---- Agent Output Healing ----
    // Detect + fix invalid agent output using the hotpatch orchestrator.
    // This is how the agent fixes its own inference bugs.
    CorrectionOutcome healAgentOutput(const char* output, size_t outputLen,
                                       const char* prompt, size_t promptLen,
                                       char* correctedOutput, size_t correctedCapacity);

    // ---- History ----
    const std::vector<SelfHealReport>& getHistory() const;
    const std::vector<SelfHealAction>& getActions() const;

    // ---- Configuration ----
    void setAutoHealEnabled(bool enabled);
    bool isAutoHealEnabled() const;
    void setMaxPatchesPerCycle(int max);
    void setVerifyAfterPatch(bool verify);

    // ---- Callbacks ----
    void registerCycleCallback(SelfHealCycleCallback cb, void* userData);

    // ---- Diagnostics ----
    size_t dumpFullReport(char* buffer, size_t bufferSize) const;

private:
    AgentSelfHealingOrchestrator();
    ~AgentSelfHealingOrchestrator();
    AgentSelfHealingOrchestrator(const AgentSelfHealingOrchestrator&) = delete;
    AgentSelfHealingOrchestrator& operator=(const AgentSelfHealingOrchestrator&) = delete;

    void notifyCycle(const SelfHealReport& report);

    // State
    mutable std::mutex                  m_mutex;
    bool                                m_initialized;
    bool                                m_autoHealEnabled;
    bool                                m_verifyAfterPatch;
    int                                 m_maxPatchesPerCycle;
    uint64_t                            m_cycleCounter;
    std::vector<SelfHealReport>         m_history;
    std::vector<SelfHealAction>         m_actions;

    // Callbacks
    struct CycleCB { SelfHealCycleCallback fn; void* userData; };
    std::vector<CycleCB>                m_cycleCallbacks;
};
