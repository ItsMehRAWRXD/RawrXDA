<<<<<<< HEAD
// ============================================================================
// semantic_diff_analyzer.hpp — Semantic Diff Analysis
// ============================================================================
// Provides semantic-level analysis of code diffs:
// - Function/method changes
// - API modifications
// - Breaking changes detection
// - Test coverage impact
// - Code flow analysis
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional>

namespace RawrXD {
namespace Git {

// ============================================================================
// Semantic Change Categories
// ============================================================================

enum class ChangeCategory : uint8_t {
    FORMATTING = 0,                    // Whitespace, indentation only
    COMMENT = 1,                       // Comment-only changes
    VARIABLE_RENAME = 2,               // Simple variable/function rename
    LOGIC_CHANGE = 3,                  // Logic modification
    NEW_FEATURE = 4,                   // New functionality added
    BUG_FIX = 5,                       // Bug fix
    REFACTOR = 6,                      // Code restructuring
    BREAKING_CHANGE = 7,               // API/interface change
    PERFORMANCE = 8,                   // Performance optimization
    SECURITY = 9,                      // Security improvement
    TEST = 10,                         // Test changes
    DOCUMENTATION = 11                 // Doc-only changes
};

// ============================================================================
// Semantic Change Information
// ============================================================================

struct SemanticChange {
    ChangeCategory  category;
    std::string     filePath;
    int             lineStart;
    int             lineEnd;
    std::string     functionName;      // Which function contains the change
    std::string     description;       // Human-readable description
    float           riskScore;         // 0.0-1.0 risk assessment
    bool            isBreakingChange;
    std::vector<std::string> affectedSymbols;  // Functions/classes affected
};

// ============================================================================
// Diff Statistics
// ============================================================================

struct DiffStats {
    int             totalLines;
    int             addedLines;
    int             removedLines;
    int             modifiedLines;
    int             fileCount;
    std::vector<std::string> newFiles;
    std::vector<std::string> deletedFiles;
    std::vector<std::string> renamedFiles;
};

// ============================================================================
// Semantic Diff Analyzer
// ============================================================================

class SemanticDiffAnalyzer {
public:
    SemanticDiffAnalyzer();
    ~SemanticDiffAnalyzer();

    // ---- Analysis ----

    // Analyze a git diff for semantic changes
    std::vector<SemanticChange> analyzeDiff(const std::string& diffContent,
                                           const std::string& filePath = "");

    // Analyze specific hunk for semantic meaning
    SemanticChange analyzeHunk(const std::string& hunkContent,
                              const std::string& filePath,
                              int startLine);

    // Determine change category from content
    ChangeCategory classifyChange(const std::string& oldContent,
                                  const std::string& newContent,
                                  const std::string& filePath = "");

    // ---- Impact Assessment ----

    // Assess breaking changes
    bool isBreakingChange(const SemanticChange& change) const;

    // Calculate risk score (0.0-1.0)
    float assessRisk(const SemanticChange& change) const;

    // Detect security implications
    bool hasSecurityImplications(const std::string& oldContent,
                                 const std::string& newContent) const;

    // Detect test impact
    bool affectsTests(const SemanticChange& change) const;

    // ---- Statistics ----

    // Gather diff statistics
    DiffStats analyzeDiffStats(const std::string& diffContent) const;

    // ---- Symbol Detection ----

    // Extract function/method signatures from code
    std::vector<std::string> extractFunctionSignatures(const std::string& code,
                                                       const std::string& fileName);

    // Find function name at given line
    std::string findFunctionAtLine(const std::string& content,
                                  int lineNumber,
                                  const std::string& fileName);

    // ---- Pattern Detection ----

    // Detect if change is likely a bug fix from commit message/code
    bool isBugFix(const std::string& oldCode,
                  const std::string& newCode) const;

    // Detect refactoring (restructure without behavior change)
    bool isRefactoring(const std::string& oldCode,
                       const std::string& newCode) const;

    // Detect performance optimization
    bool isPerformanceImprovement(const std::string& oldCode,
                                  const std::string& newCode) const;

private:
    // ---- Internal helpers ----

    // Extract variable/function name from simple declaration
    std::string extractSymbolName(const std::string& line) const;

    // Detect API changes (function signature, class interface)
    bool detectAPIChange(const std::string& oldCode,
                        const std::string& newCode) const;

    // Detect security-related keywords
    bool hasSecurityKeywords(const std::string& content) const;

    // Detect test file by path
    bool isTestFile(const std::string& filePath) const;

    // Calculate semantic similarity (0.0-1.0)
    float calculateSimilarity(const std::string& a,
                             const std::string& b) const;

    // Extract meaningful tokens from code
    std::vector<std::string> tokenize(const std::string& code) const;

    // Parse function signature
    struct FunctionSignature {
        std::string returnType;
        std::string name;
        std::vector<std::string> parameters;
        bool isAsync;
        bool isStatic;
        bool isVirtual;
    };

    std::optional<FunctionSignature> parseFunctionSignature(const std::string& sig);

    // Detect imports/dependencies
    std::vector<std::string> extractImports(const std::string& code) const;

    // Check for dead code markers
    bool isDeadCode(const std::string& code) const;

    // Track cache for performance
    std::unordered_map<std::string, ChangeCategory> m_categoryCache;
};

} // namespace Git
} // namespace RawrXD
=======
#pragma once


#include <chrono>
#include <memory>

/**
 * @class SemanticDiffAnalyzer
 * @brief Production-ready AI-powered semantic diff analysis
 * 
 * Features:
 * - Semantic diff analysis beyond line-by-line comparison
 * - Breaking change detection with AI understanding
 * - Impact analysis across codebase
 * - Diff context enrichment with AI insights
 * - Structured logging with performance metrics
 * - Configuration-driven analysis strategies
 * - GDPR-compliant data handling
 */
class SemanticDiffAnalyzer : public void {

public:
    explicit SemanticDiffAnalyzer(void* parent = nullptr);
    ~SemanticDiffAnalyzer() override;

    // Configuration
    struct Config {
        std::string aiEndpoint;
        std::string apiKey;
        bool enableBreakingChangeDetection = true;
        bool enableImpactAnalysis = true;
        int maxDiffSize = 50000;
        bool enableMetrics = true;
        bool enableCaching = true;
        std::string cacheDirectory;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Diff analysis structures
    struct SemanticChange {
        std::string type;              // "function_modified", "class_added", "signature_changed", etc.
        std::string name;              // Name of the changed entity
        std::string description;       // AI-generated description
        std::string file;              // File path
        int startLine;             // Start line in diff
        int endLine;               // End line in diff
        bool isBreaking;           // Whether this is a breaking change
        double impactScore;        // 0.0-1.0 impact severity
        std::vector<std::string> affectedFiles;  // Files potentially affected
    };

    struct DiffAnalysis {
        std::vector<SemanticChange> changes;
        std::string summary;           // AI-generated summary
        int breakingChangeCount;
        double overallImpactScore;
        void* metadata;      // Additional analysis data
    };

    // Core functionality
    DiffAnalysis analyzeDiff(const std::string& diff);
    DiffAnalysis compareFiles(const std::string& oldContent, const std::string& newContent, const std::string& filePath);
    std::vector<std::string> detectBreakingChanges(const DiffAnalysis& analysis);
    void* analyzeImpact(const SemanticChange& change);
    std::string enrichDiffContext(const std::string& diff);

    // Metrics
    struct Metrics {
        int64_t diffsAnalyzed = 0;
        int64_t semanticChangesDetected = 0;
        int64_t breakingChangesDetected = 0;
        int64_t impactAnalysesPerformed = 0;
        int64_t cacheHits = 0;
        int64_t cacheMisses = 0;
        int64_t errorCount = 0;
        double avgAnalysisLatencyMs = 0.0;
        double avgImpactScore = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();
    void clearCache();

    void analysisCompleted(const DiffAnalysis& analysis);
    void breakingChangeDetected(const SemanticChange& change);
    void highImpactChangeDetected(const SemanticChange& change);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;

    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Cache
    std::map<std::string, DiffAnalysis> m_analysisCache;
    mutable std::mutex m_cacheMutex;

    // Helper methods
    void logStructured(const std::string& level, const std::string& message, const void*& context = void*());
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void* makeAiRequest(const std::string& endpoint, const void*& payload);
    std::string calculateDiffHash(const std::string& diff);
    DiffAnalysis getCachedAnalysis(const std::string& diffHash);
    void cacheAnalysis(const std::string& diffHash, const DiffAnalysis& analysis);
    bool validateDiff(const std::string& diff);
};


>>>>>>> origin/main
