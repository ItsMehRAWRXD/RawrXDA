// problems_aggregator.hpp — Top-50 IDE: single list for LSP, SAST, SCA, secrets
// One Problems view; all tools push here. Thread-safe.
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace RawrXD {

struct ProblemEntry {
    std::string source;     // "LSP", "SAST", "SCA", "Secrets", "Build"
    std::string path;       // file path (can be empty)
    int         line        = 0;   // 1-based
    int         column      = 0;   // 1-based
    int         endLine     = 0;
    int         endColumn  = 0;
    int         severity   = 0;   // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string code;       // e.g. "C6011", "SEC001"
    std::string message;
    std::string ruleId;     // optional rule identifier
};

/** Aggregator for all diagnostic sources (LSP, SAST, secrets, SCA, build). */
class ProblemsAggregator {
public:
    static ProblemsAggregator& instance() {
        static ProblemsAggregator s;
        return s;
    }

    void add(ProblemEntry p);
    void add(const std::string& source, const std::string& path, int line, int column,
             int severity, const std::string& code, const std::string& message,
             const std::string& ruleId = "");
    /** Overload with full range (endLine/endColumn for SARIF and LSP). */
    void add(const std::string& source, const std::string& path, int line, int column,
             int endLine, int endColumn, int severity, const std::string& code,
             const std::string& message, const std::string& ruleId = "");

    /** Get all problems; optional filter by path or source. */
    std::vector<ProblemEntry> getProblems(const std::string& pathFilter = "",
                                           const std::string& sourceFilter = "") const;

    /** Clear by source (e.g. "LSP") or clear all if source empty. */
    void clear(const std::string& source = "");

    size_t count() const { std::lock_guard<std::mutex> lock(m_mutex); return m_problems.size(); }

private:
    ProblemsAggregator() = default;
    mutable std::mutex m_mutex;
    std::vector<ProblemEntry> m_problems;
};

} // namespace RawrXD
