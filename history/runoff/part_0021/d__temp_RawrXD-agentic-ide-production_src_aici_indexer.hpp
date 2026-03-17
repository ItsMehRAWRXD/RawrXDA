#pragma once

#include "analyzer.hpp"
#include "findings.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

struct AICIIndexOptions {
    std::vector<std::string> exts;
    std::vector<std::string> include;
    std::vector<std::string> exclude;
    int threads = 0;
    bool useDefaultExcludes = true;
};

class AICIIndex {
public:
    void add(const AICIAnalysisResult& ar);
    void add_findings(const std::vector<AICIFinding>& f);

    const std::map<std::string, std::vector<AICISymbol>>& symbols_by_name() const { return m_symbolsByName; }
    const std::vector<AICIReference>& references() const { return m_references; }
    const std::map<std::string, AICIFileMetrics>& metrics_by_file() const { return m_metricsByFile; }
    const std::vector<AICIFinding>& findings() const { return m_findings; }

    std::vector<AICISymbol> find_symbols(const std::string& name) const;
    std::vector<AICIReference> find_references(const std::string& name) const;

    void to_json(std::string& out) const;
    void to_ndjson(std::string& out) const;
    void to_sarif(std::string& out) const;

private:
    std::map<std::string, std::vector<AICISymbol>> m_symbolsByName;
    std::vector<AICIReference> m_references;
    std::map<std::string, AICIFileMetrics> m_metricsByFile;
    std::vector<AICIFinding> m_findings;
};

class AICIIndexer {
public:
    AICIIndex index_root(const std::string& root, const AICIIndexOptions& opts) const;
private:
    std::string detect_language(const std::string& path) const; // clike|python|unknown
    bool path_allowed(const std::string& path, const AICIIndexOptions& opts) const;
};
