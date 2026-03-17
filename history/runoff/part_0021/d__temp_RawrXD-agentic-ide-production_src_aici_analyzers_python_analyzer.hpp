#pragma once

#include "../analyzer.hpp"

class AICIPythonAnalyzer : public AICIAnalyzer {
public:
    AICIAnalysisResult analyze(const std::string& path, const std::string& content) override;
};
