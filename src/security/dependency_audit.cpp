// dependency_audit.cpp — In-house SCA: manifest parsing, no external deps
#include "dependency_audit.hpp"
#include "core/problems_aggregator.hpp"
#include "core/rawrxd_json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>

namespace RawrXD {
namespace Security {

namespace {

std::string readFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

bool hasSuffix(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

bool DependencyAudit::auditFile(const std::string& path, std::vector<DependencyEntry>& out) {
    std::string content = readFileToString(path);
    if (content.empty()) return false;

    if (hasSuffix(path, "package.json")) return auditPackageJson(path, content, out);
    if (hasSuffix(path, "requirements.txt") || hasSuffix(path, "requirements-dev.txt"))
        return auditRequirementsTxt(path, content, out);
    if (path.find("CMakeLists.txt") != std::string::npos) return auditCMakeLists(path, content, out);
    if (hasSuffix(path, "Cargo.toml")) return auditCargoToml(path, content, out);
    if (hasSuffix(path, "vcpkg.json")) return auditVcpkgJson(path, content, out);

    return false;
}

bool DependencyAudit::auditPackageJson(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out) {
    try {
        RawrXD::JsonValue root = RawrXD::JsonValue::parse(content);
        if (!root.is_object()) return false;

        RawrXD::JsonValue deps = root.value("dependencies", RawrXD::JsonValue());
        if (deps.is_object()) {
            for (const auto& kv : deps.get_object()) {
                DependencyEntry e;
                e.name = kv.first;
                e.version = kv.second.is_string() ? kv.second.get_string() : "";
                e.manifestPath = path;
                e.ecosystem = "npm";
                out.push_back(e);
            }
        }
        RawrXD::JsonValue devDeps = root.value("devDependencies", RawrXD::JsonValue());
        if (devDeps.is_object()) {
            for (const auto& kv : devDeps.get_object()) {
                DependencyEntry e;
                e.name = kv.first;
                e.version = kv.second.is_string() ? kv.second.get_string() : "";
                e.manifestPath = path;
                e.ecosystem = "npm";
                out.push_back(e);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool DependencyAudit::auditRequirementsTxt(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out) {
    int lineNo = 0;
    std::istringstream is(content);
    std::string line;
    // Match: name==version, name>=x, name, name[extras]
    std::regex re(R"(([a-zA-Z0-9_-]+)\s*([\[=<>!].*)?)");
    while (std::getline(is, line)) {
        lineNo++;
        size_t c = line.find('#');
        if (c != std::string::npos) line = line.substr(0, c);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        std::smatch m;
        if (std::regex_match(line, m, re) && m.size() >= 1) {
            std::string name = m[1].str();
            if (name.empty() || name[0] == '-') continue; // -r file.txt etc.
            DependencyEntry e;
            e.name = name;
            e.version = m.size() >= 2 ? m[2].str() : "";
            e.manifestPath = path;
            e.line = lineNo;
            e.ecosystem = "pip";
            out.push_back(e);
        }
    }
    return true;
}

bool DependencyAudit::auditCMakeLists(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out) {
    int lineNo = 0;
    std::istringstream is(content);
    std::string line;
    std::regex findPackage(R"(find_package\s*\(\s*(\w+))");
    std::regex addSubdir(R"(add_subdirectory\s*\(\s*([^)\s]+))");
    while (std::getline(is, line)) {
        lineNo++;
        std::smatch m;
        if (std::regex_search(line, m, findPackage) && m.size() >= 1) {
            DependencyEntry e;
            e.name = m[1].str();
            e.manifestPath = path;
            e.line = lineNo;
            e.ecosystem = "cmake";
            out.push_back(e);
        }
        if (std::regex_search(line, m, addSubdir) && m.size() >= 1) {
            std::string sub = m[1].str();
            if (sub != "3rdparty" && sub != "external") {
                DependencyEntry e;
                e.name = sub;
                e.manifestPath = path;
                e.line = lineNo;
                e.ecosystem = "cmake_subdir";
                out.push_back(e);
            }
        }
    }
    return true;
}

bool DependencyAudit::auditCargoToml(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out) {
    int lineNo = 0;
    bool inDeps = false;
    std::istringstream is(content);
    std::string line;
    std::regex re(R"((\w+)\s*=\s*["']([^"']+)["'])");
    while (std::getline(is, line)) {
        lineNo++;
        if (line.find("[dependencies]") != std::string::npos) {
            inDeps = true;
            continue;
        }
        if (inDeps && !line.empty() && line[0] == '[') inDeps = false;
        if (!inDeps) continue;
        std::smatch m;
        if (std::regex_search(line, m, re) && m.size() >= 2) {
            DependencyEntry e;
            e.name = m[1].str();
            e.version = m[2].str();
            e.manifestPath = path;
            e.line = lineNo;
            e.ecosystem = "cargo";
            out.push_back(e);
        }
    }
    return true;
}

bool DependencyAudit::auditVcpkgJson(const std::string& path, const std::string& content, std::vector<DependencyEntry>& out) {
    try {
        RawrXD::JsonValue root = RawrXD::JsonValue::parse(content);
        if (!root.is_object()) return false;
        RawrXD::JsonValue depsVal = root.value("dependencies", RawrXD::JsonValue());
        if (!depsVal.is_array()) return true;
        const RawrXD::JsonArray& deps = depsVal.get_array();
        for (size_t i = 0; i < deps.size(); i++) {
            const RawrXD::JsonValue& el = deps[i];
            DependencyEntry e;
            e.manifestPath = path;
            e.ecosystem = "vcpkg";
            if (el.is_string()) {
                e.name = el.get_string();
                e.version = "latest";
            } else if (el.is_object()) {
                RawrXD::JsonValue n = el.value("name", RawrXD::JsonValue());
                if (n.is_string()) e.name = n.get_string();
                RawrXD::JsonValue v = el.value("version>=", RawrXD::JsonValue());
                if (v.is_string()) e.version = v.get_string();
            }
            if (!e.name.empty()) out.push_back(e);
        }
        return true;
    } catch (...) {
        return false;
    }
}

size_t DependencyAudit::auditDirectory(const std::string& dirPath, std::vector<DependencyEntry>& out) {
    size_t before = out.size();
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    std::string base = dirPath;
    if (!base.empty() && base.back() != sep) base += sep;
    auditFile(base + "package.json", out);
    auditFile(base + "requirements.txt", out);
    auditFile(base + "requirements-dev.txt", out);
    auditFile(base + "Cargo.toml", out);
    auditFile(base + "vcpkg.json", out);
    auditFile(base + "CMakeLists.txt", out);
    return out.size() - before;
}

void DependencyAudit::reportToProblems(const std::vector<DependencyEntry>& entries) {
    RawrXD::ProblemsAggregator& agg = RawrXD::ProblemsAggregator::instance();
    for (const auto& e : entries) {
        std::string msg = "SCA: " + e.ecosystem + " dependency " + e.name;
        if (!e.version.empty()) msg += " " + e.version;
        agg.add("SCA", e.manifestPath, e.line, 1, 3, "SCA001", msg, e.name);
    }
}

} // namespace Security
} // namespace RawrXD

// ---------------------------------------------------------------------------
// C API for IDE / CLI (P0)
// ---------------------------------------------------------------------------
extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void DependencyAudit_Run(const char* projectPath) {
    if (!projectPath) return;
    RawrXD::ProblemsAggregator::instance().clear("SCA");
    RawrXD::Security::DependencyAudit auditor;
    std::vector<RawrXD::Security::DependencyEntry> entries;
    auditor.auditDirectory(projectPath, entries);
    auditor.reportToProblems(entries);
}

} // extern "C"
