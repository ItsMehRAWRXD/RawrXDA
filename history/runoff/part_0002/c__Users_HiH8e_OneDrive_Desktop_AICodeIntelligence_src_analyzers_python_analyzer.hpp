#pragma once

#include "../analyzer.hpp"

class PythonAnalyzer : public Analyzer {
public:
    AnalysisResult analyze(const std::string& path, const std::string& content) override;
};
