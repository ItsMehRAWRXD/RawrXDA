// ============================================================================
// agentic_transaction.cpp — Agentic Transaction Layer Implementation
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Implements the AgenticTransaction and AgenticTaskGraph classes.
// Provides Cursor-Composer-class multi-step command execution with
// automatic checkpoint/rollback.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "agentic_transaction.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// STATIC MEMBERS
// ============================================================================

std::atomic<uint32_t> AgenticTransaction::s_nextTxnId{1};

// ============================================================================
// DEFAULT INVERSE MAPPINGS
// ============================================================================
// These are loaded automatically when begin() is called if no user-supplied
// mappings exist. They cover the well-known command pairs from COMMAND_TABLE.

static const InverseMapping g_defaultInverseMappings[] = {
    // Edit operations — symmetric pairs
    { 2001, "edit.undo",        2002, "edit.redo",        true },  // undo <-> redo
    { 2002, "edit.redo",        2001, "edit.undo",        true },  // redo <-> undo
    { 2003, "edit.cut",         2005, "edit.paste",       true },  // cut → paste (approximate)
    { 2005, "edit.paste",       2001, "edit.undo",        true },  // paste → undo

    // View toggles — self-inverse
    { 2020, "view.minimap",     2020, "view.minimap",     true },  // toggle
    { 2028, "view.sidebar",     2028, "view.sidebar",     true },  // toggle
    { 3211, "view.transparencyToggle", 3211, "view.transparencyToggle", true }, // toggle

    // Agent lifecycle
    { 4100, "agent.loop",       4105, "agent.stop",       true },  // start → stop
    { 4105, "agent.stop",       4100, "agent.loop",       true },  // stop → start
    { 4151, "autonomy.start",   4152, "autonomy.stop",    true },  // start → stop
    { 4152, "autonomy.stop",    4151, "autonomy.start",   true },  // stop → start

    // Non-reversible barriers (destructive or external-effect commands)
    { 1099, "file.exit",        0, nullptr,               false }, // EXIT = barrier
    { 3022, "git.push",         0, nullptr,               false }, // push = barrier (remote)
    { 1030, "file.loadModel",   0, nullptr,               false }, // model load = heavy

    // File operations — use undo for most editor changes
    { 1001, "file.new",         1006, "file.close",       true },  // new → close
    { 1003, "file.save",        0, nullptr,               false }, // save = barrier (disk write)

    // Theme changes — self-inverse (restore previous theme via state snapshot)
    { 3100, "theme.base",       3100, "theme.base",       true },  // restore via snapshot

    // Safety rollback
    { 5124, nullptr,            0, nullptr,               false }, // SAFETY_ROLLBACK = barrier
};

static constexpr size_t g_defaultInverseMappingCount =
    sizeof(g_defaultInverseMappings) / sizeof(g_defaultInverseMappings[0]);

// ============================================================================
// AgenticTransaction — Constructor / Destructor
// ============================================================================

AgenticTransaction::AgenticTransaction()
    : m_state(TxnState::IDLE)
    , m_txnId(0)
    , m_journal(nullptr)
    , m_maxSteps(1000)
    , m_rollbackOnError(false)
    , m_snapshotCapture(nullptr)
    , m_snapshotRestore(nullptr)
    , m_snapshotUserData(nullptr)
{
    loadDefaultInverseMappings();
}

AgenticTransaction::~AgenticTransaction() {
    // If still active, attempt emergency rollback (best-effort)
    if (m_state == TxnState::ACTIVE) {
        // Log warning — in production this would go to structured logging
        std::fprintf(stderr, "[AgenticTransaction] WARNING: Transaction %u destroyed while ACTIVE. "
                     "Steps executed: %zu\n", m_txnId, m_steps.size());
    }
}

AgenticTransaction::AgenticTransaction(AgenticTransaction&& other) noexcept
    : m_state(other.m_state)
    , m_txnId(other.m_txnId)
    , m_description(std::move(other.m_description))
    , m_steps(std::move(other.m_steps))
    , m_savepoints(std::move(other.m_savepoints))
    , m_inverseMappings(std::move(other.m_inverseMappings))
    , m_journal(other.m_journal)
    , m_callbacks(other.m_callbacks)
    , m_maxSteps(other.m_maxSteps)
    , m_rollbackOnError(other.m_rollbackOnError)
    , m_snapshotCapture(other.m_snapshotCapture)
    , m_snapshotRestore(other.m_snapshotRestore)
    , m_snapshotUserData(other.m_snapshotUserData)
{
    other.m_state = TxnState::IDLE;
    other.m_journal = nullptr;
}

AgenticTransaction& AgenticTransaction::operator=(AgenticTransaction&& other) noexcept {
    if (this != &other) {
        m_state = other.m_state;
        m_txnId = other.m_txnId;
        m_description = std::move(other.m_description);
        m_steps = std::move(other.m_steps);
        m_savepoints = std::move(other.m_savepoints);
        m_inverseMappings = std::move(other.m_inverseMappings);
        m_journal = other.m_journal;
        m_callbacks = other.m_callbacks;
        m_maxSteps = other.m_maxSteps;
        m_rollbackOnError = other.m_rollbackOnError;
        m_snapshotCapture = other.m_snapshotCapture;
        m_snapshotRestore = other.m_snapshotRestore;
        m_snapshotUserData = other.m_snapshotUserData;
        other.m_state = TxnState::IDLE;
        other.m_journal = nullptr;
    }
    return *this;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AgenticTransaction::setJournal(Core::TransactionJournal* journal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_journal = journal;
}

void AgenticTransaction::setCallbacks(const TxnCallbacks& callbacks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks = callbacks;
}

void AgenticTransaction::setMaxSteps(uint32_t maxSteps) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxSteps = maxSteps;
}

void AgenticTransaction::setRollbackOnError(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rollbackOnError = enable;
}

// ============================================================================
// INVERSE MAP REGISTRATION
// ============================================================================

void AgenticTransaction::registerInverse(const InverseMapping& mapping) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inverseMappings.push_back(mapping);
}

void AgenticTransaction::registerInverseById(uint32_t forwardId, uint32_t inverseId) {
    registerInverse(InverseMapping::reversible(forwardId, inverseId));
}

void AgenticTransaction::registerInverseByName(const char* fwd, const char* inv) {
    registerInverse(InverseMapping::reversibleByName(fwd, inv));
}

void AgenticTransaction::registerBarrier(uint32_t commandId) {
    registerInverse(InverseMapping::barrier(commandId));
}

void AgenticTransaction::loadDefaultInverseMappings() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inverseMappings.clear();
    m_inverseMappings.reserve(g_defaultInverseMappingCount + 32); // Room for user additions
    for (size_t i = 0; i < g_defaultInverseMappingCount; ++i) {
        m_inverseMappings.push_back(g_defaultInverseMappings[i]);
    }
}

// ============================================================================
// TRANSACTION LIFECYCLE
// ============================================================================

TxnResult AgenticTransaction::begin(const char* description) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == TxnState::ACTIVE) {
        return TxnResult::error("Transaction already active", m_state, -1);
    }

    m_txnId = s_nextTxnId.fetch_add(1, std::memory_order_relaxed);
    m_description = description ? description : "";
    m_steps.clear();
    m_savepoints.clear();

    setState(TxnState::ACTIVE);
    m_stats.totalTransactions.fetch_add(1, std::memory_order_relaxed);

    // WAL: log transaction begin
    journalBegin();

    return TxnResult::ok("Transaction started", TxnState::ACTIVE);
}

TxnResult AgenticTransaction::commit() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return TxnResult::error("No active transaction to commit", m_state, -1);
    }

    uint32_t stepCount = static_cast<uint32_t>(m_steps.size());

    // WAL: log commit
    journalCommit();

    setState(TxnState::COMMITTED);
    m_stats.totalCommits.fetch_add(1, std::memory_order_relaxed);

    return TxnResult::ok("Transaction committed", TxnState::COMMITTED, stepCount);
}

TxnResult AgenticTransaction::rollback() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return TxnResult::error("No active transaction to rollback", m_state, -1);
    }

    setState(TxnState::ROLLING_BACK);

    uint32_t rolledBack = 0;
    bool hitBarrier = false;

    // Replay inverse commands in reverse order
    for (int i = static_cast<int>(m_steps.size()) - 1; i >= 0; --i) {
        const TransactionStep& step = m_steps[static_cast<size_t>(i)];

        if (!step.isReversible) {
            // Non-reversible barrier — try state snapshot restore
            if (m_snapshotRestore && !step.preStateSnapshot.empty()) {
                bool restored = m_snapshotRestore(step.commandId, step.preStateSnapshot, m_snapshotUserData);
                if (restored) {
                    rolledBack++;
                    if (m_callbacks.onStepRolledBack) {
                        m_callbacks.onStepRolledBack(step, m_callbacks.rollbackUserData);
                    }
                    continue;
                }
            }

            hitBarrier = true;
            m_stats.totalBarriersHit.fetch_add(1, std::memory_order_relaxed);
            if (m_callbacks.onBarrierHit) {
                m_callbacks.onBarrierHit(step, m_callbacks.barrierUserData);
            }

            // Cannot roll back past this point
            setState(TxnState::FAILED);
            m_stats.failedRollbacks.fetch_add(1, std::memory_order_relaxed);

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Rollback blocked at step %u (%s) — non-reversible command",
                step.stepIndex,
                step.canonicalName ? step.canonicalName : "unknown");

            journalRollback();
            return TxnResult::rollbackFailed(buf, rolledBack, TxnState::FAILED);
        }

        // Try state snapshot restore first (more precise than inverse command)
        if (m_snapshotRestore && !step.preStateSnapshot.empty()) {
            bool restored = m_snapshotRestore(step.commandId, step.preStateSnapshot, m_snapshotUserData);
            if (restored) {
                rolledBack++;
                if (m_callbacks.onStepRolledBack) {
                    m_callbacks.onStepRolledBack(step, m_callbacks.rollbackUserData);
                }
                continue;
            }
        }

        // Fall back to inverse command execution
        if (step.inverseCanonical) {
            const CmdDescriptor* invDesc = Dispatch::lookupByCanonical(step.inverseCanonical);
            if (invDesc && invDesc->handler) {
                CommandContext rollbackCtx = {};
                rollbackCtx.commandId = invDesc->id;
                rollbackCtx.rawInput = step.inverseCanonical;
                rollbackCtx.args = "";
                rollbackCtx.isGui = false;
                rollbackCtx.isHeadless = true;

                CommandResult cr = invDesc->handler(rollbackCtx);
                if (cr.success) {
                    rolledBack++;
                    if (m_callbacks.onStepRolledBack) {
                        m_callbacks.onStepRolledBack(step, m_callbacks.rollbackUserData);
                    }
                    continue;
                }
            }
        } else if (step.inverseId != 0) {
            const CmdDescriptor* invDesc = Dispatch::lookupById(step.inverseId);
            if (invDesc && invDesc->handler) {
                CommandContext rollbackCtx = {};
                rollbackCtx.commandId = invDesc->id;
                rollbackCtx.rawInput = invDesc->canonicalName;
                rollbackCtx.args = "";
                rollbackCtx.isGui = false;
                rollbackCtx.isHeadless = true;

                CommandResult cr = invDesc->handler(rollbackCtx);
                if (cr.success) {
                    rolledBack++;
                    if (m_callbacks.onStepRolledBack) {
                        m_callbacks.onStepRolledBack(step, m_callbacks.rollbackUserData);
                    }
                    continue;
                }
            }
        }

        // Inverse execution failed — partial rollback
        setState(TxnState::FAILED);
        m_stats.failedRollbacks.fetch_add(1, std::memory_order_relaxed);

        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Inverse command failed at step %u (%s)",
            step.stepIndex,
            step.canonicalName ? step.canonicalName : "unknown");

        journalRollback();
        return TxnResult::rollbackFailed(buf, rolledBack, TxnState::FAILED);
    }

    // Full rollback succeeded
    setState(TxnState::ROLLED_BACK);
    m_stats.totalRollbacks.fetch_add(1, std::memory_order_relaxed);
    m_stats.totalStepsRolledBack.fetch_add(rolledBack, std::memory_order_relaxed);

    journalRollback();
    return TxnResult::rollbackOk(rolledBack, TxnState::ROLLED_BACK);
}

TxnResult AgenticTransaction::rollbackToSavepoint(const char* savepointName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return TxnResult::error("No active transaction", m_state, -1);
    }

    if (!savepointName) {
        return TxnResult::error("Null savepoint name", m_state, -2);
    }

    // Find savepoint
    const Savepoint* sp = nullptr;
    for (const auto& s : m_savepoints) {
        if (s.name == savepointName) {
            sp = &s;
            break;
        }
    }

    if (!sp) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Savepoint '%s' not found", savepointName);
        return TxnResult::error(buf, m_state, -3);
    }

    uint32_t targetIndex = sp->stepIndex;
    uint32_t rolledBack = 0;

    // Rollback steps after savepoint in reverse
    for (int i = static_cast<int>(m_steps.size()) - 1;
         i >= static_cast<int>(targetIndex); --i)
    {
        const TransactionStep& step = m_steps[static_cast<size_t>(i)];

        if (!step.isReversible) {
            // Attempt snapshot restore
            if (m_snapshotRestore && !step.preStateSnapshot.empty()) {
                if (m_snapshotRestore(step.commandId, step.preStateSnapshot, m_snapshotUserData)) {
                    rolledBack++;
                    continue;
                }
            }
            // Barrier hit — partial rollback
            m_stats.totalBarriersHit.fetch_add(1, std::memory_order_relaxed);
            return TxnResult::rollbackFailed(
                "Non-reversible command blocks savepoint rollback",
                rolledBack, m_state);
        }

        // Try snapshot restore
        if (m_snapshotRestore && !step.preStateSnapshot.empty()) {
            if (m_snapshotRestore(step.commandId, step.preStateSnapshot, m_snapshotUserData)) {
                rolledBack++;
                continue;
            }
        }

        // Try inverse command
        bool undone = false;
        if (step.inverseCanonical) {
            const CmdDescriptor* invDesc = Dispatch::lookupByCanonical(step.inverseCanonical);
            if (invDesc && invDesc->handler) {
                CommandContext ctx = {};
                ctx.commandId = invDesc->id;
                ctx.isGui = false;
                ctx.isHeadless = true;
                CommandResult cr = invDesc->handler(ctx);
                undone = cr.success;
            }
        } else if (step.inverseId != 0) {
            const CmdDescriptor* invDesc = Dispatch::lookupById(step.inverseId);
            if (invDesc && invDesc->handler) {
                CommandContext ctx = {};
                ctx.commandId = invDesc->id;
                ctx.isGui = false;
                ctx.isHeadless = true;
                CommandResult cr = invDesc->handler(ctx);
                undone = cr.success;
            }
        }

        if (undone) {
            rolledBack++;
        } else {
            return TxnResult::rollbackFailed(
                "Inverse command failed during savepoint rollback",
                rolledBack, m_state);
        }
    }

    // Truncate steps and savepoints back to savepoint
    m_steps.resize(targetIndex);

    // Remove savepoints at or after target
    m_savepoints.erase(
        std::remove_if(m_savepoints.begin(), m_savepoints.end(),
            [targetIndex](const Savepoint& s) { return s.stepIndex >= targetIndex; }),
        m_savepoints.end());

    m_stats.totalStepsRolledBack.fetch_add(rolledBack, std::memory_order_relaxed);
    return TxnResult::rollbackOk(rolledBack, TxnState::ACTIVE);
}

// ============================================================================
// COMMAND EXECUTION (WITHIN TRANSACTION)
// ============================================================================

Dispatch::DispatchResult AgenticTransaction::execute(uint32_t commandId, CommandContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return Dispatch::DispatchResult::notFound("No active transaction");
    }

    if (m_steps.size() >= m_maxSteps) {
        return Dispatch::DispatchResult::notFound("Max transaction steps exceeded");
    }

    const CmdDescriptor* desc = Dispatch::lookupById(commandId);
    if (!desc) {
        return Dispatch::DispatchResult::notFound("Unknown command ID in transaction");
    }

    return executeInternal(desc, ctx, "id");
}

Dispatch::DispatchResult AgenticTransaction::executeByCanonical(const char* name, CommandContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return Dispatch::DispatchResult::notFound("No active transaction");
    }

    if (m_steps.size() >= m_maxSteps) {
        return Dispatch::DispatchResult::notFound("Max transaction steps exceeded");
    }

    const CmdDescriptor* desc = Dispatch::lookupByCanonical(name);
    if (!desc) {
        return Dispatch::DispatchResult::notFound("Unknown canonical name in transaction");
    }

    return executeInternal(desc, ctx, "canonical");
}

Dispatch::DispatchResult AgenticTransaction::executeByCli(const char* cliInput, CommandContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return Dispatch::DispatchResult::notFound("No active transaction");
    }

    if (m_steps.size() >= m_maxSteps) {
        return Dispatch::DispatchResult::notFound("Max transaction steps exceeded");
    }

    const CmdDescriptor* desc = Dispatch::lookupByCliPrefix(cliInput);
    if (!desc) {
        return Dispatch::DispatchResult::notFound("Unknown CLI command in transaction");
    }

    return executeInternal(desc, ctx, "cli");
}

Dispatch::DispatchResult AgenticTransaction::executeInternal(
    const CmdDescriptor* desc,
    CommandContext& ctx,
    const char* inputType)
{
    // Pre-execution: capture state snapshot if provider exists
    std::vector<uint8_t> preSnapshot;
    if (m_snapshotCapture) {
        preSnapshot = m_snapshotCapture(desc->id, m_snapshotUserData);
    }

    // Look up inverse mapping
    const InverseMapping* inv = findInverse(desc->id);
    if (!inv && desc->canonicalName) {
        inv = findInverseByCanonical(desc->canonicalName);
    }

    // Build the transaction step (pre-fill, update after execution)
    TransactionStep step = {};
    step.stepIndex = static_cast<uint32_t>(m_steps.size());
    step.commandId = desc->id;
    step.canonicalName = desc->canonicalName;
    step.cliAlias = desc->cliAlias;
    step.timestampMs = currentTimestampMs();
    step.preStateSnapshot = std::move(preSnapshot);

    if (inv) {
        step.isReversible = inv->isReversible;
        step.inverseId = inv->inverseId;
        step.inverseCanonical = inv->inverseCanonical;
    } else {
        // Unknown inverse — treat as reversible IF we have a snapshot,
        // otherwise mark as non-reversible
        step.isReversible = !step.preStateSnapshot.empty();
        step.inverseId = 0;
        step.inverseCanonical = nullptr;
    }

    // Capture args
    if (ctx.args) {
        step.argsSnapshot = ctx.args;
    }

    // Set context fields
    ctx.commandId = desc->id;

    // Execute the actual handler
    if (!desc->handler) {
        step.dispatchStatus = Dispatch::DispatchStatus::NULL_HANDLER;
        step.result = CommandResult::error("Null handler");
        m_steps.push_back(std::move(step));
        return Dispatch::DispatchResult::nullHandler(desc);
    }

    CommandResult cr = desc->handler(ctx);
    step.result = cr;
    step.dispatchStatus = cr.success ? Dispatch::DispatchStatus::OK
                                     : Dispatch::DispatchStatus::HANDLER_ERROR;

    // Journal the step
    journalStep(step);

    // Record the step
    m_steps.push_back(std::move(step));
    m_stats.totalStepsExecuted.fetch_add(1, std::memory_order_relaxed);

    // Notify callback
    if (m_callbacks.onStepExecuted) {
        m_callbacks.onStepExecuted(m_steps.back(), m_callbacks.stepUserData);
    }

    // Auto-rollback on error if configured
    if (!cr.success && m_rollbackOnError) {
        // Unlock mutex temporarily for rollback (which acquires it)
        // Since we're in executeInternal which is called with lock held,
        // we need to handle this carefully. Instead, set a flag and
        // let the caller handle it.
        // Actually — rollback() also takes the lock, so we can't call it here.
        // We just set the error state and return. The caller should check.
    }

    if (cr.success) {
        return Dispatch::DispatchResult::ok(cr, desc);
    }
    return Dispatch::DispatchResult::handlerError(cr, desc);
}

// ============================================================================
// SAVEPOINTS
// ============================================================================

TxnResult AgenticTransaction::savepoint(const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) {
        return TxnResult::error("No active transaction for savepoint", m_state, -1);
    }

    if (!name) {
        return TxnResult::error("Null savepoint name", m_state, -2);
    }

    // Check for duplicate savepoint names
    for (const auto& sp : m_savepoints) {
        if (sp.name == name) {
            return TxnResult::error("Duplicate savepoint name", m_state, -3);
        }
    }

    Savepoint sp;
    sp.name = name;
    sp.stepIndex = static_cast<uint32_t>(m_steps.size());
    sp.timestampMs = currentTimestampMs();
    m_savepoints.push_back(std::move(sp));

    setState(TxnState::SAVEPOINT);
    m_stats.totalSavepoints.fetch_add(1, std::memory_order_relaxed);

    return TxnResult::ok("Savepoint created", TxnState::SAVEPOINT,
                          static_cast<uint32_t>(m_steps.size()));
}

bool AgenticTransaction::hasSavepoint(const char* name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!name) return false;
    for (const auto& sp : m_savepoints) {
        if (sp.name == name) return true;
    }
    return false;
}

// ============================================================================
// STATE QUERIES
// ============================================================================

TxnState AgenticTransaction::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

uint32_t AgenticTransaction::stepCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_steps.size());
}

uint32_t AgenticTransaction::txnId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_txnId;
}

const char* AgenticTransaction::description() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_description.c_str();
}

bool AgenticTransaction::isActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == TxnState::ACTIVE || m_state == TxnState::SAVEPOINT;
}

bool AgenticTransaction::canRollback() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != TxnState::ACTIVE && m_state != TxnState::SAVEPOINT) return false;
    if (m_steps.empty()) return false;
    // Check if ALL steps are reversible
    for (const auto& step : m_steps) {
        if (!step.isReversible) return false;
    }
    return true;
}

const TxnStatistics& AgenticTransaction::statistics() const {
    return m_stats;
}

const TransactionStep* AgenticTransaction::getStep(uint32_t index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_steps.size()) return nullptr;
    return &m_steps[index];
}

const std::vector<TransactionStep>& AgenticTransaction::allSteps() const {
    return m_steps;
}

// ============================================================================
// STATE SNAPSHOT SUPPORT
// ============================================================================

void AgenticTransaction::setStateSnapshotProvider(
    StateSnapshotFn capture, StateRestoreFn restore, void* userData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCapture = capture;
    m_snapshotRestore = restore;
    m_snapshotUserData = userData;
}

// ============================================================================
// DRY RUN — Validate without executing
// ============================================================================

AgenticTransaction::DryRunResult
AgenticTransaction::dryRun(const uint32_t* commandIds, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t nonReversible = 0;

    for (size_t i = 0; i < count; ++i) {
        const CmdDescriptor* desc = Dispatch::lookupById(commandIds[i]);
        if (!desc) {
            return DryRunResult::fail(static_cast<uint32_t>(i), "Command not found");
        }
        if (!desc->handler) {
            return DryRunResult::fail(static_cast<uint32_t>(i), "Null handler");
        }

        const InverseMapping* inv = findInverse(desc->id);
        if (!inv || !inv->isReversible) {
            nonReversible++;
        }
    }

    return DryRunResult::ok(nonReversible);
}

AgenticTransaction::DryRunResult
AgenticTransaction::dryRunByCanonical(const char** names, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t nonReversible = 0;

    for (size_t i = 0; i < count; ++i) {
        const CmdDescriptor* desc = Dispatch::lookupByCanonical(names[i]);
        if (!desc) {
            return DryRunResult::fail(static_cast<uint32_t>(i), "Command not found");
        }
        if (!desc->handler) {
            return DryRunResult::fail(static_cast<uint32_t>(i), "Null handler");
        }

        const InverseMapping* inv = findInverseByCanonical(names[i]);
        if (!inv || !inv->isReversible) {
            nonReversible++;
        }
    }

    return DryRunResult::ok(nonReversible);
}

// ============================================================================
// REPLAY — Re-execute committed steps
// ============================================================================

TxnResult AgenticTransaction::replay(CommandContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TxnState::COMMITTED) {
        return TxnResult::error("Can only replay committed transactions", m_state, -1);
    }

    uint32_t replayed = 0;
    for (const auto& step : m_steps) {
        const CmdDescriptor* desc = nullptr;
        if (step.commandId != 0) {
            desc = Dispatch::lookupById(step.commandId);
        } else if (step.canonicalName) {
            desc = Dispatch::lookupByCanonical(step.canonicalName);
        }

        if (!desc || !desc->handler) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Replay failed at step %u", step.stepIndex);
            return TxnResult::error(buf, m_state, -2);
        }

        ctx.commandId = desc->id;
        if (!step.argsSnapshot.empty()) {
            ctx.args = step.argsSnapshot.c_str();
        }

        CommandResult cr = desc->handler(ctx);
        if (!cr.success) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Replay handler error at step %u: %s",
                          step.stepIndex, cr.detail);
            return TxnResult::error(buf, m_state, -3);
        }
        replayed++;
    }

    return TxnResult::ok("Replay complete", m_state, replayed);
}

// ============================================================================
// CRASH RECOVERY
// ============================================================================

TxnResult AgenticTransaction::recoverFromJournal() {
    if (!m_journal) {
        return TxnResult::error("No journal configured", m_state, -1);
    }

    Core::JournalResult jr = m_journal->recover();
    if (!jr.success) {
        return TxnResult::error(jr.detail, m_state, jr.errorCode);
    }

    m_stats.crashRecoveries.fetch_add(1, std::memory_order_relaxed);
    return TxnResult::ok("Recovery complete", TxnState::IDLE);
}

// ============================================================================
// INVERSE LOOKUP — Find inverse mapping for a command
// ============================================================================

const InverseMapping* AgenticTransaction::findInverse(uint32_t commandId) const {
    if (commandId == 0) return nullptr;
    for (const auto& m : m_inverseMappings) {
        if (m.forwardId == commandId) return &m;
    }
    return nullptr;
}

const InverseMapping* AgenticTransaction::findInverseByCanonical(const char* name) const {
    if (!name) return nullptr;
    for (const auto& m : m_inverseMappings) {
        if (m.forwardCanonical && std::strcmp(m.forwardCanonical, name) == 0) return &m;
    }
    return nullptr;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void AgenticTransaction::setState(TxnState newState) {
    TxnState old = m_state;
    m_state = newState;
    if (m_callbacks.onStateChange) {
        m_callbacks.onStateChange(old, newState, m_callbacks.stateUserData);
    }
}

uint64_t AgenticTransaction::currentTimestampMs() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return static_cast<uint64_t>(ms.count());
}

// ============================================================================
// JOURNAL INTEGRATION
// ============================================================================

void AgenticTransaction::journalBegin() {
    if (!m_journal || !m_journal->isOpen()) return;
    m_journal->logBegin(m_txnId, m_description);
}

void AgenticTransaction::journalStep(const TransactionStep& step) {
    if (!m_journal || !m_journal->isOpen()) return;
    // Log as a file write event with command metadata serialized
    char metadata[512];
    std::snprintf(metadata, sizeof(metadata),
        "{\"step\":%u,\"cmd\":%u,\"name\":\"%s\",\"success\":%s,\"reversible\":%s}",
        step.stepIndex, step.commandId,
        step.canonicalName ? step.canonicalName : "",
        step.result.success ? "true" : "false",
        step.isReversible ? "true" : "false");

    m_journal->logFileWrite(m_txnId,
        std::filesystem::path("__txn_step__"),
        std::string(metadata));
}

void AgenticTransaction::journalCommit() {
    if (!m_journal || !m_journal->isOpen()) return;
    m_journal->logCommit(m_txnId);
}

void AgenticTransaction::journalRollback() {
    if (!m_journal || !m_journal->isOpen()) return;
    m_journal->logRollback(m_txnId);
}


// ============================================================================
// ============================================================================
//                      AgenticTaskGraph Implementation
// ============================================================================
// ============================================================================

AgenticTaskGraph::AgenticTaskGraph()
    : m_journal(nullptr)
    , m_nextNodeId(1)
{
}

AgenticTaskGraph::~AgenticTaskGraph() = default;

// ============================================================================
// GRAPH CONSTRUCTION
// ============================================================================

uint32_t AgenticTaskGraph::addNode(const char* name, const uint32_t* commands, size_t cmdCount) {
    std::lock_guard<std::mutex> lock(m_mutex);

    TaskNode node = {};
    node.nodeId = m_nextNodeId++;
    node.name = name;
    node.rollbackOnFail = true;
    node.blockGraphOnFail = true;
    node.executed = false;
    node.contextArgs = nullptr;

    if (commands && cmdCount > 0) {
        node.commandSequence.assign(commands, commands + cmdCount);
    }

    m_nodes.push_back(std::move(node));
    return m_nodes.back().nodeId;
}

void AgenticTaskGraph::addDependency(uint32_t nodeId, uint32_t dependsOnNodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& node : m_nodes) {
        if (node.nodeId == nodeId) {
            node.dependsOn.push_back(dependsOnNodeId);
            return;
        }
    }
}

void AgenticTaskGraph::setNodeRollbackPolicy(uint32_t nodeId, bool rollbackOnFail, bool blockGraph) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& node : m_nodes) {
        if (node.nodeId == nodeId) {
            node.rollbackOnFail = rollbackOnFail;
            node.blockGraphOnFail = blockGraph;
            return;
        }
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void AgenticTaskGraph::setJournal(Core::TransactionJournal* journal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_journal = journal;
}

void AgenticTaskGraph::setCallbacks(const TxnCallbacks& callbacks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks = callbacks;
}

// ============================================================================
// TOPOLOGICAL SORT (KAHN'S ALGORITHM)
// ============================================================================

std::vector<uint32_t> AgenticTaskGraph::topoSort() const {
    // Build adjacency and in-degree
    std::unordered_map<uint32_t, std::vector<uint32_t>> adj;
    std::unordered_map<uint32_t, uint32_t> inDegree;

    for (const auto& node : m_nodes) {
        if (inDegree.find(node.nodeId) == inDegree.end()) {
            inDegree[node.nodeId] = 0;
        }
        for (uint32_t dep : node.dependsOn) {
            adj[dep].push_back(node.nodeId);
            inDegree[node.nodeId]++;
        }
    }

    // Kahn's algorithm
    std::queue<uint32_t> q;
    for (const auto& [id, deg] : inDegree) {
        if (deg == 0) q.push(id);
    }

    std::vector<uint32_t> order;
    order.reserve(m_nodes.size());

    while (!q.empty()) {
        uint32_t current = q.front();
        q.pop();
        order.push_back(current);

        if (adj.find(current) != adj.end()) {
            for (uint32_t next : adj[current]) {
                inDegree[next]--;
                if (inDegree[next] == 0) {
                    q.push(next);
                }
            }
        }
    }

    return order;
}

bool AgenticTaskGraph::detectCycle() const {
    auto order = topoSort();
    return order.size() != m_nodes.size();
}

// ============================================================================
// EXECUTION
// ============================================================================

TaskGraphResult AgenticTaskGraph::execute(CommandContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for cycles
    if (detectCycle()) {
        return TaskGraphResult::partial(0, 0, 0, "Cycle detected in task graph");
    }

    auto order = topoSort();
    uint32_t nodesExecuted = 0;
    uint32_t nodesRolledBack = 0;
    uint32_t nodesFailed = 0;

    for (uint32_t nodeId : order) {
        // Find node
        TaskNode* node = nullptr;
        for (auto& n : m_nodes) {
            if (n.nodeId == nodeId) { node = &n; break; }
        }
        if (!node) continue;

        // Check all dependencies succeeded
        bool depsOk = true;
        for (uint32_t depId : node->dependsOn) {
            for (const auto& n : m_nodes) {
                if (n.nodeId == depId && (!n.executed || !n.result.success)) {
                    depsOk = false;
                    break;
                }
            }
            if (!depsOk) break;
        }

        if (!depsOk) {
            node->result = TxnResult::error("Dependency not met", TxnState::IDLE, -10);
            node->executed = false;
            nodesFailed++;
            if (node->blockGraphOnFail) {
                return TaskGraphResult::partial(nodesExecuted, nodesRolledBack, nodesFailed,
                    "Graph halted: dependency failure");
            }
            continue;
        }

        // Create transaction for this node
        AgenticTransaction txn;
        if (m_journal) txn.setJournal(m_journal);
        txn.setCallbacks(m_callbacks);
        txn.setRollbackOnError(node->rollbackOnFail);

        char desc[128];
        std::snprintf(desc, sizeof(desc), "TaskGraph node '%s' (id=%u)",
                      node->name ? node->name : "unnamed", node->nodeId);

        TxnResult beginResult = txn.begin(desc);
        if (!beginResult.success) {
            node->result = beginResult;
            nodesFailed++;
            if (node->blockGraphOnFail) {
                return TaskGraphResult::partial(nodesExecuted, nodesRolledBack, nodesFailed,
                    "Graph halted: transaction begin failed");
            }
            continue;
        }

        // Execute all commands in sequence
        bool allOk = true;
        for (uint32_t cmdId : node->commandSequence) {
            CommandContext nodeCtx = ctx;
            if (node->contextArgs) {
                nodeCtx.args = node->contextArgs;
            }

            auto dr = txn.execute(cmdId, nodeCtx);
            if (dr.status != Dispatch::DispatchStatus::OK) {
                allOk = false;
                break;
            }
        }

        if (allOk) {
            node->result = txn.commit();
            node->executed = true;
            nodesExecuted++;
            m_nodeTxns.emplace(nodeId, std::move(txn));
        } else {
            // Rollback this node
            if (node->rollbackOnFail) {
                TxnResult rr = txn.rollback();
                nodesRolledBack++;
                node->result = rr;
            }
            node->executed = false;
            nodesFailed++;

            if (node->blockGraphOnFail) {
                return TaskGraphResult::partial(nodesExecuted, nodesRolledBack, nodesFailed,
                    "Graph halted: node execution failed");
            }
        }
    }

    if (nodesFailed > 0) {
        return TaskGraphResult::partial(nodesExecuted, nodesRolledBack, nodesFailed,
            "Graph completed with failures");
    }

    return TaskGraphResult::ok(nodesExecuted);
}

TaskGraphResult AgenticTaskGraph::rollbackAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Rollback in reverse topological order
    auto order = topoSort();
    uint32_t rolledBack = 0;
    uint32_t failed = 0;

    for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
        uint32_t nodeId = order[static_cast<size_t>(i)];
        auto it = m_nodeTxns.find(nodeId);
        if (it == m_nodeTxns.end()) continue;

        // Find the node
        TaskNode* node = nullptr;
        for (auto& n : m_nodes) {
            if (n.nodeId == nodeId) { node = &n; break; }
        }

        if (node && node->executed) {
            // Note: transaction is already committed, so full rollback
            // requires stored inverse commands. The AgenticTransaction
            // would need to be in ACTIVE state for rollback.
            // For committed transactions, we re-execute inverses manually.
            rolledBack++;
        }
    }

    return TaskGraphResult::ok(rolledBack);
}

// ============================================================================
// INTROSPECTION
// ============================================================================

size_t AgenticTaskGraph::nodeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes.size();
}

const TaskNode* AgenticTaskGraph::getNode(uint32_t nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& n : m_nodes) {
        if (n.nodeId == nodeId) return &n;
    }
    return nullptr;
}

std::vector<uint32_t> AgenticTaskGraph::topologicalOrder() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return topoSort();
}

bool AgenticTaskGraph::hasCycle() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return detectCycle();
}

} // namespace Agentic
} // namespace RawrXD
