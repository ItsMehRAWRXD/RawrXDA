// ExtensionSchedulerBridge.cpp — Integration between Extension API and ExecutionScheduler v2
#include "ExtensionSchedulerBridge.h"
#include <iostream>

namespace RawrXD {

// ============================================================================
// SINGLETON
// ============================================================================

ExtensionSchedulerBridge& ExtensionSchedulerBridge::instance() {
    static ExtensionSchedulerBridge bridge;
    return bridge;
}

void ExtensionSchedulerBridge::initialize(ExecutionScheduler_v2* scheduler) {
    if (!scheduler) return;
    scheduler_ = scheduler;
    initialized_.store(true, std::memory_order_release);
}

// ============================================================================
// EXTENSION COMMAND SUBMISSION
// ============================================================================

TaskID ExtensionSchedulerBridge::submitExtensionCommand(
    const std::string& extensionId,
    const std::string& commandId,
    std::function<void()> work,
    uint32_t priority) {
    
    if (!initialized_.load(std::memory_order_acquire) || !scheduler_) {
        return INVALID_TASK_ID;
    }
    
    // Wrap work with extension tracking
    auto wrappedWork = [this, extensionId, commandId, work]() {
        // Emit start event
        emitExtensionEvent(extensionId, "command.start", 
            "{\"command\":\"" + commandId + "\"}");
        
        // Execute work
        work();
        
        // Emit complete event
        emitExtensionEvent(extensionId, "command.complete",
            "{\"command\":\"" + commandId + "\"}");
        
        commandsCompleted_.fetch_add(1, std::memory_order_relaxed);
    };
    
    // Submit to scheduler
    TaskID id = scheduler_->submit(wrappedWork, priority, {});
    
    // Track task for extension
    {
        std::unique_lock<std::shared_mutex> lock(tasksMutex_);
        extensionTasks_[extensionId].push_back(id);
    }
    
    commandsSubmitted_.fetch_add(1, std::memory_order_relaxed);
    return id;
}

TaskID ExtensionSchedulerBridge::submitExtensionCommandWithDeps(
    const std::string& extensionId,
    const std::string& commandId,
    std::function<void()> work,
    const std::vector<TaskID>& dependencies,
    uint32_t priority) {
    
    if (!initialized_.load(std::memory_order_acquire) || !scheduler_) {
        return INVALID_TASK_ID;
    }
    
    // Wrap work with extension tracking
    auto wrappedWork = [this, extensionId, commandId, work]() {
        emitExtensionEvent(extensionId, "command.start",
            "{\"command\":\"" + commandId + "\"}");
        
        work();
        
        emitExtensionEvent(extensionId, "command.complete",
            "{\"command\":\"" + commandId + "\"}");
        
        commandsCompleted_.fetch_add(1, std::memory_order_relaxed);
    };
    
    // Submit with dependencies
    TaskID id = scheduler_->submit(wrappedWork, priority, dependencies);
    
    {
        std::unique_lock<std::shared_mutex> lock(tasksMutex_);
        extensionTasks_[extensionId].push_back(id);
    }
    
    commandsSubmitted_.fetch_add(1, std::memory_order_relaxed);
    return id;
}

// ============================================================================
// EVENT INTEGRATION
// ============================================================================

void ExtensionSchedulerBridge::emitExtensionEvent(
    const std::string& extensionId,
    const std::string& eventType,
    const std::string& jsonPayload) {
    
    if (!initialized_.load(std::memory_order_acquire) || !scheduler_) {
        return;
    }
    
    // Create telemetry event
    TelemetryEvent event;
    event.timestamp = MonotonicClock::now();
    event.task_id = fnv1a_hash(extensionId.c_str());
    event.event_type = 4; // extension event
    event.worker_id = 0;
    event.duration_ns = 0;
    
    // Record through scheduler's telemetry
    scheduler_->record_telemetry(event);
    
    // Also emit through Extension API Bridge
    using namespace RawrXD::Extensions;
    ExtensionAPIBridge::instance().emitEvent(
        (extensionId + "." + eventType).c_str(),
        jsonPayload.c_str()
    );
    
    eventsEmitted_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t ExtensionSchedulerBridge::subscribeToSchedulerEvents(
    const std::string& extensionId,
    std::function<void(const TelemetryEvent&)> callback) {
    
    std::unique_lock<std::shared_mutex> lock(subscriptionsMutex_);
    uint64_t id = nextSubscriptionId_.fetch_add(1, std::memory_order_relaxed);
    eventSubscriptions_[id] = {extensionId, callback};
    return id;
}

// ============================================================================
// PHASE BUDGET INTEGRATION
// ============================================================================

bool ExtensionSchedulerBridge::executeWithBudget(
    ExecutionPhase phase,
    const std::string& extensionId,
    std::function<void()> work) {
    
    if (!initialized_.load(std::memory_order_acquire) || !scheduler_) {
        return false;
    }
    
    auto& budget = scheduler_->budget_for(phase);
    
    if (!budget.has_budget()) {
        budgetExceeded_.fetch_add(1, std::memory_order_relaxed);
        emitExtensionEvent(extensionId, "budget.exceeded",
            "{\"phase\":" + std::to_string(static_cast<int>(phase)) + "}");
        return false;
    }
    
    // Execute work
    auto start = MonotonicClock::now();
    work();
    auto end = MonotonicClock::now();
    
    // Update budget
    budget.update();
    
    // Emit timing event
    auto duration = MonotonicClock::microseconds(end - start);
    emitExtensionEvent(extensionId, "execution.timing",
        "{\"duration_us\":" + std::to_string(duration) + "}");
    
    return true;
}

MonotonicClock::Duration ExtensionSchedulerBridge::getRemainingBudget(
    ExecutionPhase phase) const {
    
    if (!initialized_.load(std::memory_order_acquire) || !scheduler_) {
        return 0;
    }
    
    return scheduler_->budget_for(phase).remaining_ns();
}

// ============================================================================
// STATS
// ============================================================================

ExtensionSchedulerBridge::Stats ExtensionSchedulerBridge::getStats() const {
    return Stats{
        commandsSubmitted_.load(std::memory_order_relaxed),
        commandsCompleted_.load(std::memory_order_relaxed),
        eventsEmitted_.load(std::memory_order_relaxed),
        budgetExceeded_.load(std::memory_order_relaxed)
    };
}

} // namespace RawrXD

// ============================================================================
// C API IMPLEMENTATION
// ============================================================================

extern "C" {

using namespace RawrXD;

bool ExtensionSchedulerBridge_Init() {
    auto& bridge = ExtensionSchedulerBridge::instance();
    if (bridge.isInitialized()) return true;
    
    // Initialize scheduler if not already done
    auto& scheduler = ExecutionScheduler_v2::Instance();
    bridge.initialize(&scheduler);
    
    return bridge.isInitialized();
}

uint64_t ExtensionSchedulerBridge_SubmitTask(
    const char* extensionId,
    const char* commandId,
    void (*work)(void*),
    void* userData,
    uint32_t priority) {
    
    if (!extensionId || !commandId || !work) return INVALID_TASK_ID;
    
    auto wrappedWork = [work, userData]() {
        work(userData);
    };
    
    return ExtensionSchedulerBridge::instance().submitExtensionCommand(
        extensionId, commandId, wrappedWork, priority);
}

uint64_t ExtensionSchedulerBridge_SubmitTaskWithDeps(
    const char* extensionId,
    const char* commandId,
    void (*work)(void*),
    void* userData,
    const uint64_t* dependencies,
    size_t depCount,
    uint32_t priority) {
    
    if (!extensionId || !commandId || !work) return INVALID_TASK_ID;
    
    std::vector<TaskID> deps;
    if (dependencies && depCount > 0) {
        deps.assign(dependencies, dependencies + depCount);
    }
    
    auto wrappedWork = [work, userData]() {
        work(userData);
    };
    
    return ExtensionSchedulerBridge::instance().submitExtensionCommandWithDeps(
        extensionId, commandId, wrappedWork, deps, priority);
}

void ExtensionSchedulerBridge_EmitEvent(
    const char* extensionId,
    const char* eventType,
    const char* jsonPayload) {
    
    if (!extensionId || !eventType) return;
    
    ExtensionSchedulerBridge::instance().emitExtensionEvent(
        extensionId,
        eventType,
        jsonPayload ? jsonPayload : "{}"
    );
}

bool ExtensionSchedulerBridge_IsTaskComplete(uint64_t taskId) {
    // In production, would query scheduler's task completion status
    // For now, return true if task ID is valid
    return taskId != INVALID_TASK_ID;
}

void ExtensionSchedulerBridge_WaitForTask(uint64_t taskId, uint32_t timeoutMs) {
    // In production, would use scheduler's wait mechanism
    // For now, simple sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    (void)taskId;
}

} // extern "C"
