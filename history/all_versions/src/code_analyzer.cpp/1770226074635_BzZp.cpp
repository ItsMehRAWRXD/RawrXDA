#include "code_analyzer.h"
#include <regex>
#include <algorithm>
#include <sstream>
#include <cmath>

using namespace RawrXD;

std::vector<CodeIssue> CodeAnalyzer::AnalyzeCode(const std::string& code, const std::string& language) {
    std::vector<CodeIssue> issues;
    
    // Security analysis
    auto security = SecurityAudit(code);
    issues.insert(issues.end(), security.begin(), security.end());
    
    // Performance analysis
    auto perf = PerformanceAudit(code);
    issues.insert(issues.end(), perf.begin(), perf.end());
    
    // Style checking
    auto style = CheckStyle(code, "google");
    issues.insert(issues.end(), style.begin(), style.end());
    
    return issues;
}

CodeMetrics CodeAnalyzer::CalculateMetrics(const std::string& code) {
    CodeMetrics metrics;
    
    // Count lines
    metrics.lines_of_code = std::count(code.begin(), code.end(), '\n');
    
    // Count functions (simple regex)
    std::regex func_pattern(R"(\w+\s+\w+\s*\([^)]*\)\s*\{)");
    metrics.functions = std::distance(
        std::sregex_iterator(code.begin(), code.end(), func_pattern),
        std::sregex_iterator()
    );
    
    // Count classes
    std::regex class_pattern(R"(\b(class|struct)\s+\w+)");
    metrics.classes = std::distance(
        std::sregex_iterator(code.begin(), code.end(), class_pattern),
        std::sregex_iterator()
    );
    
    // Calculate cyclomatic complexity
    metrics.cyclomatic_complexity = CalculateCyclomaticComplexity(code);
    
    // Calculate maintainability index
    metrics.maintainability_index = CalculateMaintainabilityIndex(metrics);
    
    // Estimate duplication
    metrics.duplication_ratio = 0.15f; // Placeholder
    
    return metrics;
}

std::vector<CodeIssue> CodeAnalyzer::SecurityAudit(const std::string& code) {
    return DetectSecurityIssues(code);
}

std::vector<CodeIssue> CodeAnalyzer::PerformanceAudit(const std::string& code) {
    return DetectPerformanceIssues(code);
}

std::vector<CodeIssue> CodeAnalyzer::CheckStyle(const std::string& code, const std::string& style_guide) {
    std::vector<CodeIssue> issues;
    
    // Detect non-const references (Google style prefers const refs)
    std::regex non_const_ref(R"(\w+\s*&\s*\w+\s*[,\)])");
    auto begin = std::sregex_iterator(code.begin(), code.end(), non_const_ref);
    auto end = std::sregex_iterator();
    
    int line = 1;
    for (auto it = begin; it != end; ++it) {
        CodeIssue issue;
        issue.severity = CodeIssue::Warning;
        issue.code = "STYLE_001";
        issue.message = "Consider using const reference";
        issue.suggestion = "Use const T& instead of T&";
        issue.line = line;
        issues.push_back(issue);
    }
    
    // Check for trailing whitespace
    std::regex trailing_space(R"(\s+$)");
    int current_line = 1;
    std::stringstream ss(code);
    std::string line_str;
    while (std::getline(ss, line_str)) {
        if (std::regex_search(line_str, trailing_space)) {
            CodeIssue issue;
            issue.severity = CodeIssue::Info;
            issue.code = "STYLE_002";
            issue.message = "Trailing whitespace detected";
            issue.line = current_line;
            issues.push_back(issue);
        }
        current_line++;
    }
    
    return issues;
}

std::vector<std::string> CodeAnalyzer::ExtractDependencies(const std::string& code) {
    std::vector<std::string> deps;
    
    // Extract #include statements
    std::regex include_pattern(R"(#include\s+[<"]([^>"]+)[>"])");
    auto begin = std::sregex_iterator(code.begin(), code.end(), include_pattern);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        deps.push_back((*it)[1]);
    }
    
    return deps;
}

std::string CodeAnalyzer::InferType(const std::string& expression, const std::string& context) {
    // Simplified type inference
    if (expression.find("std::string") != std::string::npos) return "std::string";
    if (expression.find("int ") != std::string::npos || std::stoi(expression)) return "int";
    if (expression.find("float") != std::string::npos) return "float";
    if (expression.find("bool") != std::string::npos) return "bool";
    if (expression.find("double") != std::string::npos) return "double";
    
    return "auto";
}

std::vector<CodeIssue> CodeAnalyzer::DetectSecurityIssues(const std::string& code) {
    std::vector<CodeIssue> issues;
    
    // Detect strcpy usage
    if (code.find("strcpy") != std::string::npos) {
        CodeIssue issue;
        issue.severity = CodeIssue::Critical;
        issue.code = "SEC_001";
        issue.message = "Dangerous strcpy usage detected";
        issue.suggestion = "Use std::string or strncpy instead";
        issues.push_back(issue);
    }
    
    // Detect gets usage
    if (code.find("gets(") != std::string::npos) {
        CodeIssue issue;
        issue.severity = CodeIssue::Critical;
        issue.code = "SEC_002";
        issue.message = "Deprecated gets() function used";
        issue.suggestion = "Use std::cin or fgets instead";
        issues.push_back(issue);
    }
    
    // Detect buffer overflows
    std::regex buffer_pattern(R"(char\s+\w+\[\d+\]\s*;)");
    if (std::regex_search(code, buffer_pattern)) {
        CodeIssue issue;
        issue.severity = CodeIssue::Warning;
        issue.code = "SEC_003";
        issue.message = "Fixed-size buffer detected - potential overflow risk";
        issue.suggestion = "Use std::vector or std::string for dynamic sizing";
        issues.push_back(issue);
    }
    
    // Detect SQL injection patterns
    if (code.find("SQL") != std::string::npos && code.find("+") != std::string::npos) {
        CodeIssue issue;
        issue.severity = CodeIssue::Warning;
        issue.code = "SEC_004";
        issue.message = "String concatenation in SQL detected";
        issue.suggestion = "Use parameterized queries or ORM";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<CodeIssue> CodeAnalyzer::DetectPerformanceIssues(const std::string& code) {
    std::vector<CodeIssue> issues;
    
    // Detect string concatenation in loops
    std::regex loop_concat(R"(for\s*\([^)]*\)\s*\{[^}]*\s\+=[^}]*\})");
    if (std::regex_search(code, loop_concat)) {
        CodeIssue issue;
        issue.severity = CodeIssue::Warning;
        issue.code = "PERF_001";
        issue.message = "String concatenation in loop detected";
        issue.suggestion = "Use std::stringstream or reserve() for better performance";
        issues.push_back(issue);
    }
    
    // Detect expensive copies
    std::regex expensive_copy(R"(std::vector.*=\s*\w+;)");
    if (std::regex_search(code, expensive_copy)) {
        CodeIssue issue;
        issue.severity = CodeIssue::Warning;
        issue.code = "PERF_002";
        issue.message = "Potential expensive copy detected";
        issue.suggestion = "Use move semantics or references where appropriate";
        issues.push_back(issue);
    }
    
    return issues;
}

int CodeAnalyzer::CalculateCyclomaticComplexity(const std::string& code) {
    int complexity = 1; // Base complexity
    
    // Count decision points
    complexity += std::count(code.begin(), code.end(), 'i') / 2; // if statements (rough estimate)
    complexity += std::count(code.begin(), code.end(), 'w') / 5; // while loops
    complexity += std::count(code.begin(), code.end(), '?'); // ternary operators
    complexity += std::count(code.begin(), code.end(), '&') / 2; // && operators
    complexity += std::count(code.begin(), code.end(), '|') / 2; // || operators
    
    return std::max(1, complexity);
}

float CodeAnalyzer::CalculateMaintainabilityIndex(const CodeMetrics& metrics) {
    // Simplified Maintainability Index calculation
    if (metrics.lines_of_code == 0) return 100.0f;
    
    float volume = std::log(metrics.lines_of_code) * 1.2f;
    float complexity_factor = metrics.cyclomatic_complexity * 0.5f;
    float lines_factor = (static_cast<float>(metrics.lines_of_code) / 1000.0f) * 2.4f;
    
    float index = 171.0f - 5.2f * std::log(volume) - 0.23f * complexity_factor - 16.2f * std::log(static_cast<float>(metrics.lines_of_code)) + 50.0f;
    
    return std::clamp(index, 0.0f, 100.0f);
}
