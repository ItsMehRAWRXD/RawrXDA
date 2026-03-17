#ifndef PERFORMANCE_ANALYZER_HPP
#define PERFORMANCE_ANALYZER_HPP

#include "AICodeIntelligence.hpp"
#include <vector>
#include <string>
#include <map>

class PerformanceAnalyzer {
public:
    PerformanceAnalyzer();
    
    std::vector<CodeInsight> analyze(const std::string& code, const std::string& language, const std::string& filePath);
    
    // Specific checks
    bool hasNestedLoops(const std::string& code);
    bool hasUnnecessaryMemoryAllocation(const std::string& code);
    bool hasStringConcatenationInLoops(const std::string& code);
    bool hasRecursionWithoutMemoization(const std::string& code);
    bool hasMissingCaches(const std::string& code);
    bool hasAlgorithmicIssues(const std::string& code);
    
    // Performance metrics
    std::map<std::string, std::string> calculateMetrics(const std::string& code);
    
private:
    std::vector<CodeInsight> checkPerformanceIssues(const std::string& code, const std::string& language, const std::string& filePath);
};

#endif // PERFORMANCE_ANALYZER_HPP
