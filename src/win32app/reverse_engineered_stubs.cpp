// reverse_engineered_stubs.cpp — Stub implementations when RawrXD_Complete_ReverseEngineered.asm is not linked.
// Used by RawrXD-Win32IDE when the full ASM kernel is excluded (fragment missing struct defs).
// Provides no-op / minimal implementations so the IDE links and runs.

#include "../../include/reverse_engineered_bridge.h"
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void INFINITY_Shutdown(void) {
    (void)0;
}

int Scheduler_Initialize(uint32_t /*workerCount*/, uint32_t /*numaEnabled*/) {
    return 0;
}

void Scheduler_Shutdown(void) {
}

int ConflictDetector_Initialize(uint32_t /*maxResources*/, uint32_t /*checkIntervalMs*/) {
    return 0;
}

int Heartbeat_Initialize(uint16_t /*listenPort*/, uint32_t /*sendIntervalMs*/) {
    return 0;
}

void Heartbeat_Shutdown(void) {
}

uint64_t GetHighResTick(void) {
#ifdef _WIN32
    LARGE_INTEGER t;
    if (QueryPerformanceCounter(&t))
        return (uint64_t)t.QuadPart;
#endif
    return 0;
}

#ifdef __cplusplus
}
#endif
