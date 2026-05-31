// UEC-X Microkernel - Main Microkernel Class
// Central orchestrator for all UEC-X subsystems

#pragma once

#include "uec_core.h"
#include "command_registry.h"
#include "event_bus.h"
#include "scheduler.h"
#include "extension_host.h"
#include "security_sandbox.h"
#include "kv_store.h"

namespace uec {

// Forward declarations
class IPCChannel;
class HotpatchEngine;

// =============================================================================
// Microkernel State
// =============================================================================

enum class MicrokernelState : uint32_t {
    Uninitialized = 0,
    Initializing = 1,
    Running = 2,
    Paused = 3,
    ShuttingDown = 4,
    Shutdown = 5,
    Error = 6
};

// =============================================================================
// Microkernel
// =============================================================================

class UEC_API Microkernel {
public:
    // Singleton access
    static Microkernel& Instance();
    
    // Lifecycle
    Result<void> Initialize(const MicrokernelConfig& config);
    Result<void> Shutdown();
    Result<void> Pause();
    Result<void> Resume();
    
    // State
    MicrokernelState GetState() const;
    bool IsRunning() const;
    bool IsInitialized() const;
    
    // Subsystem access
    CommandRegistry& GetCommandRegistry();
    EventBus& GetEventBus();
    Scheduler& GetScheduler();
    ExtensionHost& GetExtensionHost();
    SecuritySandbox& GetSecuritySandbox();
    KVStore& GetKVStore();
    
    const CommandRegistry& GetCommandRegistry() const;
    const EventBus& GetEventBus() const;
    const Scheduler& GetScheduler() const;
    const ExtensionHost& GetExtensionHost() const;
    const SecuritySandbox& GetSecuritySandbox() const;
    const KVStore& GetKVStore() const;

    // High-level operations
    Result<ExtensionId> LoadExtension(const std::string& path, 
                                        const ExtensionConfig& config);
    Result<void> UnloadExtension(ExtensionId id);
    Result<void> ExecuteCommand(CommandId id, const std::vector<uint8_t>& params);
    Result<void> EmitEvent(EventType type, ExtensionId source, 
                            const std::vector<uint8_t>& payload);
    
    // Configuration
    const MicrokernelConfig& GetConfig() const;
    Result<void> UpdateConfig(const MicrokernelConfig& config);
    
    // Statistics
    DispatchStats GetStats() const;
    void ResetStats();
    
    // Health
    struct HealthStatus {
        bool healthy;
        std::string status;
        std::vector<std::pair<std::string, bool>> subsystemHealth;
        Timestamp timestamp;
    };
    HealthStatus GetHealthStatus() const;
    
    // Diagnostics
    std::string GetDiagnosticsReport() const;
    Result<void> EnableDebugMode();
    Result<void> DisableDebugMode();

private:
    Microkernel();
    ~Microkernel();
    
    // Non-copyable, non-movable
    Microkernel(const Microkernel&) = delete;
    Microkernel& operator=(const Microkernel&) = delete;
    Microkernel(Microkernel&&) = delete;
    Microkernel& operator=(Microkernel&&) = delete;

    // Subsystem initialization
    Result<void> InitializeSubsystems();
    Result<void> ShutdownSubsystems();
    
    // Internal event handlers
    void OnExtensionLoaded(const Event& event);
    void OnExtensionUnloaded(const Event& event);
    void OnCommandExecuted(const Event& event);
    void OnError(const Event& event);

    // Member variables
    std::unique_ptr<CommandRegistry> m_commandRegistry;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Scheduler> m_scheduler;
    std::unique_ptr<ExtensionHost> m_extensionHost;
    std::unique_ptr<SecuritySandbox> m_securitySandbox;
    std::unique_ptr<KVStore> m_kvStore;
    std::unique_ptr<IPCChannel> m_ipcChannel;
    std::unique_ptr<HotpatchEngine> m_hotpatchEngine;
    
    MicrokernelConfig m_config;
    std::atomic<MicrokernelState> m_state{MicrokernelState::Uninitialized};
    std::atomic<bool> m_debugMode{false};
    
    mutable std::mutex m_mutex;
    DispatchStats m_stats;
    
    // Event subscription IDs
    uint64_t m_extLoadedSubId = 0;
    uint64_t m_extUnloadedSubId = 0;
    uint64_t m_cmdExecutedSubId = 0;
    uint64_t m_errorSubId = 0;
};

// =============================================================================
// Convenience Functions
// =============================================================================

UEC_API Result<void> InitializeUEC(const MicrokernelConfig& config = MicrokernelConfig{});
UEC_API Result<void> ShutdownUEC();
UEC_API bool IsUECRunning();
UEC_API Microkernel& GetMicrokernel();

} // namespace uec
