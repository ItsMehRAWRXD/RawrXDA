// UEC-X Microkernel - Command Registry
// Manages command registration, lookup, and dispatch

#pragma once

#include "uec_core.h"
#include <unordered_map>
#include <shared_mutex>

namespace uec {

// =============================================================================
// Command Registry
// =============================================================================

class UEC_API CommandRegistry {
public:
    CommandRegistry();
    ~CommandRegistry();

    // Non-copyable, non-movable
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) = delete;
    CommandRegistry& operator=(CommandRegistry&&) = delete;

    // Lifecycle
    Result<void> Initialize(uint32_t maxCommands);
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Command registration
    Result<void> RegisterCommand(
        CommandId id,
        const std::string& name,
        CommandHandler handler,
        CapabilityMask requiredCaps = 0,
        ExtensionId owner = 0
    );
    
    Result<void> UnregisterCommand(CommandId id);
    Result<void> UnregisterAllCommands(ExtensionId owner);
    
    // Command lookup
    bool HasCommand(CommandId id) const;
    bool HasCommand(const std::string& name) const;
    Result<CommandId> GetCommandId(const std::string& name) const;
    
    // Command execution
    Result<std::vector<uint8_t>> ExecuteCommand(
        CommandId id,
        const CommandContext& context
    );
    
    Result<std::vector<uint8_t>> ExecuteCommand(
        const std::string& name,
        const CommandContext& context
    );

    // Query
    std::vector<CommandId> GetRegisteredCommands() const;
    std::vector<std::string> GetCommandNames() const;
    size_t GetCommandCount() const;
    
    // Capability checking
    bool CheckCapabilities(CommandId id, CapabilityMask caps) const;

private:
    struct CommandEntry {
        CommandId id;
        std::string name;
        CommandHandler handler;
        CapabilityMask requiredCapabilities;
        ExtensionId owner;
        Timestamp registeredAt;
        std::atomic<uint64_t> executionCount{0};
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<CommandId, std::unique_ptr<CommandEntry>> m_commandsById;
    std::unordered_map<std::string, CommandId> m_commandsByName;
    std::atomic<bool> m_initialized{false};
    uint32_t m_maxCommands = 0;
};

} // namespace uec
