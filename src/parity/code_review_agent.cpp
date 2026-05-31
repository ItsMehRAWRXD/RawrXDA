// code_review_agent.cpp - Implementation (deterministic heuristics + pluggable external reviewer)
#include "code_review_agent.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace rawrxd::parity {

namespace {

const char* sev_name(ReviewSeverity s) {
    switch (s) {
        case ReviewSeverity::Critical: return "critical";
        case ReviewSeverity::Major:    return "major";
        case ReviewSeverity::Minor:    return "minor";
        default:                       return "info";
    }
}

struct Rule {
    const char*   id;
    std::regex    rx;
    ReviewSeverity sev;
    const char*   message;
    const char*   suggestion;
};

const std::vector<Rule>& rules_for(const std::string& lang) {
    static std::unordered_map<std::string, std::vector<Rule>> table;
    static std::once_flag once;
    std::call_once(once, [&]() {
        // Common / cross-language rules (comment markers, secrets handled separately).
        std::vector<Rule> common = {
            { "todo-fixme",        std::regex(R"((?i)\b(TODO|FIXME|XXX|HACK)\b)"),
              ReviewSeverity::Minor,
              "TODO/FIXME left in code",
              "Either resolve now or link to a tracked issue." },
            { "long-line",         std::regex(R"(^.{160,}$)"),
              ReviewSeverity::Info,
              "Line exceeds 160 characters",
              "Consider wrapping for readability." },
            { "trailing-ws",       std::regex(R"([ \t]+$)"),
              ReviewSeverity::Info,
              "Trailing whitespace",
              "Remove trailing whitespace." },
        };
        // C / C++
        std::vector<Rule> cpp = common;
        cpp.insert(cpp.end(), {
            { "cpp-strcpy",        std::regex(R"(\b(strcpy|strcat|sprintf|gets)\s*\()"),
              ReviewSeverity::Major,
              "Unsafe C string function",
              "Use bounded variants (strncpy_s, snprintf)." },
            { "cpp-system",        std::regex(R"(\bsystem\s*\()"),
              ReviewSeverity::Major,
              "Use of system() — command injection risk",
              "Invoke the program directly with argv." },
            { "cpp-new-no-delete", std::regex(R"(\bnew\s+[A-Za-z_][A-Za-z_0-9]*\s*\()"),
              ReviewSeverity::Info,
              "Raw `new` detected — prefer RAII",
              "Use std::make_unique / make_shared." },
            { "cpp-magic-number",  std::regex(R"(\b(0x[0-9A-Fa-f]{6,}|[0-9]{5,})\b)"),
              ReviewSeverity::Info,
              "Magic number",
              "Consider naming this constant." },
        });
        // Python
        std::vector<Rule> py = common;
        py.insert(py.end(), {
            { "py-eval",   std::regex(R"(\b(eval|exec)\s*\()"),
              ReviewSeverity::Major, "Use of eval/exec — code injection risk",
              "Avoid dynamic evaluation of untrusted input." },
            { "py-bare-except", std::regex(R"(\bexcept\s*:)"),
              ReviewSeverity::Minor, "Bare except — hides errors",
              "Catch a specific exception type." },
            { "py-print-debug", std::regex(R"(^\s*print\()"),
              ReviewSeverity::Info,
              "Debug print left in code",
              "Switch to logging." },
        });
        // JS/TS
        std::vector<Rule> js = common;
        js.insert(js.end(), {
            { "js-eval",        std::regex(R"(\beval\s*\()"),
              ReviewSeverity::Major, "Use of eval() — injection risk", "Avoid eval." },
            { "js-var",         std::regex(R"(^\s*var\s+[A-Za-z_$])"),
              ReviewSeverity::Info, "Use `let` or `const` instead of `var`", "Prefer const/let." },
            { "js-console-log", std::regex(R"(\bconsole\.(log|debug)\s*\()"),
              ReviewSeverity::Info, "console.log left in code", "Remove or gate with a logger." },
            { "js-eq",          std::regex(R"([^=!]==[^=])"),
              ReviewSeverity::Minor, "Loose equality (==)", "Use ===." },
        });

        table["cpp"] = cpp; table["c++"] = cpp; table["c"] = cpp; table["cxx"] = cpp; table["cc"] = cpp;
        table["py"] = py; table["python"] = py;
        table["js"] = js; table["ts"] = js; table["jsx"] = js; table["tsx"] = js;
        table[""] = common;
    });
    auto it = table.find(lang);
    return it != table.end() ? it->second : table[""];
}

std::string lower(std::string_view v) {
    std::string s(v);
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}

} // namespace

void CodeReviewAgent::set_external_reviewer(ExternalReviewerFn fn) {
    std::lock_guard lk(mu_);
    external_ = std::move(fn);
}

std::vector<ReviewFinding> CodeReviewAgent::heuristics(const ReviewFileInput& f) {
    const auto& rules = rules_for(lower(f.language));
    std::vector<ReviewFinding> findings;

    std::uint32_t lineno = 0;
    std::size_t start = 0;
    while (start <= f.content.size()) {
        ++lineno;
        std::size_t nl = f.content.find('\n', start);
        std::string_view line = (nl == std::string::npos)
                                    ? std::string_view(f.content).substr(start)
                                    : std::string_view(f.content).substr(start, nl - start);
        for (const auto& r : rules) {
            if (std::regex_search(line.begin(), line.end(), r.rx)) {
                ReviewFinding fd;
                fd.rule_id = r.id;
                fd.severity = r.sev;
                fd.file = f.path;
                fd.line = lineno;
                fd.message = r.message;
                fd.suggestion = r.suggestion;
                findings.push_back(std::move(fd));
            }
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    // Whole-file: too-many-lines
    if (lineno > 1500) {
        findings.push_back({ "file-too-long", ReviewSeverity::Info, f.path, 1,
                             "File exceeds 1500 lines", "Consider splitting into smaller modules." });
    }
    return findings;
}

std::string CodeReviewAgent::render_summary(const std::vector<ReviewFinding>& findings) {
    std::unordered_map<std::string, std::size_t> by_severity;
    for (const auto& f : findings) by_severity[sev_name(f.severity)]++;
    std::ostringstream os;
    os << "### Code review\n";
    os << "- Total findings: " << findings.size() << "\n";
    for (auto sev : { "critical", "major", "minor", "info" })
        os << "- " << sev << ": " << by_severity[sev] << "\n";
    if (findings.empty()) { os << "\nNo issues detected by heuristics.\n"; return os.str(); }
    os << "\n#### Top findings\n";
    std::size_t shown = 0;
    // Sort by severity (worst first), stable.
    auto copy = findings;
    std::stable_sort(copy.begin(), copy.end(),
                     [](const ReviewFinding& a, const ReviewFinding& b) {
                         return static_cast<int>(a.severity) > static_cast<int>(b.severity);
                     });
    for (const auto& f : copy) {
        if (shown++ >= 20) break;
        os << "- **[" << sev_name(f.severity) << "]** `" << f.file << ":" << f.line
           << "` (`" << f.rule_id << "`) — " << f.message;
        if (!f.suggestion.empty()) os << " _Hint: " << f.suggestion << "_";
        os << "\n";
    }
    return os.str();
}

ReviewReport CodeReviewAgent::review(const std::vector<ReviewFileInput>& files) const {
    ExternalReviewerFn ext;
    { std::lock_guard lk(mu_); ext = external_; }

    ReviewReport report;
    for (const auto& f : files) {
        auto h = heuristics(f);
        report.findings.insert(report.findings.end(), h.begin(), h.end());
        if (ext) {
            try {
                auto extra = ext(f);
                report.findings.insert(report.findings.end(), extra.begin(), extra.end());
            } catch (...) {
                // swallow; external reviewer is best-effort
            }
        }
    }
    report.summary_markdown = render_summary(report.findings);
    return report;
}

} // namespace rawrxd::parity
