// ============================================================================
// crash_containment.cpp — Enterprise Crash Boundary Guard Implementation
// ============================================================================
// Global SEH filter → MiniDump + SelfPatch rollback + structured report.
//
// Pattern: PatchResult-style, no exceptions, no STL in crash path.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "crash_containment.h"
#include "patch_rollback_ledger.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <atomic>

// DbgHelp — dynamic load to avoid hard dependency
typedef BOOL(WINAPI* MiniDumpWriteDump_t)(
    HANDLE hProcess, DWORD ProcessId, HANDLE hFile,
    DWORD DumpType, PVOID ExceptionParam,
    PVOID UserStreamParam, PVOID CallbackParam);

// MINIDUMP_EXCEPTION_INFORMATION (manual, avoid dbghelp.h include)
#pragma pack(push, 8)
struct MINIDUMP_EXCEPTION_INFO_MANUAL {
    DWORD   ThreadId;
    PVOID   ExceptionPointers;
    BOOL    ClientPointers;
};
#pragma pack(pop)

// Minidump type flags (matching dbghelp.h values)
static constexpr DWORD MINIDUMP_NORMAL         = 0x00000000;
static constexpr DWORD MINIDUMP_WITH_DATA_SEGS = 0x00000001;
static constexpr DWORD MINIDUMP_WITH_FULL_MEM  = 0x00000002;
static constexpr DWORD MINIDUMP_WITH_HANDLE    = 0x00000004;

namespace RawrXD {
namespace Crash {

// ============================================================================
// Module State
// ============================================================================

static CrashConfig      g_config;
static CrashReport      g_lastReport;
static LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
static std::atomic<bool> g_installed{false};
static std::atomic<bool> g_inCrashHandler{false};

// Active patches tracking (lock-free, crash-safe)
static constexpr int MAX_ACTIVE_PATCHES = 64;
static uint32_t g_activePatchIds[MAX_ACTIVE_PATCHES];
static std::atomic<int> g_activePatchCount{0};

// Quarantined patches
static constexpr int MAX_QUARANTINED = 64;
static uint32_t g_quarantinedPatchIds[MAX_QUARANTINED];
static std::atomic<int> g_quarantinedCount{0};

// ============================================================================
// Helpers
// ============================================================================

static void safeStrCopy(char* dst, const char* src, size_t maxLen) {
    if (!dst || !src || maxLen == 0) return;
    size_t i = 0;
    while (i < maxLen - 1 && src[i]) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static uint64_t getCurrentTimestampMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

static void buildDumpPath(char* out, size_t outLen, const char* dir, const char* ext) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(out, outLen, "%s\\rawrxd_crash_%04d%02d%02d_%02d%02d%02d.%s",
             dir, st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, ext);
}

// ============================================================================
// MiniDump Writer (dynamic DbgHelp.dll)
// ============================================================================

static bool writeMiniDump(const char* path, EXCEPTION_POINTERS* ep, DWORD dumpType) {
    HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
    if (!hDbgHelp) return false;

    auto pWriteDump = (MiniDumpWriteDump_t)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (!pWriteDump) {
        FreeLibrary(hDbgHelp);
        return false;
    }

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        FreeLibrary(hDbgHelp);
        return false;
    }

    MINIDUMP_EXCEPTION_INFO_MANUAL mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    BOOL ok = pWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                         hFile, dumpType, &mei, nullptr, nullptr);

    CloseHandle(hFile);
    FreeLibrary(hDbgHelp);
    return ok != FALSE;
}

// ============================================================================
// Crash Log Writer
// ============================================================================

static void writeCrashLog(const char* path, const CrashReport& report) {
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    char buf[2048];
    DWORD written = 0;

    int len = snprintf(buf, sizeof(buf),
        "=== RawrXD IDE Crash Report ===\r\n"
        "Timestamp: %llu\r\n"
        "Thread: %u  Process: %u\r\n"
        "Exception: 0x%08X at 0x%016llX\r\n"
        "Module: %s\r\n\r\n"
        "Registers:\r\n"
        "  RIP = 0x%016llX  RSP = 0x%016llX  RBP = 0x%016llX\r\n"
        "  RAX = 0x%016llX  RBX = 0x%016llX  RCX = 0x%016llX\r\n"
        "  RDX = 0x%016llX  RSI = 0x%016llX  RDI = 0x%016llX\r\n"
        "  R8  = 0x%016llX  R9  = 0x%016llX  R10 = 0x%016llX\r\n"
        "  R11 = 0x%016llX  R12 = 0x%016llX  R13 = 0x%016llX\r\n"
        "  R14 = 0x%016llX  R15 = 0x%016llX\r\n\r\n"
        "Active Patches: %d\r\n"
        "Last Applied Patch ID: %u\r\n"
        "Patch Rollback Attempted: %s\r\n"
        "Patch Rollback Succeeded: %s\r\n\r\n"
        "Dump File: %s\r\n",
        (unsigned long long)report.timestampMs,
        report.threadId, report.processId,
        report.exceptionCode, (unsigned long long)report.rip,
        report.moduleName,
        (unsigned long long)report.rip,
        (unsigned long long)report.rsp,
        (unsigned long long)report.rbp,
        (unsigned long long)report.registers[0],
        (unsigned long long)report.registers[1],
        (unsigned long long)report.registers[2],
        (unsigned long long)report.registers[3],
        (unsigned long long)report.registers[4],
        (unsigned long long)report.registers[5],
        (unsigned long long)report.registers[6],
        (unsigned long long)report.registers[7],
        (unsigned long long)report.registers[8],
        (unsigned long long)report.registers[9],
        (unsigned long long)report.registers[10],
        (unsigned long long)report.registers[11],
        (unsigned long long)report.registers[12],
        (unsigned long long)report.registers[13],
        (unsigned long long)report.registers[14],
        (unsigned long long)report.registers[15],
        report.activePatchCount,
        report.lastAppliedPatchId,
        report.patchRollbackAttempted ? "Yes" : "No",
        report.patchRollbackSucceeded ? "Yes" : "No",
        report.dumpPath);

    WriteFile(hFile, buf, (DWORD)len, &written, nullptr);
    CloseHandle(hFile);
}

// ============================================================================
// Faulting Module Detection
// ============================================================================

static void findFaultingModule(void* addr, char* out, size_t outLen) {
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &hMod)) {
        GetModuleFileNameA(hMod, out, (DWORD)outLen);
    } else {
        safeStrCopy(out, "<unknown>", outLen);
    }
}

// ============================================================================
// SelfPatch Emergency Rollback
// ============================================================================

static void emergencyPatchRollback(CrashReport& report) {
    report.patchRollbackAttempted = true;
    report.patchRollbackSucceeded = false;

    // Use the PatchRollbackLedger for safe rollback
    auto& ledger = RawrXD::Patch::PatchRollbackLedger::Global();
    int rolledBack = ledger.emergencyRollbackAll();

    report.patchRollbackSucceeded = (rolledBack >= 0);

    // Quarantine all active patches
    if (g_config.enablePatchQuarantine) {
        int count = g_activePatchCount.load(std::memory_order_acquire);
        for (int i = 0; i < count && i < MAX_ACTIVE_PATCHES; ++i) {
            QuarantinePatch(g_activePatchIds[i]);
        }
    }
}

// ============================================================================
// Global Exception Filter
// ============================================================================

static LONG WINAPI CathedralCrashFilter(EXCEPTION_POINTERS* ep) {
    // Prevent re-entrancy (crash in crash handler)
    bool expected = false;
    if (!g_inCrashHandler.compare_exchange_strong(expected, true)) {
        // Already in crash handler — just terminate
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Build crash report
    CrashReport& report = g_lastReport;
    memset(&report, 0, sizeof(report));

    report.exceptionCode = ep->ExceptionRecord->ExceptionCode;
    report.exceptionAddress = ep->ExceptionRecord->ExceptionAddress;
    report.rip = ep->ContextRecord->Rip;
    report.rsp = ep->ContextRecord->Rsp;
    report.rbp = ep->ContextRecord->Rbp;
    report.threadId = GetCurrentThreadId();
    report.processId = GetCurrentProcessId();
    report.timestampMs = getCurrentTimestampMs();

    // Save all 16 GP registers
    report.registers[0]  = ep->ContextRecord->Rax;
    report.registers[1]  = ep->ContextRecord->Rbx;
    report.registers[2]  = ep->ContextRecord->Rcx;
    report.registers[3]  = ep->ContextRecord->Rdx;
    report.registers[4]  = ep->ContextRecord->Rsi;
    report.registers[5]  = ep->ContextRecord->Rdi;
    report.registers[6]  = ep->ContextRecord->R8;
    report.registers[7]  = ep->ContextRecord->R9;
    report.registers[8]  = ep->ContextRecord->R10;
    report.registers[9]  = ep->ContextRecord->R11;
    report.registers[10] = ep->ContextRecord->R12;
    report.registers[11] = ep->ContextRecord->R13;
    report.registers[12] = ep->ContextRecord->R14;
    report.registers[13] = ep->ContextRecord->R15;
    // Remaining slots reserved

    // Faulting module
    findFaultingModule(ep->ExceptionRecord->ExceptionAddress,
                       report.moduleName, sizeof(report.moduleName));

    // Patch state
    report.activePatchCount = g_activePatchCount.load(std::memory_order_acquire);
    // Last applied is the most recent in the active list
    if (report.activePatchCount > 0) {
        report.lastAppliedPatchId = g_activePatchIds[report.activePatchCount - 1];
    }

    // 1. Emergency patch rollback (before dump, to stabilize state)
    if (g_config.enablePatchRollback) {
        emergencyPatchRollback(report);
    }

    // 2. Write MiniDump
    if (g_config.enableMiniDump) {
        buildDumpPath(report.dumpPath, sizeof(report.dumpPath),
                      g_config.dumpDirectory, "dmp");

        DWORD dumpType = MINIDUMP_NORMAL;
        switch (g_config.dumpType) {
            case DumpType::Mini:   dumpType = MINIDUMP_NORMAL; break;
            case DumpType::Normal: dumpType = MINIDUMP_NORMAL | MINIDUMP_WITH_DATA_SEGS; break;
            case DumpType::Full:   dumpType = MINIDUMP_WITH_FULL_MEM; break;
        }

        writeMiniDump(report.dumpPath, ep, dumpType);
    }

    // 3. Write crash log
    buildDumpPath(report.logPath, sizeof(report.logPath),
                  g_config.dumpDirectory, "log");
    writeCrashLog(report.logPath, report);

    // 4. Custom callback
    if (g_config.onCrashCallback) {
        g_config.onCrashCallback(&report, g_config.callbackUserData);
    }

    // 5. Show message box (if enabled)
    if (g_config.showMessageBox) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "RawrXD IDE has encountered a fatal error.\n\n"
            "Exception: 0x%08X\n"
            "Address: 0x%016llX\n"
            "Module: %s\n\n"
            "A crash dump has been saved to:\n%s\n\n"
            "Active patches rolled back: %s",
            report.exceptionCode,
            (unsigned long long)report.rip,
            report.moduleName,
            report.dumpPath,
            report.patchRollbackSucceeded ? "Yes" : "No");

        MessageBoxA(nullptr, msg, "RawrXD IDE — Crash Report",
                    MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    }

    // 6. Output to debugger
    OutputDebugStringA("[RawrXD] CRASH: Dump written to ");
    OutputDebugStringA(report.dumpPath);
    OutputDebugStringA("\n");

    g_inCrashHandler.store(false, std::memory_order_release);

    return g_config.terminateAfterDump ?
        EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

// ============================================================================
// Public API
// ============================================================================

void Install(const CrashConfig& config) {
    g_config = config;
    g_previousFilter = SetUnhandledExceptionFilter(CathedralCrashFilter);
    g_installed.store(true, std::memory_order_release);
    OutputDebugStringA("[RawrXD] Crash containment boundary installed\n");
}

void Uninstall() {
    if (g_installed.exchange(false)) {
        SetUnhandledExceptionFilter(g_previousFilter);
        g_previousFilter = nullptr;
        OutputDebugStringA("[RawrXD] Crash containment boundary uninstalled\n");
    }
}

const CrashReport* GetLastCrashReport() {
    return &g_lastReport;
}

bool WriteDiagnosticDump(const char* reason) {
    char path[260];
    buildDumpPath(path, sizeof(path), g_config.dumpDirectory, "diag.dmp");

    // No exception pointers for diagnostic dump
    HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
    if (!hDbgHelp) return false;

    auto pWriteDump = (MiniDumpWriteDump_t)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (!pWriteDump) { FreeLibrary(hDbgHelp); return false; }

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) { FreeLibrary(hDbgHelp); return false; }

    BOOL ok = pWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                         hFile, MINIDUMP_NORMAL, nullptr, nullptr, nullptr);

    CloseHandle(hFile);
    FreeLibrary(hDbgHelp);

    if (ok) {
        OutputDebugStringA("[RawrXD] Diagnostic dump: ");
        OutputDebugStringA(path);
        OutputDebugStringA(" reason: ");
        OutputDebugStringA(reason ? reason : "(none)");
        OutputDebugStringA("\n");
    }
    return ok != FALSE;
}

void RegisterActivePatch(uint32_t patchId) {
    int idx = g_activePatchCount.load(std::memory_order_acquire);
    if (idx < MAX_ACTIVE_PATCHES) {
        g_activePatchIds[idx] = patchId;
        g_activePatchCount.fetch_add(1, std::memory_order_release);
    }
}

void UnregisterActivePatch(uint32_t patchId) {
    int count = g_activePatchCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (g_activePatchIds[i] == patchId) {
            // Shift remaining
            for (int j = i; j < count - 1; ++j) {
                g_activePatchIds[j] = g_activePatchIds[j + 1];
            }
            g_activePatchCount.fetch_sub(1, std::memory_order_release);
            return;
        }
    }
}

void QuarantinePatch(uint32_t patchId) {
    // Check if already quarantined
    int count = g_quarantinedCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (g_quarantinedPatchIds[i] == patchId) return;
    }
    if (count < MAX_QUARANTINED) {
        g_quarantinedPatchIds[count] = patchId;
        g_quarantinedCount.fetch_add(1, std::memory_order_release);
    }
}

bool IsPatchQuarantined(uint32_t patchId) {
    int count = g_quarantinedCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (g_quarantinedPatchIds[i] == patchId) return true;
    }
    return false;
}

} // namespace Crash
} // namespace RawrXD
