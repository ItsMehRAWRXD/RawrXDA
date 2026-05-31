// ============================================================================
// crash_containment.cpp — Enterprise Crash Boundary Guard Implementation
// ============================================================================
// Global SEH filter → MiniDump + SelfPatch rollback + structured report.
//
// Pattern: PatchResult-style, no exceptions, no STL in crash path.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "crash_containment.h"
#include "patch_rollback_ledger.h"

#include <windows.h>
#include <tlhelp32.h>
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

static constexpr DWORD CRASH_DUMP_INLINE = 0;
static constexpr DWORD CRASH_DUMP_WORKER = 1;

namespace RawrXD {
namespace Crash {

// ============================================================================
// Module State
// ============================================================================

static CrashConfig      g_config;  // Plain struct, safe
static CrashReport      g_lastReport;  // Plain struct, safe
static LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;  // Plain pointer, safe

// LAZY SINGLETON PATTERN: Avoid SIOF - std::atomic with non-trivial constructor
inline std::atomic<bool>& GetInstalled() {
    static std::atomic<bool>* inst = new std::atomic<bool>(false);
    return *inst;
}
#define g_installed GetInstalled()

inline std::atomic<bool>& GetInCrashHandler() {
    static std::atomic<bool>* inst = new std::atomic<bool>(false);
    return *inst;
}
#define g_inCrashHandler GetInCrashHandler()

// Active patches tracking (lock-free, crash-safe)
static constexpr int MAX_ACTIVE_PATCHES = 64;
static uint32_t g_activePatchIds[MAX_ACTIVE_PATCHES];  // Plain array, safe

inline std::atomic<int>& GetActivePatchCount() {
    static std::atomic<int>* inst = new std::atomic<int>(0);
    return *inst;
}
#define g_activePatchCount GetActivePatchCount()

// Quarantined patches
static constexpr int MAX_QUARANTINED = 64;
static uint32_t g_quarantinedPatchIds[MAX_QUARANTINED];  // Plain array, safe

inline std::atomic<int>& GetQuarantinedCount() {
    static std::atomic<int>* inst = new std::atomic<int>(0);
    return *inst;
}
#define g_quarantinedCount GetQuarantinedCount()

// Dedicated dump worker state
static HANDLE g_dumpWorkerThread = nullptr;
static HANDLE g_dumpRequestEvent = nullptr;
static HANDLE g_dumpCompleteEvent = nullptr;
static volatile LONG g_dumpWorkerExit = 0;

struct DumpRequest {
    char dumpPath[260];
    DWORD dumpType;
    DWORD crashThreadId;
    bool hasException;
    EXCEPTION_RECORD exceptionRecord;
    CONTEXT contextRecord;
    DWORD writeError;
    uint64_t bytesWritten;
    bool success;
};

static DumpRequest g_dumpRequest;

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

// Ensure dump directory exists so .dmp and .log files can be written and stored for full news
static void ensureDumpDirectoryExists(const char* dir) {
    if (!dir || !dir[0]) return;
    CreateDirectoryA(dir, nullptr);
}

// Append one line to crash_manifest.txt so dmp paths and errors are stored for full news
static void appendCrashManifest(const char* dir, const CrashReport* report) {
    if (!dir || !dir[0] || !report) return;
    char manifestPath[268];
    snprintf(manifestPath, sizeof(manifestPath), "%s\\crash_manifest.txt", dir);
    HANDLE h = CreateFileA(manifestPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    char line[768];
    int len = snprintf(line, sizeof(line), "%llu\t0x%08X\t%s\t%s\n",
                       (unsigned long long)report->timestampMs,
                       (unsigned)report->exceptionCode,
                       report->dumpPath,
                       report->logPath);
    if (len > 0 && (size_t)len < sizeof(line)) {
        DWORD written = 0;
        WriteFile(h, line, (DWORD)(size_t)len, &written, nullptr);
    }
    CloseHandle(h);
}

// ============================================================================
// MiniDump Writer (dynamic DbgHelp.dll)
// ============================================================================

// Re-entrancy guard for crash dump writing
static std::atomic_flag s_dumpInProgress = ATOMIC_FLAG_INIT;
static thread_local bool s_insideDump = false;

// Suspend all other threads to prevent NtGetContextThread deadlock during dump
static void SuspendAllOtherThreads(DWORD currentTid, DWORD currentPid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;
    
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == currentPid && te.th32ThreadID != currentTid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
}

// Resume all other threads after dump is complete
static void ResumeAllOtherThreads(DWORD currentTid, DWORD currentPid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;
    
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == currentPid && te.th32ThreadID != currentTid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
}

static bool writeMiniDumpImpl(const char* path, EXCEPTION_POINTERS* ep, DWORD dumpType,
                              DWORD crashThreadId, DWORD* outLastError, uint64_t* outBytesWritten) {
    if (outLastError) *outLastError = ERROR_SUCCESS;
    if (outBytesWritten) *outBytesWritten = 0;

    // Guard 1: thread-local prevents same-thread recursion
    if (s_insideDump) {
        OutputDebugStringA("[CrashDump] Re-entrant call blocked (thread-local)\n");
        return false;
    }
    s_insideDump = true;
    
    // Guard 2: atomic prevents cross-thread double-dump
    if (s_dumpInProgress.test_and_set()) {
        s_insideDump = false;
        OutputDebugStringA("[CrashDump] Concurrent dump in progress, skipping\n");
        return false;
    }

    DWORD currentTid = GetCurrentThreadId();
    DWORD currentPid = GetCurrentProcessId();
    
    // Guard 3: suspend all other threads to prevent NtGetContextThread deadlock
    SuspendAllOtherThreads(currentTid, currentPid);

    HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
    if (!hDbgHelp) {
        if (outLastError) *outLastError = GetLastError();
        return false;
    }

    auto pWriteDump = (MiniDumpWriteDump_t)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (!pWriteDump) {
        if (outLastError) *outLastError = GetLastError();
        FreeLibrary(hDbgHelp);
        return false;
    }

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outLastError) *outLastError = GetLastError();
        FreeLibrary(hDbgHelp);
        return false;
    }

    MINIDUMP_EXCEPTION_INFO_MANUAL mei;
    mei.ThreadId = crashThreadId ? crashThreadId : GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    DWORD lastErr = ERROR_SUCCESS;
    uint64_t lastBytes = 0;

    const auto attemptDumpWrite = [&](DWORD type, MINIDUMP_EXCEPTION_INFO_MANUAL* info, const char* tag) -> bool {
        SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
        SetEndOfFile(hFile);

        BOOL ok = pWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                             hFile, (DWORD)type, info, nullptr, nullptr);
        const DWORD writeErr = ok ? ERROR_SUCCESS : GetLastError();
        FlushFileBuffers(hFile);

        LARGE_INTEGER dumpSize{};
        if (!GetFileSizeEx(hFile, &dumpSize)) {
            dumpSize.QuadPart = 0;
        }
        lastErr = writeErr;
        lastBytes = (uint64_t)dumpSize.QuadPart;

        if (!ok || dumpSize.QuadPart <= 0) {
            char dbg[256];
            snprintf(dbg, sizeof(dbg),
                     "[RawrXD] MiniDumpWriteDump failed/empty (%s): ok=%lu err=%lu size=%llu\n",
                     tag ? tag : "unknown", (unsigned long)ok, (unsigned long)writeErr,
                     (unsigned long long)dumpSize.QuadPart);
            OutputDebugStringA(dbg);
            return false;
        }

        return true;
    };

    bool wroteDump = attemptDumpWrite((DWORD)dumpType, &mei, "primary");
    if (!wroteDump && dumpType != MINIDUMP_NORMAL) {
        wroteDump = attemptDumpWrite(MINIDUMP_NORMAL, &mei, "fallback-normal-with-exception");
    }
    if (!wroteDump) {
        wroteDump = attemptDumpWrite(MINIDUMP_NORMAL, nullptr, "fallback-normal-no-exception");
    }

    CloseHandle(hFile);
    FreeLibrary(hDbgHelp);
    if (outLastError) *outLastError = lastErr;
    if (outBytesWritten) *outBytesWritten = lastBytes;
    return wroteDump;
}

static bool writeMiniDump(const char* path, EXCEPTION_POINTERS* ep, DWORD dumpType) {
    return writeMiniDumpImpl(path, ep, dumpType, GetCurrentThreadId(), nullptr, nullptr);
}

static DWORD WINAPI dumpWorkerProc(LPVOID) {
    for (;;) {
        DWORD wr = WaitForSingleObject(g_dumpRequestEvent, INFINITE);
        if (wr != WAIT_OBJECT_0) {
            continue;
        }
        if (InterlockedCompareExchange(&g_dumpWorkerExit, 0, 0) != 0) {
            break;
        }

        EXCEPTION_POINTERS ep{};
        EXCEPTION_POINTERS* pep = nullptr;
        if (g_dumpRequest.hasException) {
            ep.ExceptionRecord = &g_dumpRequest.exceptionRecord;
            ep.ContextRecord = &g_dumpRequest.contextRecord;
            pep = &ep;
        }

        g_dumpRequest.success = writeMiniDumpImpl(g_dumpRequest.dumpPath, pep, g_dumpRequest.dumpType,
                                                  g_dumpRequest.crashThreadId, &g_dumpRequest.writeError,
                                                  &g_dumpRequest.bytesWritten);
        SetEvent(g_dumpCompleteEvent);
    }

    SetEvent(g_dumpCompleteEvent);
    return 0;
}

static void installDumpWorkerIfNeeded() {
    if (g_dumpWorkerThread && g_dumpRequestEvent && g_dumpCompleteEvent) {
        return;
    }

    g_dumpRequestEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    g_dumpCompleteEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_dumpRequestEvent || !g_dumpCompleteEvent) {
        if (g_dumpRequestEvent) CloseHandle(g_dumpRequestEvent);
        if (g_dumpCompleteEvent) CloseHandle(g_dumpCompleteEvent);
        g_dumpRequestEvent = nullptr;
        g_dumpCompleteEvent = nullptr;
        return;
    }

    InterlockedExchange(&g_dumpWorkerExit, 0);
    g_dumpWorkerThread = CreateThread(nullptr, 0, dumpWorkerProc, nullptr, 0, nullptr);
    if (!g_dumpWorkerThread) {
        CloseHandle(g_dumpRequestEvent);
        CloseHandle(g_dumpCompleteEvent);
        g_dumpRequestEvent = nullptr;
        g_dumpCompleteEvent = nullptr;
    }
}

static void uninstallDumpWorker() {
    if (!g_dumpWorkerThread) {
        if (g_dumpRequestEvent) { CloseHandle(g_dumpRequestEvent); g_dumpRequestEvent = nullptr; }
        if (g_dumpCompleteEvent) { CloseHandle(g_dumpCompleteEvent); g_dumpCompleteEvent = nullptr; }
        return;
    }

    InterlockedExchange(&g_dumpWorkerExit, 1);
    if (g_dumpRequestEvent) {
        SetEvent(g_dumpRequestEvent);
    }
    WaitForSingleObject(g_dumpWorkerThread, 2000);

    CloseHandle(g_dumpWorkerThread);
    g_dumpWorkerThread = nullptr;
    if (g_dumpRequestEvent) { CloseHandle(g_dumpRequestEvent); g_dumpRequestEvent = nullptr; }
    if (g_dumpCompleteEvent) { CloseHandle(g_dumpCompleteEvent); g_dumpCompleteEvent = nullptr; }
}

static bool writeMiniDumpViaWorker(const char* path, EXCEPTION_POINTERS* ep, DWORD dumpType,
                                   DWORD waitMs, DWORD crashThreadId,
                                   DWORD* outLastError, uint64_t* outBytesWritten) {
    if (outLastError) *outLastError = ERROR_INVALID_STATE;
    if (outBytesWritten) *outBytesWritten = 0;

    if (!g_dumpWorkerThread || !g_dumpRequestEvent || !g_dumpCompleteEvent || !ep ||
        !ep->ExceptionRecord || !ep->ContextRecord || !path) {
        return false;
    }

    memset(&g_dumpRequest, 0, sizeof(g_dumpRequest));
    safeStrCopy(g_dumpRequest.dumpPath, path, sizeof(g_dumpRequest.dumpPath));
    g_dumpRequest.dumpType = dumpType;
    g_dumpRequest.crashThreadId = crashThreadId;
    g_dumpRequest.hasException = true;
    memcpy(&g_dumpRequest.exceptionRecord, ep->ExceptionRecord, sizeof(EXCEPTION_RECORD));
    memcpy(&g_dumpRequest.contextRecord, ep->ContextRecord, sizeof(CONTEXT));

    ResetEvent(g_dumpCompleteEvent);
    SetEvent(g_dumpRequestEvent);
    DWORD wr = WaitForSingleObject(g_dumpCompleteEvent, waitMs ? waitMs : 5000);
    if (wr != WAIT_OBJECT_0) {
        if (outLastError) *outLastError = WAIT_TIMEOUT;
        return false;
    }

    if (outLastError) *outLastError = g_dumpRequest.writeError;
    if (outBytesWritten) *outBytesWritten = g_dumpRequest.bytesWritten;
    return g_dumpRequest.success;
}

// ============================================================================
// Crash Log Writer
// ============================================================================

static void writeCrashLog(const char* path, const CrashReport& report) {
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    auto writeBytes = [&](const char* data, size_t len) {
        if (!data || len == 0)
            return;
        DWORD written = 0;
        WriteFile(hFile, data, (DWORD)len, &written, nullptr);
    };

    auto writeLiteral = [&](const char* s) {
        if (!s)
            return;
        size_t n = 0;
        while (s[n])
            ++n;
        writeBytes(s, n);
    };

    auto writeDecU64 = [&](uint64_t v) {
        char tmp[32];
        int pos = 0;
        if (v == 0) {
            tmp[pos++] = '0';
        } else {
            char rev[32];
            int r = 0;
            while (v > 0 && r < (int)sizeof(rev)) {
                rev[r++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (r > 0)
                tmp[pos++] = rev[--r];
        }
        writeBytes(tmp, (size_t)pos);
    };

    auto writeDecS32 = [&](int32_t v) {
        if (v < 0) {
            writeLiteral("-");
            writeDecU64((uint64_t)(-(int64_t)v));
        } else {
            writeDecU64((uint64_t)v);
        }
    };

    auto writeHexU64 = [&](uint64_t v) {
        static const char kHex[] = "0123456789ABCDEF";
        char out[16];
        for (int i = 15; i >= 0; --i) {
            out[i] = kHex[v & 0xF];
            v >>= 4;
        }
        writeBytes(out, sizeof(out));
    };

    auto writeHexU32 = [&](uint32_t v) {
        static const char kHex[] = "0123456789ABCDEF";
        char out[8];
        for (int i = 7; i >= 0; --i) {
            out[i] = kHex[v & 0xF];
            v >>= 4;
        }
        writeBytes(out, sizeof(out));
    };

    auto writeBoundedCString = [&](const char* s, size_t maxLen) {
        if (!s || maxLen == 0)
            return;
        size_t n = 0;
        while (n < maxLen && s[n])
            ++n;
        if (n > 0)
            writeBytes(s, n);
    };

    writeLiteral("=== RawrXD IDE Crash Report ===\r\n");
    writeLiteral("Timestamp: ");
    writeDecU64(report.timestampMs);
    writeLiteral("\r\nThread: ");
    writeDecU64((uint64_t)report.threadId);
    writeLiteral("  Process: ");
    writeDecU64((uint64_t)report.processId);
    writeLiteral("\r\nException: 0x");
    writeHexU32(report.exceptionCode);
    writeLiteral(" at 0x");
    writeHexU64(report.rip);
    writeLiteral("\r\nModule: ");
    writeBoundedCString(report.moduleName, sizeof(report.moduleName));
    writeLiteral("\r\n\r\nRegisters:\r\n");

    writeLiteral("  RIP = 0x"); writeHexU64(report.rip);
    writeLiteral("  RSP = 0x"); writeHexU64(report.rsp);
    writeLiteral("  RBP = 0x"); writeHexU64(report.rbp);
    writeLiteral("\r\n");

    const char* regNames[16] = {
        "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "R8", "R9",
        "R10", "R11", "R12", "R13", "R14", "R15", "RSV0", "RSV1"
    };
    for (int i = 0; i < 16; ++i) {
        writeLiteral("  ");
        writeLiteral(regNames[i]);
        writeLiteral(" = 0x");
        writeHexU64(report.registers[i]);
        writeLiteral(((i % 3) == 2) ? "\r\n" : "  ");
    }
    writeLiteral("\r\nActive Patches: ");
    writeDecS32(report.activePatchCount);
    writeLiteral("\r\nLast Applied Patch ID: ");
    writeDecU64((uint64_t)report.lastAppliedPatchId);
    writeLiteral("\r\nPatch Rollback Attempted: ");
    writeLiteral(report.patchRollbackAttempted ? "Yes" : "No");
    writeLiteral("\r\nPatch Rollback Succeeded: ");
    writeLiteral(report.patchRollbackSucceeded ? "Yes" : "No");
    writeLiteral("\r\nMiniDump Last Error: ");
    writeDecU64((uint64_t)report.miniDumpLastError);
    writeLiteral("\r\nMiniDump Bytes Written: ");
    writeDecU64(report.miniDumpBytesWritten);
    writeLiteral("\r\nMiniDump Writer: ");
    if (report.miniDumpViaWorkerThread) {
        writeLiteral("dedicated_thread");
    } else {
        writeLiteral("faulting_thread");
    }
    writeLiteral("\r\nMiniDump Fallback Inline: ");
    writeLiteral(report.miniDumpFallbackInline ? "Yes" : "No");
    writeLiteral("\r\n\r\nDump File: ");
    writeBoundedCString(report.dumpPath, sizeof(report.dumpPath));
    writeLiteral("\r\n");

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
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // OutputDebugStringA/W can raise debug-print exceptions as part of normal telemetry.
    // Never treat those as fatal process crashes.
    constexpr DWORD kDbgPrintExceptionC = 0x40010006u;
    constexpr DWORD kDbgPrintExceptionWideC = 0x4001000Au;
    const DWORD exceptionCode = ep->ExceptionRecord->ExceptionCode;
    if (exceptionCode == kDbgPrintExceptionC || exceptionCode == kDbgPrintExceptionWideC) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }

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
    report.miniDumpLastError = ERROR_SUCCESS;
    report.miniDumpBytesWritten = 0;
    report.miniDumpViaWorkerThread = false;
    report.miniDumpFallbackInline = false;

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

    // Ensure dump directory exists so .dmp and .log are stored
    ensureDumpDirectoryExists(g_config.dumpDirectory);

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

        DWORD dumpErr = ERROR_SUCCESS;
        uint64_t dumpBytes = 0;
        bool wroteDump = false;

        if (g_config.useDedicatedDumpThread) {
            wroteDump = writeMiniDumpViaWorker(report.dumpPath, ep, dumpType, g_config.dumpWorkerWaitMs,
                                               report.threadId, &dumpErr, &dumpBytes);
            report.miniDumpViaWorkerThread = true;
        }

        if (!wroteDump) {
            report.miniDumpFallbackInline = report.miniDumpViaWorkerThread;
            wroteDump = writeMiniDumpImpl(report.dumpPath, ep, dumpType, report.threadId, &dumpErr, &dumpBytes);
            report.miniDumpViaWorkerThread = false;
        }

        report.miniDumpLastError = dumpErr;
        report.miniDumpBytesWritten = dumpBytes;

        char dumpDiag[320];
        snprintf(dumpDiag, sizeof(dumpDiag),
                 "[RawrXD] dump_result success=%lu via_worker=%lu fallback_inline=%lu err=%lu bytes=%llu\n",
                 (unsigned long)(wroteDump ? 1 : 0),
                 (unsigned long)(report.miniDumpViaWorkerThread ? 1 : 0),
                 (unsigned long)(report.miniDumpFallbackInline ? 1 : 0),
                 (unsigned long)report.miniDumpLastError,
                 (unsigned long long)report.miniDumpBytesWritten);
        OutputDebugStringA(dumpDiag);
    }

    // 3. Write crash log
    buildDumpPath(report.logPath, sizeof(report.logPath),
                  g_config.dumpDirectory, "log");
    writeCrashLog(report.logPath, report);

    // 3b. Store dump and error paths in manifest for full news
    appendCrashManifest(g_config.dumpDirectory, &report);

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
    ensureDumpDirectoryExists(g_config.dumpDirectory);
    if (g_config.useDedicatedDumpThread) {
        installDumpWorkerIfNeeded();
    }
    g_previousFilter = SetUnhandledExceptionFilter(CathedralCrashFilter);
    g_installed.store(true, std::memory_order_release);
    OutputDebugStringA("[RawrXD] Crash containment boundary installed\n");
}

void Uninstall() {
    if (g_installed.exchange(false)) {
        SetUnhandledExceptionFilter(g_previousFilter);
        g_previousFilter = nullptr;
        uninstallDumpWorker();
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
