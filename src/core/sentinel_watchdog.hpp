// ============================================================================
// sentinel_watchdog.hpp — Sentinel Watchdog Anti-Tamper System
// ============================================================================
//
// PURPOSE:
//   Real-time .text section integrity monitoring with anti-debug detection.
//   The Sentinel runs a background watchdog thread that continuously hashes
//   the executable .text section via BCrypt SHA-256, comparing against a
//   known-good baseline. If unauthorized modification is detected (i.e.,
//   modification NOT preceded by a deactivate/reactivate cycle from the
//   Shadow-Page Detour system), the Sentinel triggers a cryptographic
//   lockdown: encrypts all workspace files via Camellia-256 and terminates.
//
// INTEGRATION WITH SHADOW-PAGE DETOUR:
//   Authorized hotpatches follow this lifecycle:
//     1. sentinel_deactivate()     — pause watchdog, prevent false alarm
//     2. VirtualProtect → RWX
//     3. asm_hotpatch_atomic_swap  — atomic prologue rewrite
//     4. VirtualProtect → restore
//     5. sentinel_update_baseline  — rehash .text to new known-good state
//     6. sentinel_activate()       — resume monitoring
//
// ANTI-DEBUG CHECKS (performed each watchdog cycle):
//   - PEB.BeingDebugged (gs:[60h]+2)
//   - PEB.NtGlobalFlag  (gs:[60h]+0BCh)
//   - Hardware debug registers DR0-DR3 (via GetThreadContext)
//   - RDTSC timing analysis (delta > 50,000 cycles = debugger stepping)
//
// Architecture: C++20 | Win32 | BCrypt SHA-256 | No exceptions
// Pattern:      Singleton | PatchResult returns | std::mutex
// Threading:    Dedicated watchdog thread (CreateThread)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_SENTINEL_WATCHDOG_HPP
#define RAWRXD_SENTINEL_WATCHDOG_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>

// Forward declarations (avoid circular includes)
struct PatchResult;

// ============================================================================
// Sentinel Configuration Constants
// ============================================================================

#define SENTINEL_POLL_INTERVAL_MS       500     // Watchdog poll interval
#define SENTINEL_SHA256_DIGEST_SIZE     32      // SHA-256 = 256 bits = 32 bytes
#define SENTINEL_RDTSC_THRESHOLD        50000   // Cycles — debugger step detection
#define SENTINEL_MAX_VIOLATIONS         3       // Max violations before lockdown

// ============================================================================
// Sentinel Event Types
// ============================================================================

enum class SentinelEventType : uint32_t {
    None                    = 0x00,
    TextHashMismatch        = 0x01,     // .text section modified without auth
    DebuggerDetected        = 0x02,     // PEB.BeingDebugged or NtGlobalFlag
    HardwareBreakpoint      = 0x04,     // DR0-DR3 non-zero
    TimingAnomaly           = 0x08,     // RDTSC delta exceeds threshold
    LockdownTriggered       = 0x10,     // Workspace encrypted, process terminating
    BaselineUpdated         = 0x20,     // Authorized patch — baseline rehashed
    Activated               = 0x40,     // Watchdog started
    Deactivated             = 0x80      // Watchdog paused (authorized patch window)
};

// ============================================================================
// Sentinel Statistics
// ============================================================================

struct SentinelStats {
    uint64_t    totalChecks;            // Total watchdog cycles completed
    uint64_t    hashMismatches;         // .text hash mismatches detected
    uint64_t    debuggerDetections;     // Debugger presence detections
    uint64_t    hwBreakpointHits;       // Hardware breakpoint detections
    uint64_t    timingAnomalies;        // RDTSC timing anomalies
    uint64_t    baselineUpdates;        // Authorized baseline rehashes
    uint64_t    lockdownsTriggered;     // Full lockdown events
    uint64_t    uptimeMs;              // Total active monitoring time
};

// ============================================================================
// Sentinel Event Log Entry
// ============================================================================

struct SentinelEvent {
    SentinelEventType   type;           // Event classification
    uint64_t            timestamp;      // GetTickCount64() at event time
    uint64_t            rdtscValue;     // RDTSC snapshot at event
    uint32_t            violationCount; // Running violation counter
    char                detail[128];    // Human-readable description
};

// ============================================================================
// SentinelWatchdog — Singleton Anti-Tamper Monitor
// ============================================================================

class SentinelWatchdog {
public:
    static SentinelWatchdog& instance();

    // ---- Lifecycle ----
    // Activate the watchdog thread. Computes initial .text SHA-256 baseline.
    // Returns PatchResult::ok on success, PatchResult::error on failure.
    PatchResult activate();

    // Deactivate the watchdog thread. Called BEFORE an authorized hotpatch.
    // The watchdog will NOT check .text integrity until reactivated.
    PatchResult deactivate();

    // Trigger emergency lockdown: encrypt workspace + log event + ExitProcess.
    // Called when an UNAUTHORIZED .text modification or debugger is detected.
    void triggerLockdown(const char* reason);

    // Update the .text SHA-256 baseline AFTER an authorized hotpatch.
    // Called between atomic_swap and sentinel_activate in the patch lifecycle.
    PatchResult updateBaseline();

    // ---- Query ----
    bool isActive() const;
    SentinelStats getStats() const;
    uint32_t getViolationCount() const;

    // ---- Event Log ----
    // Retrieve the last N events (ring buffer, max 64 entries)
    static constexpr uint32_t kMaxEventLog = 64;
    const SentinelEvent* getEventLog(uint32_t& outCount) const;

private:
    SentinelWatchdog();
    ~SentinelWatchdog();
    SentinelWatchdog(const SentinelWatchdog&) = delete;
    SentinelWatchdog& operator=(const SentinelWatchdog&) = delete;

    // ---- Internal Operations ----
    // Compute SHA-256 of the .text section using BCrypt
    bool computeTextHash(uint8_t outHash[SENTINEL_SHA256_DIGEST_SIZE]);

    // Locate the .text section of the current module
    bool locateTextSection();

    // Anti-debug: check PEB.BeingDebugged
    bool checkPEBDebugger() const;

    // Anti-debug: check PEB.NtGlobalFlag
    bool checkNtGlobalFlag() const;

    // Anti-debug: check hardware debug registers DR0-DR3
    bool checkHardwareBreakpoints() const;

    // Anti-debug: RDTSC timing analysis
    bool checkTimingAnomaly();

    // Log an event to the ring buffer
    void logEvent(SentinelEventType type, const char* detail);

    // Encrypt all workspace files (lockdown response)
    void encryptWorkspace();

    // ---- Watchdog Thread ----
    static DWORD WINAPI WatchdogThreadProc(LPVOID param);
    void watchdogLoop();

    // ---- State ----
    mutable std::mutex          m_mutex;
    std::atomic<bool>           m_active;
    std::atomic<bool>           m_shutdownRequested;
    HANDLE                      m_watchdogThread;
    DWORD                       m_watchdogThreadId;

    // .text section location (cached on first locate)
    void*                       m_textBase;
    size_t                      m_textSize;
    bool                        m_textLocated;

    // SHA-256 baseline hash of the .text section
    uint8_t                     m_baselineHash[SENTINEL_SHA256_DIGEST_SIZE];
    bool                        m_baselineValid;

    // Statistics
    SentinelStats               m_stats;

    // Violation tracking
    std::atomic<uint32_t>       m_violationCount;

    // Event log (ring buffer)
    SentinelEvent               m_eventLog[kMaxEventLog];
    uint32_t                    m_eventLogHead;
    uint32_t                    m_eventLogCount;

    // RDTSC baseline for timing checks
    uint64_t                    m_lastRdtsc;
};

// ============================================================================
// C-ABI Exports (for cross-module / ASM callers)
// ============================================================================
// These are thin wrappers around SentinelWatchdog::instance() methods.
// Used by the Unified Hotpatch Manager and MASM kernels.
// ============================================================================

extern "C" {
    // Activate the sentinel watchdog (computes initial .text baseline)
    int sentinel_activate(void);

    // Deactivate the sentinel watchdog (pause for authorized patch)
    int sentinel_deactivate(void);

    // Trigger emergency lockdown (unauthorized tamper detected)
    void sentinel_trigger(const char* reason);

    // Update .text baseline after authorized hotpatch
    int sentinel_update_baseline(void);

    // Query sentinel state
    int sentinel_is_active(void);
    int sentinel_get_violation_count(void);
}

#endif // RAWRXD_SENTINEL_WATCHDOG_HPP
