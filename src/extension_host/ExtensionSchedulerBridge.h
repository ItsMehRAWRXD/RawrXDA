#pragma once

#include "extensions/extension_api_bridge.h"
#include "ExecutionScheduler_v2.h"
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>

namespace ExtensionScheduler {

// Extension task wrapper for scheduler integration
struct ExtensionTask {
    std::string extensionId;
    std::string commandId;
    std::function<void()> callback;
    std::vector<std::string> dependencies;
    uint64_t submitTime;
    uint64_t budgetNs;
};

// Event emitted from scheduler to extension
struct SchedulerEvent {
    std::string type;
    std::string extensionId;
    std::string data;
    uint64_t timestamp;
};

struct SchedulerTelemetry {
    uint64_t tasksSubmitted{0};
    uint64_t tasksCompleted{0};
    uint64_t budgetExceeded{0};
};

// Integration bridge between Extension API and ExecutionScheduler v2
class ExtensionSchedulerBridge {
public:
    static ExtensionSchedulerBridge& getInstance();
    
    // Initialize the bridge
    bool initialize();
    
    // Submit an extension command as a scheduler task
    uint64_t submitExtensionCommand(
        const std::string& extensionId,
        const std::string& commandId,
        std::function<void()> callback,
        const std::vector<std::string>& dependencies = {},
        uint64_t budgetNs = 0
    );
    
    // Emit event from scheduler to extension
    void emitExtensionEvent(const SchedulerEvent& event);
    
    // Register event handler for extension
    void registerEventHandler(
        const std::string& extensionId,
        std::function<void(const SchedulerEvent&)> handler
    );
    
    // Execute with phase budget enforcement
    bool executeWithBudget(uint64_t taskId, uint64_t budgetNs);
    
    // Get scheduler telemetry for extensions
    SchedulerTelemetry getTelemetry();
    
    // Shutdown the bridge
    void shutdown();

private:
    ExtensionSchedulerBridge() = default;
    ~ExtensionSchedulerBridge() = default;
    
    ExtensionSchedulerBridge(const ExtensionSchedulerBridge&) = delete;
    ExtensionSchedulerBridge& operator=(const ExtensionSchedulerBridge&) = delete;
    
    bool initialized_ = false;
    std::mutex taskMutex_;
    std::unordered_map<uint64_t, ExtensionTask> tasks_;
    std::unordered_map<std::string, std::function<void(const SchedulerEvent&)>> eventHandlers_;
    std::mutex eventMutex_;
    uint64_t nextTaskId_ = 1;
};

// C API for FFI integration
extern "C" {
    typedef void* ExtensionSchedulerBridgeHandle;
    typedef void (*SchedulerEventCallback)(const char* type, const char* extensionId, const char* data);
    
    ExtensionSchedulerBridgeHandle ExtensionSchedulerBridge_Init();
    void ExtensionSchedulerBridge_Shutdown(ExtensionSchedulerBridgeHandle handle);
    uint64_t ExtensionSchedulerBridge_SubmitTask(
        ExtensionSchedulerBridgeHandle handle,
        const char* extensionId,
        const char* commandId,
        void (*callback)(),
        const char** dependencies,
        size_t depCount,
        uint64_t budgetNs
    );
    void ExtensionSchedulerBridge_RegisterEventHandler(
        ExtensionSchedulerBridgeHandle handle,
        const char* extensionId,
        SchedulerEventCallback callback
    );
}

} // namespace ExtensionScheduler
