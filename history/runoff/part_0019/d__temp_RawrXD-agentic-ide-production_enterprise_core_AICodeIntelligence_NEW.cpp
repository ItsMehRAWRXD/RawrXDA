
#include "AICodeIntelligence.hpp"
#include "CodeAnalysisUtils.hpp"
#include "SecurityAnalyzer.hpp"
#include "PerformanceAnalyzer.hpp"
#include "MaintainabilityAnalyzer.hpp"
#include "PatternDetector.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

struct AICodeIntelligence::Private {
    SecurityAnalyzer securityAnalyzer;
    PerformanceAnalyzer performanceAnalyzer;
    MaintainabilityAnalyzer maintainabilityAnalyzer;
    PatternDetector patternDetector;
    
    std::map<std::string, std::vector<std::string>> userFeedback;
    std::vector<CodePattern> learningDatabase;
};

AICodeIntelligence::AICodeIntelligence() : d_ptr(std::make_unique<Private>()) {}
AICodeIntelligence::~AICodeIntelligence() = default;

// Advanced code analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeCode(const std::string& filePath, const std::string& code) {
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    std::vector<CodeInsight> insights;
    
    // Run all analyzers
    auto secInsights = d_ptr->securityAnalyzer.analyze(code, language, filePath);
    insights.insert(insights.end(), secInsights.begin(), secInsights.end());
    
    auto perfInsights = d_ptr->performanceAnalyzer.analyze(code, language, filePath);
    insights.insert(insights.end(), perfInsights.begin(), perfInsights.end());
    
    auto maintInsights = d_ptr->maintainabilityAnalyzer.analyze(code, language, filePath);
    insights.insert(insights.end(), maintInsights.begin(), maintInsights.end());
    
    return insights;
}

std::vector<CodeInsight> AICodeIntelligence::analyzeProject(const std::string& projectPath) {
    std::vector<CodeInsight> allInsights;
    auto files = FileUtils::listFilesRecursive(projectPath);
    
    for (const auto& file : files) {
        std::string code = FileUtils::readFile(file);
        auto insights = analyzeCode(file, code);
        allInsights.insert(allInsights.end(), insights.begin(), insights.end());
    }
    
    return allInsights;
}

// AI-powered pattern detection
std::vector<CodePattern> AICodeIntelligence::detectPatterns(const std::string& code, const std::string& language) {
    return d_ptr->patternDetector.detectPatterns(code, language);
}

bool AICodeIntelligence::isSecurityVulnerability(const std::string& code, const std::string& language) {
    auto insights = d_ptr->securityAnalyzer.analyze(code, language, "");
    return !insights.empty() && insights[0].severity == "critical";
}

bool AICodeIntelligence::isPerformanceIssue(const std::string& code, const std::string& language) {
    auto insights = d_ptr->performanceAnalyzer.analyze(code, language, "");
    return !insights.empty();
}

// Machine learning insights
std::map<std::string, std::string> AICodeIntelligence::predictCodeQuality(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    
    JsonObject quality;
    int lines = CodeAnalysisUtils::countLines(code);
    int cc = CodeAnalysisUtils::countCyclomaticComplexity(code);
    
    quality.set("quality_score", std::min(100.0, 100.0 - (cc * 2.5)));
    quality.set("language", language);
    quality.set("complexity", cc);
    quality.set("lines_of_code", lines);
    
    std::map<std::string, std::string> result;
    result["quality_assessment"] = quality.toString();
    return result;
}

std::map<std::string, std::string> AICodeIntelligence::suggestOptimizations(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    return d_ptr->performanceAnalyzer.calculateMetrics(code);
}

std::map<std::string, std::string> AICodeIntelligence::generateDocumentation(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    
    std::map<std::string, std::string> doc;
    doc["functions"] = std::to_string(CodeAnalysisUtils::countFunctions(code, language));
    doc["classes"] = std::to_string(CodeAnalysisUtils::countClasses(code, language));
    doc["file"] = filePath;
    doc["language"] = language;
    
    return doc;
}

// Enterprise knowledge base
void AICodeIntelligence::trainOnEnterpriseCodebase(const std::string& codebasePath) {
    auto files = FileUtils::listFilesRecursive(codebasePath);
    for (const auto& file : files) {
        std::string code = FileUtils::readFile(file);
        auto patterns = detectPatterns(code, CodeAnalysisUtils::detectLanguage(code));
    }
}

std::map<std::string, std::string> AICodeIntelligence::getEnterpriseBestPractices(const std::string& language) {
    std::map<std::string, std::string> practices;
    
    if (language == "cpp") {
        practices["naming"] = "Use CamelCase for classes, snake_case for variables";
        practices["memory"] = "Use smart pointers (unique_ptr, shared_ptr)";
        practices["const_correctness"] = "Mark methods as const when appropriate";
    } else if (language == "python") {
        practices["naming"] = "Use snake_case for functions and variables";
        practices["documentation"] = "Use docstrings for all public functions";
        practices["typing"] = "Use type hints for clarity";
    } else if (language == "javascript") {
        practices["async"] = "Use async/await instead of callbacks";
        practices["const"] = "Prefer const and let over var";
        practices["modules"] = "Use ES6 modules for organization";
    }
    
    return practices;
}

// Code metrics and statistics
std::map<std::string, std::string> AICodeIntelligence::calculateCodeMetrics(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    
    std::map<std::string, std::string> metrics;
    metrics["lines"] = std::to_string(CodeAnalysisUtils::countLines(code));
    metrics["non_empty_lines"] = std::to_string(CodeAnalysisUtils::countNonEmptyLines(code));
    metrics["comment_lines"] = std::to_string(CodeAnalysisUtils::countCommentLines(code));
    metrics["cyclomatic_complexity"] = std::to_string(CodeAnalysisUtils::countCyclomaticComplexity(code));
    metrics["functions"] = std::to_string(CodeAnalysisUtils::countFunctions(code, language));
    metrics["classes"] = std::to_string(CodeAnalysisUtils::countClasses(code, language));
    metrics["estimated_complexity"] = std::to_string(static_cast<int>(CodeAnalysisUtils::calculateComplexity(code)));
    
    return metrics;
}

std::map<std::string, std::string> AICodeIntelligence::compareCodeVersions(const std::string& filePath1, const std::string& filePath2) {
    std::string code1 = FileUtils::readFile(filePath1);
    std::string code2 = FileUtils::readFile(filePath2);
    
    std::map<std::string, std::string> comparison;
    int lines1 = CodeAnalysisUtils::countLines(code1);
    int lines2 = CodeAnalysisUtils::countLines(code2);
    
    comparison["file1_lines"] = std::to_string(lines1);
    comparison["file2_lines"] = std::to_string(lines2);
    comparison["line_difference"] = std::to_string(std::abs(lines2 - lines1));
    comparison["complexity_v1"] = std::to_string(CodeAnalysisUtils::countCyclomaticComplexity(code1));
    comparison["complexity_v2"] = std::to_string(CodeAnalysisUtils::countCyclomaticComplexity(code2));
    
    return comparison;
}

std::map<std::string, std::string> AICodeIntelligence::generateCodeHealthReport(const std::string& projectPath) {
    auto insights = analyzeProject(projectPath);
    
    std::map<std::string, std::string> health;
    int critical = 0, high = 0, warning = 0, info = 0;
    
    for (const auto& insight : insights) {
        if (insight.severity == "critical") critical++;
        else if (insight.severity == "high") high++;
        else if (insight.severity == "warning") warning++;
        else if (insight.severity == "info") info++;
    }
    
    health["critical_issues"] = std::to_string(critical);
    health["high_issues"] = std::to_string(high);
    health["warning_issues"] = std::to_string(warning);
    health["info_messages"] = std::to_string(info);
    health["total_issues"] = std::to_string(insights.size());
    
    double healthScore = 100.0 - (critical * 10) - (high * 5) - (warning * 1);
    health["health_score"] = std::to_string(std::max(0.0, healthScore));
    
    return health;
}

// Security analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeSecurity(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    return d_ptr->securityAnalyzer.analyze(code, language, filePath);
}

std::map<std::string, std::string> AICodeIntelligence::generateSecurityReport(const std::string& projectPath) {
    auto insights = analyzeProject(projectPath);
    
    std::map<std::string, std::string> report;
    int vulnCount = 0;
    
    for (const auto& insight : insights) {
        if (insight.type == "security") {
            vulnCount++;
        }
    }
    
    report["vulnerabilities_found"] = std::to_string(vulnCount);
    report["security_risk"] = vulnCount > 5 ? "high" : (vulnCount > 0 ? "medium" : "low");
    report["recommendation"] = "Review and fix all critical security issues immediately";
    
    return report;
}

bool AICodeIntelligence::hasKnownVulnerabilities(const std::string& code, const std::string& language) {
    auto insights = d_ptr->securityAnalyzer.analyze(code, language, "");
    return insights.size() > 0;
}

// Performance analysis
std::vector<CodeInsight> AICodeIntelligence::analyzePerformance(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    return d_ptr->performanceAnalyzer.analyze(code, language, filePath);
}

std::map<std::string, std::string> AICodeIntelligence::generatePerformanceReport(const std::string& projectPath) {
    auto insights = analyzeProject(projectPath);
    
    std::map<std::string, std::string> report;
    int perfIssues = 0;
    
    for (const auto& insight : insights) {
        if (insight.type == "performance") {
            perfIssues++;
        }
    }
    
    report["performance_issues"] = std::to_string(perfIssues);
    report["optimization_needed"] = perfIssues > 0 ? "yes" : "no";
    
    return report;
}

std::map<std::string, std::string> AICodeIntelligence::suggestPerformanceOptimizations(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    return d_ptr->performanceAnalyzer.calculateMetrics(code);
}

// Maintainability analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeMaintainability(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    return d_ptr->maintainabilityAnalyzer.analyze(code, language, filePath);
}

std::map<std::string, std::string> AICodeIntelligence::calculateMaintainabilityIndex(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    return d_ptr->maintainabilityAnalyzer.calculateMaintainabilityIndex(code);
}

std::map<std::string, std::string> AICodeIntelligence::generateMaintainabilityReport(const std::string& projectPath) {
    auto insights = analyzeProject(projectPath);
    
    std::map<std::string, std::string> report;
    int maintIssues = 0;
    
    for (const auto& insight : insights) {
        if (insight.type == "maintainability") {
            maintIssues++;
        }
    }
    
    report["maintainability_issues"] = std::to_string(maintIssues);
    report["refactoring_priority"] = maintIssues > 10 ? "high" : (maintIssues > 0 ? "medium" : "low");
    
    return report;
}

// Code transformation suggestions
std::map<std::string, std::string> AICodeIntelligence::suggestRefactoring(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    
    std::map<std::string, std::string> suggestions;
    suggestions["extract_methods"] = "Break down long methods into smaller functions";
    suggestions["reduce_complexity"] = "Simplify conditional logic";
    suggestions["improve_naming"] = "Use more descriptive variable and function names";
    
    return suggestions;
}

std::map<std::string, std::string> AICodeIntelligence::suggestCodeModernization(const std::string& filePath) {
    std::string code = FileUtils::readFile(filePath);
    std::string language = CodeAnalysisUtils::detectLanguage(code);
    
    std::map<std::string, std::string> suggestions;
    
    if (language == "cpp") {
        suggestions["use_auto"] = "Use auto keyword for type deduction";
        suggestions["use_nullptr"] = "Replace NULL with nullptr";
        suggestions["use_smart_pointers"] = "Replace raw pointers with smart pointers";
        suggestions["range_based_for"] = "Use range-based for loops";
    } else if (language == "python") {
        suggestions["use_f_strings"] = "Use f-strings instead of .format()";
        suggestions["use_type_hints"] = "Add type hints for better code documentation";
    }
    
    return suggestions;
}

std::map<std::string, std::string> AICodeIntelligence::suggestArchitectureImprovements(const std::string& projectPath) {
    std::map<std::string, std::string> suggestions;
    suggestions["modularity"] = "Increase code modularity by separating concerns";
    suggestions["coupling"] = "Reduce coupling between components";
    suggestions["testability"] = "Improve testability with dependency injection";
    suggestions["documentation"] = "Add architectural documentation";
    
    return suggestions;
}

// Learning and adaptation
void AICodeIntelligence::learnFromUserFeedback(const std::string& filePath, const std::map<std::string, std::string>& feedback) {
    d_ptr->userFeedback[filePath] = std::vector<std::string>();
    for (const auto& [key, value] : feedback) {
        d_ptr->userFeedback[filePath].push_back(key + ":" + value);
    }
}

void AICodeIntelligence::updatePatternDatabase(const std::map<std::string, std::string>& newPatterns) {
    for (const auto& [key, pattern] : newPatterns) {
        CodePattern cp;
        cp.pattern = pattern;
        cp.language = "cpp";
        cp.category = key;
        d_ptr->patternDetector.addPattern(cp);
    }
}

void AICodeIntelligence::optimizeDetectionAlgorithms() {
    // Implement algorithm optimization based on feedback
}
