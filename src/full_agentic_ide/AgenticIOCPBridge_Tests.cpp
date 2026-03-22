// =============================================================================
// AgenticIOCPBridge_Tests — Validation suite for async approval gates
// =============================================================================

#include "AgenticIOCPBridge.hpp"
#include "AgenticPlanningOrchestrator.h"

#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

namespace full_agentic_ide {

class AgenticIOCPBridgeTests {
public:
    static bool runAllTests() {
        std::cout << "[IOCP Tests] Starting validation suite...\n";
        
        if (!testInitializeShutdown()) {
            std::cerr << "[IOCP Tests] FAILED: Initialize/Shutdown\n";
            return false;
        }
        std::cout << "[IOCP Tests] ✓ Initialize/Shutdown\n";

        if (!testAsyncQueueing()) {
            std::cerr << "[IOCP Tests] FAILED: Async Queueing\n";
            return false;
        }
        std::cout << "[IOCP Tests] ✓ Async Queueing\n";

        if (!testApprovalCallback()) {
            std::cerr << "[IOCP Tests] FAILED: Approval Callback\n";
            return false;
        }
        std::cout << "[IOCP Tests] ✓ Approval Callback\n";

        if (!testConcurrentLoad()) {
            std::cerr << "[IOCP Tests] FAILED: Concurrent Load\n";
            return false;
        }
        std::cout << "[IOCP Tests] ✓ Concurrent Load\n";

        std::cout << "[IOCP Tests] All tests PASSED\n";
        return true;
    }

private:
    static bool testInitializeShutdown() {
        if (!AgenticIOCPBridge::initialize(2)) {
            return false;
        }
        if (!AgenticIOCPBridge::isActive()) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!AgenticIOCPBridge::shutdown()) {
            return false;
        }
        return true;
    }

    static bool testAsyncQueueing() {
        if (!AgenticIOCPBridge::initialize(2)) {
            return false;
        }

        IOCPWorkItem item{};
        item.planStepId = 1;
        item.riskTier = 1;  // Medium
        item.mutationGate = 2;

        bool queued = AgenticIOCPBridge::queueApprovalRequest(
            item,
            [](const IOCPWorkItem& w, bool approved) {
                // Callback executed on worker thread
                assert(w.planStepId == 1);
            });

        if (!queued) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        int pending = AgenticIOCPBridge::getPendingCount();
        if (pending > 1) {
            // Should process quickly
            return false;
        }

        AgenticIOCPBridge::shutdown();
        return true;
    }

    static bool testApprovalCallback() {
        if (!AgenticIOCPBridge::initialize(1)) {
            return false;
        }

        volatile bool callbackInvoked = false;
        volatile bool approvalResult = false;

        IOCPWorkItem item{};
        item.planStepId = 42;
        item.riskTier = 0;  // Low

        auto callback = [&](const IOCPWorkItem& w, bool approved) {
            callbackInvoked = true;
            approvalResult = approved;
        };

        bool queued = AgenticIOCPBridge::queueApprovalRequest(item, callback);
        if (!queued) {
            return false;
        }

        // Wait for callback
        int waitCount = 0;
        while (!callbackInvoked && waitCount < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waitCount++;
        }

        if (!callbackInvoked) {
            return false;
        }

        AgenticIOCPBridge::shutdown();
        return true;
    }

    static bool testConcurrentLoad() {
        if (!AgenticIOCPBridge::initialize(4)) {
            return false;
        }

        volatile int completedCount = 0;

        // Queue multiple work items
        for (int i = 0; i < 10; ++i) {
            IOCPWorkItem item{};
            item.planStepId = i;
            item.riskTier = i % 3;

            auto callback = [&completedCount](const IOCPWorkItem&, bool) {
                ++completedCount;
            };

            AgenticIOCPBridge::queueApprovalRequest(item, callback);
        }

        // Wait for all to complete
        int waitCount = 0;
        while (completedCount < 10 && waitCount < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waitCount++;
        }

        if (completedCount < 10) {
            return false;
        }

        AgenticIOCPBridge::shutdown();
        return true;
    }
};

} // namespace full_agentic_ide

// Entry point for smoke testing
extern "C" int RunIOCPBridgeTests() {
    using namespace full_agentic_ide;
    return AgenticIOCPBridgeTests::runAllTests() ? 0 : 1;
}
