#include "PerformanceAnalyzer.hpp"
#include "CodeAnalysisUtils.hpp"
#include <regex>
#include <sstream>

PerformanceAnalyzer::PerformanceAnalyzer() {}

std::vector<CodeInsight> PerformanceAnalyzer::analyze(const std::string& code, const std::string& language, const std::string& filePath) {
    return checkPerformanceIssues(code, language, filePath);
}

std::vector<CodeInsight> PerformanceAnalyzer::checkPerformanceIssues(const std::string& code, const std::string& language, const std::string& filePath) {
    std::vector<CodeInsight> insights;
    
    if (hasNestedLoops(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "warning";
        insight.description = "Nested loops detected - potential O(n²) or worse complexity";
        insight.suggestion = "Consider using more efficient algorithms or data structures";
        insight.filePath = filePath;
        insight.confidenceScore = 0.80;
        insights.push_back(insight);
    }
    
    if (hasStringConcatenationInLoops(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "warning";
        insight.description = "String concatenation in loops detected";
        insight.suggestion = "Use StringBuilder or string streams for efficient concatenation";
        insight.filePath = filePath;
        insight.confidenceScore = 0.85;
        insights.push_back(insight);
    }
    
    if (hasUnnecessaryMemoryAllocation(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "info";
        insight.description = "Unnecessary memory allocations detected";
        insight.suggestion = "Reuse objects or use stack allocation where possible";
        insight.filePath = filePath;
        insight.confidenceScore = 0.70;
        insights.push_back(insight);
    }
    
    if (hasRecursionWithoutMemoization(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "warning";
        insight.description = "Recursive function without memoization detected";
        insight.suggestion = "Add memoization to cache results and improve performance";
        insight.filePath = filePath;
        insight.confidenceScore = 0.75;
        insights.push_back(insight);
    }
    
    if (hasMissingCaches(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "info";
        insight.description = "Repeated calculations without caching detected";
        insight.suggestion = "Implement caching for expensive computations";
        insight.filePath = filePath;
        insight.confidenceScore = 0.65;
        insights.push_back(insight);
    }
    
    if (hasAlgorithmicIssues(code)) {
        CodeInsight insight;
        insight.type = "performance";
        insight.severity = "warning";
        insight.description = "Inefficient algorithm pattern detected";
        insight.suggestion = "Review algorithm complexity and consider alternatives";
        insight.filePath = filePath;
        insight.confidenceScore = 0.70;
        insights.push_back(insight);
    }
    
    return insights;
}

bool PerformanceAnalyzer::hasNestedLoops(const std::string& code) {
    // Count nested for/while/do loops
    int forCount = std::count(code.begin(), code.end(), '{');
    int loopCount = 0;
    
    std::regex loopPattern(R"(\b(for|while|do)\b)");
    loopCount = std::distance(std::sregex_iterator(code.begin(), code.end(), loopPattern),
                             std::sregex_iterator());
    
    // Simple heuristic: if we have multiple loops and braces, likely nested
    return loopCount >= 2 && forCount >= 2;
}

bool PerformanceAnalyzer::hasStringConcatenationInLoops(const std::string& code) {
    // Look for string += in loops
    std::regex pattern(R"(\b(for|while)\s*\([^)]*\)\s*\{[^}]*([\w]+\s*\+=|strcat))");
    return std::regex_search(code, pattern);
}

bool PerformanceAnalyzer::hasUnnecessaryMemoryAllocation(const std::string& code) {
    // Check for new/malloc without proper deletion
    int newCount = code.find("new ") != std::string::npos ? 1 : 0;
    int deleteCount = code.find("delete") != std::string::npos ? 1 : 0;
    
    return newCount > deleteCount;
}

bool PerformanceAnalyzer::hasRecursionWithoutMemoization(const std::string& code) {
    // Look for recursive calls without cache/memo
    std::regex recursivePattern(R"((\w+)\s*\([^)]*\)\s*\{[^}]*\1\s*\()");
    if (!std::regex_search(code, recursivePattern)) return false;
    
    // Check if there's a cache mechanism
    if (StringUtils::contains(code, "cache") || StringUtils::contains(code, "memo") ||
        StringUtils::contains(code, "map<") || StringUtils::contains(code, "unordered_map")) {
        return false;
    }
    
    return true;
}

bool PerformanceAnalyzer::hasMissingCaches(const std::string& code) {
    // Check for expensive operations without caching
    std::regex expensivePattern(R"(\b(pow|sqrt|log|sin|cos|atan|exp)\s*\()");
    
    if (std::regex_search(code, expensivePattern)) {
        // Check if there's a loop doing repeated calculations
        std::regex loopPattern(R"(\b(for|while)\s*\([^)]*\)[^}]*\b(pow|sqrt|log|sin|cos)\b)");
        return std::regex_search(code, loopPattern);
    }
    
    return false;
}

bool PerformanceAnalyzer::hasAlgorithmicIssues(const std::string& code) {
    // Check for common inefficient patterns
    // Linear search where binary search should be used
    if (StringUtils::contains(code, "find(") && StringUtils::contains(code, "for ")) {
        return true;
    }
    
    // Array copying in loops
    std::regex copyPattern(R"(\b(for|while)\b[^}]*(memcpy|copy|=.*\[.*\])");
    if (std::regex_search(code, copyPattern)) {
        return true;
    }
    
    return false;
}

std::map<std::string, std::string> PerformanceAnalyzer::calculateMetrics(const std::string& code) {
    std::map<std::string, std::string> metrics;
    
    metrics["cyclomatic_complexity"] = std::to_string(CodeAnalysisUtils::countCyclomaticComplexity(code));
    metrics["estimated_complexity"] = std::to_string(static_cast<int>(CodeAnalysisUtils::calculateComplexity(code)));
    metrics["nested_depth"] = "2"; // Simplified
    metrics["function_count"] = std::to_string(CodeAnalysisUtils::countFunctions(code, "cpp"));
    
    return metrics;
}
