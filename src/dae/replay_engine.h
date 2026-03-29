// ============================================================================
// replay_engine.h — DAE Deterministic Replay Engine
// ============================================================================
// Executes an IntentGraph against a ShadowFilesystem, records a TraceLog, and
// validates that repeated executions with identical inputs produce identical
// state hashes.
//
// Operating modes:
//
//   ReplayMode::Execute   — Apply all Intent IR nodes; write to shadow FS.
//   ReplayMode::Speculate — Dry-run against shadow FS; no side effects.
//                           Used for speculation validation before Commit.
//   ReplayMode::Audit     — Walk a historical TraceLog; no execution.
//
// Validation pipeline:
//   ┌──────────────────────────────────────────────────────────────┐
//   │  1. IntentGraph::Validate()         structural check         │
//   │  2. IntentGraph::ValidatePreconditions(shadowFs)             │
//   │  3. DAE_ValidateTransition (MASM hotpath or C++ fallback)    │
//   │  4. Apply node via shadow FS Write/Delete/Move               │
//   │  5. TraceLog::Append (seq, stateBeforeHash, actionDigest,    │
//   │                        stateAfterHash, prevEventHash)        │
//   └──────────────────────────────────────────────────────────────┘
//
// On divergence (hash mismatch vs historical trace):
//   - Emit DivergenceEvent with full context diff.
//   - Abort execution and return ReplayError::Diverged.
//   - ShadowFilesystem is reset; no workspace changes occur.
//
// Determinism Contract (matches DeterministicReplayEngine.h):
//   Given identical IntentGraph + workspace snapshot + tool registry version,
//   every execution produces identical stateAfterHash at every step.
//
// No exceptions. Fail closed.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <expected>

#include "shadow_fs.h"
#include "intent_ir.h"
#include "trace_log.h"
#include "tool_abi.h"

namespace RawrXD {
namespace DAE {

// ============================================================================
// C-linkage bindings for MASM hotpath
// (implemented in dae_validator.asm; C++ fallback in replay_engine.cpp)
// ============================================================================

#pragma pack(push, 1)
struct TransitionContext {
    uint64_t seq;           // +0x00
    uint8_t  stateHash[32]; // +0x08
    uint8_t  actionDigest[32]; // +0x28
    uint32_t opType;        // +0x48
    uint32_t reserved;      // +0x4C
};                          // total: 0x50 = 80 bytes
#pragma pack(pop)

static_assert(sizeof(TransitionContext) == 80, "TransitionContext layout mismatch");

extern "C" uint32_t DAE_ValidateTransition(const TransitionContext* ctx);
extern "C" void     DAE_HashChainStep(const uint8_t* stateHash,
                                       const uint8_t* actionDigest,
                                       uint8_t*       output);

// ============================================================================
// Error type
// ============================================================================

enum class ReplayError : uint32_t {
    None               = 0,
    GraphInvalid       = 1,
    PreconditionFailed = 2,
    TransitionInvalid  = 3,
    ToolFailed         = 4,
    Diverged           = 5,   // Hash mismatch vs historical trace
    LogWriteFailed     = 6,
    Aborted            = 7,
};

template<typename T>
using ReplayResult = std::expected<T, ReplayError>;

// ============================================================================
// DivergenceEvent — emitted when stateAfterHash doesn't match expected
// ============================================================================

struct DivergenceEvent {
    uint64_t    seq;
    std::string opTypeName;
    std::string targetPath;
    ContentHash expectedHash;
    ContentHash actualHash;
    std::string context;    // Diff summary, bounded to 2048 chars
};

// ============================================================================
// ReplayMode
// ============================================================================

enum class ReplayMode : uint8_t {
    Execute   = 0,
    Speculate = 1,
    Audit     = 2,
};

// ============================================================================
// ReplayConfig
// ============================================================================

struct ReplayConfig {
    ReplayMode   mode             = ReplayMode::Execute;
    ToolPolicy   toolPolicy;
    bool         abortOnDivergence = true;

    // If non-empty: load this trace and compare hashes on each step.
    std::string  referenceTracePath;
};

// ============================================================================
// ReplaySummary
// ============================================================================

struct ReplaySummary {
    uint64_t nodesExecuted  = 0;
    uint64_t nodessSkipped  = 0;   // NoOp or deduped via idempotency key
    uint64_t toolsInvoked   = 0;
    bool     allSucceeded   = false;
    ContentHash finalStateHash;
    std::vector<DivergenceEvent> divergences;
};

// ============================================================================
// ReplayEngine
// ============================================================================

class ReplayEngine {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    explicit ReplayEngine(ShadowFilesystem& shadowFs,
                          TraceLog&         traceLog);

    // -------------------------------------------------------------------------
    // Execution
    // -------------------------------------------------------------------------

    /// Execute (or speculate / audit) an IntentGraph.
    ReplayResult<ReplaySummary> Run(const IntentGraph& graph,
                                    const ReplayConfig& config = {});

    // -------------------------------------------------------------------------
    // Divergence observer
    // -------------------------------------------------------------------------

    using DivergenceCb = std::function<void(const DivergenceEvent&)>;
    void SetDivergenceCallback(DivergenceCb cb);

    // -------------------------------------------------------------------------
    // MASM hotpath feature flag
    // -------------------------------------------------------------------------

    /// Enable/disable the MASM DAE_ValidateTransition hotpath.
    /// Defaults to true if AVX2 is detected at init time.
    static void SetAsmHotpathEnabled(bool enabled) noexcept;
    static bool IsAsmHotpathEnabled() noexcept;

private:
    ShadowFilesystem& m_shadowFs;
    TraceLog&         m_traceLog;
    DivergenceCb      m_divergenceCb;

    // C++ reference implementation for TransitionContext validation
    static uint32_t ValidateTransitionCpp(const TransitionContext& ctx) noexcept;

    ReplayResult<void> ApplyNode(const IntentNode&    node,
                                  const ReplayConfig&  config,
                                  uint64_t             seq,
                                  const ContentHash&   prevHash,
                                  ReplaySummary&       summary);
};

} // namespace DAE
} // namespace RawrXD
