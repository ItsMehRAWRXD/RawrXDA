// problems_aggregator.cpp — Unified Problems (LSP, SAST, SCA, Secrets, Build)
#include "problems_aggregator.hpp"
#include <algorithm>

namespace RawrXD {

void ProblemsAggregator::add(ProblemEntry p) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_problems.push_back(std::move(p));
}

void ProblemsAggregator::add(const std::string& source, const std::string& path, int line, int column,
                              int severity, const std::string& code, const std::string& message,
                              const std::string& ruleId) {
    add(source, path, line, column, 0, 0, severity, code, message, ruleId);
}

void ProblemsAggregator::add(const std::string& source, const std::string& path, int line, int column,
                              int endLine, int endColumn, int severity, const std::string& code,
                              const std::string& message, const std::string& ruleId) {
    ProblemEntry p;
    p.source    = source;
    p.path      = path;
    p.line      = line;
    p.column    = column;
    p.endLine   = endLine;
    p.endColumn = endColumn;
    p.severity  = severity;
    p.code      = code;
    p.message   = message;
    p.ruleId    = ruleId;
    add(std::move(p));
}

std::vector<ProblemEntry> ProblemsAggregator::getProblems(const std::string& pathFilter,
                                                           const std::string& sourceFilter) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProblemEntry> out;
    for (const auto& p : m_problems) {
        if (!pathFilter.empty() && p.path != pathFilter) continue;
        if (!sourceFilter.empty() && p.source != sourceFilter) continue;
        out.push_back(p);
    }
    return out;
}

void ProblemsAggregator::clear(const std::string& source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (source.empty()) {
        m_problems.clear();
        return;
    }
    m_problems.erase(
        std::remove_if(m_problems.begin(), m_problems.end(),
                       [&source](const ProblemEntry& p) { return p.source == source; }),
        m_problems.end());
}

} // namespace RawrXD
