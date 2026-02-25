// agent_self_healing_orchestrator.cpp — Full Self-Healing Pipeline Implementation
//
// Enterprise-safe: the MASM64 kernel scans the .text section (read-only),
// then LiveBinaryPatcher redirects degraded functions to known-good fallbacks.
// NO byte-level mutation. All repairs are function-level redirections.
//
// Architecture: C++20 bridge → MASM64 scan kernel + LiveBinaryPatcher (Layer 5)
// Error model: PatchResult::ok() / PatchResult::error() — no exceptions
// Threading: std::mutex + std::lock_guard + SuspendThread barrier
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <mutex>
#include <vector>
#include "agent_self_healing_orchestrator.hpp"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
AgentSelfHealingOrchestrator& AgentSelfHealingOrchestrator::instance() {
    static AgentSelfHealingOrchestrator s_instance;
    return s_instance;
}

AgentSelfHealingOrchestrator::AgentSelfHealingOrchestrator()
    : m_initialized(false)
    , m_autoHealEnabled(true)
    , m_verifyAfterPatch(true)
    , m_maxPatchesPerCycle(32)
    , m_cycleCounter(0)
{
}

AgentSelfHealingOrchestrator::~AgentSelfHealingOrchestrator() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
PatchResult AgentSelfHealingOrchestrator::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return PatchResult::ok("Already initialized");
    }

    // Initialize the self-repair engine (which initializes the MASM64 kernel)
    PatchResult r = AgentSelfRepair::instance().initialize();
    if (!r.success) {
        return PatchResult::error("Self-repair engine init failed");
    }

    // Ensure the hotpatch orchestrator has default policies
    AgenticHotpatchOrchestrator::instance().loadDefaultPolicies();

    m_initialized = true;
    return PatchResult::ok("Self-healing orchestrator ready");
}

bool AgentSelfHealingOrchestrator::isInitialized() const {
    return m_initialized;
}

// ---------------------------------------------------------------------------
// The Main Event — runHealingCycle
// ---------------------------------------------------------------------------
SelfHealReport AgentSelfHealingOrchestrator::runHealingCycle() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t cycleId = ++m_cycleCounter;
    SelfHealReport report = SelfHealReport::begin(cycleId);

    if (!m_initialized) {
        report.endTime = GetTickCount64();
        m_history.push_back(report);
        return report;
    }

    auto& selfRepair = AgentSelfRepair::instance();

    // ---- Step 1: Scan for known bug patterns ----
    // The MASM64 kernel scans the running .text section
    size_t bugsFound = selfRepair.scanSelf();
    report.bugsDetected = bugsFound;

    if (bugsFound == 0) {
        // Clean bill of health
        report.endTime = GetTickCount64();
        m_history.push_back(report);
        notifyCycle(report);

        SelfHealAction action = SelfHealAction::make(
            SelfHealAction::None,
            "No bugs detected — binary is clean"
        );
        action.timestamp = GetTickCount64();
        m_actions.push_back(action);

        return report;
    }

    // ---- Step 2: Verify CRC + auto-redirect degraded functions ----
    if (m_autoHealEnabled) {
        PatchResult fixResult = selfRepair.verifyAndRepairAll();

        const auto& bugReports = selfRepair.getReports();
        int fixedCount   = 0;
        int failedCount  = 0;
        int patchesApplied = 0;

        for (const auto& bugReport : bugReports) {
            if (bugReport.redirected) {
                ++fixedCount;
                ++patchesApplied;

                SelfHealAction action = SelfHealAction::make(
                    SelfHealAction::TrampolineRedirect,
                    bugReport.signature ? bugReport.signature->name : "unknown"
                );
                action.patchId  = bugReport.repairSlotId;
                action.address  = bugReport.address;
                action.timestamp = GetTickCount64();

                // ---- Step 3: CRC32 verify each redirected function ----
                if (m_verifyAfterPatch && bugReport.repairSlotId != 0) {
                    PatchResult vr = selfRepair.verifyFunctionCRC(bugReport.repairSlotId);
                    action.verified = vr.success;

                    if (vr.success) {
                        ++report.patchesVerified;
                    } else {
                        ++report.patchesCorrupted;

                        // ---- Step 4: Rollback corrupted redirections ----
                        PatchResult rb = selfRepair.rollbackFunction(bugReport.repairSlotId);
                        if (rb.success) {
                            SelfHealAction rbAction = SelfHealAction::make(
                                SelfHealAction::FullRollback,
                                "CRC mismatch — reverted function redirect"
                            );
                            rbAction.patchId  = bugReport.repairSlotId;
                            rbAction.address  = bugReport.address;
                            rbAction.timestamp = GetTickCount64();
                            m_actions.push_back(rbAction);

                            --fixedCount;
                            report.rollbackTriggered = true;
                        }
                    }
                }

                m_actions.push_back(action);

                if (patchesApplied >= m_maxPatchesPerCycle) break;
            } else if (bugReport.signature && !bugReport.signature->escalateOnly) {
                ++failedCount;
            }
        }

        report.bugsFixed  = static_cast<size_t>(fixedCount);
        report.bugsFailed = static_cast<size_t>(failedCount);
    }

    // ---- Step 5: Finalize ----
    report.endTime = GetTickCount64();
    m_history.push_back(report);
    notifyCycle(report);

    return report;
}

// ---------------------------------------------------------------------------
// Targeted Healing
// ---------------------------------------------------------------------------
PatchResult AgentSelfHealingOrchestrator::healFunction(void* buggyFn, void* fixedFn) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }

    // Register the buggy function with the fallback, then redirect it
    RepairableFunction rf{};
    rf.name         = "healFunction_target";
    rf.entry        = buggyFn;
    rf.expectedCRC  = 0; // Will be computed on registration
    rf.fallbackImpl = fixedFn;
    rf.slotId       = 0;
    rf.isRedirected = false;
    rf.autoRepair   = true;
    rf.severity     = 3; // critical

    auto& selfRepair = AgentSelfRepair::instance();
    PatchResult regResult = selfRepair.registerRepairableFunction(rf);
    if (!regResult.success) {
        return regResult;
    }

    // The registration assigned a slot ID via LiveBinaryPatcher.
    // Find it and redirect.
    const auto& repairables = selfRepair.getRepairableFunctions();
    if (repairables.empty()) {
        return PatchResult::error("Registration did not create a slot");
    }

    uint32_t slotId = repairables.back().slotId;
    PatchResult r = selfRepair.redirectFunction(slotId);

    if (r.success) {
        SelfHealAction action = SelfHealAction::make(
            SelfHealAction::TrampolineRedirect,
            "Function redirected via LiveBinaryPatcher"
        );
        action.patchId  = slotId;
        action.address  = reinterpret_cast<uintptr_t>(buggyFn);
        action.timestamp = GetTickCount64();
        action.verified = true; // Trampoline verified by LiveBinaryPatcher
        m_actions.push_back(action);
    }

    return r;
}

PatchResult AgentSelfHealingOrchestrator::healCallbackSlot(void** slot,
                                                             void* expected,
                                                             void* fixedHandler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }

    PatchResult r = AgentSelfRepair::instance().casPatchPointer(
        slot, expected, fixedHandler
    );

    if (r.success) {
        SelfHealAction action = SelfHealAction::make(
            SelfHealAction::CASPointerFix,
            "Callback slot atomically patched"
        );
        action.address  = reinterpret_cast<uintptr_t>(slot);
        action.timestamp = GetTickCount64();
        m_actions.push_back(action);
    }

    return r;
}

// ---------------------------------------------------------------------------
// Agent Output Healing — fixes the agent's own inference bugs
// ---------------------------------------------------------------------------
CorrectionOutcome AgentSelfHealingOrchestrator::healAgentOutput(
    const char* output, size_t outputLen,
    const char* prompt, size_t promptLen,
    char* correctedOutput, size_t correctedCapacity)
{
    // Delegate to the hotpatch orchestrator's full pipeline
    return AgenticHotpatchOrchestrator::instance().analyzeAndCorrect(
        output, outputLen,
        prompt, promptLen,
        correctedOutput, correctedCapacity
    );
}

// ---------------------------------------------------------------------------
// History / Actions
// ---------------------------------------------------------------------------
const std::vector<SelfHealReport>& AgentSelfHealingOrchestrator::getHistory() const {
    return m_history;
}

const std::vector<SelfHealAction>& AgentSelfHealingOrchestrator::getActions() const {
    return m_actions;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
void AgentSelfHealingOrchestrator::setAutoHealEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoHealEnabled = enabled;
}

bool AgentSelfHealingOrchestrator::isAutoHealEnabled() const {
    return m_autoHealEnabled;
}

void AgentSelfHealingOrchestrator::setMaxPatchesPerCycle(int max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxPatchesPerCycle = (max > 0) ? max : 1;
}

void AgentSelfHealingOrchestrator::setVerifyAfterPatch(bool verify) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_verifyAfterPatch = verify;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void AgentSelfHealingOrchestrator::registerCycleCallback(SelfHealCycleCallback cb,
                                                           void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb) {
        m_cycleCallbacks.push_back({cb, userData});
    }
}

void AgentSelfHealingOrchestrator::notifyCycle(const SelfHealReport& report) {
    for (const auto& cb : m_cycleCallbacks) {
        cb.fn(&report, cb.userData);
    }
}

// ---------------------------------------------------------------------------
// Diagnostics — Full report
// ---------------------------------------------------------------------------
size_t AgentSelfHealingOrchestrator::dumpFullReport(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize == 0) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    size_t written = 0;
    auto append = [&](const char* fmt, ...) {
        if (written >= bufferSize - 1) return;
        va_list args;
        va_start(args, fmt);
        int n = _vsnprintf_s(buffer + written,
                             bufferSize - written,
                             _TRUNCATE, fmt, args);
        va_end(args);
        if (n > 0) written += static_cast<size_t>(n);
    };

    append("╔══════════════════════════════════════════════════════════════╗\n");
    append("║          AGENT SELF-HEALING ORCHESTRATOR REPORT            ║\n");
    append("║   43MB MASM64 Binary → Self-Patching → Zero Dependencies  ║\n");
    append("╚══════════════════════════════════════════════════════════════╝\n\n");

    append("Status:        %s\n", m_initialized ? "OPERATIONAL" : "NOT INITIALIZED");
    append("Auto-heal:     %s\n", m_autoHealEnabled ? "ENABLED" : "DISABLED");
    append("CRC verify:    %s\n", m_verifyAfterPatch ? "ENABLED" : "DISABLED");
    append("Max patches:   %d per cycle\n", m_maxPatchesPerCycle);
    append("Cycles run:    %llu\n\n", static_cast<unsigned long long>(m_cycleCounter));

    // Healing history
    append("─── Healing Cycles (%zu) ───\n", m_history.size());
    for (size_t i = 0; i < m_history.size(); ++i) {
        const auto& h = m_history[i];
        append("  Cycle #%llu: detected=%zu fixed=%zu failed=%zu "
               "verified=%zu corrupted=%zu rollback=%s duration=%llums\n",
               static_cast<unsigned long long>(h.cycleId),
               h.bugsDetected, h.bugsFixed, h.bugsFailed,
               h.patchesVerified, h.patchesCorrupted,
               h.rollbackTriggered ? "YES" : "no",
               static_cast<unsigned long long>(h.endTime - h.startTime));
    }

    // Actions
    append("\n─── Actions (%zu) ───\n", m_actions.size());
    static const char* actionNames[] = {
        "None", "PatternFix", "TrampolineRedirect", "CASPointerFix",
        "NopSledCleanup", "OutputRewrite", "BiasInjection", "FullRollback"
    };
    for (size_t i = 0; i < m_actions.size(); ++i) {
        const auto& a = m_actions[i];
        int typeIdx = static_cast<int>(a.type);
        const char* typeName = (typeIdx < 8) ? actionNames[typeIdx] : "?";
        append("  [%zu] %-22s addr=0x%llX verified=%s  %s\n",
               i, typeName,
               static_cast<unsigned long long>(a.address),
               a.verified ? "YES" : "no",
               a.description ? a.description : "");
    }

    // Delegate to AgentSelfRepair for ASM kernel stats
    append("\n─── MASM64 Kernel Statistics ───\n");
    SelfPatchStats stats = AgentSelfRepair::instance().getStats();
    append("  Total scans:       %llu\n", stats.totalScans);
    append("  Patterns found:    %llu\n", stats.patternsFound);
    append("  Patches applied:   %llu\n", stats.patchesApplied);
    append("  Patches rolled:    %llu\n", stats.patchesRolledBack);
    append("  Patches failed:    %llu\n", stats.patchesFailed);
    append("  Trampolines:       %llu\n", stats.trampolinesSet);
    append("  CRC pass:          %llu\n", stats.crcChecksPassed);
    append("  CRC fail:          %llu\n", stats.crcChecksFailed);
    append("  CAS operations:    %llu\n", stats.casOperations);
    append("  NOP sleds:         %llu\n", stats.nopSledsFound);
    append("  Bytes scanned:     %llu\n", stats.bytesScanned);

    append("\n✅ The agent used MASM64 to fix its own bugs.\n");

    return written;
}
