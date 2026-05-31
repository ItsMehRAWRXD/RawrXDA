// ============================================================================
// slash_command_dag.h — Slash Commands as DAG Tasks
// ============================================================================
// Converts slash commands from direct chat handlers into scheduled DAG tasks
// that flow through the ExecutionScheduler.
//
// Before: Editor → Chat → Direct Execution
// After:  Editor → Chat → Scheduler Queue → DAG Execution
//
// Benefits:
//   - Deterministic execution order
//   - Backpressure handling (drops stale commands)
//   - Unified telemetry
//   - Resource accounting
//   - Cancellation support
// ============================================================================

#pragma once

#include "execution_scheduler.h"
#include "ast_graph_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <queue>
#include <atomic>

namespace RawrXD {
namespace SlashCommand {

// ============================================================================
// Command Types
// ============================================================================
enum class CommandType : uint8_t {
    Unknown = 0,
    Explain,        // /explain — Explain code
    Fix,            // /fix — Fix issues
    Test,           // /test — Generate/run tests
    Optimize,       // /optimize — Optimize code
    Edit,           // /edit — Multi-file editing
    Terminal,       // /terminal — Execute commands
    Search,         // /search — Semantic search
    Read,           // /read — Read file
    Write,          // /write — Write file
    Memory,         // /memory — Memory management
    Refactor,       // /refactor — Code refactoring
    Git,            // /git — Git operations
    Help,           // /help — Command help
};

// ============================================================================
// Command Priority — For scheduler queue ordering
// ============================================================================
enum class CommandPriority : uint8_t {
    Critical = 0,   // User-initiated, blocking
    High = 1,       // User-initiated, non-blocking
    Normal = 2,     // Background tasks
    Low = 3,        // Deferred work
};

// ============================================================================
// Command State — DAG lifecycle
// ============================================================================
enum class CommandState : uint8_t {
    Pending = 0,    // Queued, waiting for scheduler
    Prefetching,    // Loading AST context
    Validating,     // Validating command parameters
    Executing,      // Running command logic
    PostProcessing, // Formatting results
    Completed,      // Done, results available
    Cancelled,      // Cancelled before completion
    Failed          // Error during execution
};

// ============================================================================
// Command Task — DAG node for slash commands
// ============================================================================
struct CommandTask {
    uint64_t                taskID;
    CommandType             type;
    CommandPriority         priority;
    CommandState            state;
    
    // Context
    uint32_t                fileID;
    uint32_t                line;
    uint32_t                column;
    std::string             target;         // Target symbol/file
    std::vector<std::string> arguments;    // Command arguments
    
    // AST context (pre-fetched)
    AST::NodeID             cursorNode;
    AST::NodeID             scopeNode;
    std::vector<AST::NodeID> relatedNodes; // Context nodes
    
    // Timing
    uint64_t                enqueueTime;
    uint64_t                startTime;
    uint64_t                completeTime;
    
    // Result
    std::string             result;
    bool                    success;
    std::string             errorMessage;
    
    // Cancellation
    std::atomic<bool>       cancelRequested{false};
    
    CommandTask() : taskID(0), type(CommandType::Unknown), 
                    priority(CommandPriority::Normal), state(CommandState::Pending),
                    fileID(0), line(0), column(0), cursorNode(0), scopeNode(0),
                    enqueueTime(0), startTime(0), completeTime(0), success(false) {}
};

// ============================================================================
// Command Handler — Interface for command implementations
// ============================================================================
class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;
    
    // Get command type
    virtual CommandType getType() const = 0;
    
    // Validate command (can it run with given context?)
    virtual bool validate(const CommandTask* task, std::string& error) = 0;
    
    // Execute command (called by scheduler)
    virtual bool execute(CommandTask* task) = 0;
    
    // Cancel ongoing execution
    virtual void cancel(CommandTask* task) = 0;
    
    // Get estimated cost (for resource scheduling)
    virtual uint64_t estimateCost(const CommandTask* task) const = 0;
};

// ============================================================================
// Slash Command Scheduler — DAG-integrated command execution
// ============================================================================
class SlashCommandScheduler {
public:
    SlashCommandScheduler();
    ~SlashCommandScheduler();

    // ---- Lifecycle ----
    bool initialize(ExecutionScheduler* execScheduler, AST::ASTGraphEngine* astEngine);
    void shutdown();
    
    // ---- Command Registration ----
    void registerHandler(CommandType type, std::unique_ptr<ICommandHandler> handler);
    void unregisterHandler(CommandType type);
    
    // ---- Command Submission ----
    // Submit a slash command (returns task ID, 0 if rejected)
    uint64_t submitCommand(CommandType type, 
                          uint32_t fileID, uint32_t line, uint32_t column,
                          const std::string& target,
                          const std::vector<std::string>& arguments,
                          CommandPriority priority = CommandPriority::Normal);
    
    // Cancel a pending or running command
    void cancelCommand(uint64_t taskID);
    
    // Check command state
    CommandState getCommandState(uint64_t taskID) const;
    
    // Get command result (blocking until complete or timeout)
    bool awaitCommand(uint64_t taskID, std::string& result, uint64_t timeoutMs);
    
    // ---- DAG Integration ----
    // Process one command task (called by ExecutionScheduler)
    // Returns true if work was done
    bool processCommandTask();
    
    // Get pending command count
    size_t getPendingCount() const;
    
    // ---- Configuration ----
    struct Config {
        uint32_t    maxConcurrentCommands = 2;      // Max parallel commands
        uint32_t    maxQueueDepth = 32;             // Backpressure threshold
        uint64_t    commandTimeoutMs = 30000;       // Max time per command
        bool        enablePrefetch = true;          // Pre-fetch AST context
        bool        enableBackpressure = true;        // Drop low-priority when busy
        bool        enableCancellation = true;        // Allow mid-flight cancel
    };
    void configure(const Config& config);
    Config getConfig() const;
    
    // ---- Statistics ----
    struct Stats {
        uint64_t    commandsSubmitted;
        uint64_t    commandsCompleted;
        uint64_t    commandsCancelled;
        uint64_t    commandsDropped;
        uint64_t    commandsFailed;
        double      avgLatencyMs;
        double      p99LatencyMs;
        std::unordered_map<CommandType, uint64_t> commandsByType;
    };
    Stats getStats() const;
    void resetStats();
    
private:
    // ---- State ----
    ExecutionScheduler*     m_execScheduler;
    AST::ASTGraphEngine*    m_astEngine;
    Config                    m_config;
    
    // Command handlers
    std::unordered_map<CommandType, std::unique_ptr<ICommandHandler>> m_handlers;
    mutable std::shared_mutex m_handlerMutex;
    
    // Task queue (priority queue)
    struct TaskCompare {
        bool operator()(const std::pair<uint64_t, uint64_t>& a,
                       const std::pair<uint64_t, uint64_t>& b) const {
            return a.first > b.first; // Lower priority value = higher priority
        }
    };
    std::priority_queue<std::pair<uint64_t, uint64_t>,  // <priority, taskID>
                        std::vector<std::pair<uint64_t, uint64_t>>,
                        TaskCompare> m_taskQueue;
    std::unordered_map<uint64_t, std::unique_ptr<CommandTask>> m_tasks;
    mutable std::mutex      m_taskMutex;
    
    // Active tasks
    std::atomic<uint32_t> m_activeTasks{0};
    std::atomic<uint64_t> m_nextTaskID{1};
    
    // Statistics
    mutable std::atomic<uint64_t> m_commandsSubmitted{0};
    mutable std::atomic<uint64_t> m_commandsCompleted{0};
    mutable std::atomic<uint64_t> m_commandsCancelled{0};
    mutable std::atomic<uint64_t> m_commandsDropped{0};
    mutable std::atomic<uint64_t> m_commandsFailed{0};
    
    // ---- Internal Methods ----
    void prefetchASTContext(CommandTask* task);
    bool validateCommand(CommandTask* task);
    bool executeCommand(CommandTask* task);
    void completeCommand(CommandTask* task, bool success, const std::string& result);
    void dropLowPriorityTasks();
    uint64_t calculatePriorityValue(CommandPriority priority, uint64_t enqueueTime) const;
};

// ============================================================================
// Built-in Command Handlers
// ============================================================================

// /explain — Explain code
class ExplainCommandHandler : public ICommandHandler {
public:
    CommandType getType() const override { return CommandType::Explain; }
    bool validate(const CommandTask* task, std::string& error) override;
    bool execute(CommandTask* task) override;
    void cancel(CommandTask* task) override;
    uint64_t estimateCost(const CommandTask* task) const override;
};

// /fix — Fix code issues
class FixCommandHandler : public ICommandHandler {
public:
    CommandType getType() const override { return CommandType::Fix; }
    bool validate(const CommandTask* task, std::string& error) override;
    bool execute(CommandTask* task) override;
    void cancel(CommandTask* task) override;
    uint64_t estimateCost(const CommandTask* task) const override;
};

// /test — Generate/run tests
class TestCommandHandler : public ICommandHandler {
public:
    CommandType getType() const override { return CommandType::Test; }
    bool validate(const CommandTask* task, std::string& error) override;
    bool execute(CommandTask* task) override;
    void cancel(CommandTask* task) override;
    uint64_t estimateCost(const CommandTask* task) const override;
};

// /optimize — Optimize code
class OptimizeCommandHandler : public ICommandHandler {
public:
    CommandType getType() const override { return CommandType::Optimize; }
    bool validate(const CommandTask* task, std::string& error) override;
    bool execute(CommandTask* task) override;
    void cancel(CommandTask* task) override;
    uint64_t estimateCost(const CommandTask* task) const override;
};

// ============================================================================
// Global Access
// ============================================================================
SlashCommandScheduler& getSlashCommandScheduler();

} // namespace SlashCommand
} // namespace RawrXD
