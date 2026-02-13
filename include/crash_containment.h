// ============================================================================
// crash_containment.h — Enterprise Crash Boundary Guard
// ============================================================================
// Global unhandled exception filter with:
//   - MiniDump generation (DbgHelp.dll dynamic load)
//   - SelfPatch rollback on fault (quarantine bad patches)
//   - Structured crash report (registers, stack, modules, patches)
//   - Thread-safe crash serialization
//
// Pattern: PatchResult-style, no exceptions, no STL in crash path.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>

namespace RawrXD {
namespace Crash {

// ============================================================================
// Crash Dump Type
// ============================================================================

enum class DumpType : uint32_t {
    Mini        = 0,    // Fast — basic thread + stack only
    Normal      = 1,    // Includes heap summary
    Full        = 2     // Complete process memory (large file)
};

// ============================================================================
// Crash Report — written alongside minidump
// ============================================================================

struct CrashReport {
    uint32_t exceptionCode;
    void*    exceptionAddress;
    uint64_t registers[16];     // RAX-R15
    uint64_t rip;
    uint64_t rsp;
    uint64_t rbp;
    uint32_t threadId;
    uint32_t processId;
    uint64_t timestampMs;       // Epoch ms
    char     moduleName[260];   // Faulting module
    char     dumpPath[260];     // Path to .dmp file
    char     logPath[260];      // Path to .log file

    // Patch state at crash time
    int32_t  activePatchCount;
    uint32_t lastAppliedPatchId;
    bool     patchRollbackAttempted;
    bool     patchRollbackSucceeded;
};

// ============================================================================
// Configuration
// ============================================================================

struct CrashConfig {
    DumpType    dumpType        = DumpType::Normal;
    const char* dumpDirectory   = ".";           // Dir for .dmp and .log files
    bool        enableMiniDump  = true;
    bool        enablePatchRollback = true;       // Rollback SelfPatch on crash
    bool        enablePatchQuarantine = true;     // Quarantine faulting patches
    bool        showMessageBox  = true;           // Show crash dialog
    bool        terminateAfterDump = true;        // Exit after dump

    // Callback for custom crash handling (e.g., telemetry upload)
    void (*onCrashCallback)(const CrashReport* report, void* userData) = nullptr;
    void* callbackUserData = nullptr;
};

// ============================================================================
// API
// ============================================================================

/// Install the crash containment boundary as the global exception filter.
/// Must be called once at process start (before any MASM subsystem init).
/// Replaces any previously installed filter.
void Install(const CrashConfig& config = {});

/// Uninstall the crash boundary (restores previous filter).
void Uninstall();

/// Get the last crash report (valid after a crash was caught).
const CrashReport* GetLastCrashReport();

/// Manually trigger a crash dump (for diagnostics, not a real crash).
/// Returns true if dump was written successfully.
bool WriteDiagnosticDump(const char* reason);

/// Register a patch ID as "active" so crash handler can rollback.
/// Called by SelfPatch engine after applying a patch.
void RegisterActivePatch(uint32_t patchId);

/// Unregister a patch (rolled back or removed).
void UnregisterActivePatch(uint32_t patchId);

/// Quarantine a patch ID (will never be applied again this session).
void QuarantinePatch(uint32_t patchId);

/// Check if a patch is quarantined.
bool IsPatchQuarantined(uint32_t patchId);

} // namespace Crash
} // namespace RawrXD
