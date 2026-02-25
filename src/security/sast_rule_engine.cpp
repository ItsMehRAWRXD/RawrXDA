// sast_rule_engine.cpp — In-house SAST: rules over source, no external deps
#include "sast_rule_engine.hpp"
#include "core/problems_aggregator.hpp"
#include <regex>
#include <cstring>
#include <algorithm>

namespace RawrXD {
namespace Security {

namespace {

struct Rule {
    std::string id;
    std::string pattern;   // regex
    std::string message;
    int         severity = 2;
    bool        caseInsensitive = false;
};

std::vector<Rule> buildDefaultRules() {
    return {
        { "SAST001", R"(gets\s*\()", "Use of gets() is unsafe; use fgets or secure alternative.", 1, true },
        { "SAST002", R"(sprintf\s*\()", "sprintf can overflow; use snprintf.", 2, true },
        { "SAST003", R"(strcpy\s*\()", "strcpy can overflow; use strncpy or strlcpy.", 2, true },
        { "SAST004", R"(strcat\s*\()", "strcat can overflow; use strncat or safe concat.", 2, true },
        { "SAST005", R"(\bscanf\s*\([^)]*[^n])", "Unbounded scanf can overflow; use width or fgets.", 2, true },
        { "SAST006", R"(system\s*\()", "system() can lead to injection; prefer exec APIs.", 2, true },
        { "SAST007", R"(eval\s*\()", "eval() can execute arbitrary code; avoid in production.", 1, true },
        { "SAST008", R"(innerHTML\s*=)", "innerHTML assignment can cause XSS; sanitize or use textContent.", 2, true },
        { "SAST009", R"(document\.write\s*\()", "document.write can introduce XSS.", 2, true },
        { "SAST010", R"(\bexec\s*\(|execfile\s*\()", "exec/execfile can run arbitrary code.", 2, true },
        { "SAST011", R"(password\s*=\s*['\"][^'\"]+['\"])", "Possible hardcoded password.", 2, true },
        { "SAST012", R"(SELECT\s+.*\s+FROM\s+.*\s+WHERE\s+.*\+)", "String concat in SQL may be injection; use parameterized queries.", 2, true },
    };
}

void lineColumnFromOffset(const char* base, size_t offset, size_t totalSize, int& line, int& column) {
    line = 1;
    column = 1;
    for (size_t i = 0; i < offset && i < totalSize; i++) {
        if (base[i] == '\n') { line++; column = 1; }
        else column++;
    }
}

std::string getSnippet(const std::string& content, size_t pos, size_t len, size_t context) {
    size_t start = (pos >= context) ? pos - context : 0;
    size_t end = std::min(pos + len + context, content.size());
    std::string s = content.substr(start, end - start);
    if (s.size() > 80) s = s.substr(0, 77) + "...";
    return s;
}

} // namespace

size_t SastRuleEngine::scan(const std::string& path, const uint8_t* data, size_t size, std::vector<SastFinding>& out) {
    if (!data) return 0;
    std::string content(reinterpret_cast<const char*>(data), size);
    size_t before = out.size();
    runRules(path, content, out);
    return out.size() - before;
}

size_t SastRuleEngine::scan(const std::string& path, const std::string& content, std::vector<SastFinding>& out) {
    size_t before = out.size();
    runRules(path, content, out);
    return out.size() - before;
}

void SastRuleEngine::runRules(const std::string& path, const std::string& content, std::vector<SastFinding>& out) {
    std::vector<Rule> rules = buildDefaultRules();
    const char* base = content.data();
    const size_t totalSize = content.size();

    for (const Rule& rule : rules) {
        try {
            std::regex re(rule.pattern, rule.caseInsensitive ? std::regex::icase : std::regex_constants::ECMAScript);
            std::cregex_iterator it(base, base + totalSize, re);
            std::cregex_iterator end;
            for (; it != end; ++it) {
                SastFinding f;
                f.path = path;
                f.ruleId = rule.id;
                f.message = rule.message;
                f.severity = rule.severity;
                size_t pos = it->position(0);
                size_t len = it->length(0);
                lineColumnFromOffset(base, pos, totalSize, f.line, f.column);
                f.snippet = getSnippet(content, pos, len, 20);
                out.push_back(f);
            }
        } catch (...) {
            // regex error; skip rule
        }
    }
}

void SastRuleEngine::reportToProblems(const std::vector<SastFinding>& findings) {
    RawrXD::ProblemsAggregator& agg = RawrXD::ProblemsAggregator::instance();
    for (const auto& f : findings) {
        agg.add("SAST", f.path, f.line, f.column, f.severity, f.ruleId, f.message, f.ruleId);
    }
}

} // namespace Security
} // namespace RawrXD
