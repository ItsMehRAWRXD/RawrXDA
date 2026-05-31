// ============================================================================
// react_tool_loop.cpp — ReAct (Reason + Act) Dynamic Tool Loop
// ============================================================================
#include "react_tool_loop.h"
#include "../config/IDEConfig.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

namespace RawrXD {
namespace Agent {

// ============================================================================
// Construction / destruction
// ============================================================================

ReactToolLoop::ReactToolLoop(const ReactConfig& cfg)
    : m_cfg(cfg)
{
}

ReactToolLoop::~ReactToolLoop() = default;

// ============================================================================
// isDuplicate — cheap string-level similarity for anti-loop detection
// ============================================================================

bool ReactToolLoop::isDuplicate(const std::string& a, const std::string& b,
                                float threshold)
{
    if (a.empty() || b.empty())
        return false;

    // Build character bigram multisets and compute Jaccard
    auto makeBigrams = [](const std::string& s) {
        std::unordered_map<uint32_t, int> bg;
        for (size_t i = 0; i + 1 < s.size(); ++i) {
            uint32_t key = (static_cast<uint8_t>(s[i]) << 8)
                         | static_cast<uint8_t>(s[i + 1]);
            bg[key]++;
        }
        return bg;
    };

    auto bgA = makeBigrams(a);
    auto bgB = makeBigrams(b);

    int intersect = 0;
    int unionSize = 0;

    // Merge keys
    std::unordered_set<uint32_t> allKeys;
    for (auto& [k, _] : bgA) allKeys.insert(k);
    for (auto& [k, _] : bgB) allKeys.insert(k);

    for (auto k : allKeys) {
        int ca = 0, cb = 0;
        auto ia = bgA.find(k);
        auto ib = bgB.find(k);
        if (ia != bgA.end()) ca = ia->second;
        if (ib != bgB.end()) cb = ib->second;
        intersect += std::min(ca, cb);
        unionSize += std::max(ca, cb);
    }

    if (unionSize == 0)
        return false;

    float jaccard = static_cast<float>(intersect) / static_cast<float>(unionSize);
    return jaccard >= threshold;
}

// ============================================================================
// run — main ReAct loop
// ============================================================================

ReactResult ReactToolLoop::run(const std::string& goal)
{
    ReactResult result{};
    result.goalReached = false;
    METRICS.increment("runtime.react.runs_total");
    METRICS.gauge("runtime.react.running", 1.0);

    if (!m_thinkFn || !m_actFn) {
        result.failReason = "ThinkFn or ActFn not set";
        METRICS.increment("runtime.react.fail_total");
        METRICS.gauge("runtime.react.running", 0.0);
        return result;
    }

    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_running.store(true, std::memory_order_release);
        m_abortFlag.store(false, std::memory_order_release);
    }

    auto t0 = std::chrono::steady_clock::now();
    auto deadline = t0 + std::chrono::milliseconds(m_cfg.totalTimeoutMs);

    std::vector<ReactTrace> scratchpad;
    scratchpad.reserve(m_cfg.maxIterations);

    for (uint32_t iter = 0; iter < m_cfg.maxIterations; ++iter) {
        // Check abort
        if (m_abortFlag.load(std::memory_order_acquire)) {
            result.failReason = "Aborted by caller";
            break;
        }

        // Check timeout
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            result.failReason = "Total timeout exceeded";
            break;
        }

        auto iterStart = std::chrono::steady_clock::now();

        // ---- REASON ----
        ReactThought thought = m_thinkFn(goal, scratchpad);
        METRICS.gauge("runtime.react.last_confidence", thought.confidence);

        // Check confidence gate
        if (thought.confidence < m_cfg.confidenceThreshold) {
            result.failReason = "Confidence below threshold ("
                + std::to_string(thought.confidence) + " < "
                + std::to_string(m_cfg.confidenceThreshold) + ")";
            break;
        }

        // Check if goal reached
        if (thought.goalReached) {
            ReactTrace tr;
            tr.iteration   = iter;
            tr.thought     = thought.thought;
            tr.confidence  = thought.confidence;
            tr.goalReached = true;
            auto iterEnd = std::chrono::steady_clock::now();
            tr.durationMs = std::chrono::duration<double, std::milli>(iterEnd - iterStart).count();
            scratchpad.push_back(std::move(tr));
            METRICS.recordDuration("runtime.react.iteration_ms", scratchpad.back().durationMs);

            result.goalReached = true;
            result.finalAnswer = thought.finalAnswer;
            break;
        }

        // ---- ACT ----
        std::string observation;
        if (!thought.toolName.empty()) {
            METRICS.increment("runtime.react.tool_actions_total");
            observation = m_actFn(thought.toolName, thought.toolArgsJson);
        } else {
            observation = "[no tool selected]";
        }

        auto iterEnd = std::chrono::steady_clock::now();

        // ---- OBSERVE ----
        ReactTrace tr;
        tr.iteration   = iter;
        tr.thought     = thought.thought;
        tr.action      = thought.toolName;
        tr.actionArgs  = thought.toolArgsJson;
        tr.observation = observation;
        tr.confidence  = thought.confidence;
        tr.goalReached = false;
        tr.durationMs  = std::chrono::duration<double, std::milli>(iterEnd - iterStart).count();

        // Anti-loop: check if observation is nearly identical to previous
        if (!scratchpad.empty()) {
            const auto& prev = scratchpad.back();
            if (isDuplicate(prev.observation, observation, m_cfg.duplicationThreshold)
                && prev.action == thought.toolName)
            {
                scratchpad.push_back(std::move(tr));
                result.failReason = "Duplicate observation detected — breaking loop";
                break;
            }
        }

        scratchpad.push_back(std::move(tr));
        METRICS.recordDuration("runtime.react.iteration_ms", scratchpad.back().durationMs);
        METRICS.increment("runtime.react.iterations_total");

        // Prune scratchpad if too long
        if (m_cfg.enableScratchpadPrune
            && scratchpad.size() > m_cfg.scratchpadMaxEntries)
        {
            // Keep the first entry (original context) + last N-1
            size_t excess = scratchpad.size() - m_cfg.scratchpadMaxEntries;
            scratchpad.erase(scratchpad.begin() + 1,
                             scratchpad.begin() + 1 + static_cast<ptrdiff_t>(excess));
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.trace           = std::move(scratchpad);
    result.totalIterations = static_cast<uint32_t>(result.trace.size());
    result.totalMs         = std::chrono::duration<double, std::milli>(t1 - t0).count();
    METRICS.recordDuration("runtime.react.total_ms", result.totalMs);
    METRICS.gauge("runtime.react.last_iterations", static_cast<double>(result.totalIterations));
    if (result.goalReached) {
        METRICS.increment("runtime.react.success_total");
    } else {
        METRICS.increment("runtime.react.fail_total");
    }

    m_running.store(false, std::memory_order_release);
    METRICS.gauge("runtime.react.running", 0.0);
    return result;
}

// ============================================================================
// abort / isRunning
// ============================================================================

void ReactToolLoop::abort()
{
    m_abortFlag.store(true, std::memory_order_release);
}

bool ReactToolLoop::isRunning() const
{
    return m_running.load(std::memory_order_acquire);
}

} // namespace Agent
} // namespace RawrXD
