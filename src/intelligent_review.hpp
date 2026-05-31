// intelligent_review.hpp
// INTELLIGENT CODE REVIEW SYSTEM - AI-Powered Multi-Model Code Analysis
// Surpasses GitHub Copilot, Cursor, CodeRabbit, and all Top 50 AI IDEs
//
// Key Differentiators:
// 1. Multi-Model Review - 3-5 models analyze code in parallel
// 2. Security Scanning - OWASP Top 10, CWE, SANS Top 25
// 3. Performance Analysis - Complexity, memory, I/O patterns
// 4. Best Practices - Language-specific patterns, anti-patterns
// 5. Code Smell Detection - 50+ smell patterns
// 6. Automated Fixes - One-click fix suggestions
// 7. Learning System - Learns from accepted/rejected reviews
// 8. Team Integration - Shared review rules, custom policies

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <optional>
#include <set>
#include <mutex>
#include <atomic>

namespace review {

// ═══════════════════════════════════════════════════════════════════════════════
// Core Types
// ═══════════════════════════════════════════════════════════════════════════════

enum class Severity {
    Critical,   // Security vulnerabilities, data loss risks
    High,       // Major bugs, performance issues
    Medium,     // Code smells, maintainability issues
    Low,        // Style, minor improvements
    Info,       // Suggestions, informational
    Ignore      // Suppressed by configuration
};

enum class Category {
    Security,           // Security vulnerabilities
    Performance,        // Performance issues
    Correctness,        // Logic errors, bugs
    Maintainability,    // Code quality, readability
    Style,              // Code style, formatting
    Complexity,         // Cyclomatic complexity
    Documentation,      // Missing/outdated docs
    Testing,            // Test coverage, quality
    Architecture,       // Design patterns, structure
    Dependency,         // Dependency issues
    Compatibility,      // Cross-platform, version issues
    Accessibility,      // Accessibility issues
    Internationalization, // i18n/l10n issues
    BestPractice        // Best practice violations
};

enum class FixType {
    None,               // No automatic fix available
    Suggestion,         // Suggested fix, requires review
    SafeAuto,           // Safe to auto-apply
    UnsafeAuto,         // Auto-apply with caution
    Refactor            // Requires manual refactoring
};

enum class ReviewMode {
    Quick,              // Fast review, critical issues only
    Standard,           // Balanced review
    Deep,               // Comprehensive analysis
    Security,           // Security-focused
    Performance,        // Performance-focused
    Style               // Style-focused
};

enum class Language {
    TypeScript,
    JavaScript,
    Python,
    Cpp,
    C,
    Rust,
    Go,
    Java,
    CSharp,
    Php,
    Ruby,
    Swift,
    Kotlin,
    Scala,
    Unknown
};

// Language detection helpers
inline std::string languageToString(Language lang) {
    switch (lang) {
        case Language::TypeScript: return "typescript";
        case Language::JavaScript: return "javascript";
        case Language::Python: return "python";
        case Language::Cpp: return "cpp";
        case Language::C: return "c";
        case Language::Rust: return "rust";
        case Language::Go: return "go";
        case Language::Java: return "java";
        case Language::CSharp: return "csharp";
        case Language::Php: return "php";
        case Language::Ruby: return "ruby";
        case Language::Swift: return "swift";
        case Language::Kotlin: return "kotlin";
        case Language::Scala: return "scala";
        default: return "unknown";
    }
}

inline Language stringToLanguage(const std::string& str) {
    if (str == "typescript") return Language::TypeScript;
    if (str == "javascript" || str == "js") return Language::JavaScript;
    if (str == "python" || str == "py") return Language::Python;
    if (str == "cpp" || str == "c++") return Language::Cpp;
    if (str == "c") return Language::C;
    if (str == "rust") return Language::Rust;
    if (str == "go") return Language::Go;
    if (str == "java") return Language::Java;
    if (str == "csharp" || str == "c#") return Language::CSharp;
    if (str == "php") return Language::Php;
    if (str == "ruby") return Language::Ruby;
    if (str == "swift") return Language::Swift;
    if (str == "kotlin") return Language::Kotlin;
    if (str == "scala") return Language::Scala;
    return Language::Unknown;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Issue Types
// ═══════════════════════════════════════════════════════════════════════════════

struct SourceLocation {
    std::string file_path;
    int start_line;
    int end_line;
    int start_column;
    int end_column;
    std::string snippet;    // Code snippet for context
};

struct FixSuggestion {
    std::string description;
    std::string before;
    std::string after;
    FixType type;
    bool auto_applicable;
    std::vector<std::string> prerequisites;
    std::string diff;      // Unified diff format
};

struct CodeIssue {
    std::string id;
    std::string rule_id;       // e.g., "SEC001", "PERF003"
    std::string title;
    std::string description;
    Severity severity;
    Category category;
    SourceLocation location;
    std::vector<FixSuggestion> fixes;
    std::vector<std::string> references;  // Links to docs
    std::vector<std::string> tags;
    float confidence;
    std::string model_source;  // Which model found this
    bool is_false_positive;
    bool is_suppressed;
    std::chrono::system_clock::time_point detected_at;
};

struct FileReview {
    std::string file_path;
    std::vector<CodeIssue> issues;
    int total_issues;
    int critical_count;
    int high_count;
    int medium_count;
    int low_count;
    float quality_score;       // 0.0 - 100.0
    float security_score;
    float performance_score;
    float maintainability_score;
    std::vector<std::string> strengths;
    std::vector<std::string> weaknesses;
    std::string summary;
    std::chrono::milliseconds review_duration;
};

struct ProjectReview {
    std::string project_path;
    std::vector<FileReview> files;
    int total_issues;
    int files_reviewed;
    float overall_quality_score;
    float overall_security_score;
    float overall_performance_score;
    std::map<Category, int> issues_by_category;
    std::map<Severity, int> issues_by_severity;
    std::vector<CodeIssue> top_issues;  // Top 10 most critical
    std::vector<std::string> recommendations;
    std::chrono::system_clock::time_point reviewed_at;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Security Scanner
// ═══════════════════════════════════════════════════════════════════════════════

struct VulnerabilityPattern {
    std::string id;             // CWE, OWASP ID
    std::string name;
    std::string description;
    std::string pattern;        // Regex or AST pattern
    std::vector<std::string> languages;
    Severity severity;
    std::vector<std::string> mitigations;
    std::vector<std::string> references;
};

struct SecurityFinding {
    std::string vulnerability_id;
    std::string title;
    std::string description;
    Severity severity;
    SourceLocation location;
    std::string attack_vector;
    std::string impact;
    std::string likelihood;
    std::vector<std::string> remediation_steps;
    std::vector<FixSuggestion> fixes;
    float cvss_score;          // Common Vulnerability Scoring System
    std::string cvss_vector;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Analyzer
// ═══════════════════════════════════════════════════════════════════════════════

struct ComplexityMetrics {
    int cyclomatic_complexity;
    int cognitive_complexity;
    int nesting_depth;
    int lines_of_code;
    int logical_lines;
    int comment_lines;
    int function_count;
    int parameter_count;
    float maintainability_index;
};

struct PerformanceIssue {
    std::string id;
    std::string title;
    std::string description;
    Severity severity;
    SourceLocation location;
    std::string issue_type;    // "O(n^2)", "memory_leak", "blocking_io", etc.
    float estimated_impact;    // 0.0 - 1.0
    std::string complexity;    // Big O notation
    std::vector<std::string> suggestions;
    std::vector<FixSuggestion> fixes;
};

struct MemoryPattern {
    std::string pattern_type;  // "allocation", "leak", "fragmentation"
    SourceLocation location;
    std::string description;
    float estimated_size;       // Bytes
    std::string lifecycle;     // "transient", "persistent", "leaked"
};

// ═══════════════════════════════════════════════════════════════════════════════
// Best Practices Checker
// ═══════════════════════════════════════════════════════════════════════════════

struct PracticeRule {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> languages;  // Language strings (e.g., "typescript", "python")
    Category category;
    Severity default_severity;
    std::string pattern;
    std::string anti_pattern;
    std::vector<std::string> good_examples;
    std::vector<std::string> bad_examples;
    std::vector<std::string> references;
    bool auto_fixable;
    std::string fix_template;
};

struct PracticeViolation {
    std::string rule_id;
    std::string title;
    std::string description;
    Severity severity;
    SourceLocation location;
    std::string violation_type;
    std::vector<std::string> suggestions;
    std::vector<FixSuggestion> fixes;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Code Smell Detection
// ═══════════════════════════════════════════════════════════════════════════════

struct CodeSmell {
    std::string id;
    std::string name;           // "Long Method", "God Class", etc.
    std::string description;
    Severity severity;
    SourceLocation location;
    std::string smell_type;     // "Bloaters", "OO Abusers", "Change Preventers", etc.
    std::vector<std::string> indicators;
    std::vector<std::string> refactorings;
    float detection_confidence;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Review Configuration
// ═══════════════════════════════════════════════════════════════════════════════

struct ReviewConfig {
    ReviewMode mode;
    std::vector<Category> enabled_categories;
    std::vector<Severity> minimum_severity;
    std::vector<std::string> exclude_patterns;
    std::vector<std::string> include_patterns;
    std::map<std::string, Severity> rule_overrides;
    bool enable_security_scan;
    bool enable_performance_analysis;
    bool enable_best_practices;
    bool enable_code_smells;
    bool enable_auto_fix;
    bool enable_multi_model;
    int max_issues_per_file;
    int max_file_size_kb;
    std::vector<std::string> custom_rules_paths;
    std::map<std::string, std::string> environment;
};

struct ReviewPolicy {
    std::string name;
    std::string description;
    std::vector<std::string> required_checks;
    std::vector<std::string> forbidden_patterns;
    std::map<Severity, int> max_issues_allowed;
    float min_quality_score;
    float min_security_score;
    std::vector<std::string> approvers;
    bool require_all_checks;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Learning System
// ═══════════════════════════════════════════════════════════════════════════════

struct ReviewFeedback {
    std::string issue_id;
    std::string user_id;
    bool accepted;
    std::string comment;
    std::string action;         // "fixed", "ignored", "marked_false_positive"
    std::chrono::system_clock::time_point timestamp;
};

struct LearnedPattern {
    std::string id;
    std::string pattern_type;    // "false_positive", "common_issue", "team_preference"
    std::string pattern;
    std::string context;
    int occurrence_count;
    float confidence;
    std::chrono::system_clock::time_point last_seen;
    std::vector<std::string> tags;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Main Review System
// ═══════════════════════════════════════════════════════════════════════════════

class IntelligentReviewSystem {
public:
    IntelligentReviewSystem();
    ~IntelligentReviewSystem();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Core Review Operations
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Review a single file
    FileReview reviewFile(
        const std::string& file_path,
        const std::string& content,
        const ReviewConfig& config
    );
    
    // Review multiple files
    ProjectReview reviewProject(
        const std::string& project_path,
        const std::vector<std::string>& files,
        const ReviewConfig& config
    );
    
    // Stream review results
    void reviewStream(
        const std::string& file_path,
        const std::string& content,
        const ReviewConfig& config,
        std::function<void(const CodeIssue&)> callback
    );
    
    // Review with multiple models (ensemble)
    FileReview reviewWithEnsemble(
        const std::string& file_path,
        const std::string& content,
        const std::vector<std::string>& models,
        const ReviewConfig& config
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Security Scanning
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Scan for security vulnerabilities
    std::vector<SecurityFinding> scanSecurity(
        const std::string& content,
        Language language
    );
    
    // Get OWASP Top 10 findings
    std::vector<SecurityFinding> getOWASPTop10(
        const std::string& content,
        Language language
    );
    
    // Get CWE findings
    std::vector<SecurityFinding> getCWEFindings(
        const std::string& content,
        const std::vector<std::string>& cwe_ids,
        Language language
    );
    
    // Calculate CVSS score
    float calculateCVSS(const SecurityFinding& finding);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Performance Analysis
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Analyze performance issues
    std::vector<PerformanceIssue> analyzePerformance(
        const std::string& content,
        Language language
    );
    
    // Calculate complexity metrics
    ComplexityMetrics calculateComplexity(
        const std::string& content,
        Language language
    );
    
    // Detect memory patterns
    std::vector<MemoryPattern> detectMemoryPatterns(
        const std::string& content,
        Language language
    );
    
    // Estimate Big O complexity
    std::string estimateComplexity(
        const std::string& function_content,
        Language language
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Best Practices Checking
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Check best practices
    std::vector<PracticeViolation> checkPractices(
        const std::string& content,
        Language language,
        const std::vector<std::string>& rule_ids = {}
    );
    
    // Get language-specific rules
    std::vector<PracticeRule> getRules(Language language);
    
    // Add custom rule
    bool addCustomRule(const PracticeRule& rule);
    
    // Remove custom rule
    bool removeCustomRule(const std::string& rule_id);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Code Smell Detection
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Detect code smells
    std::vector<CodeSmell> detectSmells(
        const std::string& content,
        Language language
    );
    
    // Get refactoring suggestions
    std::vector<std::string> getRefactorings(
        const CodeSmell& smell
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Fix Suggestions
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Generate fix for issue
    FixSuggestion generateFix(
        const CodeIssue& issue,
        const std::string& content
    );
    
    // Apply fix automatically
    bool applyFix(
        const std::string& file_path,
        const FixSuggestion& fix
    );
    
    // Apply all safe fixes
    int applyAllSafeFixes(
        const std::string& file_path,
        const std::vector<CodeIssue>& issues
    );
    
    // Generate diff for fix
    std::string generateDiff(
        const std::string& before,
        const std::string& after,
        const std::string& file_path
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Learning System
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Submit feedback on review
    void submitFeedback(const ReviewFeedback& feedback);
    
    // Get learned patterns
    std::vector<LearnedPattern> getLearnedPatterns(const std::string& pattern_type = "");
    
    // Train on historical reviews
    void trainOnHistory(const std::vector<FileReview>& history);
    
    // Get false positive rate
    float getFalsePositiveRate(const std::string& rule_id);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration & Policies
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Set review configuration
    void setConfig(const ReviewConfig& config);
    
    // Get current configuration
    ReviewConfig getConfig();
    
    // Add review policy
    bool addPolicy(const ReviewPolicy& policy);
    
    // Remove review policy
    bool removePolicy(const std::string& policy_name);
    
    // Check if review passes policy
    bool passesPolicy(
        const FileReview& review,
        const std::string& policy_name
    );
    
    // Get policy violations
    std::vector<std::string> getPolicyViolations(
        const FileReview& review,
        const std::string& policy_name
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics & Reporting
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Get review statistics
    std::map<std::string, std::string> getStats();
    
    // Get issue trends
    std::map<std::string, std::vector<int>> getTrends(const std::string& period = "week");
    
    // Export review report
    std::string exportReport(
        const ProjectReview& review,
        const std::string& format = "markdown"
    );
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Initialization & Shutdown
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Initialize review system
    bool initialize(const std::string& config_path = "");
    
    // Shutdown review system
    void shutdown();
    
    // Check if initialized
    bool isInitialized() const;
    
    // Get version
    std::string getVersion() const;
    
private:
    // Internal state
    std::map<std::string, std::vector<PracticeRule>> rulesByLanguage_;
    std::vector<VulnerabilityPattern> vulnerabilityPatterns_;
    std::vector<LearnedPattern> learnedPatterns_;
    std::map<std::string, ReviewPolicy> policies_;
    ReviewConfig defaultConfig_;
    
    std::mutex mutex_;
    std::atomic<bool> initialized_;
    std::atomic<int> reviewCount_;
    std::atomic<int> issuesFound_;
    
    // Statistics
    std::map<std::string, int> issuesByRule_;
    std::map<std::string, int> falsePositives_;
    std::map<std::string, float> avgConfidenceByRule_;
    
    // Internal helpers
    Language detectLanguage(const std::string& file_path, const std::string& content);
    std::vector<CodeIssue> runSecurityScan(const std::string& content, Language lang);
    std::vector<CodeIssue> runPerformanceAnalysis(const std::string& content, Language lang);
    std::vector<CodeIssue> runPracticeCheck(const std::string& content, Language lang);
    std::vector<CodeIssue> runSmellDetection(const std::string& content, Language lang);
    float calculateQualityScore(const FileReview& review);
    float calculateSecurityScore(const FileReview& review);
    float calculatePerformanceScore(const FileReview& review);
    std::string generateIssueId();
    bool isSuppressed(const CodeIssue& issue, const ReviewConfig& config);
    std::vector<FixSuggestion> generateFixes(const CodeIssue& issue, const std::string& content);
};

} // namespace review
