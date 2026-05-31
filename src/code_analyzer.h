#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace RawrXD {

struct CodeIssue {
    enum Severity { Info, Warning, Error, Critical };
    
    Severity severity;
    std::string code;
    std::string message;
    int line;
    int column;
    std::string suggestion;
};

struct CodeMetrics {
    int lines_of_code = 0;
    int cyclomatic_complexity = 0;
    float maintainability_index = 0.0f;
    int functions = 0;
    int classes = 0;
    float duplication_ratio = 0.0f;
    std::unordered_map<std::string, int> language_breakdown;
};

class CodeAnalyzer {
public:
    CodeAnalyzer() = default;
    virtual ~CodeAnalyzer() = default;
    
    // Code analysis
    std::vector<CodeIssue> AnalyzeCode(const std::string& code, const std::string& language = "cpp");
    CodeMetrics CalculateMetrics(const std::string& code);
    
    // Security analysis
    std::vector<CodeIssue> SecurityAudit(const std::string& code);
    
    // Performance analysis
    std::vector<CodeIssue> PerformanceAudit(const std::string& code);
    
    // Style checking
    std::vector<CodeIssue> CheckStyle(const std::string& code, const std::string& style_guide = "google");
    
    // Dependency analysis
    std::vector<std::string> ExtractDependencies(const std::string& code);
    
    // Type inference
    std::string InferType(const std::string& expression, const std::string& context);
    
private:
    std::vector<CodeIssue> DetectSecurityIssues(const std::string& code);
    std::vector<CodeIssue> DetectPerformanceIssues(const std::string& code);
    int CalculateCyclomaticComplexity(const std::string& code);
    float CalculateMaintainabilityIndex(const CodeMetrics& metrics);
};

}
