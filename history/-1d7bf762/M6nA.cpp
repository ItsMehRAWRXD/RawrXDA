
#include "AICodeIntelligence.hpp"
#include <fstream>
#include <sstream>
#include "AICodeIntelligence.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace {
    std::string readFile(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return {};
        std::ostringstream ss; ss << in.rdbuf();
        return ss.str();
    }

    int countOccurrences(const std::string& haystack, const std::string& needle) {
        if (needle.empty()) return 0;
        int count = 0;
        size_t pos = haystack.find(needle);
        while (pos != std::string::npos) {
            ++count;
            pos = haystack.find(needle, pos + needle.size());
        }
        return count;
    }

    int countRegex(const std::string& haystack, const std::regex& re) {
        return static_cast<int>(std::distance(std::sregex_iterator(haystack.begin(), haystack.end(), re), std::sregex_iterator()));
    }
}

struct AICodeIntelligence::Private {
    std::map<std::string, std::string> enterpriseBestPractices {
        {"cpp", "Prefer RAII, avoid raw new/delete, use smart pointers"},
        {"python", "Use virtualenv/requirements, type hints, and f-strings"},
        {"js", "Avoid implicit globals, prefer const/let, lint with ESLint"}
    };
    std::map<std::string, std::string> patternDb {
        {"strcpy", "potential buffer overflow"},
        {"gets", "unsafe input"},
        {"malloc", "consider RAII/smart pointers"},
        {"std::async", "potential uncontrolled concurrency"},
        {"TODO", "unfinished work"}
    };
};

AICodeIntelligence::AICodeIntelligence() : d_ptr(std::make_unique<Private>()) {}
AICodeIntelligence::~AICodeIntelligence() = default;

// Advanced code analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeCode(const std::string& filePath, const std::string& code) {
    std::vector<CodeInsight> insights;
    const std::string content = code.empty() ? readFile(filePath) : code;
    if (content.empty()) return insights;

    auto addInsight = [&](const std::string& type, const std::string& severity, const std::string& desc, const std::string& sugg, int line, int col, double conf){
        CodeInsight ci; ci.type = type; ci.severity = severity; ci.description = desc; ci.suggestion = sugg; ci.filePath = filePath; ci.lineNumber = line; ci.columnNumber = col; ci.confidenceScore = conf; insights.push_back(ci);
    };

    // Simple heuristics
    if (content.find("TODO") != std::string::npos) {
        addInsight("maintainability", "info", "TODO found in code", "Resolve or ticket the TODO", 0, 0, 0.4);
    }
    if (content.find("printf(") != std::string::npos && content.find("std::cout") == std::string::npos) {
        addInsight("best_practice", "info", "Legacy C I/O detected", "Prefer iostreams or logging framework", 0, 0, 0.5);
    }
    if (countOccurrences(content, "strcpy") > 0) {
        addInsight("security", "warning", "Potential unsafe strcpy usage", "Use strncpy or safer alternatives", 0, 0, 0.65);
    }
    if (countOccurrences(content, "gets(") > 0) {
        addInsight("security", "error", "Use of gets() is unsafe", "Replace with fgets or safer input", 0, 0, 0.9);
    }
    if (countOccurrences(content, "new ") > countOccurrences(content, "delete")) {
        addInsight("maintainability", "warning", "Potential ownership leak (new without delete)", "Prefer smart pointers or ensure delete is paired", 0, 0, 0.6);
    }
    if (countRegex(content, std::regex("\\bwhile\\s*\\(true\\)")) > 0) {
        addInsight("performance", "warning", "Infinite loop pattern detected", "Ensure loop has exit or sleep", 0, 0, 0.55);
    }

    return insights;
}

std::vector<CodeInsight> AICodeIntelligence::analyzeProject(const std::string& projectPath) {
    std::vector<CodeInsight> all;
    for (auto& entry : std::filesystem::recursive_directory_iterator(projectPath)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" || ext == ".py" || ext == ".js") {
            auto fileInsights = analyzeCode(entry.path().string(), readFile(entry.path().string()));
            all.insert(all.end(), fileInsights.begin(), fileInsights.end());
        }
    }
    return all;
}

// AI-powered pattern detection
std::vector<CodePattern> AICodeIntelligence::detectPatterns(const std::string& code, const std::string& language) {
    std::vector<CodePattern> patterns;
    for (const auto& [needle, category] : d_ptr->patternDb) {
        if (code.find(needle) != std::string::npos) {
            CodePattern p; p.pattern = needle; p.language = language; p.category = category; p.detectionAccuracy = 0.6; patterns.push_back(p);
        }
    }
    return patterns;
}

bool AICodeIntelligence::isSecurityVulnerability(const std::string& code, const std::string& /*language*/) {
    return code.find("strcpy") != std::string::npos || code.find("gets(") != std::string::npos;
}

bool AICodeIntelligence::isPerformanceIssue(const std::string& code, const std::string& /*language*/) {
    return countRegex(code, std::regex("\\bwhile\\s*\\(true\\)")) > 0 || code.find("std::this_thread::sleep_for(0)") != std::string::npos;
}

// Machine learning insights (heuristic placeholders)
std::map<std::string, std::string> AICodeIntelligence::predictCodeQuality(const std::string& filePath) {
    auto content = readFile(filePath);
    std::map<std::string, std::string> r;
    r["file"] = filePath;
    r["todo_count"] = std::to_string(countOccurrences(content, "TODO"));
    r["length"] = std::to_string(content.size());
    r["quality"] = content.size() > 2000 ? "medium" : "high";
    return r;
}

std::map<std::string, std::string> AICodeIntelligence::suggestOptimizations(const std::string& filePath) {
    std::map<std::string, std::string> r;
    auto content = readFile(filePath);
    r["file"] = filePath;
    if (content.find("std::endl") != std::string::npos) {
        r["io"] = "Consider using '\\n' instead of std::endl to avoid flush";
    }
    if (countOccurrences(content, "new ") > countOccurrences(content, "delete")) {
        r["memory"] = "Prefer smart pointers to manage allocations";
    }
    return r;
}

std::map<std::string, std::string> AICodeIntelligence::generateDocumentation(const std::string& filePath) {
    std::map<std::string, std::string> doc;
    doc["file"] = filePath;
    doc["summary"] = "Auto-generated summary placeholder.";
    return doc;
}

// Enterprise knowledge base
void AICodeIntelligence::trainOnEnterpriseCodebase(const std::string& codebasePath) {
    // Placeholder: load best practices from files, currently no-op
    (void)codebasePath;
}

std::map<std::string, std::string> AICodeIntelligence::getEnterpriseBestPractices(const std::string& language) {
    std::map<std::string, std::string> r;
    auto it = d_ptr->enterpriseBestPractices.find(language);
    if (it != d_ptr->enterpriseBestPractices.end()) r["guidance"] = it->second;
    return r;
}

// Code metrics and statistics
std::map<std::string, std::string> AICodeIntelligence::calculateCodeMetrics(const std::string& filePath) {
    std::map<std::string, std::string> m;
    auto content = readFile(filePath);
    m["file"] = filePath;
    m["lines"] = std::to_string(static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1);
    m["todos"] = std::to_string(countOccurrences(content, "TODO"));
    m["functions_guess"] = std::to_string(countRegex(content, std::regex("\\b[a-zA-Z_][a-zA-Z0-9_]*\\s*\\(")));
    return m;
}

std::map<std::string, std::string> AICodeIntelligence::compareCodeVersions(const std::string& filePath1, const std::string& filePath2) {
    auto c1 = readFile(filePath1);
    auto c2 = readFile(filePath2);
    std::map<std::string, std::string> diff;
    diff["file1"] = filePath1;
    diff["file2"] = filePath2;
    diff["size_delta"] = std::to_string(static_cast<long long>(c2.size()) - static_cast<long long>(c1.size()));
    diff["todo_delta"] = std::to_string(countOccurrences(c2, "TODO") - countOccurrences(c1, "TODO"));
    return diff;
}

std::map<std::string, std::string> AICodeIntelligence::generateCodeHealthReport(const std::string& projectPath) {
    std::map<std::string, std::string> report;
    int files = 0;
    int todos = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(projectPath)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".py" || ext == ".js") {
            ++files;
            todos += countOccurrences(readFile(entry.path().string()), "TODO");
        }
    }
    report["files"] = std::to_string(files);
    report["todos"] = std::to_string(todos);
    report["status"] = todos == 0 ? "healthy" : "needs_attention";
    return report;
}

// Security analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeSecurity(const std::string& filePath) {
    auto content = readFile(filePath);
    return analyzeCode(filePath, content);
}

std::map<std::string, std::string> AICodeIntelligence::generateSecurityReport(const std::string& projectPath) {
    std::map<std::string, std::string> r;
    int insecure = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(projectPath)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp") {
            auto content = readFile(entry.path().string());
            if (isSecurityVulnerability(content, "cpp")) ++insecure;
        }
    }
    r["files_with_findings"] = std::to_string(insecure);
    r["status"] = insecure == 0 ? "pass" : "fail";
    return r;
}

bool AICodeIntelligence::hasKnownVulnerabilities(const std::string& code, const std::string& language) {
    (void)language;
    return isSecurityVulnerability(code, language);
}

// Performance analysis
std::vector<CodeInsight> AICodeIntelligence::analyzePerformance(const std::string& filePath) {
    auto content = readFile(filePath);
    std::vector<CodeInsight> insights;
    if (content.find("std::this_thread::sleep_for(0)") != std::string::npos) {
        CodeInsight ci; ci.type = "performance"; ci.severity = "info"; ci.description = "sleep_for(0) detected"; ci.suggestion = "Remove or adjust sleep"; ci.filePath = filePath; ci.confidenceScore = 0.4; insights.push_back(ci);
    }
    if (countRegex(content, std::regex("\\bwhile\\s*\\(true\\)")) > 0) {
        CodeInsight ci; ci.type = "performance"; ci.severity = "warning"; ci.description = "Potential busy loop"; ci.suggestion = "Add wait or exit condition"; ci.filePath = filePath; ci.confidenceScore = 0.6; insights.push_back(ci);
    }
    return insights;
}

std::map<std::string, std::string> AICodeIntelligence::generatePerformanceReport(const std::string& projectPath) {
    std::map<std::string, std::string> r;
    int loops = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(projectPath)) {
        if (!entry.is_regular_file()) continue;
        auto content = readFile(entry.path().string());
        loops += countRegex(content, std::regex("\\bwhile\\s*\\(true\\)"));
    }
    r["busy_loops"] = std::to_string(loops);
    return r;
}

std::map<std::string, std::string> AICodeIntelligence::suggestPerformanceOptimizations(const std::string& filePath) {
    return suggestOptimizations(filePath);
}

// Maintainability analysis
std::vector<CodeInsight> AICodeIntelligence::analyzeMaintainability(const std::string& filePath) {
    auto content = readFile(filePath);
    std::vector<CodeInsight> insights;
    if (countOccurrences(content, "TODO") > 3) {
        CodeInsight ci; ci.type = "maintainability"; ci.severity = "info"; ci.description = "Multiple TODOs found"; ci.suggestion = "Prioritize TODO backlog"; ci.filePath = filePath; ci.confidenceScore = 0.5; insights.push_back(ci);
    }
    return insights;
}

std::map<std::string, std::string> AICodeIntelligence::calculateMaintainabilityIndex(const std::string& filePath) {
    std::map<std::string, std::string> m;
    auto content = readFile(filePath);
    int lines = static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
    int todos = countOccurrences(content, "TODO");
    double index = std::max(0.0, 100.0 - (lines * 0.01) - (todos * 2.0));
    m["maintainability_index"] = std::to_string(index);
    return m;
}

std::map<std::string, std::string> AICodeIntelligence::generateMaintainabilityReport(const std::string& projectPath) {
    std::map<std::string, std::string> r;
    double total = 0.0;
    int files = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(projectPath)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
            auto idxStr = calculateMaintainabilityIndex(entry.path().string())["maintainability_index"];
            total += std::stod(idxStr);
            ++files;
        }
    }
    if (files > 0) r["average_index"] = std::to_string(total / files);
    return r;
}

// Code transformation suggestions
std::map<std::string, std::string> AICodeIntelligence::suggestRefactoring(const std::string& filePath) {
    std::map<std::string, std::string> r;
    auto content = readFile(filePath);
    r["file"] = filePath;
    if (countOccurrences(content, "class ") > 5) r["suggestion"] = "Consider splitting classes into separate files";
    return r;
}

std::map<std::string, std::string> AICodeIntelligence::suggestCodeModernization(const std::string& filePath) {
    std::map<std::string, std::string> r;
    auto content = readFile(filePath);
    if (content.find("auto ") == std::string::npos) r["modernize"] = "Use auto for iterator-heavy code";
    return r;
}

std::map<std::string, std::string> AICodeIntelligence::suggestArchitectureImprovements(const std::string& projectPath) {
    std::map<std::string, std::string> r;
    r["project"] = projectPath;
    r["suggestion"] = "Consider modular boundaries for agentic components";
    return r;
}

// Learning and adaptation
void AICodeIntelligence::learnFromUserFeedback(const std::string& filePath, const std::map<std::string, std::string>& feedback) {
    (void)filePath;
    // In a real system, incorporate feedback into models; here we no-op.
    for (const auto& kv : feedback) {
        d_ptr->patternDb[kv.first] = kv.second;
    }
}

void AICodeIntelligence::updatePatternDatabase(const std::map<std::string, std::string>& newPatterns) {
    for (const auto& kv : newPatterns) {
        d_ptr->patternDb[kv.first] = kv.second;
    }
}

void AICodeIntelligence::optimizeDetectionAlgorithms() {
    // Placeholder: adjust internal thresholds; currently no-op
}
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
