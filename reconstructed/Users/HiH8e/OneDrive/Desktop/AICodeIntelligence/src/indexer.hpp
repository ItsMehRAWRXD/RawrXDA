#pragma once

#include "analyzer.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "findings.hpp"
struct FileAnalysis {
    AnalysisResult result;
};

struct IndexOptions {
    std::vector<std::string> exts;      // file extensions to include; empty = all
    std::vector<std::string> include;   // wildcard patterns to include (match path); empty = all
    std::vector<std::string> exclude;   // wildcard patterns to exclude (match path)
    int threads = 0;                    // 0 = auto
    bool useDefaultExcludes = true;     // exclude common build/cache/vendor dirs by default
};

class Index {
public:
    void add(const AnalysisResult& ar);
    void add_findings(const std::vector<Finding>& f);

    const std::map<std::string, std::vector<Symbol>>& symbols_by_name() const { return m_symbolsByName; }
    const std::vector<Reference>& references() const { return m_references; }
    const std::map<std::string, FileMetrics>& metrics_by_file() const { return m_metricsByFile; }
    const std::vector<Finding>& findings() const { return m_findings; }

    std::vector<Symbol> find_symbols(const std::string& name) const;
    std::vector<Reference> find_references(const std::string& name) const;

    void to_json(std::string& out) const;
    void to_ndjson(std::string& out) const;
    void to_sarif(std::string& out) const;

private:
    std::map<std::string, std::vector<Symbol>> m_symbolsByName;
    std::vector<Reference> m_references;
    std::map<std::string, FileMetrics> m_metricsByFile;
    std::vector<Finding> m_findings;
};

class Indexer {
public:
    // root: directory to index with options
    Index index_root(const std::string& root, const IndexOptions& opts) const;

private:
    std::string detect_language(const std::string& path) const; // "clike", "python", "unknown"
    bool path_allowed(const std::string& path, const IndexOptions& opts) const;
};
