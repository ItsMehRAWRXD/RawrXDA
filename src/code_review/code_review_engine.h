// ============================================================================
// code_review_engine.h — AI-Powered Code Review Engine
// ============================================================================
// Intelligent code review with AI-powered suggestions, complexity analysis,
// technical debt tracking, and integration with security analyzer.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "code_review/security_analyzer.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>

namespace RawrXD::CodeReview {

// ---------------------------------------------------------------------------
// Code Quality Metrics
// ---------------------------------------------------------------------------
struct QualityMetrics {
    // Complexity
    uint32_t cyclomaticComplexity = 0;    // McCabe complexity
    uint32_t cognitiveComplexity = 0;      // Human-readable complexity
    uint32_t nestingDepth = 0;             // Maximum nesting level
    
    // Size
    uint32_t linesOfCode = 0;              // LOC (excluding comments/blanks)
    uint32_t totalLines = 0;               // Total lines including blanks
    uint32_t commentLines = 0;             // Comment lines
    uint32_t blankLines = 0;                // Blank lines
    float commentRatio = 0.0f;             // Comments / LOC
    
    // Structure
    uint32_t functionCount = 0;            // Number of functions
    uint32_t classCount = 0;               // Number of classes
    uint32_t avgFunctionLength = 0;        // Average function LOC
    uint32_t maxFunctionLength = 0;        // Longest function LOC
    uint32_t avgParamsPerFunction = 0;     // Average parameter count
    uint32_t maxParamsPerFunction = 0;     // Max parameter count
    
    // Maintainability
    float maintainabilityIndex = 0.0f;     // 0-100 scale
    uint32_t halsteadVolume = 0;           // Halstead volume
    uint32_t halsteadDifficulty = 0;       // Halstead difficulty
    
    // Duplications
    uint32_t duplicateBlocks = 0;          // Duplicate code blocks
    uint32_t duplicateLines = 0;           // Lines of duplicated code
    float duplicationRatio = 0.0f;        // Duplicate / Total
    
    // Technical Debt
    uint32_t codeSmells = 0;               // Code smell count
    uint32_t todoCount = 0;                // TODO/FIXME count
    uint32_t deprecatedCount = 0;          // Deprecated usage count
    float technicalDebtMinutes = 0.0f;     // Estimated debt in minutes
};

// ---------------------------------------------------------------------------
// Code Smell Types
// ---------------------------------------------------------------------------
enum class CodeSmellType : uint8_t {
    LongMethod = 1,           // Method too long
    LargeClass = 2,           // Class too large
    LongParameterList = 3,    // Too many parameters
    DuplicateCode = 4,        // Code duplication
    DeadCode = 5,             // Unreachable code
    MagicNumber = 6,          // Unnamed constants
    DeepNesting = 7,          // Excessive nesting
    ComplexCondition = 8,     // Complex boolean expression
    GodClass = 9,             // Class doing too much
    FeatureEnvy = 10,          // Method using another class heavily
    DataClump = 11,           // Same data items together often
    PrimitiveObsession = 12,  // Overuse of primitives
    SwitchStatements = 13,    // Switch instead of polymorphism
    SpeculativeGenerality = 14, // Unused abstraction
    MiddleMan = 15,           // Unnecessary delegation
    InappropriateIntimacy = 16, // Classes too coupled
    AlternativeClasses = 17,  // Interchangeable classes
    RefusedBequest = 18,      // Unused inheritance
    Comments = 19,            // Excessive comments
    Naming = 20,              // Poor naming
};

// ---------------------------------------------------------------------------
// Code Smell Finding
// ---------------------------------------------------------------------------
struct CodeSmellFinding {
    uint64_t id = 0;
    CodeSmellType type;
    std::string name;
    std::string description;
    SourceLocation location;
    Severity severity;
    std::string suggestion;
    float confidence = 0.0f;
    std::string ruleId;
};

// ---------------------------------------------------------------------------
// Review Suggestion
// ---------------------------------------------------------------------------
struct ReviewSuggestion {
    uint64_t id = 0;
    std::string title;
    std::string description;
    SourceLocation location;
    std::string originalCode;
    std::string suggestedCode;
    std::string explanation;
    std::string category;          // "style", "performance", "security", "maintainability"
    Severity severity;
    float confidence = 0.0f;
    bool isAutoFixable = false;
    std::string autoFixId;
};

// ---------------------------------------------------------------------------
// Review Comment
// ---------------------------------------------------------------------------
struct ReviewComment {
    uint64_t id = 0;
    std::string author;
    std::string content;
    SourceLocation location;
    int64_t timestamp = 0;
    bool isResolved = false;
    std::string resolution;
    int64_t resolvedAt = 0;
    std::string resolvedBy;
};

// ---------------------------------------------------------------------------
// Review Result
// ---------------------------------------------------------------------------
struct ReviewResult {
    std::string uri;
    QualityMetrics metrics;
    std::vector<CodeSmellFinding> codeSmells;
    std::vector<ReviewSuggestion> suggestions;
    std::vector<VulnerabilityFinding> securityFindings;
    std::vector<ReviewComment> comments;
    
    // Summary
    uint32_t totalIssues = 0;
    uint32_t criticalIssues = 0;
    uint32_t highIssues = 0;
    uint32_t mediumIssues = 0;
    uint32_t lowIssues = 0;
    uint32_t infoIssues = 0;
    
    float overallScore = 0.0f;       // 0-100 quality score
    float securityScore = 0.0f;      // 0-100 security score
    float maintainabilityScore = 0.0f; // 0-100 maintainability score
    
    int64_t reviewTimeMs = 0;
    bool success = false;
    std::string errorMessage;
};

// ---------------------------------------------------------------------------
// Review Configuration
// ---------------------------------------------------------------------------
struct ReviewConfig {
    bool enableSecurityAnalysis = true;
    bool enableCodeSmells = true;
    bool enableSuggestions = true;
    bool enableMetrics = true;
    bool enableDuplicationDetection = true;
    
    // Thresholds
    uint32_t maxFunctionLength = 50;      // Lines
    uint32_t maxClassLength = 500;        // Lines
    uint32_t maxParameters = 5;
    uint32_t maxNestingDepth = 4;
    uint32_t maxCyclomaticComplexity = 10;
    uint32_t minCommentRatio = 10;        // Percent
    
    // Severity for threshold violations
    Severity longMethodSeverity = Severity::Medium;
    Severity largeClassSeverity = Severity::Medium;
    Severity longParamsSeverity = Severity::Low;
    Severity deepNestingSeverity = Severity::Medium;
    Severity highComplexitySeverity = Severity::Medium;
    
    // Exclusions
    std::unordered_set<std::string> excludedRules;
    std::unordered_set<std::string> excludedFiles;
};

// ---------------------------------------------------------------------------
// Code Review Engine
// ---------------------------------------------------------------------------
class CodeReviewEngine {
public:
    explicit CodeReviewEngine(SecurityAnalyzer* securityAnalyzer = nullptr);
    ~CodeReviewEngine();
    
    // Configuration
    void setConfig(const ReviewConfig& config);
    ReviewConfig getConfig() const;
    
    // Review
    ReviewResult reviewFile(const std::string& uri, 
                             const std::string& content,
                             const std::string& language);
    ReviewResult reviewFile(const std::string& uri);
    
    std::vector<ReviewResult> reviewProject(
        const std::vector<std::string>& uris);
    
    // Metrics
    QualityMetrics calculateMetrics(const std::string& content,
                                    const std::string& language);
    
    // Code smell detection
    std::vector<CodeSmellFinding> detectCodeSmells(
        const std::string& content,
        const std::string& language);
    
    // Suggestions
    std::vector<ReviewSuggestion> generateSuggestions(
        const std::string& content,
        const std::string& language,
        const std::vector<CodeSmellFinding>& smells,
        const std::vector<VulnerabilityFinding>& vulns);
    
    // Duplication detection
    std::vector<std::pair<SourceLocation, SourceLocation>> findDuplicates(
        const std::string& content,
        uint32_t minLines = 5);
    
    // Scoring
    float calculateOverallScore(const ReviewResult& result) const;
    float calculateSecurityScore(const ReviewResult& result) const;
    float calculateMaintainabilityScore(const ReviewResult& result) const;
    
    // Statistics
    struct ReviewStats {
        uint64_t filesReviewed = 0;
        uint64_t totalIssues = 0;
        uint64_t securityIssues = 0;
        uint64_t codeSmells = 0;
        uint64_t suggestions = 0;
        int64_t totalReviewTimeMs = 0;
        double avgReviewTimeMs = 0.0;
        float avgQualityScore = 0.0f;
        float avgSecurityScore = 0.0f;
        float avgMaintainabilityScore = 0.0f;
    };
    
    ReviewStats getStats() const;
    void resetStats();
    
    // Export
    std::string exportToJson(const std::vector<ReviewResult>& results) const;
    std::string exportToMarkdown(const std::vector<ReviewResult>& results) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ---------------------------------------------------------------------------
// Complexity Calculator
// ---------------------------------------------------------------------------
class ComplexityCalculator {
public:
    // Cyclomatic complexity (McCabe)
    static uint32_t calculateCyclomatic(const std::string& content,
                                        const std::string& language);
    
    // Cognitive complexity (human-readable)
    static uint32_t calculateCognitive(const std::string& content,
                                       const std::string& language);
    
    // Nesting depth
    static uint32_t calculateNestingDepth(const std::string& content,
                                          const std::string& language);
    
    // Halstead metrics
    struct HalsteadMetrics {
        uint32_t vocabulary = 0;      // Unique operators + operands
        uint32_t length = 0;          // Total operators + operands
        uint32_t volume = 0;          // length * log2(vocabulary)
        uint32_t difficulty = 0;      // (n1/2) * (N2/n2)
        uint32_t effort = 0;           // difficulty * volume
        uint32_t bugs = 0;             // volume / 3000 (estimated)
    };
    
    static HalsteadMetrics calculateHalstead(const std::string& content,
                                             const std::string& language);
    
    // Maintainability index
    static float calculateMaintainabilityIndex(
        uint32_t cyclomatic,
        uint32_t halsteadVolume,
        uint32_t loc);
};

// ---------------------------------------------------------------------------
// Code Smell Detector
// ---------------------------------------------------------------------------
class CodeSmellDetector {
public:
    explicit CodeSmellDetector(const ReviewConfig& config);
    
    std::vector<CodeSmellFinding> detect(const std::string& content,
                                         const std::string& language);
    
private:
    std::vector<CodeSmellFinding> detectLongMethods(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectLargeClasses(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectLongParameterLists(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectDeepNesting(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectMagicNumbers(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectDeadCode(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectComplexConditions(
        const std::string& content,
        const std::string& language);
    
    std::vector<CodeSmellFinding> detectNamingIssues(
        const std::string& content,
        const std::string& language);
    
    ReviewConfig m_config;
};

} // namespace RawrXD::CodeReview
