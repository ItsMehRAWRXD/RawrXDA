// ============================================================================
// inference_cancellation.h — Inference Cancellation Token Propagation
// ============================================================================
// Threads a cooperative cancellation token through the entire inference
// pipeline — from LSP/API request → batch scheduler → forward pass →
// speculative decoder → token output.
//
// Cancellation points are inserted every N tokens in the generation loop,
// at speculative draft tree expansion, and at KV cache allocation.
// This eliminates the "30-second freeze on cancel" problem.
//
// Architecture:
//   InferenceCancelToken — shared atomic flag with scope tracking
//   CancelScope          — RAII hierarchical scope (parent→child propagation)
//   CancelCheckpoint     — lightweight inline check (branch predicts not-taken)
//   CancelableForward    — wrapper that injects checkpoints into forward pass
//
// Thread safety: atomic flags only; no locks on the check path.
// Hot-path: isCancelled() is a single relaxed atomic load (~1 cycle).
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// CancelReason — why the request was cancelled
// ---------------------------------------------------------------------------
enum class CancelReason : uint8_t {
    None            = 0,
    UserAbort       = 1,   // User pressed cancel / closed tab
    Timeout         = 2,   // Request exceeded time limit
    ParentCancelled = 3,   // Parent scope was cancelled
    ResourceLimit   = 4,   // OOM or VRAM exhaustion
    Preempted       = 5,   // Higher-priority request preempted
    ServerShutdown  = 6    // Graceful shutdown
};

// ---------------------------------------------------------------------------
// InferenceCancelToken — the core cancellation primitive
// ---------------------------------------------------------------------------
class InferenceCancelToken {
public:
    InferenceCancelToken()
        : m_cancelled(false), m_reason(CancelReason::None), m_cancelTimeNs(0) {}

    // Create a child token linked to a parent.
    // When parent is cancelled, child is also cancelled.
    static std::shared_ptr<InferenceCancelToken> createChild(
        std::shared_ptr<InferenceCancelToken> parent);

    // Cancel with a reason
    void cancel(CancelReason reason = CancelReason::UserAbort) {
        CancelReason expected = CancelReason::None;
        if (m_reason.compare_exchange_strong(expected, reason,
                std::memory_order_acq_rel)) {
            m_cancelled.store(true, std::memory_order_release);
            m_cancelTimeNs.store(
                (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count(),
                std::memory_order_relaxed);
            // Propagate to children
            propagateToChildren();
        }
    }

    // Hot-path check — single relaxed load, branch-predicts false
    bool isCancelled() const {
        return m_cancelled.load(std::memory_order_acquire);
    }

    CancelReason reason() const {
        return m_reason.load(std::memory_order_acquire);
    }

    uint64_t cancelTimeNs() const {
        return m_cancelTimeNs.load(std::memory_order_relaxed);
    }

    // Set a timeout: auto-cancel after durationMs
    void setTimeout(uint32_t durationMs) {
        m_timeoutMs = durationMs;
        m_startTimeNs.store(
            (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed);
    }

    // Check timeout (call periodically from checkpoint)
    bool checkTimeout() {
        if (m_timeoutMs == 0) return false;
        auto now = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
        auto start = m_startTimeNs.load(std::memory_order_relaxed);
        uint64_t elapsedMs = (now - start) / 1000000ULL;
        if (elapsedMs >= m_timeoutMs) {
            cancel(CancelReason::Timeout);
            return true;
        }
        return false;
    }

    // Register a child token for propagation
    void addChild(std::weak_ptr<InferenceCancelToken> child) {
        // Simple push; no lock needed — only called during setup
        m_children.push_back(std::move(child));
    }

private:
    void propagateToChildren() {
        CancelReason r = m_reason.load(std::memory_order_acquire);
        for (auto& weakChild : m_children) {
            if (auto child = weakChild.lock()) {
                child->cancel(CancelReason::ParentCancelled);
            }
        }
    }

    std::atomic<bool>         m_cancelled;
    std::atomic<CancelReason> m_reason;
    std::atomic<uint64_t>     m_cancelTimeNs;
    std::atomic<uint64_t>     m_startTimeNs{0};
    uint32_t                  m_timeoutMs = 0;
    std::vector<std::weak_ptr<InferenceCancelToken>> m_children;
};

// ---------------------------------------------------------------------------
// CancelScope — RAII scope that creates a child token
// ---------------------------------------------------------------------------
class CancelScope {
public:
    CancelScope(std::shared_ptr<InferenceCancelToken> parent,
                const std::string& scopeName = "")
        : m_parent(parent)
        , m_scopeName(scopeName)
    {
        m_token = InferenceCancelToken::createChild(parent);
    }

    ~CancelScope() {
        // Detach on scope exit (children become orphans, which is fine)
    }

    // Forward to the scoped token
    bool isCancelled() const { return m_token->isCancelled(); }
    void cancel(CancelReason r = CancelReason::UserAbort) { m_token->cancel(r); }
    std::shared_ptr<InferenceCancelToken> token() { return m_token; }
    const std::string& name() const { return m_scopeName; }

private:
    std::shared_ptr<InferenceCancelToken> m_parent;
    std::shared_ptr<InferenceCancelToken> m_token;
    std::string                           m_scopeName;
};

// ---------------------------------------------------------------------------
// CancelCheckpoint — inline check with configurable frequency
// ---------------------------------------------------------------------------
struct CancelCheckpoint {
    // Check every N tokens (0 = every token)
    uint32_t checkInterval = 10;
    uint32_t counter       = 0;

    // Returns true if cancelled. Call in tight loops.
    bool check(const InferenceCancelToken& token) {
        if (++counter < checkInterval) return false;
        counter = 0;
        if (token.isCancelled()) return true;
        // Also check timeout
        return false;
    }

    // Version that also checks timeout
    bool checkWithTimeout(InferenceCancelToken& token) {
        if (++counter < checkInterval) return false;
        counter = 0;
        if (token.isCancelled()) return true;
        return token.checkTimeout();
    }
};

// ---------------------------------------------------------------------------
// CancelableResult — result of a cancellable operation
// ---------------------------------------------------------------------------
enum class CancelableStatus : uint8_t {
    Completed  = 0,
    Cancelled  = 1,
    TimedOut   = 2,
    Error      = 3
};

template <typename T>
struct CancelableResult {
    CancelableStatus status;
    T                value;
    CancelReason     cancelReason = CancelReason::None;
    uint32_t         tokensGenerated = 0;

    bool ok() const { return status == CancelableStatus::Completed; }
};

// ---------------------------------------------------------------------------
// CancelableForward — wrapper for cancellable forward pass
// ---------------------------------------------------------------------------
class CancelableForward {
public:
    using ForwardFn = std::function<bool(int32_t layerIndex)>; // returns false to abort

    explicit CancelableForward(std::shared_ptr<InferenceCancelToken> token,
                               uint32_t checkpointInterval = 10)
        : m_token(std::move(token))
    {
        m_checkpoint.checkInterval = checkpointInterval;
    }

    // Run a layer-by-layer forward pass with cancellation checkpoints.
    // layerFn is called for each layer; return false from it to abort.
    CancelableResult<uint32_t> runLayers(uint32_t numLayers, ForwardFn layerFn)
    {
        CancelableResult<uint32_t> result;
        result.status = CancelableStatus::Completed;
        result.value = 0;

        for (uint32_t i = 0; i < numLayers; ++i) {
            // Cancellation check between layers
            if (m_checkpoint.checkWithTimeout(*m_token)) {
                result.status = (m_token->reason() == CancelReason::Timeout)
                    ? CancelableStatus::TimedOut
                    : CancelableStatus::Cancelled;
                result.cancelReason = m_token->reason();
                result.value = i;
                return result;
            }

            if (!layerFn((int32_t)i)) {
                result.status = CancelableStatus::Error;
                result.value = i;
                return result;
            }
            result.value = i + 1;
        }

        return result;
    }

    // Check cancellation (for use in generation loops)
    bool shouldStop() {
        return m_checkpoint.checkWithTimeout(*m_token);
    }

    // Access the underlying token
    InferenceCancelToken& token() { return *m_token; }

private:
    std::shared_ptr<InferenceCancelToken> m_token;
    CancelCheckpoint                      m_checkpoint;
};

// ---------------------------------------------------------------------------
// InferenceCancelToken factory (inline)
// ---------------------------------------------------------------------------
inline std::shared_ptr<InferenceCancelToken>
InferenceCancelToken::createChild(std::shared_ptr<InferenceCancelToken> parent)
{
    auto child = std::make_shared<InferenceCancelToken>();
    if (parent) {
        parent->addChild(child);
        // If parent already cancelled, immediately cancel child
        if (parent->isCancelled())
            child->cancel(CancelReason::ParentCancelled);
    }
    return child;
}

} // namespace rawrxd
