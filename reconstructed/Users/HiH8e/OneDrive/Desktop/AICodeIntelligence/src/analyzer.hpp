#pragma once

#include <string>
#include <vector>

struct Symbol {
    std::string name;
    std::string kind; // function, class, method
    std::string file;
    int line = 0;
};

struct Reference {
    std::string symbol;
    std::string file;
    int line = 0;
};

struct FileMetrics {
    int lines = 0;
    int codeLines = 0;
    int commentLines = 0;
    int complexity = 1; // cyclomatic baseline
};

struct AnalysisResult {
    std::vector<Symbol> symbols;
    std::vector<Reference> references;
    FileMetrics metrics;
};

class Analyzer {
public:
    virtual ~Analyzer() = default;
    virtual AnalysisResult analyze(const std::string& path, const std::string& content) = 0;
};
