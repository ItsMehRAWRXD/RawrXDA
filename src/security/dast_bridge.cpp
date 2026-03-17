#include "dast_bridge.hpp"
#include "core/problems_aggregator.hpp"
#include "core/rawrxd_json.hpp"

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <cctype>

namespace RawrXD {
namespace Security {
namespace {

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

int severityFromText(const std::string& s) {
    std::string lower = s;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "error" || lower == "high" || lower == "critical") return 1;
    if (lower == "warning" || lower == "medium") return 2;
    if (lower == "note" || lower == "low" || lower == "info" || lower == "informational") return 3;
    return 2;
}

std::string getStringField(const JsonValue& obj, const char* key) {
    JsonValue v = obj.value(key, JsonValue());
    return v.is_string() ? v.get_string() : "";
}

void parseSarifResult(const JsonValue& result, std::vector<DastFinding>& out) {
    if (!result.is_object()) return;
    DastFinding f;
    f.source = "DAST";
    f.ruleId = getStringField(result, "ruleId");
    f.code = f.ruleId.empty() ? "DAST-SARIF" : f.ruleId;
    f.severity = severityFromText(getStringField(result, "level"));
    JsonValue msg = result.value("message", JsonValue());
    if (msg.is_object()) {
        f.message = getStringField(msg, "text");
    }
    if (f.message.empty()) f.message = "Imported DAST finding";

    JsonValue locs = result.value("locations", JsonValue());
    if (locs.is_array() && !locs.get_array().empty()) {
        const JsonValue& loc = locs.get_array()[0];
        JsonValue phys = loc.value("physicalLocation", JsonValue());
        if (phys.is_object()) {
            JsonValue art = phys.value("artifactLocation", JsonValue());
            if (art.is_object()) f.path = getStringField(art, "uri");
            JsonValue region = phys.value("region", JsonValue());
            if (region.is_object()) {
                JsonValue sl = region.value("startLine", JsonValue());
                JsonValue sc = region.value("startColumn", JsonValue());
                if (sl.is_number()) f.line = static_cast<int>(sl.get_number());
                if (sc.is_number()) f.column = static_cast<int>(sc.get_number());
            }
        }
    }
    out.push_back(std::move(f));
}

void parseSarif(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue runs = root.value("runs", JsonValue());
    if (!runs.is_array()) return;
    for (const auto& run : runs.get_array()) {
        JsonValue results = run.value("results", JsonValue());
        if (!results.is_array()) continue;
        for (const auto& r : results.get_array()) {
            parseSarifResult(r, out);
        }
    }
}

void parseZap(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue sites = root.value("site", JsonValue());
    if (!sites.is_array()) return;
    for (const auto& site : sites.get_array()) {
        JsonValue alerts = site.value("alerts", JsonValue());
        if (!alerts.is_array()) continue;
        for (const auto& alert : alerts.get_array()) {
            if (!alert.is_object()) continue;
            const std::string alertName = getStringField(alert, "alert");
            const std::string riskCode = getStringField(alert, "riskcode");
            JsonValue instances = alert.value("instances", JsonValue());
            if (instances.is_array() && !instances.get_array().empty()) {
                for (const auto& inst : instances.get_array()) {
                    DastFinding f;
                    f.source = "DAST";
                    f.code = "ZAP-" + (riskCode.empty() ? std::string("0") : riskCode);
                    f.ruleId = getStringField(alert, "pluginid");
                    f.message = alertName.empty() ? "ZAP imported finding" : alertName;
                    f.path = getStringField(inst, "uri");
                    f.severity = severityFromText(
                        riskCode == "3" ? "high" : (riskCode == "2" ? "medium" : (riskCode == "1" ? "low" : "warning")));
                    out.push_back(std::move(f));
                }
            } else {
                DastFinding f;
                f.source = "DAST";
                f.code = "ZAP-" + (riskCode.empty() ? std::string("0") : riskCode);
                f.ruleId = getStringField(alert, "pluginid");
                f.message = alertName.empty() ? "ZAP imported finding" : alertName;
                f.severity = severityFromText(
                    riskCode == "3" ? "high" : (riskCode == "2" ? "medium" : (riskCode == "1" ? "low" : "warning")));
                out.push_back(std::move(f));
            }
        }
    }
}

void parseGenericFindings(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue findings = root.value("findings", JsonValue());
    if (!findings.is_array()) return;
    for (const auto& entry : findings.get_array()) {
        if (!entry.is_object()) continue;
        DastFinding f;
        f.source = "DAST";
        f.path = getStringField(entry, "path");
        if (f.path.empty()) f.path = getStringField(entry, "url");
        f.code = getStringField(entry, "code");
        if (f.code.empty()) f.code = "DAST-GENERIC";
        f.ruleId = getStringField(entry, "ruleId");
        f.message = getStringField(entry, "message");
        if (f.message.empty()) f.message = getStringField(entry, "title");
        if (f.message.empty()) f.message = "Imported DAST finding";
        f.severity = severityFromText(getStringField(entry, "severity"));
        JsonValue line = entry.value("line", JsonValue());
        JsonValue col = entry.value("column", JsonValue());
        if (line.is_number()) f.line = static_cast<int>(line.get_number());
        if (col.is_number()) f.column = static_cast<int>(col.get_number());
        out.push_back(std::move(f));
    }
}

void parseBurpIssues(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue issues = root.value("issues", JsonValue());
    if (!issues.is_array()) return;
    for (const auto& issue : issues.get_array()) {
        if (!issue.is_object()) continue;
        DastFinding f;
        f.source = "DAST";
        f.message = getStringField(issue, "name");
        if (f.message.empty()) f.message = getStringField(issue, "title");
        if (f.message.empty()) f.message = "Burp imported issue";
        f.severity = severityFromText(getStringField(issue, "severity"));
        f.code = "BURP-" + getStringField(issue, "type_index");
        if (f.code == "BURP-") f.code = "BURP";
        f.path = getStringField(issue, "path");
        if (f.path.empty()) f.path = getStringField(issue, "url");
        f.ruleId = getStringField(issue, "confidence");
        JsonValue line = issue.value("line", JsonValue());
        if (line.is_number()) f.line = static_cast<int>(line.get_number());
        out.push_back(std::move(f));
    }
}

void parseVulnerabilities(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue vulns = root.value("vulnerabilities", JsonValue());
    if (!vulns.is_array()) return;
    for (const auto& v : vulns.get_array()) {
        if (!v.is_object()) continue;
        DastFinding f;
        f.source = "DAST";
        f.code = getStringField(v, "id");
        if (f.code.empty()) f.code = "DAST-VULN";
        f.ruleId = getStringField(v, "type");
        f.message = getStringField(v, "name");
        if (f.message.empty()) f.message = getStringField(v, "description");
        if (f.message.empty()) f.message = "Imported DAST vulnerability";
        f.path = getStringField(v, "file");
        if (f.path.empty()) f.path = getStringField(v, "url");
        f.severity = severityFromText(getStringField(v, "severity"));
        JsonValue line = v.value("line", JsonValue());
        JsonValue col = v.value("column", JsonValue());
        if (line.is_number()) f.line = static_cast<int>(line.get_number());
        if (col.is_number()) f.column = static_cast<int>(col.get_number());
        out.push_back(std::move(f));
    }
}

void parseAlertArray(const JsonValue& root, std::vector<DastFinding>& out) {
    JsonValue alerts = root.value("alerts", JsonValue());
    if (!alerts.is_array()) return;
    for (const auto& a : alerts.get_array()) {
        if (!a.is_object()) continue;
        DastFinding f;
        f.source = "DAST";
        f.message = getStringField(a, "title");
        if (f.message.empty()) f.message = getStringField(a, "message");
        if (f.message.empty()) f.message = "Imported DAST alert";
        f.code = getStringField(a, "id");
        if (f.code.empty()) f.code = "DAST-ALERT";
        f.ruleId = getStringField(a, "rule");
        f.path = getStringField(a, "path");
        if (f.path.empty()) f.path = getStringField(a, "url");
        f.severity = severityFromText(getStringField(a, "severity"));
        JsonValue line = a.value("line", JsonValue());
        if (line.is_number()) f.line = static_cast<int>(line.get_number());
        out.push_back(std::move(f));
    }
}

void parseSingleFindingObject(const JsonValue& entry, std::vector<DastFinding>& out) {
    if (!entry.is_object()) return;
    const std::string msg = getStringField(entry, "message");
    const std::string title = getStringField(entry, "title");
    const std::string name = getStringField(entry, "name");
    const std::string code = getStringField(entry, "code");
    const std::string id = getStringField(entry, "id");
    const std::string path = getStringField(entry, "path");
    const std::string url = getStringField(entry, "url");
    const std::string uri = getStringField(entry, "uri");
    const std::string sev = getStringField(entry, "severity");
    if (msg.empty() && title.empty() && name.empty() && code.empty() && id.empty() &&
        path.empty() && url.empty() && uri.empty() && sev.empty()) {
        return;
    }

    DastFinding f;
    f.source = "DAST";
    f.path = path;
    if (f.path.empty()) f.path = url;
    if (f.path.empty()) f.path = uri;
    f.code = code;
    if (f.code.empty()) f.code = id;
    if (f.code.empty()) f.code = "DAST-GENERIC";
    f.ruleId = getStringField(entry, "ruleId");
    if (f.ruleId.empty()) f.ruleId = getStringField(entry, "rule");
    f.message = msg;
    if (f.message.empty()) f.message = title;
    if (f.message.empty()) f.message = name;
    if (f.message.empty()) f.message = "Imported DAST finding";
    f.severity = severityFromText(sev);
    JsonValue line = entry.value("line", JsonValue());
    JsonValue col = entry.value("column", JsonValue());
    if (line.is_number()) f.line = static_cast<int>(line.get_number());
    if (col.is_number()) f.column = static_cast<int>(col.get_number());
    out.push_back(std::move(f));
}

bool parseKnownShape(const JsonValue& root, std::vector<DastFinding>& out) {
    if (!root.is_object()) return false;
    const size_t before = out.size();
    if (root.value("runs", JsonValue()).is_array()) parseSarif(root, out);
    if (root.value("site", JsonValue()).is_array()) parseZap(root, out);
    if (root.value("findings", JsonValue()).is_array()) parseGenericFindings(root, out);
    if (root.value("issues", JsonValue()).is_array()) parseBurpIssues(root, out);
    if (root.value("vulnerabilities", JsonValue()).is_array()) parseVulnerabilities(root, out);
    if (root.value("alerts", JsonValue()).is_array()) parseAlertArray(root, out);

    // Single-object fallback for NDJSON entries and ad-hoc reports.
    if (out.size() == before) {
        parseSingleFindingObject(root, out);
    }
    return out.size() > before;
}

std::string normalize(std::string v) {
    for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return v;
}

void deduplicateFindings(std::vector<DastFinding>& findings) {
    std::unordered_set<std::string> seen;
    std::vector<DastFinding> unique;
    unique.reserve(findings.size());
    for (auto& f : findings) {
        const std::string key = normalize(f.source) + "|" + normalize(f.path) + "|" +
                                std::to_string(f.line) + "|" + std::to_string(f.column) + "|" +
                                std::to_string(f.severity) + "|" + normalize(f.code) + "|" +
                                normalize(f.ruleId) + "|" + normalize(f.message);
        if (seen.insert(key).second) unique.push_back(std::move(f));
    }
    findings = std::move(unique);
}

} // namespace

bool DastBridge::importReport(const std::string& reportPath, std::vector<DastFinding>& out) const {
    const std::string jsonText = readAll(reportPath);
    if (jsonText.empty()) return false;
    try {
        JsonValue root = JsonValue::parse(jsonText);
        if (parseKnownShape(root, out)) {
            deduplicateFindings(out);
            return !out.empty();
        }
    } catch (...) {
        // Fall through to NDJSON fallback below.
    }

    // NDJSON fallback: one JSON object per line.
    std::istringstream lines(jsonText);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        bool nonSpace = false;
        for (char c : line) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                nonSpace = true;
                break;
            }
        }
        if (!nonSpace) continue;

        try {
            JsonValue root = JsonValue::parse(line);
            parseKnownShape(root, out);
        } catch (...) {
            // Ignore malformed line and continue importing others.
        }
    }

    deduplicateFindings(out);
    return !out.empty();
}

void DastBridge::reportToProblems(const std::vector<DastFinding>& findings) const {
    auto& agg = RawrXD::ProblemsAggregator::instance();
    std::vector<DastFinding> dedup = findings;
    deduplicateFindings(dedup);
    for (const auto& f : dedup) {
        agg.add("DAST", f.path, f.line, f.column, f.severity, f.code, f.message, f.ruleId);
    }
}

} // namespace Security
} // namespace RawrXD

extern "C" {
void DastBridge_ImportReport(const char* reportPath) {
    if (!reportPath) return;
    RawrXD::ProblemsAggregator::instance().clear("DAST");
    RawrXD::Security::DastBridge bridge;
    std::vector<RawrXD::Security::DastFinding> findings;
    if (!bridge.importReport(reportPath, findings)) return;
    bridge.reportToProblems(findings);
}
}
