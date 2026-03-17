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

void AICIIndex::add(const AICIAnalysisResult& ar) {
    for (const auto& s : ar.symbols) { m_symbolsByName[s.name].push_back(s); m_metricsByFile[s.file] = ar.metrics; }
    for (const auto& r : ar.references) { m_references.push_back(r); if (!m_metricsByFile.count(r.file)) m_metricsByFile[r.file] = ar.metrics; }
}

void AICIIndex::add_findings(const std::vector<AICIFinding>& f) { for (const auto& x : f) m_findings.push_back(x); }

std::vector<AICISymbol> AICIIndex::find_symbols(const string& name) const { auto it = m_symbolsByName.find(name); if (it == m_symbolsByName.end()) return {}; return it->second; }
std::vector<AICIReference> AICIIndex::find_references(const string& name) const { std::vector<AICIReference> out; for (const auto& r : m_references) if (r.symbol == name) out.push_back(r); return out; }

void AICIIndex::to_json(string& out) const {
    out.clear(); out += "{\n  \"symbols\": [\n"; bool first = true;
    for (const auto& kv : m_symbolsByName) for (const auto& s : kv.second) { if (!first) out += ",\n"; first = false; out += "    {\"name\": \"" + aici_escape_json(s.name) + "\", \"kind\": \"" + aici_escape_json(s.kind) + "\", \"file\": \"" + aici_escape_json(s.file) + "\", \"line\": " + std::to_string(s.line) + "}"; }
    out += "\n  ],\n  \"references\": [\n"; first = true;
    for (const auto& r : m_references) { if (!first) out += ",\n"; first = false; out += "    {\"symbol\": \"" + aici_escape_json(r.symbol) + "\", \"file\": \"" + aici_escape_json(r.file) + "\", \"line\": " + std::to_string(r.line) + "}"; }
    out += "\n  ],\n  \"metrics\": {\n"; first = true;
    for (const auto& mkv : m_metricsByFile) { if (!first) out += ",\n"; first = false; const auto& fm = mkv.second; out += "    \"" + aici_escape_json(mkv.first) + "\": {\"lines\": " + std::to_string(fm.lines) + ", \"codeLines\": " + std::to_string(fm.codeLines) + ", \"commentLines\": " + std::to_string(fm.commentLines) + ", \"complexity\": " + std::to_string(fm.complexity) + "}"; }
    out += "\n  },\n  \"findings\": [\n"; bool first2 = true;
    for (const auto& f : m_findings) { if (!first2) out += ",\n"; first2 = false; out += "    {\"id\": \"" + aici_escape_json(f.id) + "\", \"title\": \"" + aici_escape_json(f.title) + "\", \"severity\": \"" + aici_escape_json(f.severity) + "\", \"file\": \"" + aici_escape_json(f.file) + "\", \"line\": " + std::to_string(f.line) + ", \"details\": \"" + aici_escape_json(f.details) + "\"}"; }
    out += "\n  ]\n}\n";
}

void AICIIndex::to_ndjson(string& out) const {
    out.clear();
    for (const auto& kv : m_symbolsByName) for (const auto& s : kv.second) out += "{\"type\":\"symbol\",\"name\":\"" + aici_escape_json(s.name) + "\",\"kind\":\"" + aici_escape_json(s.kind) + "\",\"file\":\"" + aici_escape_json(s.file) + "\",\"line\":" + std::to_string(s.line) + "}\n";
    for (const auto& r : m_references) out += "{\"type\":\"reference\",\"symbol\":\"" + aici_escape_json(r.symbol) + "\",\"file\":\"" + aici_escape_json(r.file) + "\",\"line\":" + std::to_string(r.line) + "}\n";
    for (const auto& f : m_findings) out += "{\"type\":\"finding\",\"id\":\"" + aici_escape_json(f.id) + "\",\"title\":\"" + aici_escape_json(f.title) + "\",\"severity\":\"" + aici_escape_json(f.severity) + "\",\"file\":\"" + aici_escape_json(f.file) + "\",\"line\":" + std::to_string(f.line) + ",\"details\":\"" + aici_escape_json(f.details) + "\"}\n";
}

void AICIIndex::to_sarif(string& out) const {
    out.clear(); out += "{\n  \"version\": \"2.1.0\",\n  \"runs\": [\n    {\n      \"tool\": { \"driver\": { \"name\": \"AICodeIntelligence\", \"version\": \"0.1\" } },\n      \"results\": [\n"; bool first = true;
    for (const auto& f : m_findings) { if (!first) out += ",\n"; first = false; std::string level = "note"; auto sev = aici_to_lower(f.severity); if (sev == "high") level = "error"; else if (sev == "medium") level = "warning"; out += "        {\n          \"ruleId\": \"" + aici_escape_json(f.id) + "\",\n          \"level\": \"" + level + "\",\n          \"message\": { \"text\": \"" + aici_escape_json(f.title + ": " + f.details) + "\" },\n          \"locations\": [ { \"physicalLocation\": { \"artifactLocation\": { \"uri\": \"" + aici_escape_json(f.file) + "\" }, \"region\": { \"startLine\": " + std::to_string(f.line) + " } } } ]\n        }"; }
    out += "\n      ]\n    }\n  ]\n}\n";
}

AICIIndex AICIIndexer::index_root(const string& root, const AICIIndexOptions& opts) const {
    AICIIndex idx; AICICLikeAnalyzer clike; AICIPythonAnalyzer pyan; AICISecurityAnalyzer sec;
    std::vector<string> files;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied); it != fs::recursive_directory_iterator(); ++it) {
        const auto& p = *it; if (!p.is_regular_file()) continue; const auto path = p.path().string(); if (!path_allowed(path, opts)) continue; files.push_back(path);
    }
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency()); const unsigned nth = static_cast<unsigned>(opts.threads > 0 ? opts.threads : hw);
    std::atomic<size_t> index{0}; std::vector<std::vector<AICIAnalysisResult>> buckets(nth); std::vector<std::thread> workers; workers.reserve(nth);
    for (unsigned t = 0; t < nth; ++t) { workers.emplace_back([&, t](){ std::string content; buckets[t].reserve(64); while (true) { size_t i = index.fetch_add(1, std::memory_order_relaxed); if (i >= files.size()) break; const auto& path = files[i]; content.clear(); if (!aici_read_file_text(path, content)) continue; const auto lang = detect_language(path); if (lang == "python") buckets[t].push_back(pyan.analyze(path, content)); else if (lang == "clike") buckets[t].push_back(clike.analyze(path, content)); }}); }
    for (auto& th : workers) th.join();
    for (const auto& b : buckets) for (const auto& ar : b) idx.add(ar);
    for (const auto& path : files) { std::string content; if (!aici_read_file_text(path, content)) continue; const auto lang = detect_language(path); auto f = sec.analyze(lang, path, content); idx.add_findings(f); }
    return idx;
}

std::string AICIIndexer::detect_language(const string& path) const {
    auto pl = aici_to_lower(path);
    if (aici_ends_with(pl, ".c") || aici_ends_with(pl, ".h") || aici_ends_with(pl, ".cpp") || aici_ends_with(pl, ".hpp") || aici_ends_with(pl, ".cc") || aici_ends_with(pl, ".hh") || aici_ends_with(pl, ".cxx") || aici_ends_with(pl, ".hxx") || aici_ends_with(pl, ".js") || aici_ends_with(pl, ".ts")) return "clike";
    if (aici_ends_with(pl, ".py")) return "python";
    return "unknown";
}

bool AICIIndexer::path_allowed(const std::string& path, const AICIIndexOptions& opts) const {
    if (!opts.exts.empty()) { bool ok = false; for (const auto& e : opts.exts) if (aici_ends_with(aici_to_lower(path), aici_to_lower(e))) { ok = true; break; } if (!ok) return false; }
    if (!opts.include.empty()) { bool any = false; for (const auto& pat : opts.include) if (aici_wildcard_match(pat, path)) { any = true; break; } if (!any) return false; }
    for (const auto& pat : opts.exclude) if (aici_wildcard_match(pat, path)) return false;
    if (opts.useDefaultExcludes) { static const char* d[] = {"*\\.git\\*","*\\.hg\\*","*\\.svn\\*","*\\.idea\\*","*\\.vscode\\*","*\\.cache\\*","*\\node_modules\\*","*\\dist\\*","*\\build\\*","*\\out\\*","*\\bin\\*","*\\obj\\*","*\\Debug\\*","*\\Release\\*","*\\target\\*","*\\.venv\\*","*\\venv\\*","*\\__pycache__\\*"}; for (auto* pat : d) if (aici_wildcard_match(pat, path)) return false; }
    return true;
}
