// ============================================================================
// agentic_transaction.hpp — Agentic Transaction Layer
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Provides Cursor-Composer-class multi-step command execution with
// automatic checkpoint/rollback using the existing zero-drift
// unified_command_dispatch + transaction_journal infrastructure.
//
// Key design:
//   - Every dispatched command is recorded with its inverse
//   - Rollback replays inverse commands in reverse order
//   - Transaction state is journaled (WAL) for crash recovery
//   - Integrates directly with g_commandRegistry[] SSOT
//   - Supports nested transactions (savepoints)
//   - Thread-safe via mutex (no recursive locks)
//
// Dependencies:
//   - command_registry.hpp      (CmdDescriptor, COMMAND_TABLE)
//   - unified_command_dispatch.hpp (dispatchByGuiId, dispatchByCanonical, lookup*)
//   - transaction_journal.hpp   (WAL for crash recovery)
//   - shared_feature_dispatch.h (CommandContext, CommandResult)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_AGENTIC_TRANSACTION_HPP
#define RAWRXD_AGENTIC_TRANSACTION_HPP

#include "../core/unified_command_dispatch.hpp"
#include "../core/transaction_journal.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// INVERSE COMMAND MAP — Maps command IDs to their logical inverse
// ============================================================================
// Not all commands have inverses. Commands without inverses are marked
// as non-reversible and block rollback past that point.

struct InverseMapping {
    uint32_t    forwardId;          // The command being reversed
    const char* forwardCanonical;   // "file.new" (for CLI-only with ID=0)
    uint32_t    inverseId;          // Command that reverses it (0 = use canonical)
    const char* inverseCanonical;   // Canonical name of inverse (nullptr = use ID)
    bool        isReversible;       // False = rollback barrier

    static InverseMapping reversible(uint32_t fwd, uint32_t inv) {
        return { fwd, nullptr, inv, nullptr, true };
    }
    static InverseMapping reversibleByName(const char* fwd, const char* inv) {
        return { 0, fwd, 0, inv, true };
    }
    static InverseMapping barrier(uint32_t fwd) {
        return { fwd, nullptr, 0, nullptr, false };
    }
};

// ============================================================================
// TRANSACTION STEP — One recorded command execution within a transaction
// ============================================================================

struct TransactionStep {
    uint32_t        stepIndex;      // 0-based within transaction
    uint32_t        commandId;      // IDM_* or 0 for CLI-only
    const char*     canonicalName;  // "file.save", "edit.paste", etc.
    const char*     cliAlias;       // "!save", "!paste"
    CommandResult   result;         // What the handler returned
    Dispatch::DispatchStatus dispatchStatus; // OK, HANDLER_ERROR, etc.
    bool            isReversible;   // Can this step be undone?
    uint32_t        inverseId;      // Command to undo this step
    const char*     inverseCanonical;
    uint64_t        timestampMs;    // When executed
    std::string     argsSnapshot;   // Captured args for replay

    // State snapshot for stateful rollback (editor content, etc.)
    std::vector<uint8_t> preStateSnapshot;
};

// ============================================================================
// TRANSACTION STATE — Lifecycle of a transaction
// ============================================================================

enum class TxnState : uint8_t {
    IDLE            = 0,   // No active transaction
    ACTIVE          = 1,   // Transaction in progress, accepting commands
    ROLLING_BACK    = 2,   // Rollback in progress
    COMMITTED       = 3,   // Successfully committed
    ROLLED_BACK     = 4,   // Successfully rolled back
    FAILED          = 5,   // Rollback failed mid-way (partial state)
    SAVEPOINT       = 6    // At a named savepoint within ACTIVE
};

inline const char* txnStateToString(TxnState s) {
    switch (s) {
        case TxnState::IDLE:         return "IDLE";
        case TxnState::ACTIVE:       return "ACTIVE";
        case TxnState::ROLLING_BACK: return "ROLLING_BACK";
        case TxnState::COMMITTED:    return "COMMITTED";
        case TxnState::ROLLED_BACK:  return "ROLLED_BACK";
        case TxnState::FAILED:       return "FAILED";
        case TxnState::SAVEPOINT:    return "SAVEPOINT";
    }
    return "UNKNOWN";
}

// ============================================================================
// TRANSACTION RESULT — Outcome of transaction operations
// ============================================================================

struct TxnResult {
    bool        success;
    const char* detail;
    int         errorCode;
    TxnState    finalState;
    uint32_t    stepsExecuted;
    uint32_t    stepsRolledBack;

    static TxnResult ok(const char* msg, TxnState state, uint32_t steps = 0) {
        return { true, msg, 0, state, steps, 0 };
    }
    static TxnResult error(const char* msg, TxnState state, int code = -1) {
        return { false, msg, code, state, 0, 0 };
    }
    static TxnResult rollbackOk(uint32_t stepsRolled, TxnState state) {
        return { true, "Rollback complete", 0, state, 0, stepsRolled };
    }
    static TxnResult rollbackFailed(const char* msg, uint32_t rolledSoFar, TxnState state) {
        return { false, msg, -2, state, 0, rolledSoFar };
    }
};

// ============================================================================
// SAVEPOINT — Named checkpoint within an active transaction
// ============================================================================

struct Savepoint {
    std::string     name;
    uint32_t        stepIndex;      // Step index at time of savepoint
    uint64_t        timestampMs;
};

// ============================================================================
// AGENTIC TRANSACTION STATISTICS
// ============================================================================

struct TxnStatistics {
    std::atomic<uint64_t> totalTransactions{0};
    std::atomic<uint64_t> totalCommits{0};
    std::atomic<uint64_t> totalRollbacks{0};
    std::atomic<uint64_t> totalStepsExecuted{0};
    std::atomic<uint64_t> totalStepsRolledBack{0};
    std::atomic<uint64_t> totalBarriersHit{0};       // Non-reversible commands encountered
    std::atomic<uint64_t> totalSavepoints{0};
    std::atomic<uint64_t> failedRollbacks{0};
    std::atomic<uint64_t> crashRecoveries{0};
    uint64_t              lastTxnDurationMs{0};
};

// ============================================================================
// CALLBACK HOOKS — Event notification (no signals, function pointers only)
// ============================================================================

struct TxnCallbacks {
    // Called after each step executes successfully
    void (*onStepExecuted)(const TransactionStep& step, void* userData);
    void* stepUserData;

    // Called when a step is rolled back
    void (*onStepRolledBack)(const TransactionStep& step, void* userData);
    void* rollbackUserData;

    // Called on state transitions
    void (*onStateChange)(TxnState oldState, TxnState newState, void* userData);
    void* stateUserData;

    // Called when a non-reversible barrier is hit
    void (*onBarrierHit)(const TransactionStep& step, void* userData);
    void* barrierUserData;

    TxnCallbacks()
        : onStepExecuted(nullptr), stepUserData(nullptr)
        , onStepRolledBack(nullptr), rollbackUserData(nullptr)
        , onStateChange(nullptr), stateUserData(nullptr)
        , onBarrierHit(nullptr), barrierUserData(nullptr)
    {}
};

// ============================================================================
// AgenticTransaction — The Core Transaction Manager
// ============================================================================
// Usage:
//   AgenticTransaction txn;
//   txn.setJournal(&journal);   // Optional: WAL for crash recovery
//   txn.begin("Refactor module X");
//   txn.execute(1003, ctx);     // file.save — recorded & journaled
//   txn.execute(2005, ctx);     // edit.paste — recorded & journaled
//   txn.savepoint("checkpoint1");
//   txn.execute(4100, ctx);     // agent.loop — if this fails...
//   txn.rollbackToSavepoint("checkpoint1");  // ...undo agent.loop
//   txn.commit();               // finalize
//
// Or for full rollback:
//   txn.rollback();             // undo ALL steps in reverse

class AgenticTransaction {
public:
    AgenticTransaction();
    ~AgenticTransaction();

    // Non-copyable, movable
    AgenticTransaction(const AgenticTransaction&) = delete;
    AgenticTransaction& operator=(const AgenticTransaction&) = delete;
    AgenticTransaction(AgenticTransaction&&) noexcept;
    AgenticTransaction& operator=(AgenticTransaction&&) noexcept;

    // ---- Configuration ----
    void setJournal(Core::TransactionJournal* journal);
    void setCallbacks(const TxnCallbacks& callbacks);
    void setMaxSteps(uint32_t maxSteps);    // Safety limit (default: 1000)
    void setRollbackOnError(bool enable);   // Auto-rollback on handler error

    // ---- Inverse Map Registration ----
    // Call before begin() to register known inverse commands.
    // Built-in defaults are loaded automatically.
    void registerInverse(const InverseMapping& mapping);
    void registerInverseById(uint32_t forwardId, uint32_t inverseId);
    void registerInverseByName(const char* forwardCanonical, const char* inverseCanonical);
    void registerBarrier(uint32_t commandId);
    void loadDefaultInverseMappings();

    // ---- Transaction Lifecycle ----
    TxnResult begin(const char* description = nullptr);
    TxnResult commit();
    TxnResult rollback();
    TxnResult rollbackToSavepoint(const char* savepointName);

    // ---- Command Execution (within transaction) ----
    // These wrap the unified dispatcher and record each step.
    Dispatch::DispatchResult execute(uint32_t commandId, CommandContext& ctx);
    Dispatch::DispatchResult executeByCanonical(const char* name, CommandContext& ctx);
    Dispatch::DispatchResult executeByCli(const char* cliInput, CommandContext& ctx);

    // ---- Savepoints ----
    TxnResult savepoint(const char* name);
    bool      hasSavepoint(const char* name) const;

    // ---- State Queries ----
    TxnState            state() const;
    uint32_t            stepCount() const;
    uint32_t            txnId() const;
    const char*         description() const;
    bool                isActive() const;
    bool                canRollback() const;
    const TxnStatistics& statistics() const;

    // ---- Step Inspection ----
    const TransactionStep* getStep(uint32_t index) const;
    const std::vector<TransactionStep>& allSteps() const;

    // ---- State Snapshot Support ----
    // Register a callback that captures pre-execution state for a command.
    // This enables rollback for commands that modify state not captured
    // by the inverse mapping (e.g., editor buffer content).
    using StateSnapshotFn = std::vector<uint8_t>(*)(uint32_t commandId, void* userData);
    using StateRestoreFn  = bool(*)(uint32_t commandId, const std::vector<uint8_t>& snapshot, void* userData);
    void setStateSnapshotProvider(StateSnapshotFn capture, StateRestoreFn restore, void* userData);

    // ---- Dry Run ----
    // Validates a sequence of commands without executing them.
    // Checks: existence, exposure, handler linkage, reversibility.
    struct DryRunResult {
        bool valid;
        uint32_t firstFailIndex;   // Index of first failing command
        const char* reason;
        uint32_t nonReversibleCount;

        static DryRunResult ok(uint32_t nonRev = 0) {
            return { true, 0, "All commands valid", nonRev };
        }
        static DryRunResult fail(uint32_t idx, const char* reason) {
            return { false, idx, reason, 0 };
        }
    };

    DryRunResult dryRun(const uint32_t* commandIds, size_t count) const;
    DryRunResult dryRunByCanonical(const char** names, size_t count) const;

    // ---- Replay ----
    // Re-execute all committed steps (from journal or in-memory log)
    TxnResult replay(CommandContext& ctx);

    // ---- Crash Recovery ----
    // Reads journal for uncommitted transactions and rolls them back
    TxnResult recoverFromJournal();

private:
    // ---- Internal dispatch + recording ----
    Dispatch::DispatchResult executeInternal(
        const CmdDescriptor* desc,
        CommandContext& ctx,
        const char* inputType // "id", "canonical", "cli"
    );

    // ---- Inverse lookup ----
    const InverseMapping* findInverse(uint32_t commandId) const;
    const InverseMapping* findInverseByCanonical(const char* name) const;

    // ---- State management ----
    void setState(TxnState newState);
    uint64_t currentTimestampMs() const;

    // ---- Journal integration ----
    void journalBegin();
    void journalStep(const TransactionStep& step);
    void journalCommit();
    void journalRollback();

    // ---- Members ----
    mutable std::mutex              m_mutex;
    TxnState                        m_state;
    uint32_t                        m_txnId;
    std::string                     m_description;
    std::vector<TransactionStep>    m_steps;
    std::vector<Savepoint>          m_savepoints;
    std::vector<InverseMapping>     m_inverseMappings;
    Core::TransactionJournal*       m_journal;       // Nullable
    TxnCallbacks                    m_callbacks;
    TxnStatistics                   m_stats;
    uint32_t                        m_maxSteps;
    bool                            m_rollbackOnError;

    // State snapshot providers
    StateSnapshotFn                 m_snapshotCapture;
    StateRestoreFn                  m_snapshotRestore;
    void*                           m_snapshotUserData;

    // Transaction ID generator (process-global)
    static std::atomic<uint32_t>    s_nextTxnId;
};

// ============================================================================
// AGENTIC TASK GRAPH — Multi-transaction orchestration
// ============================================================================
// Composes multiple AgenticTransactions into a dependency-ordered task graph.
// Each node is a transaction that can depend on outputs of prior nodes.
// The graph executor handles fan-out, fan-in, and partial rollback.

struct TaskNode {
    uint32_t                nodeId;
    const char*             name;
    std::vector<uint32_t>   dependsOn;       // Node IDs this depends on
    std::vector<uint32_t>   commandSequence; // Commands to execute (by ID)

    // Per-node context overrides
    const char*             contextArgs;     // Extra args for this node's commands
    bool                    rollbackOnFail;  // Rollback this node if any step fails
    bool                    blockGraphOnFail;// Halt entire graph if this node fails

    // Result (filled after execution)
    TxnResult               result;
    bool                    executed;
};

// Task graph execution result
struct TaskGraphResult {
    bool        success;
    const char* detail;
    uint32_t    nodesExecuted;
    uint32_t    nodesRolledBack;
    uint32_t    nodesFailed;

    static TaskGraphResult ok(uint32_t executed) {
        return { true, "Task graph complete", executed, 0, 0 };
    }
    static TaskGraphResult partial(uint32_t exec, uint32_t rolled, uint32_t failed, const char* msg) {
        return { false, msg, exec, rolled, failed };
    }
};

class AgenticTaskGraph {
public:
    AgenticTaskGraph();
    ~AgenticTaskGraph();

    // Non-copyable
    AgenticTaskGraph(const AgenticTaskGraph&) = delete;
    AgenticTaskGraph& operator=(const AgenticTaskGraph&) = delete;

    // ---- Graph Construction ----
    uint32_t addNode(const char* name, const uint32_t* commands, size_t cmdCount);
    void     addDependency(uint32_t nodeId, uint32_t dependsOnNodeId);
    void     setNodeRollbackPolicy(uint32_t nodeId, bool rollbackOnFail, bool blockGraph);

    // ---- Configuration ----
    void setJournal(Core::TransactionJournal* journal);
    void setCallbacks(const TxnCallbacks& callbacks);

    // ---- Execution ----
    TaskGraphResult execute(CommandContext& ctx);
    TaskGraphResult rollbackAll();

    // ---- Introspection ----
    size_t               nodeCount() const;
    const TaskNode*      getNode(uint32_t nodeId) const;
    std::vector<uint32_t> topologicalOrder() const;
    bool                 hasCycle() const;

private:
    // Topological sort (Kahn's algorithm)
    std::vector<uint32_t> topoSort() const;
    bool detectCycle() const;

    mutable std::mutex                     m_mutex;
    std::vector<TaskNode>                  m_nodes;
    std::unordered_map<uint32_t, AgenticTransaction> m_nodeTxns;
    Core::TransactionJournal*              m_journal;
    TxnCallbacks                           m_callbacks;
    uint32_t                               m_nextNodeId;
};

} // namespace Agentic
} // namespace RawrXD

#endif // RAWRXD_AGENTIC_TRANSACTION_HPP
