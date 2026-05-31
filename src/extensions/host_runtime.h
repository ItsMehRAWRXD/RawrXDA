/**
 * @file host_runtime.h
 * @brief Native extension host core
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <functional>
#include <filesystem>
#include <queue>
#include <condition_variable>
#include <chrono>

namespace RawrXD::Extensions {

// Forward declarations
class ProcessBroker;
class OSSandbox;

// ============================================================================
// Version
// ============================================================================

#define RAWRXD_VERSION "14.7.3"

// ============================================================================
// Extension State
// ============================================================================

enum class ExtensionState {
    Unknown,
    Loading,
    Loaded,
    Active,
    Error,
    Unloading
};

// ============================================================================
// Extension Manifest
// ============================================================================

struct ExtensionManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    int apiVersion = 1;
    bool autoActivate = false;
    std::vector<std::string> requiredCapabilities;
    std::vector<std::string> requestedPermissions;
};

// ============================================================================
// Extension Context
// ============================================================================

struct ExtensionContext {
    const char* hostVersion;
    int64_t extensionId;
    int apiVersion;
};

// ============================================================================
// Extension Function Types
// ============================================================================

using ExtensionInitFunc = bool (*)(ExtensionContext* context);
using ExtensionActivateFunc = bool (*)();
using ExtensionDeactivateFunc = bool (*)();
using ExtensionMessageFunc = bool (*)(const char* message, size_t length);

// ============================================================================
// Extension Instance
// ============================================================================

struct ExtensionInstance {
    int64_t id = -1;
    ExtensionManifest manifest;
    std::string path;
    ExtensionState state = ExtensionState::Unknown;
    void* hModule = nullptr;
    uint64_t lastHeartbeat = 0;
    size_t peakMemory = 0;
    
    ExtensionInitFunc initFunc = nullptr;
    ExtensionActivateFunc activateFunc = nullptr;
    ExtensionDeactivateFunc deactivateFunc = nullptr;
    ExtensionMessageFunc messageFunc = nullptr;
};

// ============================================================================
// Host Configuration
// ============================================================================

struct HostConfig {
    std::filesystem::path extensionDir;
    int maxApiVersion = 1;
    std::vector<std::string> allowedPermissions;
    size_t maxExtensions = 64;
    size_t maxMemoryPerExtension = 256 * 1024 * 1024; // 256MB
};

// ============================================================================
// Capability Handler
// ============================================================================

using CapabilityHandler = std::function<bool(const std::string& params, std::string& result)>;

// ============================================================================
// Host Statistics
// ============================================================================

struct HostStats {
    int totalExtensions = 0;
    int activeExtensions = 0;
    size_t totalMemoryUsage = 0;
};

// ============================================================================
// Extension Host
// ============================================================================

class ExtensionHost {
public:
    explicit ExtensionHost(const HostConfig& config);
    ~ExtensionHost();
    
    // Lifecycle
    bool initialize(ProcessBroker* broker = nullptr, OSSandbox* sandbox = nullptr);
    void shutdown();
    bool isRunning() const { return m_running; }
    
    // Extension management
    int64_t loadExtension(const std::string& path, const ExtensionManifest& manifest);
    bool unloadExtension(int64_t extId);
    void unloadAllExtensions();
    
    // Extension state
    bool activateExtension(int64_t extId);
    bool deactivateExtension(int64_t extId);
    ExtensionState getExtensionState(int64_t extId) const;
    
    // Message passing
    bool sendMessage(int64_t extId, const std::string& message);
    bool sendMessageWithResponse(int64_t extId, const std::string& message,
                                  std::string& response, uint32_t timeoutMs);
    void broadcastMessage(const std::string& message);
    
    // Capability registration
    void registerCapability(const std::string& name, const CapabilityHandler& handler);
    void unregisterCapability(const std::string& name);
    bool invokeCapability(const std::string& name, const std::string& params, std::string& result);
    
    // Statistics
    HostStats getStats() const;
    
private:
    bool validateExtension(const std::string& path, const ExtensionManifest& manifest);
    bool isPermissionAllowed(const std::string& permission) const;
    bool initializeIPC();
    void cleanupIPC();
    void processMessages();
    void processSingleMessage(const QueuedMessage& msg);
    void healthMonitorLoop();
    void handleExtensionCrash(int64_t extId, const std::string& reason);
    void sendShutdownHandshake(int64_t extId);
    
    HostConfig m_config;
    mutable std::mutex m_mutex;
    
    bool m_running;
    int64_t m_nextExtensionId;
    std::map<int64_t, ExtensionInstance> m_extensions;
    std::map<std::string, CapabilityHandler> m_capabilities;
    
    std::thread m_messageThread;
    std::thread m_healthThread;
    void* m_ipcPipe;
    
    // Message queue for async processing
    struct QueuedMessage {
        int64_t extId;
        std::string payload;
    };
    std::queue<QueuedMessage> m_messageQueue;
    std::condition_variable m_messageCv;
    std::condition_variable m_healthCv;
    
    // External dependencies
    ProcessBroker* m_broker = nullptr;
    OSSandbox* m_sandbox = nullptr;
};

} // namespace RawrXD::Extensions
