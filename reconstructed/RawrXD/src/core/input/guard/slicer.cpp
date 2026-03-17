// ============================================================================
// input_guard_slicer.cpp — Input Guard with Backend Slicing Implementation
// ============================================================================
// Ensures the 1B-token input limit is safe by slicing oversized inputs,
// routing chunks to appropriate backends, and merging results.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#include "input_guard_slicer.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <numeric>

// ============================================================================
// Singleton
// ============================================================================
InputGuardSlicer& InputGuardSlicer::instance() {
    static InputGuardSlicer inst;
    return inst;
}

InputGuardSlicer::InputGuardSlicer() {
    // Default config is set by InputGuardConfig constructor
}

// ============================================================================
// Configuration
// ============================================================================
void InputGuardSlicer::setConfig(const InputGuardConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

InputGuardConfig InputGuardSlicer::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

// ============================================================================
// Token Estimation — ~4 chars per token heuristic (matches GPT-family BPE avg)
// ============================================================================
size_t InputGuardSlicer::estimateTokens(const std::string& input) const {
    if (input.empty()) return 0;
    // Conservative: ~4 chars per token, with code being denser (~3.5)
    // Use 3.8 as a balanced estimate
    return static_cast<size_t>(static_cast<double>(input.size()) / 3.8 + 0.5);
}

// ============================================================================
// Pre-flight Check — Validate input before processing
// ============================================================================
PatchResult InputGuardSlicer::preflightCheck(const std::string& input) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (input.empty()) {
        return PatchResult::error("Empty input", 1);
    }

    size_t estimatedTokens = estimateTokens(input);

    // Check hard token limit
    if (estimatedTokens > m_config.maxTokensPerRequest) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Input exceeds max token limit: ~%zu tokens > %zu limit",
            estimatedTokens, m_config.maxTokensPerRequest);
        return PatchResult::error(buf, 2);
    }

    // Check memory safety
    if (!isMemorySafe()) {
        return PatchResult::error("Memory usage too high for safe processing", 3);
    }

    // Check budget
    if (m_config.dailyTokenBudget > 0) {
        size_t used = m_dailyTokensUsed.load(std::memory_order_relaxed);
        if (used + estimatedTokens > m_config.dailyTokenBudget) {
            return PatchResult::error("Daily token budget exceeded", 4);
        }
    }

    if (m_config.perSessionTokenBudget > 0) {
        size_t used = m_sessionTokensUsed.load(std::memory_order_relaxed);
        if (used + estimatedTokens > m_config.perSessionTokenBudget) {
            return PatchResult::error("Session token budget exceeded", 5);
        }
    }

    // Check chunk count limit
    size_t chunkCount = (estimatedTokens + m_config.maxTokensPerChunk - 1) / m_config.maxTokensPerChunk;
    if (static_cast<int>(chunkCount) > m_config.maxChunksPerRequest) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Input would require %zu chunks, exceeds max of %d",
            chunkCount, m_config.maxChunksPerRequest);
        return PatchResult::error(buf, 6);
    }

    // Issue warning if above threshold
    if (estimatedTokens > m_config.warningThresholdTokens) {
        if (m_warningCb) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Large input detected: ~%zu tokens (warning threshold: %zu)",
                estimatedTokens, m_config.warningThresholdTokens);
            // Note: can't call cb while holding lock without risk, but we trust
            // the callback to be brief
            m_warningCb(std::string(buf));
        }
    }

    return PatchResult::ok("Preflight passed");
}

// ============================================================================
// Slicing — Dispatch to strategy
// ============================================================================
std::vector<InputSlice> InputGuardSlicer::sliceInput(const std::string& input) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t estimatedTokens = estimateTokens(input);

    // If input fits in a single chunk, no slicing needed
    if (estimatedTokens <= m_config.maxTokensPerChunk) {
        InputSlice single;
        single.sliceIndex = 0;
        single.content = input;
        single.estimatedTokens = estimatedTokens;
        single.originalStartByte = 0;
        single.originalEndByte = input.size();
        single.targetBackend = m_config.defaultBackend;
        single.hasOverlap = false;
        single.overlapStartBytes = 0;
        return { single };
    }

    switch (m_config.sliceStrategy) {
        case SliceStrategy::FixedChunk:
            return sliceFixed(input);
        case SliceStrategy::SemanticBoundary:
            return sliceSemantic(input);
        case SliceStrategy::SlidingWindow:
            return sliceSlidingWindow(input);
        case SliceStrategy::Hierarchical:
            return sliceHierarchical(input);
        default:
            return sliceFixed(input);
    }
}

// ============================================================================
// Full Guarded Pipeline — Guard → Slice → Route → Process → Merge
// ============================================================================
GuardedInputResult InputGuardSlicer::processGuarded(
    const std::string& input,
    InferenceFn inferenceFn)
{
    auto startTime = std::chrono::steady_clock::now();

    m_stats.totalRequests.fetch_add(1, std::memory_order_relaxed);

    // Preflight
    PatchResult preResult = preflightCheck(input);
    if (!preResult.success) {
        m_stats.rejected.fetch_add(1, std::memory_order_relaxed);
        return GuardedInputResult::error(preResult.detail ? preResult.detail : "Preflight failed");
    }

    // Slice
    std::vector<InputSlice> slices = sliceInput(input);
    if (slices.empty()) {
        m_stats.rejected.fetch_add(1, std::memory_order_relaxed);
        return GuardedInputResult::error("Slicing produced zero chunks");
    }

    bool wasSliced = (slices.size() > 1);
    if (wasSliced) {
        m_stats.slicedRequests.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_stats.passedThrough.fetch_add(1, std::memory_order_relaxed);
    }

    // Route each slice to optimal backend
    for (auto& slice : slices) {
        slice.targetBackend = routeSlice(slice);
    }

    // Process each slice
    std::vector<SliceResult> results;
    results.reserve(slices.size());
    size_t totalTokensUsed = 0;
    int failedCount = 0;

    for (const auto& slice : slices) {
        auto sliceStart = std::chrono::steady_clock::now();

        SliceResult sr;
        sr.sliceIndex = slice.sliceIndex;
        sr.processedBy = slice.targetBackend;

        try {
            // Use the provided inference function
            if (inferenceFn) {
                sr.output = inferenceFn(slice.content, slice.targetBackend);
                sr.success = !sr.output.empty();
                sr.tokensUsed = estimateTokens(slice.content) + estimateTokens(sr.output);
            } else {
                sr.success = false;
                sr.errorDetail = "No inference function provided";
            }
        } catch (...) {
            // Catch-all safety (even though project is no-exceptions,
            // the inference fn is user-provided and may throw)
            sr.success = false;
            sr.errorDetail = "Exception during inference";
        }

        auto sliceEnd = std::chrono::steady_clock::now();
        sr.latencyMs = std::chrono::duration<double, std::milli>(sliceEnd - sliceStart).count();

        if (!sr.success) {
            ++failedCount;
            if (m_config.abortOnChunkFailure) {
                results.push_back(sr);
                m_stats.totalSlicesProcessed.fetch_add(
                    static_cast<uint64_t>(results.size()), std::memory_order_relaxed);
                auto endTime = std::chrono::steady_clock::now();
                double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

                GuardedInputResult final_r;
                final_r.success = false;
                final_r.totalSlices = static_cast<int>(slices.size());
                final_r.successfulSlices = static_cast<int>(results.size()) - failedCount;
                final_r.failedSlices = failedCount;
                final_r.totalTokensUsed = totalTokensUsed;
                final_r.totalLatencyMs = totalMs;
                final_r.wasSliced = wasSliced;
                final_r.sliceResults = results;
                final_r.detail = "Aborted: chunk failure (abortOnChunkFailure=true)";
                return final_r;
            }
        } else {
            totalTokensUsed += sr.tokensUsed;
        }

        // Fire slice-complete callback
        if (m_sliceCompleteCb) {
            m_sliceCompleteCb(sr);
        }

        results.push_back(sr);
    }

    m_stats.totalSlicesProcessed.fetch_add(
        static_cast<uint64_t>(results.size()), std::memory_order_relaxed);
    m_stats.totalTokensProcessed.fetch_add(totalTokensUsed, std::memory_order_relaxed);

    // Consume budget
    consumeBudget(totalTokensUsed);

    // Merge results
    std::string merged = mergeResults(results);

    auto endTime = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    bool allSuccess = (failedCount == 0);

    GuardedInputResult final_r;
    final_r.success = allSuccess || (!m_config.abortOnChunkFailure && !merged.empty());
    final_r.mergedOutput = merged;
    final_r.sliceResults = results;
    final_r.totalSlices = static_cast<int>(slices.size());
    final_r.successfulSlices = static_cast<int>(slices.size()) - failedCount;
    final_r.failedSlices = failedCount;
    final_r.totalTokensUsed = totalTokensUsed;
    final_r.totalLatencyMs = totalMs;
    final_r.wasSliced = wasSliced;
    final_r.detail = allSuccess ? "OK" : "Partial success — some chunks failed";
    return final_r;
}

// ============================================================================
// Backend Routing — Determine optimal backend for a slice
// ============================================================================
BackendTarget InputGuardSlicer::routeSlice(const InputSlice& slice) const {
    // If config says a specific backend, use it
    if (m_config.defaultBackend != BackendTarget::Auto) {
        return m_config.defaultBackend;
    }

    // Auto-routing logic:
    // - Small slices (<2K tokens) → CPU (fast, low overhead)
    // - Medium slices (2K-16K) → GPU (good throughput)
    // - Large slices (>16K) → Hybrid (split across CPU+GPU)
    if (slice.estimatedTokens < 2048) {
        return BackendTarget::CPU;
    } else if (slice.estimatedTokens < 16384) {
        return BackendTarget::GPU;
    } else {
        return BackendTarget::Hybrid;
    }
}

// ============================================================================
// Result Merging — Dispatch to strategy
// ============================================================================
std::string InputGuardSlicer::mergeResults(const std::vector<SliceResult>& results) const {
    if (results.empty()) return "";

    // Collect only successful results, in order
    std::vector<const SliceResult*> ordered;
    ordered.reserve(results.size());
    for (const auto& r : results) {
        if (r.success) {
            ordered.push_back(&r);
        }
    }

    if (m_config.preserveChunkOrder) {
        std::sort(ordered.begin(), ordered.end(),
            [](const SliceResult* a, const SliceResult* b) {
                return a->sliceIndex < b->sliceIndex;
            });
    }

    // Rebuild a temp vector of sorted results for strategy dispatch
    std::vector<SliceResult> sortedResults;
    sortedResults.reserve(ordered.size());
    for (auto* p : ordered) {
        sortedResults.push_back(*p);
    }

    switch (m_config.mergeStrategy) {
        case MergeStrategy::Concatenate:
            return mergeConcatenate(sortedResults);
        case MergeStrategy::Deduplicate:
            return mergeDeduplicate(sortedResults);
        case MergeStrategy::BestOf: {
            // Pick the longest output (heuristic for "best")
            const SliceResult* best = nullptr;
            for (const auto& r : sortedResults) {
                if (!best || r.output.size() > best->output.size()) {
                    best = &r;
                }
            }
            return best ? best->output : "";
        }
        default:
            return mergeConcatenate(sortedResults);
    }
}

// ============================================================================
// Budget Management
// ============================================================================
TokenBudgetInfo InputGuardSlicer::getBudgetInfo() const {
    TokenBudgetInfo info;
    info.dailyUsed = m_dailyTokensUsed.load(std::memory_order_relaxed);
    info.sessionUsed = m_sessionTokensUsed.load(std::memory_order_relaxed);

    if (m_config.dailyTokenBudget > 0) {
        info.dailyRemaining = (info.dailyUsed < m_config.dailyTokenBudget)
            ? (m_config.dailyTokenBudget - info.dailyUsed) : 0;
        info.dailyExhausted = (info.dailyUsed >= m_config.dailyTokenBudget);
    } else {
        info.dailyRemaining = SIZE_MAX;
        info.dailyExhausted = false;
    }

    if (m_config.perSessionTokenBudget > 0) {
        info.sessionRemaining = (info.sessionUsed < m_config.perSessionTokenBudget)
            ? (m_config.perSessionTokenBudget - info.sessionUsed) : 0;
        info.sessionExhausted = (info.sessionUsed >= m_config.perSessionTokenBudget);
    } else {
        info.sessionRemaining = SIZE_MAX;
        info.sessionExhausted = false;
    }

    return info;
}

void InputGuardSlicer::resetSessionBudget() {
    m_sessionTokensUsed.store(0, std::memory_order_relaxed);
}

void InputGuardSlicer::resetDailyBudget() {
    m_dailyTokensUsed.store(0, std::memory_order_relaxed);
}

bool InputGuardSlicer::consumeBudget(size_t tokens) {
    m_dailyTokensUsed.fetch_add(tokens, std::memory_order_relaxed);
    m_sessionTokensUsed.fetch_add(tokens, std::memory_order_relaxed);

    // Check if we crossed an alert threshold
    if (m_budgetAlertCb) {
        TokenBudgetInfo info = getBudgetInfo();
        if (info.dailyExhausted || info.sessionExhausted) {
            m_budgetAlertCb(info);
        }
        // Also alert at 80% usage
        if (m_config.dailyTokenBudget > 0 &&
            info.dailyUsed > (m_config.dailyTokenBudget * 80 / 100)) {
            m_budgetAlertCb(info);
        }
    }
    return true;
}

// ============================================================================
// Memory Safety — Check system memory usage
// ============================================================================
float InputGuardSlicer::getCurrentMemoryUsage() const {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memInfo)) {
        return 100.0f; // Fail-safe: assume full if we can't read
    }
    return static_cast<float>(memInfo.dwMemoryLoad);
}

bool InputGuardSlicer::isMemorySafe() const {
    return getCurrentMemoryUsage() < m_config.maxMemoryUsagePercent;
}

// ============================================================================
// Callbacks
// ============================================================================
void InputGuardSlicer::setSliceCompleteCallback(SliceCompleteCb cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sliceCompleteCb = cb;
}

void InputGuardSlicer::setWarningCallback(InputWarningCb cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_warningCb = cb;
}

void InputGuardSlicer::setBudgetAlertCallback(BudgetAlertCb cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_budgetAlertCb = cb;
}

// ============================================================================
// Statistics
// ============================================================================
void InputGuardSlicer::resetStats() {
    m_stats.totalRequests.store(0, std::memory_order_relaxed);
    m_stats.slicedRequests.store(0, std::memory_order_relaxed);
    m_stats.passedThrough.store(0, std::memory_order_relaxed);
    m_stats.rejected.store(0, std::memory_order_relaxed);
    m_stats.totalSlicesProcessed.store(0, std::memory_order_relaxed);
    m_stats.totalTokensProcessed.store(0, std::memory_order_relaxed);
    m_stats.budgetRejections.store(0, std::memory_order_relaxed);
    m_stats.memoryRejections.store(0, std::memory_order_relaxed);
}

std::string InputGuardSlicer::getStatsJSON() const {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "{"
        "\"totalRequests\":%llu,"
        "\"slicedRequests\":%llu,"
        "\"passedThrough\":%llu,"
        "\"rejected\":%llu,"
        "\"totalSlicesProcessed\":%llu,"
        "\"totalTokensProcessed\":%llu,"
        "\"budgetRejections\":%llu,"
        "\"memoryRejections\":%llu,"
        "\"dailyTokensUsed\":%zu,"
        "\"sessionTokensUsed\":%zu,"
        "\"currentMemoryPercent\":%.1f"
        "}",
        (unsigned long long)m_stats.totalRequests.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.slicedRequests.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.passedThrough.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.rejected.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.totalSlicesProcessed.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.totalTokensProcessed.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.budgetRejections.load(std::memory_order_relaxed),
        (unsigned long long)m_stats.memoryRejections.load(std::memory_order_relaxed),
        m_dailyTokensUsed.load(std::memory_order_relaxed),
        m_sessionTokensUsed.load(std::memory_order_relaxed),
        getCurrentMemoryUsage());
    return std::string(buf);
}

// ============================================================================
// Slicing Strategy: Fixed Chunk
// ============================================================================
std::vector<InputSlice> InputGuardSlicer::sliceFixed(const std::string& input) const {
    std::vector<InputSlice> slices;

    // Calculate bytes per chunk based on token estimate (~3.8 chars/token)
    size_t bytesPerChunk = static_cast<size_t>(
        static_cast<double>(m_config.maxTokensPerChunk) * 3.8);

    if (bytesPerChunk == 0) bytesPerChunk = 1024;

    size_t offset = 0;
    int idx = 0;
    while (offset < input.size()) {
        size_t chunkEnd = std::min(offset + bytesPerChunk, input.size());

        InputSlice slice;
        slice.sliceIndex = idx++;
        slice.content = input.substr(offset, chunkEnd - offset);
        slice.estimatedTokens = estimateTokens(slice.content);
        slice.originalStartByte = offset;
        slice.originalEndByte = chunkEnd;
        slice.targetBackend = m_config.defaultBackend;
        slice.hasOverlap = false;
        slice.overlapStartBytes = 0;
        slices.push_back(slice);

        offset = chunkEnd;
    }

    return slices;
}

// ============================================================================
// Slicing Strategy: Semantic Boundary
// ============================================================================
std::vector<InputSlice> InputGuardSlicer::sliceSemantic(const std::string& input) const {
    std::vector<InputSlice> slices;

    size_t bytesPerChunk = static_cast<size_t>(
        static_cast<double>(m_config.maxTokensPerChunk) * 3.8);
    if (bytesPerChunk == 0) bytesPerChunk = 1024;

    // Semantic boundary detection: prefer splitting at paragraph breaks,
    // section headers, or double newlines
    size_t offset = 0;
    int idx = 0;
    while (offset < input.size()) {
        size_t idealEnd = std::min(offset + bytesPerChunk, input.size());

        // If we're at the end, just take everything remaining
        if (idealEnd >= input.size()) {
            InputSlice slice;
            slice.sliceIndex = idx++;
            slice.content = input.substr(offset);
            slice.estimatedTokens = estimateTokens(slice.content);
            slice.originalStartByte = offset;
            slice.originalEndByte = input.size();
            slice.targetBackend = m_config.defaultBackend;
            slice.hasOverlap = false;
            slice.overlapStartBytes = 0;
            slices.push_back(slice);
            break;
        }

        // Search backwards from idealEnd for a good boundary
        size_t bestBoundary = idealEnd;
        bool foundBoundary = false;

        // Priority 1: Double newline (paragraph break)
        for (size_t i = idealEnd; i > offset + bytesPerChunk / 2; --i) {
            if (i + 1 < input.size() && input[i] == '\n' && input[i + 1] == '\n') {
                bestBoundary = i + 2;
                foundBoundary = true;
                break;
            }
        }

        // Priority 2: Single newline
        if (!foundBoundary) {
            for (size_t i = idealEnd; i > offset + bytesPerChunk / 2; --i) {
                if (input[i] == '\n') {
                    bestBoundary = i + 1;
                    foundBoundary = true;
                    break;
                }
            }
        }

        // Priority 3: Sentence boundary (period + space)
        if (!foundBoundary) {
            for (size_t i = idealEnd; i > offset + bytesPerChunk / 2; --i) {
                if (i + 1 < input.size() && input[i] == '.' && input[i + 1] == ' ') {
                    bestBoundary = i + 2;
                    foundBoundary = true;
                    break;
                }
            }
        }

        // Priority 4: Word boundary (space)
        if (!foundBoundary) {
            for (size_t i = idealEnd; i > offset + bytesPerChunk / 2; --i) {
                if (input[i] == ' ') {
                    bestBoundary = i + 1;
                    foundBoundary = true;
                    break;
                }
            }
        }

        // Fallback: hard cut
        if (!foundBoundary) {
            bestBoundary = idealEnd;
        }

        InputSlice slice;
        slice.sliceIndex = idx++;
        slice.content = input.substr(offset, bestBoundary - offset);
        slice.estimatedTokens = estimateTokens(slice.content);
        slice.originalStartByte = offset;
        slice.originalEndByte = bestBoundary;
        slice.targetBackend = m_config.defaultBackend;
        slice.hasOverlap = false;
        slice.overlapStartBytes = 0;
        slices.push_back(slice);

        offset = bestBoundary;
    }

    return slices;
}

// ============================================================================
// Slicing Strategy: Sliding Window (with overlap)
// ============================================================================
std::vector<InputSlice> InputGuardSlicer::sliceSlidingWindow(const std::string& input) const {
    std::vector<InputSlice> slices;

    size_t bytesPerChunk = static_cast<size_t>(
        static_cast<double>(m_config.maxTokensPerChunk) * 3.8);
    size_t overlapBytes = static_cast<size_t>(
        static_cast<double>(m_config.overlapTokens) * 3.8);

    if (bytesPerChunk == 0) bytesPerChunk = 1024;
    if (overlapBytes >= bytesPerChunk) overlapBytes = bytesPerChunk / 4;

    size_t stride = bytesPerChunk - overlapBytes;
    if (stride == 0) stride = 1;

    size_t offset = 0;
    int idx = 0;
    while (offset < input.size()) {
        size_t chunkEnd = std::min(offset + bytesPerChunk, input.size());

        InputSlice slice;
        slice.sliceIndex = idx++;
        slice.content = input.substr(offset, chunkEnd - offset);
        slice.estimatedTokens = estimateTokens(slice.content);
        slice.originalStartByte = offset;
        slice.originalEndByte = chunkEnd;
        slice.targetBackend = m_config.defaultBackend;
        slice.hasOverlap = (idx > 1); // All except first have overlap at start
        slice.overlapStartBytes = slice.hasOverlap ? overlapBytes : 0;
        slices.push_back(slice);

        // Advance by stride (not full chunk size — that's the overlap)
        offset += stride;
        if (chunkEnd >= input.size()) break;
    }

    return slices;
}

// ============================================================================
// Slicing Strategy: Hierarchical
// ============================================================================
std::vector<InputSlice> InputGuardSlicer::sliceHierarchical(const std::string& input) const {
    // Hierarchical slicing creates:
    //   Slice 0: A summary/overview chunk (first N tokens + last N tokens)
    //   Slices 1+: Detailed sequential chunks of the full input
    // The summary chunk provides context for the detail passes.

    std::vector<InputSlice> slices;

    size_t bytesPerChunk = static_cast<size_t>(
        static_cast<double>(m_config.maxTokensPerChunk) * 3.8);
    if (bytesPerChunk == 0) bytesPerChunk = 1024;

    // Create summary chunk: first 25% + last 25% of budget
    size_t summaryBudget = bytesPerChunk;
    size_t headSize = std::min(summaryBudget / 2, input.size());
    size_t tailSize = std::min(summaryBudget / 2, input.size() - headSize);

    std::string summary;
    summary.reserve(headSize + tailSize + 32);
    summary += input.substr(0, headSize);
    if (tailSize > 0 && (input.size() - tailSize) > headSize) {
        summary += "\n\n[... content truncated for summary ...]\n\n";
        summary += input.substr(input.size() - tailSize);
    }

    InputSlice summarySlice;
    summarySlice.sliceIndex = 0;
    summarySlice.content = summary;
    summarySlice.estimatedTokens = estimateTokens(summary);
    summarySlice.originalStartByte = 0;
    summarySlice.originalEndByte = input.size();
    summarySlice.targetBackend = m_config.defaultBackend;
    summarySlice.hasOverlap = false;
    summarySlice.overlapStartBytes = 0;
    slices.push_back(summarySlice);

    // Create detail chunks (full sequential pass)
    size_t offset = 0;
    int idx = 1;
    while (offset < input.size()) {
        size_t chunkEnd = std::min(offset + bytesPerChunk, input.size());

        InputSlice slice;
        slice.sliceIndex = idx++;
        slice.content = input.substr(offset, chunkEnd - offset);
        slice.estimatedTokens = estimateTokens(slice.content);
        slice.originalStartByte = offset;
        slice.originalEndByte = chunkEnd;
        slice.targetBackend = m_config.defaultBackend;
        slice.hasOverlap = false;
        slice.overlapStartBytes = 0;
        slices.push_back(slice);

        offset = chunkEnd;
    }

    return slices;
}

// ============================================================================
// Merge Strategy: Concatenate
// ============================================================================
std::string InputGuardSlicer::mergeConcatenate(const std::vector<SliceResult>& results) const {
    std::string merged;
    size_t totalSize = 0;
    for (const auto& r : results) {
        totalSize += r.output.size();
    }
    merged.reserve(totalSize + results.size()); // +1 per result for potential separator

    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0 && !results[i].output.empty() && !merged.empty()) {
            // Add separator between chunks if needed
            if (merged.back() != '\n' && results[i].output.front() != '\n') {
                merged += '\n';
            }
        }
        merged += results[i].output;
    }

    return merged;
}

// ============================================================================
// Merge Strategy: Deduplicate (remove overlap content)
// ============================================================================
std::string InputGuardSlicer::mergeDeduplicate(const std::vector<SliceResult>& results) const {
    if (results.empty()) return "";
    if (results.size() == 1) return results[0].output;

    std::string merged = results[0].output;

    for (size_t i = 1; i < results.size(); ++i) {
        const std::string& curr = results[i].output;
        if (curr.empty()) continue;

        // Try to find overlap between end of merged and start of curr
        // Check up to 200 chars of overlap
        size_t maxOverlap = std::min(size_t(200), std::min(merged.size(), curr.size()));
        size_t bestOverlap = 0;

        for (size_t overlapLen = maxOverlap; overlapLen >= 10; --overlapLen) {
            // Check if the last overlapLen chars of merged match
            // the first overlapLen chars of curr
            if (merged.compare(merged.size() - overlapLen, overlapLen,
                               curr, 0, overlapLen) == 0) {
                bestOverlap = overlapLen;
                break;
            }
        }

        if (bestOverlap > 0) {
            // Skip the overlapping portion
            merged += curr.substr(bestOverlap);
        } else {
            // No overlap found — concatenate with separator
            if (!merged.empty() && merged.back() != '\n' && curr.front() != '\n') {
                merged += '\n';
            }
            merged += curr;
        }
    }

    return merged;
}
