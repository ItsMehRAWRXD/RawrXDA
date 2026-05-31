// ============================================================================
// phase_latency_tracker.hpp — Predictive Phase-Aware Latency Tracker
// ============================================================================
// Instruments parser and pipeline stages with lightweight scope timers.
// Feeds "micros-per-node" metrics into the CreditGovernor for predictive
// threshold adjustment.
//
// Usage:
//   {
//       PhaseScopeGuard guard(tracker, Phase::RustFunctionDecl);
//       auto node = parseFunction(...);
//   } // guard destructor records latency
//
//   tracker.FlushToGovernor(governor); // preemptive credit boost
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>
#include <array>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace RawrXD {
namespace FlowControl {

// Forward declaration
class CreditGovernor;

// ============================================================================
// Phase taxonomy — matches AST node "weight classes"
// ============================================================================
enum class Phase : uint8_t {
    Unknown = 0,
    // Rust AST phases
    RustFunctionDecl,
    RustStructDecl,
    RustEnumDecl,
    RustTraitDecl,
    RustImplBlock,
    RustMatchBlock,
    RustUseStatement,
    RustLetBinding,
    RustGenericParams,
    RustWhereClause,
    // Pipeline phases
    Tokenize,
    Parse,
    TypeCheck,
    Lower,
    Optimize,
    Codegen,
    // Inference phases
    Prefill,
    Decode,
    Verify,
    // Sentinel
    Count
};

const char* PhaseName(Phase p);

// ============================================================================
// Per-phase latency statistics
// ============================================================================
struct PhaseStats {
    uint64_t totalCalls = 0;
    uint64_t totalMicros = 0;
    uint64_t maxMicros = 0;
    uint64_t minMicros = UINT64_MAX;
    double emaMicros = 0.0;      // Exponential moving average
    double emaAlpha = 0.3;       // Smoothing factor
    uint64_t lastMicros = 0;
    std::chrono::steady_clock::time_point lastCall;

    void Record(uint64_t micros);
    double AvgMicros() const { return totalCalls > 0 ? static_cast<double>(totalMicros) / totalCalls : 0.0; }
    bool IsHeavy() const { return emaMicros > 500.0; }  // >500μs = heavy node
    bool IsBursty() const;  // High variance in recent calls
};

// ============================================================================
// Nesting context — tracks recursive depth per phase
// ============================================================================
struct NestingContext {
    Phase phase = Phase::Unknown;
    uint32_t depth = 0;
    uint64_t cumulativeMicros = 0;
    bool isHeavyNesting = false;  // depth > 3 with heavy phases
};

// ============================================================================
// Phase Latency Tracker — singleton per thread
// ============================================================================
class PhaseLatencyTracker {
public:
    static PhaseLatencyTracker& instance();

    // Record a phase completion (manual, if not using ScopeGuard)
    void Record(Phase phase, uint64_t micros);

    // Check if we are currently in "heavy nesting" territory
    bool IsHeavyNesting() const;

    // Get the deepest active nesting context
    NestingContext DeepestNesting() const;

    // Flush aggregated stats to CreditGovernor for predictive adjustment
    void FlushToGovernor(CreditGovernor* governor);

    // Get stats for a specific phase
    const PhaseStats* GetStats(Phase phase) const;

    // Reset all statistics
    void Reset();

    // JSON report for telemetry
    std::string ReportJson() const;

    // Thread-local nesting stack (push/pop via ScopeGuard)
    void PushPhase(Phase phase);
    void PopPhase(uint64_t micros);

private:
    PhaseLatencyTracker() = default;
    ~PhaseLatencyTracker() = default;
    PhaseLatencyTracker(const PhaseLatencyTracker&) = delete;
    PhaseLatencyTracker& operator=(const PhaseLatencyTracker&) = delete;

    std::array<PhaseStats, static_cast<size_t>(Phase::Count)> stats_;
    std::vector<NestingContext> nestingStack_;
    std::atomic<uint64_t> totalRecordedCalls_{0};
    mutable std::mutex mutex_;
};

// ============================================================================
// ScopeGuard — RAII phase timer
// ============================================================================
class PhaseScopeGuard {
public:
    explicit PhaseScopeGuard(Phase phase);
    ~PhaseScopeGuard();

    // Disable copy/move
    PhaseScopeGuard(const PhaseScopeGuard&) = delete;
    PhaseScopeGuard& operator=(const PhaseScopeGuard&) = delete;
    PhaseScopeGuard(PhaseScopeGuard&&) = delete;
    PhaseScopeGuard& operator=(PhaseScopeGuard&&) = delete;

    // Manual early completion (optional)
    void Complete();

private:
    Phase phase_;
    std::chrono::steady_clock::time_point start_;
    bool completed_ = false;
};

// ============================================================================
// Predictive Governor Extension — wraps CreditGovernor with phase awareness
// ============================================================================
class PredictiveGovernor {
public:
    PredictiveGovernor(CreditGovernor* baseGovernor);

    // Call this after every parser phase completion
    void OnPhaseComplete(Phase phase, uint64_t micros);

    // Call this before entering a known-heavy region
    void EnterHeavyRegion(Phase phase);
    void ExitHeavyRegion(Phase phase);

    // Preemptive credit boost based on predicted load
    void ApplyPredictiveBoost();

    // Get current predicted load factor (1.0 = normal, >1.0 = heavy)
    double PredictedLoadFactor() const;

private:
    CreditGovernor* baseGovernor_ = nullptr;
    PhaseLatencyTracker& tracker_;
    double currentLoadFactor_ = 1.0;
    bool inHeavyRegion_ = false;
    Phase heavyPhase_ = Phase::Unknown;
};

} // namespace FlowControl
} // namespace RawrXD
