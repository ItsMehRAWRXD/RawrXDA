#pragma once

#include "../analyzer.hpp"

class AICICLikeAnalyzer : public AICIAnalyzer {
public:
    AICIAnalysisResult analyze(const std::string& path, const std::string& content) override;
};
