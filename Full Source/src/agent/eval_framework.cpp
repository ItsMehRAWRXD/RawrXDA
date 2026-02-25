/**
 * @file eval_framework.cpp
 * @brief Implementation of agent evaluation framework
 */
#include "eval_framework.hpp"
#include <fstream>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace RawrXD {
namespace Agent {

namespace {
    std::string isoTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        return std::string(buf);
    }
}

EvalReport EvalFramework::runEvaluation(const std::vector<EvalTestCase>& cases)
{
    EvalReport report;
    report.totalRuns = static_cast<int>(cases.size());
    report.successCount = 0;
    report.failureCount = 0;
    report.totalElapsedMs = 0;

    if (!m_executor) {
        fprintf(stderr, "[EvalFramework] No executor set; returning empty report\n");
        return report;
    }

    for (const auto& tc : cases) {
        EvalRunResult result = m_executor(tc.wish, tc.maxExecutionTimeMs);
        result.wish = tc.wish;
        report.results.push_back(result);
        report.totalElapsedMs += result.elapsedMs;
        if (result.success)
            report.successCount++;
        else
            report.failureCount++;

        logExecution(tc.wish, result.success, result.elapsedMs, result.error);
    }

    if (report.totalRuns > 0)
        report.successRate = static_cast<double>(report.successCount) / static_cast<double>(report.totalRuns);

    fprintf(stderr, "[EvalFramework] Evaluation complete: %d/%d passed (%.1f%%)\n",
            report.successCount, report.totalRuns, report.successRate * 100.0);

    return report;
}

void EvalFramework::logExecution(const std::string& wish, bool success, int timeMs, const std::string& error)
{
    if (m_logPath.empty()) return;

    std::ofstream out(m_logPath, std::ios::app);
    if (!out.is_open()) return;

    out << isoTimestamp() << "\t"
        << (success ? "OK" : "FAIL") << "\t"
        << timeMs << "ms\t"
        << wish.substr(0, 120) << "\t";
    if (!error.empty())
        out << error.substr(0, 200);
    out << "\n";
}

} // namespace Agent
} // namespace RawrXD
