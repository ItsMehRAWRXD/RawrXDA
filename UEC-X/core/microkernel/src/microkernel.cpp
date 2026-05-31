// UEC-X Microkernel - Core Implementation

#include "microkernel.h"
#include <iostream>
#include <sstream>

namespace uec {

// =============================================================================
// Version Info
// =============================================================================

UEC_API VersionInfo GetVersion() {
    return VersionInfo{
        UEC_VERSION_MAJOR,
        UEC_VERSION_MINOR,
        UEC_VERSION_PATCH,
        __DATE__ " " __TIME__,
        "unknown"
    };
}

UEC_API const char* GetErrorString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::InvalidParameter: return "Invalid parameter";
        case ErrorCode::NoMemory: return "Out of memory";
        case ErrorCode::NotFound: return "Not found";
        case ErrorCode::Busy: return "Resource busy";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::Timeout: return "Operation timed out";
        case ErrorCode::Cancelled: return "Operation cancelled";
        case ErrorCode::AlreadyExists: return "Already exists";
        case ErrorCode::NotInitialized: return "Not initialized";
        case ErrorCode::InternalError: return "Internal error";
        default: return "Unknown error";
    }
}

UEC_API CapabilityMask ParseCapabilities(const std::vector<std::string>& caps) {
    CapabilityMask mask = 0;
    for (const auto& cap : caps) {
        if (cap == "file:read") mask |= static_cast<CapabilityMask>(Capability::FileRead);
        else if (cap == "file:write") mask |= static_cast<CapabilityMask>(Capability::FileWrite);
        else if (cap == "file:delete") mask |= static_cast<CapabilityMask>(Capability::FileDelete);
        else if (cap == "network") mask |= static_cast<CapabilityMask>(Capability::Network);
        else if (cap == "process:spawn") mask |= static_cast<CapabilityMask>(Capability::ProcessSpawn);
        else if (cap == "process:kill") mask |= static_cast<CapabilityMask>(Capability::ProcessKill);
        else if (cap == "memory:map") mask |= static_cast<CapabilityMask>(Capability::MemoryMap);
        else if (cap == "memory:allocate") mask |= static_cast<CapabilityMask>(Capability::MemoryAllocate);
        else if (cap == "ipc:send") mask |= static_cast<CapabilityMask>(Capability::IPCSend);
        else if (cap == "ipc:receive") mask |= static_cast<CapabilityMask>(Capability::IPCReceive);
        else if (cap == "hotpatch") mask |= static_cast<CapabilityMask>(Capability::Hotpatch);
        else if (cap == "debug") mask |= static_cast<CapabilityMask>(Capability::Debug);
        else if (cap == "telemetry") mask |= static_cast<CapabilityMask>(Capability::Telemetry);
        else if (cap == "ai:inference") mask |= static_cast<CapabilityMask>(Capability::AIInference);
    }
    return mask;
}

// =============================================================================
// Microkernel Implementation
// =============================================================================

Microkernel::Microkernel() = default;
Microkernel::~Microkernel() {
    if (m_state != MicrokernelState::Uninitialized && 
        m_state != MicrokernelState::Shutdown) {
        Shutdown();
    }
}

Microkernel& Microkernel::Instance() {
    static Microkernel instance;
    return instance;
}

Result<void> Microkernel::Initialize(const MicrokernelConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != MicrokernelState::Uninitialized) {
        return ErrorCode::AlreadyExists;
    }
    
    m_state = MicrokernelState::Initializing;
    m_config = config;
    
    // Initialize subsystems
    auto result = InitializeSubsystems();
    if (result.IsError()) {
        m_state = MicrokernelState::Error;
        return result.Error();
    }
    
    m_state = MicrokernelState::Running;
    return ErrorCode::Success;
}

Result<void> Microkernel::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == MicrokernelState::Uninitialized || 
        m_state == MicrokernelState::Shutdown) {
        return ErrorCode::Success;
    }
    
    m_state = MicrokernelState::ShuttingDown;
    
    // Shutdown subsystems
    ShutdownSubsystems();
    
    m_state = MicrokernelState::Shutdown;
    return ErrorCode::Success;
}

Result<void> Microkernel::Pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != MicrokernelState::Running) {
        return ErrorCode::InvalidParameter;
    }
    
    m_state = MicrokernelState::Paused;
    return ErrorCode::Success;
}

Result<void> Microkernel::Resume() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state != MicrokernelState::Paused) {
        return ErrorCode::InvalidParameter;
    }
    
    m_state = MicrokernelState::Running;
    return ErrorCode::Success;
}

MicrokernelState Microkernel::GetState() const {
    return m_state;
}

bool Microkernel::IsRunning() const {
    return m_state == MicrokernelState::Running;
}

bool Microkernel::IsInitialized() const {
    return m_state != MicrokernelState::Uninitialized &&
           m_state != MicrokernelState::Shutdown;
}

Result<void> Microkernel::InitializeSubsystems() {
    // Create subsystems
    m_commandRegistry = std::make_unique<CommandRegistry>();
    m_eventBus = std::make_unique<EventBus>();
    m_scheduler = std::make_unique<Scheduler>();
    m_extensionHost = std::make_unique<ExtensionHost>();
    m_securitySandbox = std::make_unique<SecuritySandbox>();
    m_kvStore = std::make_unique<KVStore>();
    
    // Initialize in dependency order
    auto result = m_commandRegistry->Initialize(m_config.maxCommands);
    if (result.IsError()) return result;
    
    result = m_eventBus->Initialize();
    if (result.IsError()) return result;
    
    result = m_scheduler->Initialize(m_config.workerThreads);
    if (result.IsError()) return result;
    
    result = m_securitySandbox->Initialize();
    if (result.IsError()) return result;
    
    result = m_extensionHost->Initialize(m_config);
    if (result.IsError()) return result;
    
    result = m_kvStore->Initialize();
    if (result.IsError()) return result;
    
    // Subscribe to events
    m_extLoadedSubId = m_eventBus->Subscribe(
        static_cast<EventType>(1),  // EXTENSION_LOADED
        [this](const Event& e) { OnExtensionLoaded(e); }
    ).ValueOr(0);
    
    m_extUnloadedSubId = m_eventBus->Subscribe(
        static_cast<EventType>(2),  // EXTENSION_UNLOADED
        [this](const Event& e) { OnExtensionUnloaded(e); }
    ).ValueOr(0);
    
    return ErrorCode::Success;
}

Result<void> Microkernel::ShutdownSubsystems() {
    // Shutdown in reverse order
    if (m_kvStore) m_kvStore->Shutdown();
    if (m_extensionHost) m_extensionHost->Shutdown();
    if (m_securitySandbox) m_securitySandbox->Shutdown();
    if (m_scheduler) m_scheduler->Shutdown();
    if (m_eventBus) m_eventBus->Shutdown();
    if (m_commandRegistry) m_commandRegistry->Shutdown();
    
    return ErrorCode::Success;
}

CommandRegistry& Microkernel::GetCommandRegistry() { return *m_commandRegistry; }
EventBus& Microkernel::GetEventBus() { return *m_eventBus; }
Scheduler& Microkernel::GetScheduler() { return *m_scheduler; }
ExtensionHost& Microkernel::GetExtensionHost() { return *m_extensionHost; }
SecuritySandbox& Microkernel::GetSecuritySandbox() { return *m_securitySandbox; }
KVStore& Microkernel::GetKVStore() { return *m_kvStore; }

const CommandRegistry& Microkernel::GetCommandRegistry() const { return *m_commandRegistry; }
const EventBus& Microkernel::GetEventBus() const { return *m_eventBus; }
const Scheduler& Microkernel::GetScheduler() const { return *m_scheduler; }
const ExtensionHost& Microkernel::GetExtensionHost() const { return *m_extensionHost; }
const SecuritySandbox& Microkernel::GetSecuritySandbox() const { return *m_securitySandbox; }
const KVStore& Microkernel::GetKVStore() const { return *m_kvStore; }

Result<ExtensionId> Microkernel::LoadExtension(const std::string& path, 
                                                  const ExtensionConfig& config) {
    return m_extensionHost->LoadExtension(path, config);
}

Result<void> Microkernel::UnloadExtension(ExtensionId id) {
    return m_extensionHost->UnloadExtension(id);
}

Result<void> Microkernel::ExecuteCommand(CommandId id, const std::vector<uint8_t>& params) {
    CommandContext ctx;
    ctx.id = id;
    ctx.timestamp = std::chrono::steady_clock::now();
    ctx.parameters = params;
    
    return m_commandRegistry->ExecuteCommand(id, ctx);
}

Result<void> Microkernel::EmitEvent(EventType type, ExtensionId source, 
                                    const std::vector<uint8_t>& payload) {
    return m_eventBus->Emit(type, source, payload);
}

const MicrokernelConfig& Microkernel::GetConfig() const {
    return m_config;
}

Result<void> Microkernel::UpdateConfig(const MicrokernelConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    return ErrorCode::Success;
}

DispatchStats Microkernel::GetStats() const {
    return m_stats;
}

void Microkernel::ResetStats() {
    m_stats.dispatchCount = 0;
    m_stats.dispatchErrors = 0;
    m_stats.ipcBytesSent = 0;
    m_stats.ipcBytesReceived = 0;
    m_stats.eventsEmitted = 0;
    m_stats.commandsRegistered = 0;
    m_stats.extensionsLoaded = 0;
}

Microkernel::HealthStatus Microkernel::GetHealthStatus() const {
    HealthStatus status;
    status.timestamp = std::chrono::steady_clock::now();
    status.healthy = true;
    
    // Check each subsystem
    status.subsystemHealth.emplace_back("CommandRegistry", m_commandRegistry && m_commandRegistry->IsInitialized());
    status.subsystemHealth.emplace_back("EventBus", m_eventBus && m_eventBus->IsInitialized());
    status.subsystemHealth.emplace_back("Scheduler", m_scheduler && m_scheduler->IsInitialized());
    status.subsystemHealth.emplace_back("ExtensionHost", m_extensionHost && m_extensionHost->IsInitialized());
    status.subsystemHealth.emplace_back("SecuritySandbox", m_securitySandbox && m_securitySandbox->IsInitialized());
    status.subsystemHealth.emplace_back("KVStore", m_kvStore && m_kvStore->IsInitialized());
    
    for (const auto& [name, healthy] : status.subsystemHealth) {
        if (!healthy) {
            status.healthy = false;
            status.status = "Subsystem failure: " + name;
            break;
        }
    }
    
    if (status.healthy) {
        status.status = "Healthy";
    }
    
    return status;
}

std::string Microkernel::GetDiagnosticsReport() const {
    std::ostringstream report;
    report << "=== UEC-X Microkernel Diagnostics Report ===\n";
    report << "Version: " << GetVersion().ToString() << "\n";
    report << "State: " << static_cast<int>(m_state) << "\n";
    report << "\n";
    
    auto health = GetHealthStatus();
    report << "Health: " << (health.healthy ? "HEALTHY" : "UNHEALTHY") << "\n";
    report << "Status: " << health.status << "\n\n";
    
    report << "Subsystem Status:\n";
    for (const auto& [name, healthy] : health.subsystemHealth) {
        report << "  " << name << ": " << (healthy ? "OK" : "FAIL") << "\n";
    }
    
    report << "\nStatistics:\n";
    report << "  Dispatch Count: " << m_stats.dispatchCount.load() << "\n";
    report << "  Dispatch Errors: " << m_stats.dispatchErrors.load() << "\n";
    report << "  IPC Bytes Sent: " << m_stats.ipcBytesSent.load() << "\n";
    report << "  IPC Bytes Received: " << m_stats.ipcBytesReceived.load() << "\n";
    
    return report.str();
}

Result<void> Microkernel::EnableDebugMode() {
    m_debugMode = true;
    return ErrorCode::Success;
}

Result<void> Microkernel::DisableDebugMode() {
    m_debugMode = false;
    return ErrorCode::Success;
}

void Microkernel::OnExtensionLoaded(const Event& event) {
    m_stats.extensionsLoaded++;
}

void Microkernel::OnExtensionUnloaded(const Event& event) {
    // Cleanup handled by ExtensionHost
}

void Microkernel::OnCommandExecuted(const Event& event) {
    // Statistics updated by CommandRegistry
}

void Microkernel::OnError(const Event& event) {
    // Error handling
}

// =============================================================================
// Convenience Functions
// =============================================================================

UEC_API Result<void> InitializeUEC(const MicrokernelConfig& config) {
    return Microkernel::Instance().Initialize(config);
}

UEC_API Result<void> ShutdownUEC() {
    return Microkernel::Instance().Shutdown();
}

UEC_API bool IsUECRunning() {
    return Microkernel::Instance().IsRunning();
}

UEC_API Microkernel& GetMicrokernel() {
    return Microkernel::Instance();
}

} // namespace uec
