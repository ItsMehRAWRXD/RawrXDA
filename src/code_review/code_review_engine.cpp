// ============================================================================
// code_review_engine.cpp — AI-Powered Code Review Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "code_review/code_review_engine.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <regex>
#include <stack>

namespace fs = std::filesystem;

namespace RawrXD::CodeReview {

// ============================================================================
// Code Review Engine Implementation
// ============================================================================

class CodeReviewEngine::Impl {
public:
    SecurityAnalyzer* securityAnalyzer = nullptr;
    ReviewConfig config;
    ReviewStats stats;
    std::atomic<uint64_t> nextId{1};
    mutable std::shared_mutex mutex;
    
    Impl() = default;
    ~Impl() = default;
};

CodeReviewEngine::CodeReviewEngine(SecurityAnalyzer* securityAnalyzer)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->securityAnalyzer = securityAnalyzer;
}

CodeReviewEngine::~CodeReviewEngine() = default;

void CodeReviewEngine::setConfig(const ReviewConfig& config) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->config = config;
}

ReviewConfig CodeReviewEngine::getConfig() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->config;
}

ReviewResult CodeReviewEngine::reviewFile(const std::string& uri,
                                          const std::string& content,
                                          const std::string& language) {
    auto start = std::chrono::high_resolution_clock::now();
    
    ReviewResult result;
    result.uri = uri;
    
    // Calculate metrics
    if (m_impl->config.enableMetrics) {
        result.metrics = calculateMetrics(content, language);
    }
    
    // Security analysis
    if (m_impl->config.enableSecurityAnalysis && m_impl->securityAnalyzer) {
        auto secResult = m_impl->securityAnalyzer->analyzeFile(uri, content, language);
        result.securityFindings = std::move(secResult.findings);
    }
    
    // Code smell detection
    if (m_impl->config.enableCodeSmells) {
        result.codeSmells = detectCodeSmells(content, language);
    }
    
    // Generate suggestions
    if (m_impl->config.enableSuggestions) {
        result.suggestions = generateSuggestions(content, language,
                                                 result.codeSmells,
                                                 result.securityFindings);
    }
    
    // Count issues
    for (const auto& f : result.securityFindings) {
        result.totalIssues++;
        switch (f.severity) {
            case Severity::Critical: result.criticalIssues++; break;
            case Severity::High: result.highIssues++; break;
            case Severity::Medium: result.mediumIssues++; break;
            case Severity::Low: result.lowIssues++; break;
            case Severity::Info: result.infoIssues++; break;
        }
    }
    
    for (const auto& s : result.codeSmells) {
        result.totalIssues++;
        switch (s.severity) {
            case Severity::Critical: result.criticalIssues++; break;
            case Severity::High: result.highIssues++; break;
            case Severity::Medium: result.mediumIssues++; break;
            case Severity::Low: result.lowIssues++; break;
            case Severity::Info: result.infoIssues++; break;
        }
    }
    
    // Calculate scores
    result.overallScore = calculateOverallScore(result);
    result.securityScore = calculateSecurityScore(result);
    result.maintainabilityScore = calculateMaintainabilityScore(result);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.reviewTimeMs = 
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.success = true;
    
    // Update stats
    std::unique_lock lock(m_impl->mutex);
    m_impl->stats.filesReviewed++;
    m_impl->stats.totalIssues += result.totalIssues;
    m_impl->stats.securityIssues += result.securityFindings.size();
    m_impl->stats.codeSmells += result.codeSmells.size();
    m_impl->stats.suggestions += result.suggestions.size();
    m_impl->stats.totalReviewTimeMs += result.reviewTimeMs;
    m_impl->stats.avgReviewTimeMs = 
        static_cast<double>(m_impl->stats.totalReviewTimeMs) / 
        m_impl->stats.filesReviewed;
    m_impl->stats.avgQualityScore = 
        (m_impl->stats.avgQualityScore * (m_impl->stats.filesReviewed - 1) + 
         result.overallScore) / m_impl->stats.filesReviewed;
    m_impl->stats.avgSecurityScore = 
        (m_impl->stats.avgSecurityScore * (m_impl->stats.filesReviewed - 1) + 
         result.securityScore) / m_impl->stats.filesReviewed;
    m_impl->stats.avgMaintainabilityScore = 
        (m_impl->stats.avgMaintainabilityScore * (m_impl->stats.filesReviewed - 1) + 
         result.maintainabilityScore) / m_impl->stats.filesReviewed;
    
    return result;
}

ReviewResult CodeReviewEngine::reviewFile(const std::string& uri) {
    std::ifstream file(uri);
    if (!file.is_open()) {
        ReviewResult result;
        result.uri = uri;
        result.success = false;
        result.errorMessage = "Failed to open file: " + uri;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Detect language
    std::string language = "text";
    fs::path p(uri);
    std::string ext = p.extension().string();
    
    static const std::unordered_map<std::string, std::string> extToLang = {
        {".cpp", "cpp"}, {".cxx", "cpp"}, {".cc", "cpp"}, {".C", "cpp"},
        {".h", "cpp"}, {".hpp", "cpp"}, {".hxx", "cpp"},
        {".c", "c"},
        {".js", "javascript"}, {".mjs", "javascript"}, {".cjs", "javascript"},
        {".ts", "typescript"}, {".tsx", "typescript"},
        {".py", "python"}, {".pyw", "python"},
        {".java", "java"},
        {".cs", "csharp"},
        {".go", "go"},
        {".rs", "rust"},
        {".rb", "ruby"},
        {".php", "php"},
        {".asm", "asm"}, {".s", "asm"},
    };
    
    auto it = extToLang.find(ext);
    if (it != extToLang.end()) {
        language = it->second;
    }
    
    return reviewFile(uri, content, language);
}

std::vector<ReviewResult> CodeReviewEngine::reviewProject(
    const std::vector<std::string>& uris) {
    std::vector<ReviewResult> results;
    results.reserve(uris.size());
    
    for (const auto& uri : uris) {
        results.push_back(reviewFile(uri));
    }
    
    return results;
}

QualityMetrics CodeReviewEngine::calculateMetrics(const std::string& content,
                                                   const std::string& language) {
    QualityMetrics metrics;
    
    // Split into lines
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    metrics.totalLines = static_cast<uint32_t>(lines.size());
    
    // Count lines
    for (const auto& l : lines) {
        std::string trimmed = l;
        // Trim whitespace
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            metrics.blankLines++;
            continue;
        }
        trimmed = trimmed.substr(start);
        
        // Check for comments
        if (trimmed.size() >= 2) {
            if (trimmed[0] == '/' && trimmed[1] == '/') {
                metrics.commentLines++;
                continue;
            }
            if (trimmed[0] == '/' && trimmed[1] == '*') {
                metrics.commentLines++;
                continue;
            }
            if (trimmed[0] == '#') {
                metrics.commentLines++;
                continue;
            }
        }
        
        metrics.linesOfCode++;
    }
    
    // Comment ratio
    if (metrics.linesOfCode > 0) {
        metrics.commentRatio = 
            static_cast<float>(metrics.commentLines) / metrics.linesOfCode * 100.0f;
    }
    
    // Complexity metrics
    metrics.cyclomaticComplexity = 
        ComplexityCalculator::calculateCyclomatic(content, language);
    metrics.cognitiveComplexity = 
        ComplexityCalculator::calculateCognitive(content, language);
    metrics.nestingDepth = 
        ComplexityCalculator::calculateNestingDepth(content, language);
    
    // Halstead metrics
    auto halstead = ComplexityCalculator::calculateHalstead(content, language);
    metrics.halsteadVolume = halstead.volume;
    metrics.halsteadDifficulty = halstead.difficulty;
    
    // Maintainability index
    metrics.maintainabilityIndex = ComplexityCalculator::calculateMaintainabilityIndex(
        metrics.cyclomaticComplexity, metrics.halsteadVolume, metrics.linesOfCode);
    
    // Count functions and classes
    static const std::regex funcPattern(
        R"((void|int|char|float|double|bool|auto)\s+(\w+)\s*\([^)]*\)\s*\{)");
    static const std::regex classPattern(R"(\bclass\s+(\w+))");
    static const std::regex jsFuncPattern(R"((function\s+\w+|const\s+\w+\s*=\s*(?:async\s*)?\([^)]*\)\s*=>))");
    
    std::string::const_iterator it = content.begin();
    std::smatch match;
    
    while (std::regex_search(it, content.end(), match, funcPattern)) {
        metrics.functionCount++;
        it = match.suffix().first;
    }
    
    it = content.begin();
    while (std::regex_search(it, content.end(), match, classPattern)) {
        metrics.classCount++;
        it = match.suffix().first;
    }
    
    if (language == "javascript" || language == "typescript") {
        it = content.begin();
        while (std::regex_search(it, content.end(), match, jsFuncPattern)) {
            metrics.functionCount++;
            it = match.suffix().first;
        }
    }
    
    // Count TODOs
    static const std::regex todoPattern(R"((TODO|FIXME|HACK|XXX|BUG))");
    it = content.begin();
    while (std::regex_search(it, content.end(), match, todoPattern)) {
        metrics.todoCount++;
        it = match.suffix().first;
    }
    
    return metrics;
}

std::vector<CodeSmellFinding> CodeReviewEngine::detectCodeSmells(
    const std::string& content,
    const std::string& language) {
    
    CodeSmellDetector detector(m_impl->config);
    return detector.detect(content, language);
}

std::vector<ReviewSuggestion> CodeReviewEngine::generateSuggestions(
    const std::string& content,
    const std::string& language,
    const std::vector<CodeSmellFinding>& smells,
    const std::vector<VulnerabilityFinding>& vulns) {
    
    std::vector<ReviewSuggestion> suggestions;
    
    // Generate suggestions from code smells
    for (const auto& smell : smells) {
        ReviewSuggestion s;
        s.id = m_impl->nextId++;
        s.category = "maintainability";
        s.severity = smell.severity;
        s.confidence = smell.confidence;
        s.location = smell.location;
        
        switch (smell.type) {
            case CodeSmellType::LongMethod:
                s.title = "Extract Method";
                s.description = "Consider breaking this long method into smaller functions";
                s.explanation = "Long methods are harder to understand, test, and maintain. "
                               "Extract related logic into separate methods with clear names.";
                s.isAutoFixable = false;
                break;
                
            case CodeSmellType::DeepNesting:
                s.title = "Reduce Nesting";
                s.description = "Consider using early returns or extracting nested logic";
                s.explanation = "Deep nesting makes code harder to follow. "
                               "Use guard clauses or extract methods to reduce complexity.";
                s.isAutoFixable = false;
                break;
                
            case CodeSmellType::MagicNumber:
                s.title = "Extract Constant";
                s.description = "Replace magic number with named constant";
                s.explanation = "Magic numbers make code harder to understand. "
                               "Use named constants to document intent.";
                s.isAutoFixable = true;
                break;
                
            case CodeSmellType::LongParameterList:
                s.title = "Introduce Parameter Object";
                s.description = "Consider grouping related parameters into an object";
                s.explanation = "Long parameter lists are hard to remember and maintain. "
                               "Group related parameters into a struct or class.";
                s.isAutoFixable = false;
                break;
                
            default:
                s.title = smell.name;
                s.description = smell.description;
                s.explanation = smell.suggestion;
                s.isAutoFixable = false;
                break;
        }
        
        suggestions.push_back(std::move(s));
    }
    
    // Generate suggestions from security findings
    for (const auto& vuln : vulns) {
        ReviewSuggestion s;
        s.id = m_impl->nextId++;
        s.category = "security";
        s.severity = vuln.severity;
        s.confidence = vuln.confidence;
        s.location = vuln.location;
        s.title = vuln.title;
        s.description = vuln.description;
        s.explanation = vuln.remediation;
        s.isAutoFixable = false;
        
        suggestions.push_back(std::move(s));
    }
    
    return suggestions;
}

float CodeReviewEngine::calculateOverallScore(const ReviewResult& result) const {
    float score = 100.0f;
    
    // Deduct for issues
    score -= result.criticalIssues * 20.0f;
    score -= result.highIssues * 10.0f;
    score -= result.mediumIssues * 5.0f;
    score -= result.lowIssues * 2.0f;
    score -= result.infoIssues * 0.5f;
    
    // Deduct for poor metrics
    if (result.metrics.maintainabilityIndex < 20) {
        score -= 10.0f;
    }
    if (result.metrics.cyclomaticComplexity > 20) {
        score -= 5.0f;
    }
    if (result.metrics.commentRatio < 5.0f) {
        score -= 5.0f;
    }
    
    return std::max(0.0f, std::min(100.0f, score));
}

float CodeReviewEngine::calculateSecurityScore(const ReviewResult& result) const {
    float score = 100.0f;
    
    for (const auto& f : result.securityFindings) {
        switch (f.severity) {
            case Severity::Critical: score -= 25.0f; break;
            case Severity::High: score -= 15.0f; break;
            case Severity::Medium: score -= 8.0f; break;
            case Severity::Low: score -= 3.0f; break;
            case Severity::Info: score -= 1.0f; break;
        }
    }
    
    return std::max(0.0f, std::min(100.0f, score));
}

float CodeReviewEngine::calculateMaintainabilityScore(const ReviewResult& result) const {
    float score = 100.0f;
    
    // Based on maintainability index
    score = result.metrics.maintainabilityIndex;
    
    // Adjust for code smells
    score -= result.codeSmells.size() * 2.0f;
    
    // Adjust for complexity
    if (result.metrics.cyclomaticComplexity > 15) {
        score -= (result.metrics.cyclomaticComplexity - 15) * 2.0f;
    }
    
    return std::max(0.0f, std::min(100.0f, score));
}

CodeReviewEngine::ReviewStats CodeReviewEngine::getStats() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->stats;
}

void CodeReviewEngine::resetStats() {
    std::unique_lock lock(m_impl->mutex);
    m_impl->stats = ReviewStats{};
}

std::string CodeReviewEngine::exportToJson(
    const std::vector<ReviewResult>& results) const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"results\": [\n";
    
    bool firstResult = true;
    for (const auto& result : results) {
        if (!firstResult) oss << ",\n";
        firstResult = false;
        
        oss << "    {\n";
        oss << "      \"uri\": \"" << result.uri << "\",\n";
        oss << "      \"overallScore\": " << result.overallScore << ",\n";
        oss << "      \"securityScore\": " << result.securityScore << ",\n";
        oss << "      \"maintainabilityScore\": " << result.maintainabilityScore << ",\n";
        oss << "      \"totalIssues\": " << result.totalIssues << ",\n";
        oss << "      \"metrics\": {\n";
        oss << "        \"linesOfCode\": " << result.metrics.linesOfCode << ",\n";
        oss << "        \"cyclomaticComplexity\": " << result.metrics.cyclomaticComplexity << ",\n";
        oss << "        \"maintainabilityIndex\": " << result.metrics.maintainabilityIndex << "\n";
        oss << "      }\n";
        oss << "    }";
    }
    
    oss << "\n  ]\n";
    oss << "}\n";
    
    return oss.str();
}

std::string CodeReviewEngine::exportToMarkdown(
    const std::vector<ReviewResult>& results) const {
    std::ostringstream oss;
    oss << "# Code Review Report\n\n";
    
    for (const auto& result : results) {
        oss << "## " << result.uri << "\n\n";
        oss << "### Scores\n";
        oss << "- Overall: " << std::fixed << std::setprecision(1) 
            << result.overallScore << "/100\n";
        oss << "- Security: " << result.securityScore << "/100\n";
        oss << "- Maintainability: " << result.maintainabilityScore << "/100\n\n";
        
        oss << "### Metrics\n";
        oss << "| Metric | Value |\n";
        oss << "|--------|-------|\n";
        oss << "| Lines of Code | " << result.metrics.linesOfCode << " |\n";
        oss << "| Cyclomatic Complexity | " << result.metrics.cyclomaticComplexity << " |\n";
        oss << "| Maintainability Index | " << result.metrics.maintainabilityIndex << " |\n\n";
        
        if (!result.securityFindings.empty()) {
            oss << "### Security Issues (" << result.securityFindings.size() << ")\n\n";
            for (const auto& f : result.securityFindings) {
                oss << "- **" << f.title << "** (Line " << f.location.line << ")\n";
                oss << "  - " << f.description << "\n";
            }
            oss << "\n";
        }
        
        if (!result.codeSmells.empty()) {
            oss << "### Code Smells (" << result.codeSmells.size() << ")\n\n";
            for (const auto& s : result.codeSmells) {
                oss << "- **" << s.name << "** (Line " << s.location.line << ")\n";
            }
            oss << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// Complexity Calculator Implementation
// ============================================================================

uint32_t ComplexityCalculator::calculateCyclomatic(const std::string& content,
                                                    const std::string& language) {
    uint32_t complexity = 1; // Base complexity
    
    // Decision points
    static const std::vector<std::regex> decisionPatterns = {
        std::regex(R"(\bif\s*\()"),
        std::regex(R"(\belse\s+if\s*\()"),
        std::regex(R"(\bfor\s*\()"),
        std::regex(R"(\bwhile\s*\()"),
        std::regex(R"(\bcase\s+)"),
        std::regex(R"(\bcatch\s*\()"),
        std::regex(R"(\?\s*:)"),  // Ternary
        std::regex(R"(\&\&|\|\|)"), // Logical operators
    };
    
    for (const auto& pattern : decisionPatterns) {
        std::sregex_iterator it(content.begin(), content.end(), pattern);
        std::sregex_iterator end;
        while (it != end) {
            complexity++;
            ++it;
        }
    }
    
    return complexity;
}

uint32_t ComplexityCalculator::calculateCognitive(const std::string& content,
                                                   const std::string& language) {
    uint32_t complexity = 0;
    uint32_t nestingLevel = 0;
    
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Trim
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);
        
        // Check for nesting increase
        if (trimmed.find('{') != std::string::npos) {
            nestingLevel++;
        }
        
        // Check for decision points
        static const std::vector<std::regex> patterns = {
            std::regex(R"(\bif\s*\()"),
            std::regex(R"(\bfor\s*\()"),
            std::regex(R"(\bwhile\s*\()"),
            std::regex(R"(\bswitch\s*\()"),
            std::regex(R"(\bcatch\s*\()"),
        };
        
        for (const auto& pattern : patterns) {
            if (std::regex_search(trimmed, pattern)) {
                complexity += nestingLevel;
            }
        }
        
        // Check for nesting decrease
        if (trimmed.find('}') != std::string::npos) {
            if (nestingLevel > 0) nestingLevel--;
        }
    }
    
    return complexity;
}

uint32_t ComplexityCalculator::calculateNestingDepth(const std::string& content,
                                                      const std::string& language) {
    uint32_t maxDepth = 0;
    uint32_t currentDepth = 0;
    
    for (char c : content) {
        if (c == '{') {
            currentDepth++;
            maxDepth = std::max(maxDepth, currentDepth);
        } else if (c == '}') {
            if (currentDepth > 0) currentDepth--;
        }
    }
    
    return maxDepth;
}

ComplexityCalculator::HalsteadMetrics ComplexityCalculator::calculateHalstead(
    const std::string& content,
    const std::string& language) {
    
    HalsteadMetrics metrics;
    
    // Simplified: count operators and operands
    static const std::regex operatorPattern(
        R"((\+\+|--|->|::|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|<<=|>>=|==|!=|<=|>=|&&|\|\||[+\-*/%&|^~!<>?=]))");
    static const std::regex operandPattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+(\.[0-9]+)?)\b)");
    
    std::unordered_set<std::string> uniqueOperators;
    std::unordered_set<std::string> uniqueOperands;
    uint32_t totalOperators = 0;
    uint32_t totalOperands = 0;
    
    std::sregex_iterator opIt(content.begin(), content.end(), operatorPattern);
    std::sregex_iterator end;
    while (opIt != end) {
        uniqueOperators.insert(opIt->str());
        totalOperators++;
        ++opIt;
    }
    
    std::sregex_iterator orIt(content.begin(), content.end(), operandPattern);
    while (orIt != end) {
        uniqueOperands.insert(orIt->str());
        totalOperands++;
        ++orIt;
    }
    
    uint32_t n1 = static_cast<uint32_t>(uniqueOperators.size());
    uint32_t n2 = static_cast<uint32_t>(uniqueOperands.size());
    uint32_t N1 = totalOperators;
    uint32_t N2 = totalOperands;
    
    metrics.vocabulary = n1 + n2;
    metrics.length = N1 + N2;
    
    if (metrics.vocabulary > 0) {
        metrics.volume = static_cast<uint32_t>(
            metrics.length * std::log2(metrics.vocabulary));
    }
    
    if (n2 > 0) {
        metrics.difficulty = static_cast<uint32_t>((n1 / 2.0) * (N2 / n2));
    }
    
    metrics.effort = metrics.difficulty * metrics.volume;
    metrics.bugs = metrics.volume / 3000;
    
    return metrics;
}

float ComplexityCalculator::calculateMaintainabilityIndex(
    uint32_t cyclomatic,
    uint32_t halsteadVolume,
    uint32_t loc) {
    
    if (loc == 0) return 100.0f;
    
    // Simplified maintainability index
    double mi = 171.0 - 5.2 * std::log(halsteadVolume + 1) 
                - 0.23 * cyclomatic 
                - 16.2 * std::log(loc + 1);
    
    mi = std::max(0.0, mi);
    mi = std::min(100.0, mi * 100.0 / 171.0);
    
    return static_cast<float>(mi);
}

// ============================================================================
// Code Smell Detector Implementation
// ============================================================================

CodeSmellDetector::CodeSmellDetector(const ReviewConfig& config)
    : m_config(config) {
}

std::vector<CodeSmellFinding> CodeSmellDetector::detect(const std::string& content,
                                                         const std::string& language) {
    std::vector<CodeSmellFinding> findings;
    
    auto longMethods = detectLongMethods(content, language);
    findings.insert(findings.end(), longMethods.begin(), longMethods.end());
    
    auto largeClasses = detectLargeClasses(content, language);
    findings.insert(findings.end(), largeClasses.begin(), largeClasses.end());
    
    auto longParams = detectLongParameterLists(content, language);
    findings.insert(findings.end(), longParams.begin(), longParams.end());
    
    auto deepNesting = detectDeepNesting(content, language);
    findings.insert(findings.end(), deepNesting.begin(), deepNesting.end());
    
    auto magicNumbers = detectMagicNumbers(content, language);
    findings.insert(findings.end(), magicNumbers.begin(), magicNumbers.end());
    
    auto deadCode = detectDeadCode(content, language);
    findings.insert(findings.end(), deadCode.begin(), deadCode.end());
    
    auto complexConditions = detectComplexConditions(content, language);
    findings.insert(findings.end(), complexConditions.begin(), complexConditions.end());
    
    auto naming = detectNamingIssues(content, language);
    findings.insert(findings.end(), naming.begin(), naming.end());
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectLongMethods(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    // Find function definitions and count lines
    static const std::regex funcPattern(
        R"((?:void|int|char|float|double|bool|auto|std::\w+|\w+(?:<[^>]+>)?)\s+(\w+)\s*\([^)]*\)\s*(?:const\s*)?\{)");
    
    std::sregex_iterator it(content.begin(), content.end(), funcPattern);
    std::sregex_iterator end;
    
    while (it != end) {
        size_t funcStart = it->position();
        std::string funcName = (*it)[1].str();
        
        // Count lines until matching }
        int braceCount = 1;
        size_t pos = funcStart + it->length();
        uint32_t lineCount = 1;
        
        while (pos < content.size() && braceCount > 0) {
            if (content[pos] == '{') braceCount++;
            else if (content[pos] == '}') braceCount--;
            else if (content[pos] == '\n') lineCount++;
            pos++;
        }
        
        if (lineCount > m_config.maxFunctionLength) {
            // Find line number
            uint32_t lineNum = 1;
            for (size_t i = 0; i < funcStart; ++i) {
                if (content[i] == '\n') lineNum++;
            }
            
            CodeSmellFinding f;
            f.type = CodeSmellType::LongMethod;
            f.name = "Long Method";
            f.description = "Method '" + funcName + "' has " + 
                           std::to_string(lineCount) + " lines (max: " +
                           std::to_string(m_config.maxFunctionLength) + ")";
            f.location.uri = "";
            f.location.line = lineNum;
            f.severity = m_config.longMethodSeverity;
            f.suggestion = "Consider extracting parts of this method into smaller functions";
            f.confidence = 0.9f;
            f.ruleId = "SMELL001";
            findings.push_back(std::move(f));
        }
        
        ++it;
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectLargeClasses(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    static const std::regex classPattern(R"(\bclass\s+(\w+)[^{]*\{)");
    
    std::sregex_iterator it(content.begin(), content.end(), classPattern);
    std::sregex_iterator end;
    
    while (it != end) {
        size_t classStart = it->position();
        std::string className = (*it)[1].str();
        
        // Count lines until matching }
        int braceCount = 1;
        size_t pos = classStart + it->length();
        uint32_t lineCount = 1;
        
        while (pos < content.size() && braceCount > 0) {
            if (content[pos] == '{') braceCount++;
            else if (content[pos] == '}') braceCount--;
            else if (content[pos] == '\n') lineCount++;
            pos++;
        }
        
        if (lineCount > m_config.maxClassLength) {
            uint32_t lineNum = 1;
            for (size_t i = 0; i < classStart; ++i) {
                if (content[i] == '\n') lineNum++;
            }
            
            CodeSmellFinding f;
            f.type = CodeSmellType::LargeClass;
            f.name = "Large Class";
            f.description = "Class '" + className + "' has " + 
                           std::to_string(lineCount) + " lines (max: " +
                           std::to_string(m_config.maxClassLength) + ")";
            f.location.uri = "";
            f.location.line = lineNum;
            f.severity = m_config.largeClassSeverity;
            f.suggestion = "Consider splitting this class into smaller classes";
            f.confidence = 0.85f;
            f.ruleId = "SMELL002";
            findings.push_back(std::move(f));
        }
        
        ++it;
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectLongParameterLists(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    static const std::regex funcPattern(R"(\b\w+\s+\w+\s*\(([^)]+)\)\s*(?:const\s*)?\{)");
    
    std::sregex_iterator it(content.begin(), content.end(), funcPattern);
    std::sregex_iterator end;
    
    while (it != end) {
        std::string params = (*it)[1].str();
        
        // Count parameters (simplified: count commas + 1)
        uint32_t paramCount = 1;
        for (char c : params) {
            if (c == ',') paramCount++;
        }
        
        if (paramCount > m_config.maxParameters) {
            size_t funcStart = it->position();
            uint32_t lineNum = 1;
            for (size_t i = 0; i < funcStart; ++i) {
                if (content[i] == '\n') lineNum++;
            }
            
            CodeSmellFinding f;
            f.type = CodeSmellType::LongParameterList;
            f.name = "Long Parameter List";
            f.description = "Function has " + std::to_string(paramCount) + 
                           " parameters (max: " + 
                           std::to_string(m_config.maxParameters) + ")";
            f.location.uri = "";
            f.location.line = lineNum;
            f.severity = m_config.longParamsSeverity;
            f.suggestion = "Consider grouping related parameters into a struct";
            f.confidence = 0.9f;
            f.ruleId = "SMELL003";
            findings.push_back(std::move(f));
        }
        
        ++it;
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectDeepNesting(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    uint32_t lineNum = 1;
    uint32_t currentDepth = 0;
    uint32_t maxDepth = 0;
    uint32_t maxDepthLine = 0;
    
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '{') {
            currentDepth++;
            if (currentDepth > maxDepth) {
                maxDepth = currentDepth;
                maxDepthLine = lineNum;
            }
        } else if (content[i] == '}') {
            if (currentDepth > 0) currentDepth--;
        } else if (content[i] == '\n') {
            lineNum++;
        }
    }
    
    if (maxDepth > m_config.maxNestingDepth) {
        CodeSmellFinding f;
        f.type = CodeSmellType::DeepNesting;
        f.name = "Deep Nesting";
        f.description = "Maximum nesting depth is " + std::to_string(maxDepth) + 
                       " (max: " + std::to_string(m_config.maxNestingDepth) + ")";
        f.location.uri = "";
        f.location.line = maxDepthLine;
        f.severity = m_config.deepNestingSeverity;
        f.suggestion = "Use early returns or extract nested logic into methods";
        f.confidence = 0.95f;
        f.ruleId = "SMELL004";
        findings.push_back(std::move(f));
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectMagicNumbers(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    // Find numeric literals that aren't 0, 1, or common constants
    static const std::regex numberPattern(R"(\b([2-9]|[1-9][0-9]+)(?:\.[0-9]+)?\b)");
    
    std::sregex_iterator it(content.begin(), content.end(), numberPattern);
    std::sregex_iterator end;
    
    uint32_t lineNum = 1;
    size_t lastLineStart = 0;
    
    while (it != end) {
        size_t pos = it->position();
        
        // Update line number
        while (lastLineStart < pos) {
            if (content[lastLineStart] == '\n') {
                lineNum++;
            }
            lastLineStart++;
        }
        
        // Skip if in array index or common context
        bool skip = false;
        if (pos > 0) {
            char prev = content[pos - 1];
            if (prev == '[' || prev == '=') skip = true;
        }
        
        if (!skip) {
            CodeSmellFinding f;
            f.type = CodeSmellType::MagicNumber;
            f.name = "Magic Number";
            f.description = "Magic number '" + it->str() + "' should be a named constant";
            f.location.uri = "";
            f.location.line = lineNum;
            f.location.column = pos - lastLineStart + 1;
            f.severity = Severity::Info;
            f.suggestion = "Define a named constant for this value";
            f.confidence = 0.6f;
            f.ruleId = "SMELL005";
            findings.push_back(std::move(f));
        }
        
        ++it;
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectDeadCode(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    // Find unreachable code patterns
    static const std::vector<std::regex> patterns = {
        std::regex(R"(return\s*;[^}]*return)"),  // Return followed by return
        std::regex(R"(throw\s+\w+;[^}]*return)"), // Throw followed by return
        std::regex(R"(break\s*;[^}]*\w+)"),       // Break followed by code
    };
    
    uint32_t lineNum = 1;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') lineNum++;
        
        for (const auto& pattern : patterns) {
            std::smatch match;
            std::string remaining = content.substr(i);
            if (std::regex_search(remaining, match, pattern)) {
                CodeSmellFinding f;
                f.type = CodeSmellType::DeadCode;
                f.name = "Dead Code";
                f.description = "Potentially unreachable code detected";
                f.location.uri = "";
                f.location.line = lineNum;
                f.severity = Severity::Medium;
                f.suggestion = "Remove unreachable code";
                f.confidence = 0.7f;
                f.ruleId = "SMELL006";
                findings.push_back(std::move(f));
                break;
            }
        }
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectComplexConditions(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    // Find complex boolean expressions
    static const std::regex conditionPattern(R"(\bif\s*\(([^)]{50,})\))");
    
    std::sregex_iterator it(content.begin(), content.end(), conditionPattern);
    std::sregex_iterator end;
    
    while (it != end) {
        std::string condition = (*it)[1].str();
        
        // Count operators
        uint32_t opCount = 0;
        for (char c : condition) {
            if (c == '&' || c == '|' || c == '!') opCount++;
        }
        
        if (opCount > 5) {
            size_t pos = it->position();
            uint32_t lineNum = 1;
            for (size_t i = 0; i < pos; ++i) {
                if (content[i] == '\n') lineNum++;
            }
            
            CodeSmellFinding f;
            f.type = CodeSmellType::ComplexCondition;
            f.name = "Complex Condition";
            f.description = "Complex boolean expression with " + 
                           std::to_string(opCount) + " operators";
            f.location.uri = "";
            f.location.line = lineNum;
            f.severity = Severity::Medium;
            f.suggestion = "Simplify the condition or extract into named variables";
            f.confidence = 0.8f;
            f.ruleId = "SMELL007";
            findings.push_back(std::move(f));
        }
        
        ++it;
    }
    
    return findings;
}

std::vector<CodeSmellFinding> CodeSmellDetector::detectNamingIssues(
    const std::string& content,
    const std::string& language) {
    
    std::vector<CodeSmellFinding> findings;
    
    // Find single-letter or very short variable names
    static const std::regex varPattern(R"(\b(?:int|char|float|double|bool|auto|var|let|const)\s+([a-z])\b)");
    
    std::sregex_iterator it(content.begin(), content.end(), varPattern);
    std::sregex_iterator end;
    
    while (it != end) {
        size_t pos = it->position();
        uint32_t lineNum = 1;
        for (size_t i = 0; i < pos; ++i) {
            if (content[i] == '\n') lineNum++;
        }
        
        CodeSmellFinding f;
        f.type = CodeSmellType::Naming;
        f.name = "Poor Naming";
        f.description = "Single-letter variable name '" + (*it)[1].str() + "'";
        f.location.uri = "";
        f.location.line = lineNum;
        f.severity = Severity::Info;
        f.suggestion = "Use descriptive variable names";
        f.confidence = 0.7f;
        f.ruleId = "SMELL008";
        findings.push_back(std::move(f));
        
        ++it;
    }
    
    return findings;
}

} // namespace RawrXD::CodeReview
