// ============================================================================
// rawrxd_telemetry_exports.cpp — Telemetry Exports Implementation
// ============================================================================
#include "rawrxd_telemetry_exports.h"
#include <windows.h>
#include <atomic>

// When the MASM telemetry kernel is linked, it provides the canonical
// implementations of all telemetry symbols.  This C++ unit only supplies
// fallback definitions for builds that do NOT link the ASM kernel.
// ============================================================================
#if !defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM)

// ============================================================================
// Atomic Metric Counters — fallback C++ definitions
// ============================================================================
extern "C" {
alignas(64) uint64_t g_MetricTableStart = 0;
alignas(64) uint64_t g_Counter_Inference = 0;
alignas(64) uint64_t g_Counter_ScsiFails = 0;
alignas(64) uint64_t g_Counter_AgentLoop = 0;
alignas(64) uint64_t g_Counter_BytePatches = 0;
alignas(64) uint64_t g_Counter_MemPatches = 0;
alignas(64) uint64_t g_Counter_ServerPatches = 0;
alignas(64) uint64_t g_Counter_FlushOps = 0;
alignas(64) uint64_t g_Counter_Errors = 0;
alignas(64) uint64_t g_MetricTableEnd = 0;
}

// ============================================================================
// Telemetry Lifecycle — fallback C++ implementation
// ============================================================================
static HANDLE g_hLogFile = INVALID_HANDLE_VALUE;

extern "C" uint64_t UTC_InitTelemetry(const char* logFilePath) {
    const char* path = logFilePath ? logFilePath : "rawrxd_kernel.log";
    g_hLogFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_hLogFile == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }
    SetFilePointer(g_hLogFile, 0, nullptr, FILE_END);
    return 0;
}

extern "C" uint64_t UTC_ShutdownTelemetry(void) {
    if (g_hLogFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hLogFile);
        g_hLogFile = INVALID_HANDLE_VALUE;
    }
    return 0;
}

// ============================================================================
// Atomic Counter Operations — fallback C++ implementation
// ============================================================================
extern "C" uint64_t UTC_IncrementCounter(volatile uint64_t* counterAddr) {
    if (!counterAddr) return 0;
    return InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(counterAddr));
}

extern "C" uint64_t UTC_DecrementCounter(volatile uint64_t* counterAddr) {
    if (!counterAddr) return 0;
    return InterlockedDecrement64(reinterpret_cast<volatile LONG64*>(counterAddr));
}

extern "C" uint64_t UTC_ReadCounter(const volatile uint64_t* counterAddr) {
    if (!counterAddr) return 0;
    return *counterAddr;
}

extern "C" uint64_t UTC_ResetCounter(volatile uint64_t* counterAddr) {
    if (!counterAddr) return 0;
    return InterlockedExchange64(reinterpret_cast<volatile LONG64*>(counterAddr), 0);
}

#endif // !RAWRXD_LINK_TELEMETRY_KERNEL_ASM
