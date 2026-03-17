#pragma once

#include <string>
#include <vector>

struct AICISymbol {
    std::string name;
    std::string kind;
    std::string file;
    int line = 0;
};

struct AICIReference {
    std::string symbol;
    std::string file;
    int line = 0;
};

struct AICIFileMetrics {
    int lines = 0;
    int codeLines = 0;
    int commentLines = 0;
    int complexity = 1;
};

struct AICIAnalysisResult {
    std::vector<AICISymbol> symbols;
    std::vector<AICIReference> references;
    AICIFileMetrics metrics;
};

class AICIAnalyzer {
public:
    virtual ~AICIAnalyzer() = default;
    virtual AICIAnalysisResult analyze(const std::string& path, const std::string& content) = 0;
};
