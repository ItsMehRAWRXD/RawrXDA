
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
    std::map<std::string, std::string> result;
    std::ifstream f(filePath);
    if (!f) return result;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    size_t lines = std::count(content.begin(), content.end(), '\n') + 1;
    result["lines"] = std::to_string(lines);
    result["has_todo"] = (content.find("TODO") != std::string::npos) ? "yes" : "no";
    return result;
}
std::map<std::string, std::string> AICodeIntelligence::suggestOptimizations(const std::string& filePath) {
    std::map<std::string, std::string> suggestions;
    std::ifstream f(filePath);
    if (!f) return suggestions;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (content.find("for (int i = 0; i <") != std::string::npos) {
        suggestions["loop"] = "Consider using range‑based for or std::transform where appropriate";
    }
    if (content.find("std::vector") != std::string::npos && content.find("reserve") == std::string::npos) {
        suggestions["vector"] = "Reserve capacity before large insertions to avoid reallocations";
    }
    return suggestions;
}
std::map<std::string, std::string> AICodeIntelligence::generateDocumentation(const std::string& filePath) {
    std::map<std::string, std::string> doc;
    doc["summary"] = "Auto‑generated documentation placeholder for " + filePath;
    return doc;
}

// Enterprise knowledge base
void AICodeIntelligence::trainOnEnterpriseCodebase(const std::string& codebasePath) {
    // No‑op placeholder – in a real system this would ingest code samples
    (void)codebasePath;
}
std::map<std::string, std::string> AICodeIntelligence::getEnterpriseBestPractices(const std::string& language) {
    std::map<std::string, std::string> practices;
    if (language == "cpp") {
        practices["memory"] = "Prefer smart pointers over raw pointers";
        practices["error"] = "Use exceptions for error handling";
    } else if (language == "python") {
        practices["style"] = "Follow PEP‑8";
    }
    return practices;
}

// Code metrics and statistics
std::map<std::string, std::string> AICodeIntelligence::calculateCodeMetrics(const std::string& filePath) {
    std::map<std::string, std::string> metrics;
    std::ifstream f(filePath);
    if (!f) return metrics;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    size_t lines = std::count(content.begin(), content.end(), '\n') + 1;
    size_t chars = content.size();
    metrics["lines"] = std::to_string(lines);
    metrics["characters"] = std::to_string(chars);
    return metrics;
}
std::map<std::string, std::string> AICodeIntelligence::compareCodeVersions(const std::string& filePath1, const std::string& filePath2) {
    std::map<std::string, std::string> diff;
    std::ifstream f1(filePath1), f2(filePath2);
    std::string c1((std::istreambuf_iterator<char>(f1)), {}), c2((std::istreambuf_iterator<char>(f2)), {});
    diff["lines_file1"] = std::to_string(std::count(c1.begin(), c1.end(), '\n') + 1);
    diff["lines_file2"] = std::to_string(std::count(c2.begin(), c2.end(), '\n') + 1);
    diff["size_file1"] = std::to_string(c1.size());
    diff["size_file2"] = std::to_string(c2.size());
    return diff;
}
std::map<std::string, std::string> AICodeIntelligence::generateCodeHealthReport(const std::string& projectPath) {
    std::map<std::string, std::string> report;
    auto insights = analyzeProject(projectPath);
    report["total_insights"] = std::to_string(insights.size());
    return report;
}

// Security analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeSecurity(const std::string& filePath) {
    std::vector<CodeInsight> insights;
    std::ifstream f(filePath);
    if (!f) return insights;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (isSecurityVulnerability(content, "cpp")) {
        CodeInsight ci;
        ci.type = "security";
        ci.severity = "high";
        ci.description = "Potential unsafe C string function usage";
        ci.filePath = filePath;
        insights.push_back(std::move(ci));
    }
    return insights;
}
std::map<std::string, std::string> AICodeIntelligence::generateSecurityReport(const std::string& projectPath) {
    std::map<std::string, std::string> report;
    auto insights = analyzeProject(projectPath);
    size_t secCount = 0;
    for (auto& i : insights) if (i.type == "security") ++secCount;
    report["security_issues"] = std::to_string(secCount);
    return report;
}
bool AICodeIntelligence::hasKnownVulnerabilities(const std::string& code, const std::string& language) {
    return isSecurityVulnerability(code, language);
}

// Performance analysis
std::vector<CodeInsight> AICodeIntelligence::analyzePerformance(const std::string& filePath) {
    std::vector<CodeInsight> insights;
    std::ifstream f(filePath);
    if (!f) return insights;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (isPerformanceIssue(content, "cpp")) {
        CodeInsight ci;
        ci.type = "performance";
        ci.severity = "medium";
        ci.description = "Potential nested loops may affect performance";
        ci.filePath = filePath;
        insights.push_back(std::move(ci));
    }
    return insights;
}
std::map<std::string, std::string> AICodeIntelligence::generatePerformanceReport(const std::string& projectPath) {
    std::map<std::string, std::string> report;
    auto insights = analyzeProject(projectPath);
    size_t perfCount = 0;
    for (auto& i : insights) if (i.type == "performance") ++perfCount;
    report["performance_issues"] = std::to_string(perfCount);
    return report;
}
std::map<std::string, std::string> AICodeIntelligence::suggestPerformanceOptimizations(const std::string& filePath) {
    std::map<std::string, std::string> suggestions;
    std::ifstream f(filePath);
    if (!f) return suggestions;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (content.find("for (int i = 0; i <") != std::string::npos) {
        suggestions["loop"] = "Consider using algorithms from <algorithm> for better performance";
    }
    return suggestions;
}

// Maintainability analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeMaintainability(const std::string& filePath) {
    std::vector<CodeInsight> insights;
    // Simple heuristic: many TODO/FIXME comments reduce maintainability
    std::ifstream f(filePath);
    std::string content;
    if (f) {
        content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    auto codeInsights = analyzeCode(filePath, content);
    for (auto& ci : codeInsights) {
        if (ci.type == "maintainability") {
            ci.severity = "warning";
            insights.push_back(std::move(ci));
        }
    }
    return insights;
}
std::map<std::string, std::string> AICodeIntelligence::calculateMaintainabilityIndex(const std::string& filePath) {
    std::map<std::string, std::string> idx;
    std::ifstream f(filePath);
    if (!f) return idx;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    size_t lines = std::count(content.begin(), content.end(), '\n') + 1;
    size_t todo = std::count(content.begin(), content.end(), 'T'); // rough proxy
    double score = 100.0 - (static_cast<double>(todo) * 5.0);
    if (score < 0) score = 0;
    idx["maintainability_index"] = std::to_string(static_cast<int>(score));
    return idx;
}
std::map<std::string, std::string> AICodeIntelligence::generateMaintainabilityReport(const std::string& projectPath) {
    std::map<std::string, std::string> report;
    auto insights = analyzeProject(projectPath);
    size_t maintCount = 0;
    for (auto& i : insights) if (i.type == "maintainability") ++maintCount;
    report["maintainability_issues"] = std::to_string(maintCount);
    return report;
}

// Code transformation suggestions
std::map<std::string, std::string> AICodeIntelligence::suggestRefactoring(const std::string& filePath) {
    std::map<std::string, std::string> suggestions;
    std::ifstream f(filePath);
    if (!f) return suggestions;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (content.find("malloc(") != std::string::npos) {
        suggestions["memory"] = "Replace malloc/free with smart pointers or std::vector";
    }
    return suggestions;
}
std::map<std::string, std::string> AICodeIntelligence::suggestCodeModernization(const std::string& filePath) {
    std::map<std::string, std::string> suggestions;
    std::ifstream f(filePath);
    if (!f) return suggestions;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    if (content.find("auto ") == std::string::npos) {
        suggestions["auto"] = "Consider using 'auto' for type deduction where appropriate";
    }
    return suggestions;
}
std::map<std::string, std::string> AICodeIntelligence::suggestArchitectureImprovements(const std::string& projectPath) {
    std::map<std::string, std::string> suggestions;
    suggestions["modular"] = "Split large source files into smaller modules for better compile times";
    return suggestions;
}

// Learning and adaptation
void AICodeIntelligence::learnFromUserFeedback(const std::string& filePath, const std::map<std::string, std::string>& feedback) {
    // Placeholder – in a real system this would adjust internal models based on feedback
    (void)filePath; (void)feedback;
}
void AICodeIntelligence::updatePatternDatabase(const std::map<std::string, std::string>& newPatterns) {
    // Placeholder – would merge new patterns into internal database
    (void)newPatterns;
}
void AICodeIntelligence::optimizeDetectionAlgorithms() {
    // Placeholder – could recompute caches or tune heuristics
}
