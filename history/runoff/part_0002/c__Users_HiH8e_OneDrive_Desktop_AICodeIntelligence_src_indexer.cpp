#include "indexer.hpp"
#include "utils.hpp"
#include "analyzers/clike_analyzer.hpp"
#include "analyzers/python_analyzer.hpp"
#include "security_analyzer.hpp"

#include <filesystem>
#include <thread>
#include <atomic>

using std::string;
namespace fs = std::filesystem;

void Index::add(const AnalysisResult& ar) {
    for (const auto& s : ar.symbols) {
        m_symbolsByName[s.name].push_back(s);
        m_metricsByFile[s.file] = ar.metrics;
    }
    for (const auto& r : ar.references) {
        m_references.push_back(r);
        if (!m_metricsByFile.count(r.file)) {
            m_metricsByFile[r.file] = ar.metrics;
        }
    }
}

void Index::add_findings(const std::vector<Finding>& f) {
    for (const auto& x : f) m_findings.push_back(x);
}

std::vector<Symbol> Index::find_symbols(const string& name) const {
    auto it = m_symbolsByName.find(name);
    if (it == m_symbolsByName.end()) return {};
    return it->second;
}

std::vector<Reference> Index::find_references(const string& name) const {
    std::vector<Reference> out;
    for (const auto& r : m_references) if (r.symbol == name) out.push_back(r);
    return out;
}

void Index::to_json(string& out) const {
    out.clear();
    out += "{\n";
    out += "  \"symbols\": [\n";
    bool first = true;
    for (const auto& kv : m_symbolsByName) {
        for (const auto& s : kv.second) {
            if (!first) out += ",\n"; first = false;
            out += "    {\"name\": \"" + escape_json(s.name) + "\", \"kind\": \"" + escape_json(s.kind) + "\", \"file\": \"" + escape_json(s.file) + "\", \"line\": " + std::to_string(s.line) + "}";
        }
    }
    out += "\n  ],\n";
    out += "  \"references\": [\n";
    first = true;
    for (const auto& r : m_references) {
        if (!first) out += ",\n"; first = false;
        out += "    {\"symbol\": \"" + escape_json(r.symbol) + "\", \"file\": \"" + escape_json(r.file) + "\", \"line\": " + std::to_string(r.line) + "}";
    }
    out += "\n  ],\n";
    out += "  \"metrics\": {\n";
    first = true;
    for (const auto& mkv : m_metricsByFile) {
        if (!first) out += ",\n"; first = false;
        const auto& fm = mkv.second;
        out += "    \"" + escape_json(mkv.first) + "\": {\"lines\": " + std::to_string(fm.lines) + ", \"codeLines\": " + std::to_string(fm.codeLines) + ", \"commentLines\": " + std::to_string(fm.commentLines) + ", \"complexity\": " + std::to_string(fm.complexity) + "}";
    }
    out += "\n  },\n";
    out += "  \"findings\": [\n";
    bool first2 = true;
    for (const auto& f : m_findings) {
        if (!first2) out += ",\n"; first2 = false;
        out += "    {\"id\": \"" + escape_json(f.id) + "\", \"title\": \"" + escape_json(f.title) + "\", \"severity\": \"" + escape_json(f.severity) + "\", \"file\": \"" + escape_json(f.file) + "\", \"line\": " + std::to_string(f.line) + ", \"details\": \"" + escape_json(f.details) + "\"}";
    }
    out += "\n  ]\n";
    out += "}\n";
}

void Index::to_ndjson(string& out) const {
    out.clear();
    for (const auto& kv : m_symbolsByName) {
        for (const auto& s : kv.second) {
            out += "{\"type\":\"symbol\",\"name\":\"" + escape_json(s.name) + "\",\"kind\":\"" + escape_json(s.kind) + "\",\"file\":\"" + escape_json(s.file) + "\",\"line\":" + std::to_string(s.line) + "}\n";
        }
    }
    for (const auto& r : m_references) {
        out += "{\"type\":\"reference\",\"symbol\":\"" + escape_json(r.symbol) + "\",\"file\":\"" + escape_json(r.file) + "\",\"line\":" + std::to_string(r.line) + "}\n";
    }
    for (const auto& f : m_findings) {
        out += "{\"type\":\"finding\",\"id\":\"" + escape_json(f.id) + "\",\"title\":\"" + escape_json(f.title) + "\",\"severity\":\"" + escape_json(f.severity) + "\",\"file\":\"" + escape_json(f.file) + "\",\"line\":" + std::to_string(f.line) + ",\"details\":\"" + escape_json(f.details) + "\"}\n";
    }
}

void Index::to_sarif(string& out) const {
    out.clear();
    out += "{\n  \"version\": \"2.1.0\",\n  \"runs\": [\n    {\n      \"tool\": { \"driver\": { \"name\": \"AICodeIntelligence\", \"version\": \"0.1\" } },\n      \"results\": [\n";
    bool first = true;
    for (const auto& f : m_findings) {
        if (!first) out += ",\n"; first = false;
        std::string level = "note";
        auto sev = to_lower(f.severity);
        if (sev == "high") level = "error"; else if (sev == "medium") level = "warning";
        out += "        {\n          \"ruleId\": \"" + escape_json(f.id) + "\",\n          \"level\": \"" + level + "\",\n          \"message\": { \"text\": \"" + escape_json(f.title + ": " + f.details) + "\" },\n          \"locations\": [ { \"physicalLocation\": { \"artifactLocation\": { \"uri\": \"" + escape_json(f.file) + "\" }, \"region\": { \"startLine\": " + std::to_string(f.line) + " } } } ]\n        }";
    }
    out += "\n      ]\n    }\n  ]\n}\n";
}

Index Indexer::index_root(const string& root, const IndexOptions& opts) const {
    Index idx;
    CLikeAnalyzer clike;
    PythonAnalyzer pyan;
    SecurityAnalyzer sec;

    std::vector<string> files;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        const auto& p = *it;
        if (!p.is_regular_file()) continue;
        const auto path = p.path().string();
        if (!path_allowed(path, opts)) continue;
        files.push_back(path);
    }

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned nth = static_cast<unsigned>(opts.threads > 0 ? opts.threads : hw);
    std::atomic<size_t> index{0};
    std::vector<std::vector<AnalysisResult>> buckets(nth);
    std::vector<std::thread> workers;
    workers.reserve(nth);

    for (unsigned t = 0; t < nth; ++t) {
        workers.emplace_back([&, t](){
            std::string content;
            buckets[t].reserve(64);
            while (true) {
                size_t i = index.fetch_add(1, std::memory_order_relaxed);
                if (i >= files.size()) break;
                const auto& path = files[i];
                content.clear();
                if (!read_file_text(path, content)) continue;
                const auto lang = detect_language(path);
                if (lang == "python") buckets[t].push_back(pyan.analyze(path, content));
                else if (lang == "clike") buckets[t].push_back(clike.analyze(path, content));
            }
        });
    }
    for (auto& th : workers) th.join();

    for (const auto& b : buckets) for (const auto& ar : b) idx.add(ar);
    // Add security findings sequentially
    for (const auto& path : files) {
        std::string content; if (!read_file_text(path, content)) continue;
        const auto lang = detect_language(path);
        auto f = sec.analyze(lang, path, content);
        idx.add_findings(f);
    }
    return idx;
}

std::string Indexer::detect_language(const string& path) const {
    auto pl = to_lower(path);
    if (ends_with(pl, ".c") || ends_with(pl, ".h") || ends_with(pl, ".cpp") || ends_with(pl, ".hpp") || ends_with(pl, ".cc") || ends_with(pl, ".hh") || ends_with(pl, ".cxx") || ends_with(pl, ".hxx") || ends_with(pl, ".js") || ends_with(pl, ".ts")) {
        return "clike";
    }
    if (ends_with(pl, ".py")) return "python";
    return "unknown";
}

bool Indexer::path_allowed(const std::string& path, const IndexOptions& opts) const {
    // extension filter
    if (!opts.exts.empty()) {
        bool ok = false;
        for (const auto& e : opts.exts) if (ends_with(to_lower(path), to_lower(e))) { ok = true; break; }
        if (!ok) return false;
    }
    // include patterns
    if (!opts.include.empty()) {
        bool any = false;
        for (const auto& pat : opts.include) if (wildcard_match(pat, path)) { any = true; break; }
        if (!any) return false;
    }
    // exclude patterns
    for (const auto& pat : opts.exclude) if (wildcard_match(pat, path)) return false;
    if (opts.useDefaultExcludes) {
        static const char* defaults[] = {
            "*\\.git\\*", "*\\.hg\\*", "*\\.svn\\*",
            "*\\.idea\\*", "*\\.vscode\\*", "*\\.cache\\*",
            "*\\node_modules\\*", "*\\dist\\*", "*\\build\\*", "*\\out\\*",
            "*\\bin\\*", "*\\obj\\*", "*\\Debug\\*", "*\\Release\\*",
            "*\\target\\*", "*\\.venv\\*", "*\\venv\\*", "*\\__pycache__\\*"
        };
        for (auto* pat : defaults) if (wildcard_match(pat, path)) return false;
    }
    return true;
}
