#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <mutex>

/**
 * @file Win32IDE_SharedState.cpp
 * @brief Batch 3 (19/118): Shared State Synchronization.
 * Synchronizes multi-agent access to the global inference state.
 */

namespace RawrXD::Sync {

// Resolves: Sync_CreateNamedMutex
extern "C" HANDLE Sync_CreateNamedMutex(const char* name) {
    LOG_INFO("[Sync] Creating named mutex: " + std::string(name));
    return CreateMutexA(NULL, FALSE, name);
}

// Resolves: Sync_LockState
extern "C" bool Sync_LockState(HANDLE hMutex, uint32_t timeout_ms) {
    DWORD result = WaitForSingleObject(hMutex, timeout_ms);
    return (result == WAIT_OBJECT_0);
}

// Resolves: Sync_UnlockState
extern "C" void Sync_UnlockState(HANDLE hMutex) {
    ReleaseMutex(hMutex);
}

} // namespace RawrXD::Sync
