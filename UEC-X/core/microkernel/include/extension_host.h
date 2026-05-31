// UEC-X Microkernel - Extension Host
// Manages extension loading, isolation, and lifecycle

#pragma once

#include "uec_core.h"
#include <unordered_map>
#include <dlfcn.h>

#ifdef UEC_PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace uec {

// =============================================================================
// Extension Instance
// =============================================================================

class UEC_API ExtensionInstance {
public:
    ExtensionInstance(ExtensionId id, const ExtensionConfig& config);
    ~ExtensionInstance();

    // Non-copyable
    ExtensionInstance(const ExtensionInstance&) = delete;
    ExtensionInstance& operator=(const ExtensionInstance&) = delete;

    // Lifecycle
    Result<void> Load(const std::string& path);
    Result<void> Activate();
    Result<void> Deactivate();
    Result<void> Unload();

    // State
    ExtensionState GetState() const { return m_state; }
    ExtensionId GetId() const { return m_id; }
    const std::string& GetName() const { return m_config.name; }
    const ExtensionConfig& GetConfig() const { return m_config; }
    CapabilityMask GetCapabilities() const { return m_config.capabilities; }

    // Capability checking
    bool HasCapability(Capability cap) const;
    bool HasCapabilities(CapabilityMask caps) const;

    // Memory tracking
    size_t GetMemoryUsage() const;
    void TrackMemoryAllocation(size_t bytes);
    void TrackMemoryDeallocation(size_t bytes);

    // Thread tracking
    void RegisterThread(std::thread::id tid);
    void UnregisterThread(std::thread::id tid);
    std::vector<std::thread::id> GetThreads() const;

private:
    ExtensionId m_id;
    ExtensionConfig m_config;
    std::atomic<ExtensionState> m_state{ExtensionState::Unloaded};
    
#ifdef UEC_PLATFORM_WINDOWS
    HMODULE m_moduleHandle = nullptr;
#else
    void* m_moduleHandle = nullptr;
#endif

    std::atomic<size_t> m_memoryUsed{0};
    mutable std::mutex m_threadMutex;
    std::vector<std::thread::id> m_threads;

    // Entry points
    using InitFunc = int32_t(*)(void* context);
    using ShutdownFunc = int32_t(*)();
    using ActivateFunc = int32_t(*)();
    using DeactivateFunc = int32_t(*)();
    
    InitFunc m_initFunc = nullptr;
    ShutdownFunc m_shutdownFunc = nullptr;
    ActivateFunc m_activateFunc = nullptr;
    DeactivateFunc m_deactivateFunc = nullptr;
};

// =============================================================================
// Extension Host
// =============================================================================

class UEC_API ExtensionHost {
public:
    ExtensionHost();
    ~ExtensionHost();

    // Non-copyable, non-movable
    ExtensionHost(const ExtensionHost&) = delete;
    ExtensionHost& operator=(const ExtensionHost&) = delete;
    ExtensionHost(ExtensionHost&&) = delete;
    ExtensionHost& operator=(ExtensionHost&&) = delete;

    // Lifecycle
    Result<void> Initialize(const MicrokernelConfig& config);
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Extension management
    Result<ExtensionId> LoadExtension(const std::string& path, const ExtensionConfig& config);
    Result<void> UnloadExtension(ExtensionId id);
    Result<void> ActivateExtension(ExtensionId id);
    Result<void> DeactivateExtension(ExtensionId id);
    Result<void> ReloadExtension(ExtensionId id);

    // Query
    std::shared_ptr<ExtensionInstance> GetExtension(ExtensionId id) const;
    std::vector<ExtensionId> GetLoadedExtensions() const;
    std::vector<ExtensionId> GetActiveExtensions() const;
    bool IsExtensionLoaded(ExtensionId id) const;
    bool IsExtensionActive(ExtensionId id) const;

    // Capability management
    Result<void> GrantCapability(ExtensionId id, Capability cap);
    Result<void> RevokeCapability(ExtensionId id, Capability cap);
    Result<void> SetCapabilities(ExtensionId id, CapabilityMask caps);

    // Resource limits
    Result<void> SetMemoryLimit(ExtensionId id, size_t maxBytes);
    Result<void> SetThreadLimit(ExtensionId id, uint32_t maxThreads);

    // Events
    void OnExtensionError(ExtensionId id, ErrorCode error, const std::string& message);

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<ExtensionId, std::shared_ptr<ExtensionInstance>> m_extensions;
    std::atomic<ExtensionId> m_nextExtensionId{1};
    std::atomic<bool> m_initialized{false};
    MicrokernelConfig m_config;
};

} // namespace uec
