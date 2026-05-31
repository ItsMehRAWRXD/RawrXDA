// ============================================================================
// security_analyzer.h — AI-Powered Security Analysis Engine
// ============================================================================
// Static analysis for vulnerability detection: SQL injection, XSS, buffer
// overflows, path traversal, command injection, cryptographic issues, and
// more. Integrates with LSP symbol index for cross-file analysis.
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp/workspace_symbol_index.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <functional>
#include <regex>
#include <cstdint>

namespace RawrXD::CodeReview {

// ---------------------------------------------------------------------------
// Vulnerability Severity Levels
// ---------------------------------------------------------------------------
enum class Severity : uint8_t {
    Info = 0,       // Informational, best practice suggestion
    Low = 1,        // Low risk, minor issue
    Medium = 2,     // Medium risk, should be addressed
    High = 3,       // High risk, security concern
    Critical = 4,   // Critical, immediate action required
};

// ---------------------------------------------------------------------------
// Vulnerability Categories (CWE-aligned)
// ---------------------------------------------------------------------------
enum class VulnerabilityCategory : uint16_t {
    // Injection
    SQLInjection = 89,              // CWE-89: SQL Injection
    CommandInjection = 78,          // CWE-78: OS Command Injection
    PathTraversal = 22,             // CWE-22: Path Traversal
    XSS = 79,                       // CWE-79: Cross-site Scripting
    LDAPInjection = 90,             // CWE-90: LDAP Injection
    XMLInjection = 91,              // CWE-91: XML Injection
    
    // Memory Safety
    BufferOverflow = 119,           // CWE-119: Buffer Overflow
    UseAfterFree = 416,            // CWE-416: Use After Free
    DoubleFree = 415,              // CWE-415: Double Free
    NullPointerDeref = 476,        // CWE-476: NULL Pointer Dereference
    IntegerOverflow = 190,         // CWE-190: Integer Overflow
    OutOfBoundsRead = 125,         // CWE-125: Out-of-bounds Read
    OutOfBoundsWrite = 787,        // CWE-787: Out-of-bounds Write
    
    // Cryptographic
    WeakCrypto = 327,               // CWE-327: Weak Crypto
    HardcodedSecret = 798,         // CWE-798: Hardcoded Credentials
    InsecureRandom = 338,          // CWE-338: Weak PRNG
    MissingAuth = 306,             // CWE-306: Missing Authentication
    
    // Configuration
    Misconfiguration = 16,         // CWE-16: Configuration
    DebugEnabled = 215,            // CWE-215: Debug Info Exposure
    SensitiveDataExposure = 200,   // CWE-200: Information Exposure
    
    // Input Validation
    ImproperValidation = 20,       // CWE-20: Input Validation
    TypeConfusion = 843,           // CWE-843: Type Confusion
    FormatString = 134,           // CWE-134: Format String
    
    // Race Conditions
    RaceCondition = 362,          // CWE-362: Race Condition
    TOCTOU = 367,                  // CWE-367: Time-of-check Time-of-use
    
    // Other
    CodeQuality = 0,               // General code quality issue
    Performance = 1,                // Performance concern
    Maintainability = 2,            // Maintainability issue
};

// ---------------------------------------------------------------------------
// Source Location
// ---------------------------------------------------------------------------
struct SourceLocation {
    std::string uri;               // File URI
    uint32_t line = 0;             // 1-based line number
    uint32_t column = 0;           // 1-based column
    uint32_t endLine = 0;          // End line (for multi-line)
    uint32_t endColumn = 0;        // End column
    
    std::string snippet;           // Source code snippet
    std::string function;          // Containing function
    std::string className;         // Containing class
};

// ---------------------------------------------------------------------------
// Vulnerability Finding
// ---------------------------------------------------------------------------
struct VulnerabilityFinding {
    uint64_t id = 0;                        // Unique finding ID
    VulnerabilityCategory category;         // Vulnerability type
    Severity severity;                      // Risk level
    std::string title;                       // Short description
    std::string description;                 // Detailed explanation
    SourceLocation location;                 // Where found
    std::string cwe;                         // CWE identifier (e.g., "CWE-89")
    std::string owasp;                       // OWASP category if applicable
    std::string remediation;                  // How to fix
    std::vector<std::string> references;      // External references
    float confidence = 0.0f;                  // Detection confidence (0-1)
    bool isFalsePositive = false;             // User-marked false positive
    bool isSuppressed = false;               // Suppressed by config
    std::string suppressionReason;           // Why suppressed
    std::vector<std::string> relatedFindings; // IDs of related findings
    std::string ruleId;                      // Rule that triggered this
};

// ---------------------------------------------------------------------------
// Analysis Context
// ---------------------------------------------------------------------------
struct AnalysisContext {
    std::string uri;                         // File being analyzed
    std::string content;                     // Full file content
    std::vector<std::string> lines;          // Content split by lines
    std::string language;                    // Language ID (cpp, js, py, etc.)
    LSP::WorkspaceSymbolIndex* symbolIndex;  // Symbol index (optional)
    std::unordered_map<std::string, std::string> config; // Analysis config
    std::unordered_set<std::string> suppressedRules; // Suppressed rule IDs
    uint32_t maxFindings = 1000;             // Max findings per file
    bool includeInfo = true;                 // Include Info severity
    bool includeLow = true;                  // Include Low severity
};

// ---------------------------------------------------------------------------
// Analysis Result
// ---------------------------------------------------------------------------
struct AnalysisResult {
    std::string uri;                         // Analyzed file
    std::vector<VulnerabilityFinding> findings;
    uint32_t totalFindings = 0;
    uint32_t criticalCount = 0;
    uint32_t highCount = 0;
    uint32_t mediumCount = 0;
    uint32_t lowCount = 0;
    uint32_t infoCount = 0;
    uint32_t suppressedCount = 0;
    int64_t analysisTimeMs = 0;
    bool success = false;
    std::string errorMessage;
};

// ---------------------------------------------------------------------------
// Security Rule Interface
// ---------------------------------------------------------------------------
class ISecurityRule {
public:
    virtual ~ISecurityRule() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual VulnerabilityCategory category() const = 0;
    virtual Severity defaultSeverity() const = 0;
    virtual std::vector<std::string> languages() const = 0;
    virtual std::vector<VulnerabilityFinding> analyze(
        const AnalysisContext& ctx) = 0;
    virtual bool isEnabled(const std::unordered_set<std::string>& suppressed) const {
        return suppressed.find(id()) == suppressed.end();
    }
};

// ---------------------------------------------------------------------------
// Security Analyzer Configuration
// ---------------------------------------------------------------------------
struct SecurityAnalyzerConfig {
    bool enableAllRules = true;
    std::unordered_set<std::string> enabledRules;
    std::unordered_set<std::string> disabledRules;
    std::unordered_set<std::string> suppressedRules;
    uint32_t maxFindingsPerFile = 1000;
    uint32_t maxFindingsPerRule = 100;
    float minConfidence = 0.5f;              // Minimum confidence threshold
    bool includeInfo = true;
    bool includeLow = true;
    bool crossFileAnalysis = true;          // Enable cross-file analysis
    bool useSymbolIndex = true;              // Use LSP symbol index
    std::unordered_map<std::string, Severity> severityOverrides; // Rule-specific severity
};

// ---------------------------------------------------------------------------
// Security Analyzer Statistics
// ---------------------------------------------------------------------------
struct SecurityAnalyzerStats {
    uint64_t filesAnalyzed = 0;
    uint64_t totalFindings = 0;
    uint64_t criticalFindings = 0;
    uint64_t highFindings = 0;
    uint64_t mediumFindings = 0;
    uint64_t lowFindings = 0;
    uint64_t infoFindings = 0;
    uint64_t suppressedFindings = 0;
    uint64_t falsePositives = 0;
    int64_t totalAnalysisTimeMs = 0;
    double avgAnalysisTimeMs = 0.0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
};

// ---------------------------------------------------------------------------
// Security Analyzer Engine
// ---------------------------------------------------------------------------
class SecurityAnalyzer {
public:
    explicit SecurityAnalyzer(LSP::WorkspaceSymbolIndex* index = nullptr);
    ~SecurityAnalyzer();

    // Configuration
    void setConfig(const SecurityAnalyzerConfig& config);
    SecurityAnalyzerConfig getConfig() const;
    
    // Rule management
    void registerRule(std::unique_ptr<ISecurityRule> rule);
    void unregisterRule(const std::string& ruleId);
    std::vector<std::string> getAvailableRules() const;
    std::vector<std::string> getRulesForLanguage(const std::string& lang) const;
    
    // Analysis
    AnalysisResult analyzeFile(const std::string& uri, 
                                const std::string& content,
                                const std::string& language);
    AnalysisResult analyzeFile(const std::string& uri); // Load from disk
    
    std::vector<AnalysisResult> analyzeProject(
        const std::vector<std::string>& uris);
    
    // Cross-file analysis
    std::vector<VulnerabilityFinding> analyzeDataFlow(
        const std::string& sourceUri,
        const SourceLocation& source,
        const std::string& sinkUri,
        const SourceLocation& sink);
    
    // Taint tracking
    struct TaintSource {
        std::string name;
        SourceLocation location;
        std::string description;
    };
    
    struct TaintSink {
        std::string name;
        SourceLocation location;
        VulnerabilityCategory vulnCategory;
    };
    
    std::vector<TaintSource> findTaintSources(const AnalysisContext& ctx);
    std::vector<TaintSink> findTaintSinks(const AnalysisContext& ctx);
    bool isTaintPath(const TaintSource& source, const TaintSink& sink,
                     const AnalysisContext& ctx);
    
    // Finding management
    std::vector<VulnerabilityFinding> getAllFindings() const;
    std::vector<VulnerabilityFinding> getFindingsByFile(const std::string& uri) const;
    std::vector<VulnerabilityFinding> getFindingsByCategory(
        VulnerabilityCategory cat) const;
    std::vector<VulnerabilityFinding> getFindingsBySeverity(
        Severity sev) const;
    
    void markFalsePositive(uint64_t findingId, const std::string& reason);
    void suppressFinding(uint64_t findingId, const std::string& reason);
    void unsuppressFinding(uint64_t findingId);
    
    // Statistics
    SecurityAnalyzerStats getStats() const;
    void resetStats();
    void clearCache();
    
    // Export
    std::string exportToJson(const std::vector<AnalysisResult>& results) const;
    std::string exportToSarif(const std::vector<AnalysisResult>& results) const;
    std::string exportToHtml(const std::vector<AnalysisResult>& results) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ---------------------------------------------------------------------------
// Built-in Security Rules (Factory Functions)
// ---------------------------------------------------------------------------
namespace Rules {

// SQL Injection Detection
std::unique_ptr<ISecurityRule> createSQLInjectionRule();

// Command Injection Detection
std::unique_ptr<ISecurityRule> createCommandInjectionRule();

// Path Traversal Detection
std::unique_ptr<ISecurityRule> createPathTraversalRule();

// XSS Detection
std::unique_ptr<ISecurityRule> createXSSRule();

// Buffer Overflow Detection (C/C++)
std::unique_ptr<ISecurityRule> createBufferOverflowRule();

// Hardcoded Credentials
std::unique_ptr<ISecurityRule> createHardcodedCredentialsRule();

// Weak Cryptography
std::unique_ptr<ISecurityRule> createWeakCryptoRule();

// Format String Vulnerability
std::unique_ptr<ISecurityRule> createFormatStringRule();

// Integer Overflow
std::unique_ptr<ISecurityRule> createIntegerOverflowRule();

// Null Pointer Dereference
std::unique_ptr<ISecurityRule> createNullPointerRule();

// Use After Free
std::unique_ptr<ISecurityRule> createUseAfterFreeRule();

// Insecure Random
std::unique_ptr<ISecurityRule> createInsecureRandomRule();

// Debug Information Exposure
std::unique_ptr<ISecurityRule> createDebugExposureRule();

// Missing Input Validation
std::unique_ptr<ISecurityRule> createInputValidationRule();

// Race Condition
std::unique_ptr<ISecurityRule> createRaceConditionRule();

// Register all built-in rules
void registerBuiltinRules(SecurityAnalyzer& analyzer);

} // namespace Rules

} // namespace RawrXD::CodeReview
