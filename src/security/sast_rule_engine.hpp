// sast_rule_engine.hpp — In-house SAST: regex/source rules, no external SAST lib
// Runs rules over source buffers; reports to ProblemsAggregator.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace Security {

struct SastFinding {
    std::string path;
    int         line     = 0;
    int         column   = 0;
    std::string ruleId;
    std::string message;
    int         severity = 2;   // 1=Error, 2=Warning
    std::string snippet;
};

/** In-house SAST: rule set + scan. No external SAST dependency. */
class SastRuleEngine {
public:
    SastRuleEngine() = default;

    /** Scan a buffer (e.g. file content). Appends findings to out. Returns count added. */
    size_t scan(const std::string& path, const uint8_t* data, size_t size, std::vector<SastFinding>& out);

    /** Convenience: scan std::string. */
    size_t scan(const std::string& path, const std::string& content, std::vector<SastFinding>& out);

    /** Push findings to ProblemsAggregator (source "SAST"). */
    void reportToProblems(const std::vector<SastFinding>& findings);

private:
    void runRules(const std::string& path, const std::string& content, std::vector<SastFinding>& out);
};

} // namespace Security
} // namespace RawrXD
