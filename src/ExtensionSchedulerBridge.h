// ExtensionSchedulerBridge.h — Integration layer between Extension API and ExecutionScheduler v2
// Enables extensions to submit tasks to the lock-free scheduler
#pragma once

#include "extensions/extension_api_bridge.h"
#include "ExecutionScheduler_v2.h"

namespace RawrXD {

// ============================================================================
// EXTENSION SCHEDULER BRIDGE
// ============================================================================
// Connects ExtensionAPIBridge with ExecutionScheduler_v2
// - Extensions can submit async tasks to the lock-free scheduler
// - Commands are executed with proper phase budgets
// - Events are emitted through the scheduler's telemetry pipeline

class ExtensionSchedulerBridge {
public:
    static ExtensionSchedulerBridge& instance();
    
    // Initialize bridge with scheduler instance
    void initialize(ExecutionScheduler_v2* scheduler);
    
    // ------------------------------------------------------------------------
    // EXTENSION API INTEGRATION
    // ------------------------------------------------------------------------
    
    // Submit extension command to scheduler (async)
    // Returns task ID for tracking
    TaskID submitExtensionCommand(
        const std::string& extensionId,
        const std::string& commandId,
        std::function<void()> work,
        uint32_t priority = 5
    );
    
    // Submit extension command with dependencies
    TaskID submitExtensionCommandWithDeps(
        const std::string& extensionId,
        const std::string& commandId,
        std::function<void()> work,
        const std::vector<TaskID>& dependencies,
        uint32_t priority = 5
    );
    
    // ------------------------------------------------------------------------
    // EVENT INTEGRATION
    // ------------------------------------------------------------------------
    
    // Emit extension event through scheduler's telemetry pipeline
    void emitExtensionEvent(
        const std::string& extensionId,
        const std::string& eventType,
        const std::string& jsonPayload
    );
    
    // Subscribe to scheduler events from extension
    uint64_t subscribeToSchedulerEvents(
        const std::string& extensionId,
        std::function<void(const TelemetryEvent&)> callback
    );
    
    // ------------------------------------------------------------------------
    // PHASE BUDGET INTEGRATION
    // ------------------------------------------------------------------------
    
    // Execute extension work within phase budget
    bool executeWithBudget(
        ExecutionPhase phase,
        const std::string& extensionId,
        std::function<void()> work
    );
    
    // Query remaining budget for phase
    MonotonicClock::Duration getRemainingBudget(ExecutionPhase phase) const;
    
    // ------------------------------------------------------------------------
    // STATUS
    // ------------------------------------------------------------------------
    
    bool isInitialized() const { return scheduler_ != nullptr; }
    
    struct Stats {
        uint64_t commandsSubmitted;
        uint64_t commandsCompleted;
        uint64_t eventsEmitted;
        uint64_t budgetExceeded;
    };
    Stats getStats() const;
    
private:
    ExtensionSchedulerBridge() = default;
    ~ExtensionSchedulerBridge() = default;
    
    ExecutionScheduler_v2* scheduler_{nullptr};
    std::atomic<bool> initialized_{false};
    
    // Extension task tracking
    std::unordered_map<std::string, std::vector<TaskID>> extensionTasks_;
    mutable std::shared_mutex tasksMutex_;
    
    // Event subscriptions
    std::unordered_map<uint64_t, std::pair<std::string, std::function<void(const TelemetryEvent&)>>> eventSubscriptions_;
    std::atomic<uint64_t> nextSubscriptionId_{1};
    mutable std::shared_mutex subscriptionsMutex_;
    
    // Stats
    mutable std::atomic<uint64_t> commandsSubmitted_{0};
    mutable std::atomic<uint64_t> commandsCompleted_{0};
    mutable std::atomic<uint64_t> eventsEmitted_{0};
    mutable std::atomic<uint64_t> budgetExceeded_{0};
};

// ============================================================================
// C API FOR EXTENSIONS
// ============================================================================

extern "C" {
    // Initialize bridge from extension
    __declspec(dllexport) bool ExtensionSchedulerBridge_Init();
    
    // Submit async task
    __declspec(dllexport) uint64_t ExtensionSchedulerBridge_SubmitTask(
        const char* extensionId,
        const char* commandId,
        void (*work)(void*),
        void* userData,
        uint32_t priority
    );
    
    // Submit task with dependencies
    __declspec(dllexport) uint64_t ExtensionSchedulerBridge_SubmitTaskWithDeps(
        const char* extensionId,
        const char* commandId,
        void (*work)(void*),
        void* userData,
        const uint64_t* dependencies,
        size_t depCount,
        uint32_t priority
    );
    
    // Emit event
    __declspec(dllexport) void ExtensionSchedulerBridge_EmitEvent(
        const char* extensionId,
        const char* eventType,
        const char* jsonPayload
    );
    
    // Check if task completed
    __declspec(dllexport) bool ExtensionSchedulerBridge_IsTaskComplete(uint64_t taskId);
    
    // Wait for task
    __declspec(dllexport) void ExtensionSchedulerBridge_WaitForTask(uint64_t taskId, uint32_t timeoutMs);
}

} // namespace RawrXD
