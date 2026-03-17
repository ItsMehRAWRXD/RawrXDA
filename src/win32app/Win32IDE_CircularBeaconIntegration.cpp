// ============================================================================
// Win32IDE_CircularBeaconIntegration.cpp — Phase 14: MMF Beacon State Sync
// ============================================================================
// Real-time beacon state synchronization between MASM64 and Win32 GUI:
//   - Memory Mapped File (MMF) state sharing
//   - 30-second timeout heartbeat monitoring
//   - Cross-process state synchronization
//   - Beacon health monitoring and recovery
// ============================================================================

#include "BATCH2_CONTEXT.h"
#include <windows.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <thread>
#include <windows.h>
#include <ctime>

// ============================================================================
// GLOBAL SYNCHRONIZATION STATE
// ============================================================================

static BeaconState g_localBeaconState = { STAGE_INIT, 0, nullptr, nullptr, 0, "", false };
static BeaconState* g_sharedBeaconState = nullptr;
static HANDLE g_hBeaconMmf = nullptr;
static HANDLE g_hSyncMutex = nullptr;
static std::atomic<bool> g_syncActive = false;
static std::thread g_syncThread;
static std::mutex g_syncMutex;

// ============================================================================
// SYNCHRONIZATION HELPERS
// ============================================================================

bool AcquireBeaconLock(DWORD timeoutMs = 5000) {
    if (!g_hSyncMutex) return false;
    return WaitForSingleObject(g_hSyncMutex, timeoutMs) == WAIT_OBJECT_0;
}

void ReleaseBeaconLock() {
    if (g_hSyncMutex) {
        ReleaseMutex(g_hSyncMutex);
    }
}

bool ValidateBeaconMmf() {
    if (!g_hBeaconMmf || g_hBeaconMmf == INVALID_HANDLE_VALUE) {
        strcpy_s(g_localBeaconState.statusMessage, "Invalid beacon MMF handle");
        return false;
    }

    if (!g_sharedBeaconState) {
        strcpy_s(g_localBeaconState.statusMessage, "Beacon MMF not mapped");
        return false;
    }

    return true;
}

bool IsBeaconAlive() {
    if (!ValidateBeaconMmf()) return false;

    DWORD currentTick = GetTickCount();
    DWORD lastHeartbeat = g_sharedBeaconState->lastHeartbeat;

    // Check if heartbeat is within timeout window
    if (currentTick - lastHeartbeat > BEACON_TIMEOUT_MS) {
        sprintf_s(g_localBeaconState.statusMessage, "Beacon heartbeat timeout (%dms)", currentTick - lastHeartbeat);
        return false;
    }

    // Check if beacon process is still running
    if (g_sharedBeaconState->processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, g_sharedBeaconState->processId);
        if (hProcess) {
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode)) {
                CloseHandle(hProcess);
                if (exitCode != STILL_ACTIVE) {
                    sprintf_s(g_localBeaconState.statusMessage, "Beacon process terminated (exit code: %d)", exitCode);
                    return false;
                }
            }
            CloseHandle(hProcess);
        }
    }

    return g_sharedBeaconState->isAlive;
}

// ============================================================================
// MMF INITIALIZATION AND MANAGEMENT
// ============================================================================

CommandResult BeaconIntegration_InitializeMmf() {
    CommandResult result = { false, "", 0 };

    if (g_hBeaconMmf) {
        strcpy_s(result.message, "Beacon MMF already initialized");
        result.success = true;
        return result;
    }

    // Create synchronization mutex
    g_hSyncMutex = CreateMutexA(nullptr, FALSE, "RawrXD_BeaconSync_Mutex");
    if (!g_hSyncMutex) {
        strcpy_s(result.message, "Failed to create beacon sync mutex");
        return result;
    }

    // Create or open MMF for beacon state
    g_hBeaconMmf = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(BeaconState),
        "RawrXD_BeaconState_MMF"
    );

    if (!g_hBeaconMmf) {
        CloseHandle(g_hSyncMutex);
        g_hSyncMutex = nullptr;
        strcpy_s(result.message, "Failed to create beacon MMF");
        return result;
    }

    // Map the MMF
    g_sharedBeaconState = (BeaconState*)MapViewOfFile(
        g_hBeaconMmf,
        FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(BeaconState)
    );

    if (!g_sharedBeaconState) {
        CloseHandle(g_hBeaconMmf);
        CloseHandle(g_hSyncMutex);
        g_hBeaconMmf = nullptr;
        g_hSyncMutex = nullptr;
        strcpy_s(result.message, "Failed to map beacon MMF");
        return result;
    }

    // Initialize local state
    g_localBeaconState.processId = GetCurrentProcessId();
    g_localBeaconState.isAlive = true;
    g_localBeaconState.lastHeartbeat = GetTickCount();
    strcpy_s(g_localBeaconState.statusMessage, "Beacon integration initialized");

    strcpy_s(result.message, "Beacon MMF initialized successfully");
    result.success = true;
    return result;
}

CommandResult BeaconIntegration_StartSync() {
    CommandResult result = { false, "", 0 };

    if (g_syncActive) {
        strcpy_s(result.message, "Beacon sync already active");
        result.success = true;
        return result;
    }

    if (!ValidateBeaconMmf()) {
        strcpy_s(result.message, g_localBeaconState.statusMessage);
        return result;
    }

    g_syncActive = true;

    // Start synchronization thread
    g_syncThread = std::thread([]() {
        while (g_syncActive) {
            {
                std::lock_guard<std::mutex> lock(g_syncMutex);

                if (AcquireBeaconLock()) {
                    // Update local heartbeat
                    g_localBeaconState.lastHeartbeat = GetTickCount();

                    // Sync state with shared MMF
                    if (g_sharedBeaconState) {
                        // Copy our state to shared memory
                        *g_sharedBeaconState = g_localBeaconState;

                        // Read any updates from other processes
                        g_localBeaconState = *g_sharedBeaconState;
                    }

                    ReleaseBeaconLock();
                }
            }

            // Check beacon health
            if (!IsBeaconAlive()) {
                // Attempt recovery
                BeaconIntegration_RecoverBeacon();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));
        }
    });

    g_syncThread.detach();

    strcpy_s(result.message, "Beacon state synchronization started");
    result.success = true;
    return result;
}

CommandResult BeaconIntegration_StopSync() {
    CommandResult result = { false, "", 0 };

    if (!g_syncActive) {
        strcpy_s(result.message, "Beacon sync not active");
        result.success = true;
        return result;
    }

    g_syncActive = false;

    // Wait for sync thread to finish
    if (g_syncThread.joinable()) {
        g_syncThread.join();
    }

    strcpy_s(result.message, "Beacon state synchronization stopped");
    result.success = true;
    return result;
}

// ============================================================================
// BEACON STATE MANAGEMENT
// ============================================================================

CommandResult BeaconIntegration_UpdateState(BeaconStage newStage, const char* statusMessage) {
    CommandResult result = { false, "", 0 };

    if (!ValidateBeaconMmf()) {
        strcpy_s(result.message, g_localBeaconState.statusMessage);
        return result;
    }

    std::lock_guard<std::mutex> lock(g_syncMutex);

    if (!AcquireBeaconLock()) {
        strcpy_s(result.message, "Failed to acquire beacon lock for state update");
        return result;
    }

    // Update local state
    g_localBeaconState.currentStage = newStage;
    g_localBeaconState.lastHeartbeat = GetTickCount();
    if (statusMessage) {
        strcpy_s(g_localBeaconState.statusMessage, statusMessage);
    }

    // Sync to shared memory
    if (g_sharedBeaconState) {
        *g_sharedBeaconState = g_localBeaconState;
    }

    ReleaseBeaconLock();

    sprintf_s(result.message, "Beacon state updated to stage %d", (int)newStage);
    result.success = true;
    return result;
}

CommandResult BeaconIntegration_GetState(BeaconState* outState) {
    CommandResult result = { false, "", 0 };

    if (!outState) {
        strcpy_s(result.message, "Invalid output state pointer");
        return result;
    }

    if (!ValidateBeaconMmf()) {
        strcpy_s(result.message, g_localBeaconState.statusMessage);
        return result;
    }

    std::lock_guard<std::mutex> lock(g_syncMutex);

    if (!AcquireBeaconLock()) {
        strcpy_s(result.message, "Failed to acquire beacon lock for state read");
        return result;
    }

    // Read from shared memory
    if (g_sharedBeaconState) {
        *outState = *g_sharedBeaconState;
    } else {
        *outState = g_localBeaconState;
    }

    ReleaseBeaconLock();

    strcpy_s(result.message, "Beacon state retrieved successfully");
    result.success = true;
    return result;
}

CommandResult BeaconIntegration_WaitForStage(BeaconStage targetStage, DWORD timeoutMs) {
    CommandResult result = { false, "", 0 };

    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

        if (elapsed.count() > timeoutMs) {
            sprintf_s(result.message, "Timeout waiting for beacon stage %d", (int)targetStage);
            return result;
        }

        BeaconState currentState;
        CommandResult getResult = BeaconIntegration_GetState(&currentState);
        if (!getResult.success) {
            strcpy_s(result.message, getResult.message);
            return result;
        }

        if (currentState.currentStage == targetStage) {
            sprintf_s(result.message, "Beacon reached target stage %d", (int)targetStage);
            result.success = true;
            return result;
        }

        if (!IsBeaconAlive()) {
            strcpy_s(result.message, "Beacon died while waiting for stage");
            return result;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// BEACON HEALTH MONITORING AND RECOVERY
// ============================================================================

CommandResult BeaconIntegration_MonitorHealth() {
    CommandResult result = { false, "", 0 };

    if (!IsBeaconAlive()) {
        strcpy_s(result.message, "Beacon health check failed");
        result.code = 1;
        return result;
    }

    BeaconState currentState;
    CommandResult getResult = BeaconIntegration_GetState(&currentState);
    if (!getResult.success) {
        strcpy_s(result.message, getResult.message);
        result.code = 2;
        return result;
    }

    // Check for stage progression timeout
    static BeaconStage lastStage = STAGE_INIT;
    static DWORD lastStageChange = GetTickCount();

    if (currentState.currentStage != lastStage) {
        lastStage = currentState.currentStage;
        lastStageChange = GetTickCount();
    } else {
        DWORD timeSinceLastChange = GetTickCount() - lastStageChange;
        if (timeSinceLastChange > BEACON_TIMEOUT_MS) {
            sprintf_s(result.message, "Beacon stuck in stage %d for %dms", (int)lastStage, timeSinceLastChange);
            result.code = 3;
            return result;
        }
    }

    strcpy_s(result.message, "Beacon health check passed");
    result.success = true;
    return result;
}

CommandResult BeaconIntegration_RecoverBeacon() {
    CommandResult result = { false, "", 0 };

    sprintf_s(g_localBeaconState.statusMessage, "Attempting beacon recovery");

    // Try to restart beacon process
    if (g_sharedBeaconState && g_sharedBeaconState->processId != 0) {
        // Check if process is still running
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, g_sharedBeaconState->processId);
        if (hProcess) {
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                // Process is still running, just update heartbeat
                g_sharedBeaconState->lastHeartbeat = GetTickCount();
                g_sharedBeaconState->isAlive = true;
                CloseHandle(hProcess);

                strcpy_s(result.message, "Beacon heartbeat restored");
                result.success = true;
                return result;
            }
            CloseHandle(hProcess);
        }
    }

    // Process is dead, attempt restart
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char cmdLine[] = "RawrXD_Beacon.exe --recovery";

    if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {

        // Wait for beacon to initialize
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Update shared state with new process ID
        if (g_sharedBeaconState) {
            g_sharedBeaconState->processId = pi.dwProcessId;
            g_sharedBeaconState->isAlive = true;
            g_sharedBeaconState->lastHeartbeat = GetTickCount();
            g_sharedBeaconState->currentStage = STAGE_INIT;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        strcpy_s(result.message, "Beacon recovery successful - new process started");
        result.success = true;
    } else {
        sprintf_s(result.message, "Beacon recovery failed - could not start new process (error: %d)", GetLastError());
    }

    return result;
}

// ============================================================================
// CROSS-PROCESS COMMUNICATION
// ============================================================================

CommandResult BeaconIntegration_SendCommand(const char* command, char* response, size_t responseSize) {
    CommandResult result = { false, "", 0 };

    if (!command || !response || responseSize == 0) {
        strcpy_s(result.message, "Invalid command parameters");
        return result;
    }

    if (!ValidateBeaconMmf()) {
        strcpy_s(result.message, g_localBeaconState.statusMessage);
        return result;
    }

    std::lock_guard<std::mutex> lock(g_syncMutex);

    if (!AcquireBeaconLock()) {
        strcpy_s(result.message, "Failed to acquire beacon lock for command");
        return result;
    }

    // Send command through MMF
    if (g_sharedBeaconState) {
        // Use status message field for command passing
        strcpy_s(g_sharedBeaconState->statusMessage, command);

        // Set a flag to indicate command is pending
        g_sharedBeaconState->currentStage = (BeaconStage)((int)g_sharedBeaconState->currentStage | 0x8000);
    }

    ReleaseBeaconLock();

    // Wait for response with timeout
    auto startTime = std::chrono::high_resolution_clock::now();
    const DWORD commandTimeout = 10000; // 10 seconds for commands

    while (true) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

        if (elapsed.count() > commandTimeout) {
            strcpy_s(result.message, "Command timeout");
            return result;
        }

        std::lock_guard<std::mutex> lock(g_syncMutex);
        if (AcquireBeaconLock()) {
            if (g_sharedBeaconState) {
                // Check if command flag is cleared (response ready)
                if (((int)g_sharedBeaconState->currentStage & 0x8000) == 0) {
                    strcpy_s(response, responseSize, g_sharedBeaconState->statusMessage);
                    ReleaseBeaconLock();
                    strcpy_s(result.message, "Command executed successfully");
                    result.success = true;
                    return result;
                }
            }
            ReleaseBeaconLock();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ============================================================================
// MASTER SYNCHRONIZATION FUNCTION
// ============================================================================

CommandResult BeaconIntegration_SyncState() {
    CommandResult result = { false, "", 0 };

    // Initialize MMF if needed
    CommandResult initResult = BeaconIntegration_InitializeMmf();
    if (!initResult.success) {
        strcpy_s(result.message, initResult.message);
        return result;
    }

    // Start synchronization
    CommandResult syncResult = BeaconIntegration_StartSync();
    if (!syncResult.success) {
        strcpy_s(result.message, syncResult.message);
        return result;
    }

    // Wait for beacon to be alive
    auto startTime = std::chrono::high_resolution_clock::now();
    while (true) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

        if (elapsed.count() > BEACON_TIMEOUT_MS) {
            strcpy_s(result.message, "Beacon never became alive during sync");
            return result;
        }

        if (IsBeaconAlive()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Perform initial state sync
    CommandResult updateResult = BeaconIntegration_UpdateState(STAGE_INIT, "Beacon integration synchronized");
    if (!updateResult.success) {
        strcpy_s(result.message, updateResult.message);
        return result;
    }

    strcpy_s(result.message, "Beacon state synchronization completed successfully");
    result.success = true;
    return result;
}

// ============================================================================
// CLEANUP
// ============================================================================

CommandResult BeaconIntegration_Shutdown() {
    CommandResult result = { false, "", 0 };

    // Stop synchronization
    BeaconIntegration_StopSync();

    // Clean up MMF
    if (g_sharedBeaconState) {
        UnmapViewOfFile(g_sharedBeaconState);
        g_sharedBeaconState = nullptr;
    }

    if (g_hBeaconMmf) {
        CloseHandle(g_hBeaconMmf);
        g_hBeaconMmf = nullptr;
    }

    if (g_hSyncMutex) {
        CloseHandle(g_hSyncMutex);
        g_hSyncMutex = nullptr;
    }

    strcpy_s(result.message, "Beacon integration shut down successfully");
    result.success = true;
    return result;
}

// Force symbol export
#pragma comment(linker, "/EXPORT:BeaconIntegration_SyncState")
#pragma comment(linker, "/EXPORT:BeaconIntegration_Shutdown")


