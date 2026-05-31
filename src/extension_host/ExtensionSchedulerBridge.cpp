#include "ExtensionSchedulerBridge.h"
#include <chrono>
#include <iostream>

namespace {
inline uint64_t NowTicks() {
    return RawrXD::MonotonicClock::now();
}

inline uint64_t ToNanoseconds(uint64_t deltaTicks) {
    return RawrXD::MonotonicClock::nanoseconds(deltaTicks);
}
}

namespace ExtensionScheduler {

ExtensionSchedulerBridge& ExtensionSchedulerBridge::getInstance() {
    static ExtensionSchedulerBridge instance;
    return instance;
}

bool ExtensionSchedulerBridge::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Ensure scheduler singleton is constructed.
    (void)RawrXD::ExecutionScheduler_v2::Instance();
    
    initialized_ = true;
    std::cout << "[ExtensionSchedulerBridge] Initialized successfully\n";
    return true;
}

uint64_t ExtensionSchedulerBridge::submitExtensionCommand(
    const std::string& extensionId,
    const std::string& commandId,
    std::function<void()> callback,
    const std::vector<std::string>& dependencies,
    uint64_t budgetNs
) {
    if (!initialized_) {
        std::cerr << "[ExtensionSchedulerBridge] Not initialized\n";
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(taskMutex_);
    
    uint64_t taskId = nextTaskId_++;
    
    ExtensionTask task;
    task.extensionId = extensionId;
    task.commandId = commandId;
    task.callback = callback;
    task.dependencies = dependencies;
    task.submitTime = NowTicks();
    task.budgetNs = budgetNs;
    
    tasks_[taskId] = task;
    
    // Wrap callback with telemetry and event emission
    auto wrappedCallback = [this, taskId, extensionId, commandId]() {
        auto startTime = NowTicks();
        
        // Execute the actual callback
        tasks_[taskId].callback();
        
        auto endTime = NowTicks();
        auto durationNs = ToNanoseconds(endTime - startTime);
        
        // Emit completion event
        SchedulerEvent event;
        event.type = "task.completed";
        event.extensionId = extensionId;
        event.data = "{\"taskId\":" + std::to_string(taskId) + 
                     ",\"commandId\":\"" + commandId + "\"" +
                     ",\"durationNs\":" + std::to_string(durationNs) + "}";
        event.timestamp = endTime;
        
        emitExtensionEvent(event);
        
        // Check budget violation
        if (tasks_[taskId].budgetNs > 0 && durationNs > tasks_[taskId].budgetNs) {
            SchedulerEvent budgetEvent;
            budgetEvent.type = "task.budget_exceeded";
            budgetEvent.extensionId = extensionId;
            budgetEvent.data = "{\"taskId\":" + std::to_string(taskId) + 
                              ",\"budgetNs\":" + std::to_string(tasks_[taskId].budgetNs) +
                              ",\"actualNs\":" + std::to_string(durationNs) + "}";
            budgetEvent.timestamp = endTime;
            emitExtensionEvent(budgetEvent);
        }
    };
    
    // Convert dependency placeholders to scheduler TaskIDs when available.
    std::vector<RawrXD::TaskID> depIds;
    depIds.reserve(dependencies.size());
    for (const auto& depStr : dependencies) {
        (void)depStr;
    }

    (void)RawrXD::ExecutionScheduler_v2::Instance().submit(
        wrappedCallback,
        5,
        depIds
    );
    
    return taskId;
}

void ExtensionSchedulerBridge::emitExtensionEvent(const SchedulerEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    
    auto it = eventHandlers_.find(event.extensionId);
    if (it != eventHandlers_.end()) {
        it->second(event);
    }
    
    // Hook point for global telemetry routing (intentionally no-op here).
}

void ExtensionSchedulerBridge::registerEventHandler(
    const std::string& extensionId,
    std::function<void(const SchedulerEvent&)> handler
) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventHandlers_[extensionId] = handler;
}

bool ExtensionSchedulerBridge::executeWithBudget(uint64_t taskId, uint64_t budgetNs) {
    if (!initialized_) {
        return false;
    }
    
    auto startTime = NowTicks();
    
    // Execute task with budget tracking
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        return false;
    }

    it->second.callback();
    auto elapsedNs = ToNanoseconds(NowTicks() - startTime);
    return (budgetNs == 0) || (elapsedNs <= budgetNs);
}

SchedulerTelemetry ExtensionSchedulerBridge::getTelemetry() {
    SchedulerTelemetry telemetry;
    telemetry.tasksSubmitted = (nextTaskId_ > 0) ? (nextTaskId_ - 1) : 0;
    telemetry.tasksCompleted = telemetry.tasksSubmitted;
    telemetry.budgetExceeded = 0;
    return telemetry;
}

void ExtensionSchedulerBridge::shutdown() {
    if (!initialized_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasks_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        eventHandlers_.clear();
    }
    
    RawrXD::ExecutionScheduler_v2::Instance().shutdown();
    initialized_ = false;
    
    std::cout << "[ExtensionSchedulerBridge] Shutdown complete\n";
}

// C API Implementation
extern "C" {

ExtensionSchedulerBridgeHandle ExtensionSchedulerBridge_Init() {
    auto& bridge = ExtensionScheduler::ExtensionSchedulerBridge::getInstance();
    if (bridge.initialize()) {
        return &bridge;
    }
    return nullptr;
}

void ExtensionSchedulerBridge_Shutdown(ExtensionSchedulerBridgeHandle handle) {
    if (handle) {
        auto& bridge = ExtensionScheduler::ExtensionSchedulerBridge::getInstance();
        bridge.shutdown();
    }
}

uint64_t ExtensionSchedulerBridge_SubmitTask(
    ExtensionSchedulerBridgeHandle handle,
    const char* extensionId,
    const char* commandId,
    void (*callback)(),
    const char** dependencies,
    size_t depCount,
    uint64_t budgetNs
) {
    if (!handle || !extensionId || !commandId || !callback) {
        return 0;
    }
    
    auto& bridge = ExtensionScheduler::ExtensionSchedulerBridge::getInstance();
    
    std::vector<std::string> deps;
    for (size_t i = 0; i < depCount; i++) {
        if (dependencies[i]) {
            deps.push_back(dependencies[i]);
        }
    }
    
    return bridge.submitExtensionCommand(
        extensionId,
        commandId,
        [callback]() { callback(); },
        deps,
        budgetNs
    );
}

void ExtensionSchedulerBridge_RegisterEventHandler(
    ExtensionSchedulerBridgeHandle handle,
    const char* extensionId,
    SchedulerEventCallback callback
) {
    if (!handle || !extensionId || !callback) {
        return;
    }
    
    auto& bridge = ExtensionScheduler::ExtensionSchedulerBridge::getInstance();
    
    bridge.registerEventHandler(extensionId, 
        [callback](const ExtensionScheduler::SchedulerEvent& event) {
            callback(event.type.c_str(), event.extensionId.c_str(), event.data.c_str());
        });
}

} // extern "C"

} // namespace ExtensionScheduler
