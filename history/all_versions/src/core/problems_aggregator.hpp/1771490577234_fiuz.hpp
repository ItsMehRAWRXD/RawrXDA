/**
 * @file problems_aggregator.hpp
 * @brief Aggregates build/lint problems from multiple sources (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct AggregatedProblem {
    std::string file;
    int line = 0;
    int column = 0;
    std::string severity;
    std::string message;
    std::string source;
};

class ProblemsAggregator {
public:
    void add(const AggregatedProblem& p) {
        m_problems.push_back(p);
    }
    void clear() { m_problems.clear(); }
    std::vector<AggregatedProblem> all() const { return m_problems; }
    int errorCount() const {
        int count = 0;
        for (const auto& p : m_problems) if (p.severity == "error") ++count;
        return count;
    }
    int warningCount() const {
        int count = 0;
        for (const auto& p : m_problems) if (p.severity == "warning") ++count;
        return count;
    }
private:
    std::vector<AggregatedProblem> m_problems;
};
