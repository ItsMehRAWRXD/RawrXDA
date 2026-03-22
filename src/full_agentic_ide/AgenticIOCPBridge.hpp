#pragma once

// =============================================================================
// AgenticIOCPBridge — Zero-copy Async Orchestration via Win32 IOCP
// =============================================================================
// Decouples agentic mutations (inference thread) from UI approvals (main thread).
// Uses I/O Completion Ports for lock-free notification of approval gate events.
//
// Architecture:
//   [Inference Engine] → PlanStep → Async Queue (lock-free)
//                                         ↓
//                                   [IOCP Worker Pool]
//                                         ↓
//                                   [Approval Triage]
//                                         ↓
//                       [Post Async Callback to UI]
//
// Zero-copy: Plan steps pinned in BAR-mapped memory, no malloc between threads.
// =============================================================================

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <functional>

namespace full_agentic_ide {

struct IOCPWorkItem {
    uint64_t planStepId;           // References step in shared ring buffer
    uint32_t riskTier;             // Risk tier (enum)
    uint32_t mutationGate;         // Current gate state
    void*    userData;             // Optional context pointer
};

using ApprovalCallback = std::function<void(const IOCPWorkItem&, bool approved)>;

class AgenticIOCPBridge {
public:
    /// Initialize IOCP: create completion port + worker threads
    /// numWorkers: typically = CPU cores / 2 (inference + UI both compete)
    static bool initialize(int numWorkers = 4);

    /// Shutdown IOCP: flush pending work, join threads
    static bool shutdown();

    /// Queue an approval request (async)
    /// Calls approvalCallback on worker thread once decision is made.
    static bool queueApprovalRequest(
        const IOCPWorkItem& item,
        ApprovalCallback approvalCallback);

    /// Register UI callback for async notification (runs on worker pool)
    static void setUINotificationCallback(
        std::function<void(uint64_t planStepId, bool wasApproved)> callback);

    /// Check if bridge is active
    static bool isActive();

    /// Statistics
    static int getPendingCount();
    static int getCompletedCount();

private:
    AgenticIOCPBridge() = delete;
};

} // namespace full_agentic_ide
