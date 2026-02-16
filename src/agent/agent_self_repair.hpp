// agent_self_repair.hpp — Enterprise-Safe Agent Self-Repair Engine
//
// Tier-3-compliant live hotpatch manager with deterministic verification.
//
// Architectural guardrails (per security review):
//   1. NO heuristic byte-pattern mutation of arbitrary .text bytes
//   2. ALL repairs are function-level redirections via LiveBinaryPatcher
//   3. Every patch is CRC-guarded, thread-safe, and rollback-capable
//   4. Thread suspension before patching (no torn instruction hazard)
//   5. Telemetry emission on every patch/rollback via UTC kernel
//   6. Deterministic replay checkpoint before and after each patch
//   7. RepairableFunction registry — only pre-registered functions are patchable
//
// The agent can still detect its own bugs (scanning is read-only and safe).
// But it can only REPAIR them by redirecting whole functions to known-good
// fallbacks — never by NOPing instructions or rewriting bytes in-place.
//
// Architecture: C++20 bridge → LiveBinaryPatcher (Layer 5) + MASM64 scan kernel
// Threading: mutex-protected; SuspendThread barrier during patch application
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include "../core/model_memory_hotpatch.hpp"
#include "../core/live_binary_patcher.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// ASM kernel exports — SCAN-ONLY operations (read-only, no mutation)
// ---------------------------------------------------------------------------
#ifdef RAWR_HAS_MASM
extern "C" {
    // Initialize self-patch subsystem (CRC table, journal, critical section)
    int asm_selfpatch_init();

    // Scan a region for a byte pattern (bug signature) — READ-ONLY
    // Returns pointer to static SCAN_RESULT
    void* asm_selfpatch_scan_text(const void* base, size_t length,
                                   const void* pattern, size_t patternLen);

    // Verify CRC32 of a memory region — READ-ONLY
    int asm_selfpatch_verify_crc(uint64_t patchId);

    // Get pointer to statistics structure — READ-ONLY
    void* asm_selfpatch_get_stats();

    // Scan for NOP sleds (returns count found) — READ-ONLY
    int asm_selfpatch_scan_nop_sled(const void* base, size_t length);

    // Atomic 8-byte compare-and-swap patch (for callback table entries only)
    int asm_selfpatch_cas_patch(void* target, uint64_t expected, uint64_t desired);
}

// Telemetry kernel exports are declared in rawrxd_telemetry_exports.h.
// Include it rather than re-declaring to avoid redefinition conflicts.
#include "rawrxd_telemetry_exports.h"
#endif

// ---------------------------------------------------------------------------
// Self-patch telemetry counters (local, separate from ASM kernel counters)
// These are cache-line-padded to prevent false sharing.
// ---------------------------------------------------------------------------
struct alignas(64) SelfPatchTelemetry {
    volatile uint64_t bugsDetected;
    char _pad0[56];
    volatile uint64_t patchesApplied;
    char _pad1[56];
    volatile uint64_t patchesRolledBack;
    char _pad2[56];
    volatile uint64_t crcVerifyFails;
    char _pad3[56];
    volatile uint64_t threadSuspensions;
    char _pad4[56];
    volatile uint64_t replayCheckpoints;
    char _pad5[56];
};

// ---------------------------------------------------------------------------
// ScanResult — C++ mirror of ASM SCAN_RESULT structure
// ---------------------------------------------------------------------------
struct SelfPatchScanResult {
    uint32_t    statusCode;     // SCAN_OK, SCAN_PATTERN_FOUND, etc.
    uint32_t    matchCount;     // Number of pattern matches
    uintptr_t   firstMatch;     // Address of first match (0 if none)
    uint64_t    bytesScanned;   // Total bytes scanned
};

// ---------------------------------------------------------------------------
// SelfPatchStats — C++ mirror of ASM SELFPATCH_STATS
// ---------------------------------------------------------------------------
struct SelfPatchStats {
    uint64_t totalScans;
    uint64_t patternsFound;
    uint64_t patchesApplied;
    uint64_t patchesRolledBack;
    uint64_t patchesFailed;
    uint64_t trampolinesSet;
    uint64_t crcChecksPassed;
    uint64_t crcChecksFailed;
    uint64_t casOperations;
    uint64_t casRetries;
    uint64_t nopSledsFound;
    uint64_t bytesScanned;
};

// ---------------------------------------------------------------------------
// RepairableFunction — Enterprise-safe function-level repair registry entry
// ---------------------------------------------------------------------------
// Instead of scanning for byte patterns and NOPing instructions,
// the agent registers known-fragile functions with a known-good fallback.
// On detection of divergence (CRC mismatch at startup, replay failure,
// or runtime crash), the function is redirected via LiveBinaryPatcher
// trampoline to the fallback. Original code is never modified byte-by-byte.
// ---------------------------------------------------------------------------
struct RepairableFunction {
    const char*     name;               // Human-readable function name
    void*           entry;              // Function entry point
    uint32_t        expectedCRC;        // CRC32 of expected prologue (first 64 bytes)
    void*           fallbackImpl;       // Known-good replacement
    uint32_t        slotId;             // LiveBinaryPatcher slot ID (assigned on register)
    bool            isRedirected;       // Currently redirected to fallback?
    bool            autoRepair;         // Auto-redirect on CRC mismatch?
    uint32_t        severity;           // 0=info, 1=warning, 2=error, 3=critical
};

// ---------------------------------------------------------------------------
// BugSignature — Read-only detection pattern (scan only, no auto-fix bytes)
// ---------------------------------------------------------------------------
// These are used ONLY for scanning and reporting. The agent never applies
// byte-level fixes from signature data. If a signature triggers, the response
// is either: (a) redirect the containing function via trampoline, or
// (b) escalate to user for manual review.
// ---------------------------------------------------------------------------
struct BugSignature {
    const char*     name;               // Human-readable bug name
    const uint8_t*  pattern;            // Byte pattern to search for
    size_t          patternLen;         // Length of pattern
    uint32_t        severity;           // 0=info, 1=warning, 2=error, 3=critical
    bool            escalateOnly;       // true = report, never auto-fix
};

// ---------------------------------------------------------------------------
// BugReport — Result of scanning for one bug signature (detection only)
// ---------------------------------------------------------------------------
struct BugReport {
    const BugSignature* signature;      // Which bug was found
    uintptr_t           address;        // Where it was found
    uint32_t            matchCount;     // How many instances
    bool                redirected;     // Whether function was redirected
    uint32_t            repairSlotId;   // LiveBinaryPatcher slot if redirected
    uint64_t            timestamp;      // When detected

    static BugReport found(const BugSignature* sig, uintptr_t addr, uint32_t count) {
        BugReport r{};
        r.signature    = sig;
        r.address      = addr;
        r.matchCount   = count;
        r.redirected   = false;
        r.repairSlotId = 0;
        r.timestamp    = 0;
        return r;
    }
};

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------
typedef void (*BugDetectedCallback)(const BugReport* report, void* userData);
typedef void (*SelfPatchCallback)(uint64_t patchId, bool success, void* userData);

// ---------------------------------------------------------------------------
// AgentSelfRepair — Main self-healing orchestrator
// ---------------------------------------------------------------------------
class AgentSelfRepair {
public:
    static AgentSelfRepair& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;

    // ---- Bug Signature Registry ----
    // Register a known bug pattern the agent should watch for.
    PatchResult registerBugSignature(const BugSignature& sig);
    size_t getBugSignatureCount() const;
    const std::vector<BugSignature>& getSignatures() const;

    // Load built-in bug signatures (common x64 bugs the agent knows about).
    void loadBuiltinSignatures();

    // ---- Scanning ----
    // Scan the running .text section for all registered bug signatures.
    // Returns total bugs found.
    size_t scanSelf();

    // Scan a specific memory region for all registered signatures.
    size_t scanRegion(const void* base, size_t length);

    // Scan for NOP sleds (accidental code elimination).
    int scanForNopSleds(const void* base, size_t length);

    // ---- RepairableFunction Registry ----
    // Register a function that the agent is allowed to redirect.
    // On CRC mismatch or bug detection, it will be trampolined to fallbackImpl.
    PatchResult registerRepairableFunction(const RepairableFunction& func);
    size_t getRepairableFunctionCount() const;
    const std::vector<RepairableFunction>& getRepairableFunctions() const;

    // ---- Self-Repair (function-level redirection ONLY) ----
    // Verify all registered functions' CRC. If mismatch + autoRepair, redirect.
    PatchResult verifyAndRepairAll();

    // Redirect a single registered function to its fallback via LiveBinaryPatcher.
    // Thread-safe: suspends other threads during trampoline installation.
    PatchResult redirectFunction(uint32_t slotId);

    // Revert a previously redirected function back to its original entry.
    PatchResult revertFunction(uint32_t slotId);

    // Atomic CAS patch for vtable/callback table entries.
    PatchResult casPatchPointer(void** slot, void* expected, void* desired);

    // ---- Verification ----
    // Compute CRC32 of a function's prologue and compare to expected.
    PatchResult verifyFunctionCRC(uint32_t slotId) const;

    // Verify all registered functions.
    PatchResult verifyAllFunctions() const;

    // Verify all active patches via LiveBinaryPatcher integrity check.
    PatchResult verifyAllPatches();

    // ---- Rollback ----
    PatchResult rollbackFunction(uint32_t slotId);
    PatchResult rollbackAll();

    // ---- Statistics ----
    SelfPatchStats getStats() const;
    const SelfPatchTelemetry& getTelemetry() const;
    const std::vector<BugReport>& getReports() const;

    // ---- Callbacks ----
    void registerBugCallback(BugDetectedCallback cb, void* userData);
    void registerPatchCallback(SelfPatchCallback cb, void* userData);

    // ---- Diagnostics ----
    // Get the .text section base and size of the running executable.
    bool getTextSection(uintptr_t* outBase, size_t* outSize) const;

    // Dump all bug reports and patch history to a buffer.
    size_t dumpDiagnostics(char* buffer, size_t bufferSize) const;

private:
    AgentSelfRepair();
    ~AgentSelfRepair();
    AgentSelfRepair(const AgentSelfRepair&) = delete;
    AgentSelfRepair& operator=(const AgentSelfRepair&) = delete;

    // Locate .text section in PE headers
    bool locateTextSection();

    // Compute CRC32 of the first 64 bytes of a function
    uint32_t computeFunctionCRC(void* entry) const;

    // Thread suspension barrier — suspends all threads except current
    // before applying a trampoline, resumes after.
    bool suspendOtherThreads(std::vector<HANDLE>& outHandles);
    void resumeOtherThreads(const std::vector<HANDLE>& handles);

    // Check if an address is on any thread's active stack frame
    bool isAddressOnAnyStack(uintptr_t addr) const;

    // telemetry event to ASM UTC kernel
    void emitTelemetry(const char* event);
    void incrementTelemetryCounter(volatile uint64_t* counter);

    // deterministic replay checkpoint
    void emitReplayCheckpoint(const char* tag, uintptr_t address);

    // Fire callbacks
    void notifyBugDetected(const BugReport& report);
    void notifySelfPatch(uint64_t patchId, bool success);

    // State
    mutable std::mutex              m_mutex;
    bool                            m_initialized;
    uintptr_t                       m_textBase;
    size_t                          m_textSize;
    std::vector<BugSignature>       m_signatures;
    std::vector<RepairableFunction> m_repairables;
    std::vector<BugReport>          m_reports;
    std::vector<uint32_t>           m_activeRedirections; // LiveBinaryPatcher slot IDs
    SelfPatchTelemetry              m_telemetry;

    // Callbacks
    struct BugCB { BugDetectedCallback fn; void* userData; };
    struct PatchCB { SelfPatchCallback fn; void* userData; };
    std::vector<BugCB>              m_bugCallbacks;
    std::vector<PatchCB>            m_patchCallbacks;
};
