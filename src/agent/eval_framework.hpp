/**
 * @file eval_framework.hpp
 * @brief Agent evaluation framework: run wish test cases, measure success rate, log metrics
 *
 * From AUTONOMOUS_AGENT_IMPLEMENTATION_ROADMAP.md TIER-3.
 * No Qt; uses std::expected-style results and callbacks.
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdint>

namespace RawrXD {
namespace Agent {

struct EvalTestCase {
    std::string wish;
    std::string expectedOutcome;       ///< Description or regex of expected result
    int maxExecutionTimeMs = 30000;
    bool requiresNetwork = false;
};

struct EvalRunResult {
    std::string wish;
    bool success = false;
    int elapsedMs = 0;
    std::string error;
    std::string resultSummary;
};

struct EvalReport {
    int totalRuns = 0;
    int successCount = 0;
    int failureCount = 0;
    double successRate = 0.0;
    int totalElapsedMs = 0;
    std::vector<EvalRunResult> results;
};

/**
 * Pluggable executor: (wish) -> (success, resultSummary, error, elapsedMs).
 * EvalFramework calls this for each test case.
 */
using EvalExecutorFn = std::function<EvalRunResult(const std::string& wish, int timeoutMs)>;

class EvalFramework {
public:
    EvalFramework() = default;

    void setExecutor(EvalExecutorFn fn) { m_executor = std::move(fn); }
    void setLogPath(const std::string& path) { m_logPath = path; }

    /** Run all test cases and return aggregate report */
    EvalReport runEvaluation(const std::vector<EvalTestCase>& cases);

    /** Log a single execution for later analysis */
    void logExecution(const std::string& wish, bool success, int timeMs, const std::string& error);

private:
    EvalExecutorFn m_executor;
    std::string m_logPath;
};

} // namespace Agent
} // namespace RawrXD
