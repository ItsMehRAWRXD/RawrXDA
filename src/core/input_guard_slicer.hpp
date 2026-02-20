// ============================================================================
// input_guard_slicer.hpp — Input Guard with Backend Slicing
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Purpose: Make the 1B-token input limit safe by:
//   1. Detecting input size BEFORE sending to backend
//   2. Slicing oversized inputs into manageable chunks
//   3. Routing chunks to appropriate backends (CPU/GPU/hybrid)
//   4. Merging chunk results into coherent output
//   5. Budget tracking and quota enforcement
//
// This eliminates the OOM/crash risk from unbounded inputs and
// turns a potential instability into a competitive feature
// ("handle arbitrarily large inputs gracefully").
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include "../core/model_memory_hotpatch.hpp"

// ============================================================================
// SliceStrategy — How to divide oversized input
// ============================================================================
enum class SliceStrategy : uint8_t {
    FixedChunk      = 0,    // Equal-size chunks by token count
    SemanticBoundary = 1,   // Split at paragraph/section boundaries
    SlidingWindow   = 2,    // Overlapping windows for context continuity
    Hierarchical    = 3,    // Summarize first, then detail pass
    Count           = 4
};

// ============================================================================
// MergeStrategy — How to recombine chunk outputs
// ============================================================================
enum class MergeStrategy : uint8_t {
    Concatenate     = 0,    // Simple concatenation
    Deduplicate     = 1,    // Remove duplicate content from overlaps
    Summarize       = 2,    // LLM-based merging/summarization
    BestOf          = 3,    // Pick best chunk response
    Count           = 4
};

// ============================================================================
// BackendTarget — Which backend to route chunks to
// ============================================================================
enum class BackendTarget : uint8_t {
    Auto        = 0,    // System decides based on chunk size + load
    CPU         = 1,    // CPU inference
    GPU         = 2,    // GPU inference
    Hybrid      = 3,    // Split across CPU + GPU
    Remote      = 4,    // Remote server
    Count       = 5
};

// ============================================================================
// InputGuardConfig — Configuration for the input guard
// ============================================================================
struct InputGuardConfig {
    // Size limits
    size_t  maxTokensPerRequest;        // Hard limit per single request
    size_t  maxTokensPerChunk;          // Max tokens per slice
    size_t  overlapTokens;              // Overlap for sliding window strategy
    size_t  warningThresholdTokens;     // Issue warning above this

    // Strategies
    SliceStrategy   sliceStrategy;
    MergeStrategy   mergeStrategy;
    BackendTarget   defaultBackend;

    // Budget
    size_t  dailyTokenBudget;           // 0 = unlimited
    size_t  perSessionTokenBudget;      // 0 = unlimited

    // Safety
    int     maxChunksPerRequest;        // Limit chunk count (prevent abuse)
    int     chunkTimeoutMs;             // Per-chunk timeout
    bool    abortOnChunkFailure;        // Abort all chunks if one fails
    bool    preserveChunkOrder;         // Maintain output ordering
    float   maxMemoryUsagePercent;      // Abort if memory > this %

    InputGuardConfig()
        : maxTokensPerRequest(1000000000ULL) // 1B
        , maxTokensPerChunk(8192)
        , overlapTokens(512)
        , warningThresholdTokens(100000)
        , sliceStrategy(SliceStrategy::SemanticBoundary)
        , mergeStrategy(MergeStrategy::Deduplicate)
        , defaultBackend(BackendTarget::Auto)
        , dailyTokenBudget(0)
        , perSessionTokenBudget(0)
        , maxChunksPerRequest(500)
        , chunkTimeoutMs(30000)
        , abortOnChunkFailure(false)
        , preserveChunkOrder(true)
        , maxMemoryUsagePercent(85.0f)
    {}
};

// ============================================================================
// InputSlice — A single chunk of sliced input
// ============================================================================
struct InputSlice {
    int             sliceIndex;         // 0-based index
    std::string     content;            // The slice content
    size_t          estimatedTokens;    // Estimated token count
    size_t          originalStartByte;  // Start offset in original input
    size_t          originalEndByte;    // End offset in original input
    BackendTarget   targetBackend;      // Which backend processes this
    bool            hasOverlap;         // Whether this slice overlaps with neighbors
    size_t          overlapStartBytes;  // Where the overlap region starts
};

// ============================================================================
// SliceResult — Result from processing one slice
// ============================================================================
struct SliceResult {
    int             sliceIndex;
    bool            success;
    std::string     output;
    double          latencyMs;
    size_t          tokensUsed;
    BackendTarget   processedBy;
    std::string     errorDetail;
};

// ============================================================================
// GuardedInputResult — Final merged result
// ============================================================================
struct GuardedInputResult {
    bool                        success;
    std::string                 mergedOutput;
    std::vector<SliceResult>    sliceResults;
    int                         totalSlices;
    int                         successfulSlices;
    int                         failedSlices;
    size_t                      totalTokensUsed;
    double                      totalLatencyMs;
    bool                        wasSliced;          // false if input fit in one chunk
    std::string                 detail;

    static GuardedInputResult ok(const std::string& output, int slices, size_t tokens, double latency) {
        GuardedInputResult r;
        r.success = true;
        r.mergedOutput = output;
        r.totalSlices = slices;
        r.successfulSlices = slices;
        r.failedSlices = 0;
        r.totalTokensUsed = tokens;
        r.totalLatencyMs = latency;
        r.wasSliced = (slices > 1);
        r.detail = "OK";
        return r;
    }

    static GuardedInputResult error(const std::string& detail) {
        GuardedInputResult r;
        r.success = false;
        r.totalSlices = 0;
        r.successfulSlices = 0;
        r.failedSlices = 0;
        r.totalTokensUsed = 0;
        r.totalLatencyMs = 0;
        r.wasSliced = false;
        r.detail = detail;
        return r;
    }
};

// ============================================================================
// TokenBudgetInfo — Current budget state
// ============================================================================
struct TokenBudgetInfo {
    size_t dailyUsed;
    size_t dailyRemaining;
    size_t sessionUsed;
    size_t sessionRemaining;
    bool   dailyExhausted;
    bool   sessionExhausted;
};

// ============================================================================
// Callbacks
// ============================================================================
using SliceCompleteCb   = std::function<void(const SliceResult& result)>;
using InputWarningCb    = std::function<void(const std::string& warning)>;
using BudgetAlertCb     = std::function<void(const TokenBudgetInfo& budget)>;

/// Backend inference function: takes input string, returns output string
using InferenceFn       = std::function<std::string(const std::string&, BackendTarget)>;

// ============================================================================
// InputGuardSlicer — Singleton
// ============================================================================
class InputGuardSlicer {
public:
    static InputGuardSlicer& instance();

    // ---- Configuration ----
    void setConfig(const InputGuardConfig& config);
    InputGuardConfig getConfig() const;

    // ---- Token Estimation ----
    /// Estimate token count for input (uses simple heuristic: ~4 chars/token)
    size_t estimateTokens(const std::string& input) const;

    // ---- Pre-flight Check ----
    /// Check whether input needs slicing and validate budget
    PatchResult preflightCheck(const std::string& input) const;

    // ---- Slicing ----
    /// Slice input according to current strategy
    std::vector<InputSlice> sliceInput(const std::string& input) const;

    // ---- Full Pipeline ----
    /// Guard → Slice → Route → Process → Merge
    GuardedInputResult processGuarded(
        const std::string& input,
        InferenceFn inferenceFn);

    // ---- Backend Routing ----
    /// Determine optimal backend for a given slice
    BackendTarget routeSlice(const InputSlice& slice) const;

    // ---- Merging ----
    /// Merge slice results into final output
    std::string mergeResults(const std::vector<SliceResult>& results) const;

    // ---- Budget Management ----
    TokenBudgetInfo getBudgetInfo() const;
    void resetSessionBudget();
    void resetDailyBudget();

    // ---- Memory Safety ----
    /// Check current memory usage percentage
    float getCurrentMemoryUsage() const;
    bool isMemorySafe() const;

    // ---- Callbacks ----
    void setSliceCompleteCallback(SliceCompleteCb cb);
    void setWarningCallback(InputWarningCb cb);
    void setBudgetAlertCallback(BudgetAlertCb cb);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalRequests{0};
        std::atomic<uint64_t> slicedRequests{0};
        std::atomic<uint64_t> passedThrough{0};
        std::atomic<uint64_t> rejected{0};
        std::atomic<uint64_t> totalSlicesProcessed{0};
        std::atomic<uint64_t> totalTokensProcessed{0};
        std::atomic<uint64_t> budgetRejections{0};
        std::atomic<uint64_t> memoryRejections{0};
    };
    const Stats& getStats() const { return m_stats; }
    void resetStats();

    std::string getStatsJSON() const;

private:
    InputGuardSlicer();
    ~InputGuardSlicer() = default;
    InputGuardSlicer(const InputGuardSlicer&) = delete;
    InputGuardSlicer& operator=(const InputGuardSlicer&) = delete;

    // ---- Slicing Strategies ----
    std::vector<InputSlice> sliceFixed(const std::string& input) const;
    std::vector<InputSlice> sliceSemantic(const std::string& input) const;
    std::vector<InputSlice> sliceSlidingWindow(const std::string& input) const;
    std::vector<InputSlice> sliceHierarchical(const std::string& input) const;

    // ---- Merging Strategies ----
    std::string mergeConcatenate(const std::vector<SliceResult>& results) const;
    std::string mergeDeduplicate(const std::vector<SliceResult>& results) const;

    // ---- Budget tracking ----
    bool consumeBudget(size_t tokens);

    mutable std::mutex m_mutex;
    InputGuardConfig m_config;

    // Budget tracking
    std::atomic<size_t> m_dailyTokensUsed{0};
    std::atomic<size_t> m_sessionTokensUsed{0};

    Stats m_stats;

    // Callbacks
    SliceCompleteCb     m_sliceCompleteCb;
    InputWarningCb      m_warningCb;
    BudgetAlertCb       m_budgetAlertCb;
};
