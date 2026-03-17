#pragma once

#include "../analyzer.hpp"

class CLikeAnalyzer : public Analyzer {
public:
    AnalysisResult analyze(const std::string& path, const std::string& content) override;
};
