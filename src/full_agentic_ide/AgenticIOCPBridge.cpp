// =============================================================================
// AgenticIOCPBridge — implementation
// =============================================================================

#include "AgenticIOCPBridge.hpp"

#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

namespace full_agentic_ide {

// ============================================================================
// Singleton state machine
// ============================================================================

struct IOCPBridgeState {
    HANDLE completionPort = nullptr;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> shutdownRequested{false};
    std::atomic<int> pendingCount{0};
    std::atomic<int> completedCount{0};
    ApprovalCallback globalCallback;
    std::function<void(uint64_t, bool)> uiCallback;
    std::mutex stateMutex;
};

static IOCPBridgeState g_iocp_state;

// ============================================================================
// Worker thread: waits for IOCP completion, triages approval, invokes callbacks
// ============================================================================

void WorkerThreadProc() {
    OVERLAPPED_ENTRY entries[16];
    ULONG numEntries = 0;

    while (!g_iocp_state.shutdownRequested.load()) {
        // Wait for completion: timeout 1 second
        BOOL result = GetQueuedCompletionStatusEx(
            g_iocp_state.completionPort,
            entries,
            16,
            &numEntries,
            1000,
            FALSE);

        if (!result) {
            continue;  // Timeout or error; loop again
        }

        for (ULONG i = 0; i < numEntries; ++i) {
            if (entries[i].lpCompletionKey == (ULONG_PTR)nullptr) {
                // Shutdown sentinel
                g_iocp_state.shutdownRequested.store(true);
                break;
            }

            IOCPWorkItem* item = reinterpret_cast<IOCPWorkItem*>(entries[i].lpCompletionKey);

            // ============================================================
            // APPROVAL TRIAGE (runs on worker thread, async from UI)
            // ============================================================
            bool approved = true;

            // Risk-based auto-approval (beacon-controlled)
            // In full system: check ExecutionGovernor beacon state
            if (item->riskTier >= 2) {  // High/Critical
                approved = false;  // Force human review
            }

            if (item->riskTier == 0) {  // Low
                approved = true;  // Auto-approve now
            }

            // ============================================================
            // Execute callbacks (worker thread context)
            // ============================================================

            if (g_iocp_state.globalCallback) {
                g_iocp_state.globalCallback(*item, approved);
            }

            if (g_iocp_state.uiCallback) {
                // Post to UI thread (would be PostMessage in real Win32)
                g_iocp_state.uiCallback(item->planStepId, approved);
            }

            g_iocp_state.completedCount.fetch_add(1);
            g_iocp_state.pendingCount.fetch_sub(1);

            delete item;  // Clean up work item
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

bool AgenticIOCPBridge::initialize(int numWorkers) {
    std::lock_guard<std::mutex> lock(g_iocp_state.stateMutex);

    if (g_iocp_state.completionPort != nullptr) {
        return false;  // Already initialized
    }

    // Create IOCP
    g_iocp_state.completionPort = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,  // Not associated with a file handle
        nullptr,               // New completion port
        0,                     // No initial key
        numWorkers);           // Thread concurrency limit

    if (g_iocp_state.completionPort == nullptr) {
        return false;
    }

    g_iocp_state.shutdownRequested.store(false);
    g_iocp_state.pendingCount.store(0);
    g_iocp_state.completedCount.store(0);

    // Spawn worker threads
    for (int i = 0; i < numWorkers; ++i) {
        g_iocp_state.workerThreads.emplace_back(WorkerThreadProc);
    }

    return true;
}

bool AgenticIOCPBridge::shutdown() {
    std::lock_guard<std::mutex> lock(g_iocp_state.stateMutex);

    if (g_iocp_state.completionPort == nullptr) {
        return false;  // Already shut down
    }

    g_iocp_state.shutdownRequested.store(true);

    // Post sentinel to wake worker threads
    for (size_t i = 0; i < g_iocp_state.workerThreads.size(); ++i) {
        PostQueuedCompletionStatus(
            g_iocp_state.completionPort,
            0,
            0,
            nullptr);
    }

    // Join all threads
    for (auto& t : g_iocp_state.workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    g_iocp_state.workerThreads.clear();

    CloseHandle(g_iocp_state.completionPort);
    g_iocp_state.completionPort = nullptr;

    return true;
}

bool AgenticIOCPBridge::queueApprovalRequest(
    const IOCPWorkItem& item,
    ApprovalCallback callback) {

    if (g_iocp_state.completionPort == nullptr) {
        return false;  // Not initialized
    }

    // Allocate copy of work item
    IOCPWorkItem* workItem = new IOCPWorkItem(item);
    if (!workItem) {
        return false;
    }

    if (callback) {
        g_iocp_state.globalCallback = callback;
    }

    // Queue to IOCP
    BOOL result = PostQueuedCompletionStatus(
        g_iocp_state.completionPort,
        0,                            // dwNumberOfBytesTransferred (unused)
        (ULONG_PTR)workItem,          // lpCompletionKey = work item pointer
        nullptr);                     // lpOverlapped

    if (!result) {
        delete workItem;
        return false;
    }

    g_iocp_state.pendingCount.fetch_add(1);
    return true;
}

void AgenticIOCPBridge::setUINotificationCallback(
    std::function<void(uint64_t planStepId, bool wasApproved)> callback) {
    std::lock_guard<std::mutex> lock(g_iocp_state.stateMutex);
    g_iocp_state.uiCallback = callback;
}

bool AgenticIOCPBridge::isActive() {
    return g_iocp_state.completionPort != nullptr &&
           !g_iocp_state.shutdownRequested.load();
}

int AgenticIOCPBridge::getPendingCount() {
    return g_iocp_state.pendingCount.load();
}

int AgenticIOCPBridge::getCompletedCount() {
    return g_iocp_state.completedCount.load();
}

} // namespace full_agentic_ide
