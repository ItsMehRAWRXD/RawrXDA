<<<<<<< HEAD
// ============================================================================
// ai_merge_resolver.hpp — AI-Assisted Merge Conflict Resolution
// ============================================================================
// Provides semantic understanding and automated resolution of git merge
// conflicts without requiring external AI API calls. Uses heuristics and
// pattern matching for intelligent conflict resolution.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace RawrXD {
namespace Git {

// ============================================================================
// Conflict Marker Structure
// ============================================================================

struct ConflictMarker {
    int         startLine;              // Line number of <<<<<< marker
    int         endLine;                // Line number of >>>>>> marker
    std::string ours;                   // Content from HEAD (current branch)
    std::string theirs;                 // Content from incoming branch
    std::string base;                   // Content from merge base (3-way merge)
    std::string filePath;               // Which file contains this conflict
    bool        is3Way;                 // true if merge base is present
};

// ============================================================================
// Resolution Strategy
// ============================================================================

enum class ResolutionStrategy : uint8_t {
    OURS = 0,                           // Keep our version entirely
    THEIRS = 1,                         // Take their version entirely
    COMBINED = 2,                       // Intelligently combine both
    MANUAL = 3,                         // Requires manual intervention
    SEMANTIC = 4                        // Use semantic analysis
};

struct ResolutionResult {
    ResolutionStrategy  strategy;
    std::string         resolved;
    float               confidence;     // 0.0-1.0 confidence in resolution
    std::string         explanation;
};

// ============================================================================
// AI Merge Resolver
// ============================================================================

class AIMergeResolver {
public:
    AIMergeResolver();
    ~AIMergeResolver();

    // ---- Parsing ----
    
    // Extract all conflict markers from file content
    // Returns markers in order of appearance
    std::vector<ConflictMarker> parseConflicts(const std::string& fileContent,
                                               const std::string& filePath = "");

    // Extract conflict from a specific line range
    std::optional<ConflictMarker> extractConflict(const std::string& content,
                                                   int startLine);

    // ---- Resolution ----

    // Attempt to resolve a single conflict marker
    ResolutionResult resolveConflict(const ConflictMarker& conflict,
                                     const std::string& context = "");

    // Automatically resolve entire file content (processes all conflicts)
    std::string autoResolve(const std::string& fileContent,
                           const std::string& filePath = "");

    // Get resolution strategy for a conflict (without actually resolving)
    ResolutionStrategy suggestStrategy(const ConflictMarker& conflict) const;

    // ---- Utilities ----

    // Check if content has merge conflicts
    bool hasConflicts(const std::string& content) const;

    // Count total conflicts in content
    int countConflicts(const std::string& content) const;

    // Remove conflict markers and keep specified version
    std::string keepOurs(const std::string& content) const;
    std::string keepTheirs(const std::string& content) const;

private:
    // ---- Internal helpers ----

    // Analyze if conflicts are truly syntactic or represent semantic differences
    bool analyzeSemantic(const ConflictMarker& conflict,
                         std::string& resolvedContent,
                         float& confidence);

    // Detect patterns: imports, blank lines, simple vs complex changes
    struct ConflictPattern {
        bool        isImportConflict;
        bool        isBlankLineConflict;
        bool        isBothAddsToSameBlock;
        bool        isSimpleLineChange;
        bool        isCommentConflict;
    };

    ConflictPattern detectPattern(const ConflictMarker& conflict) const;

    // Intelligent combination of conflicting sections
    std::string combineIntelligently(const ConflictMarker& conflict,
                                     const ConflictPattern& pattern);

    // Handle 3-way merge intelligently
    std::string merge3Way(const ConflictMarker& conflict);

    // Compare similarity of two strings (basic Levenshtein-like distance)
    float stringSimilarity(const std::string& a, const std::string& b) const;

    // Detect if content is code (vs text)
    bool isCodeContent(const std::string& content) const;

    // Extract meaningful lines (skip pure whitespace)
    std::vector<std::string> extractMeaningfulLines(const std::string& content) const;

    // Analyze code structure to find common context
    std::string findCommonContext(const std::string& ours,
                                  const std::string& theirs) const;
};

} // namespace Git
} // namespace RawrXD
=======
#pragma once


#include <chrono>
#include <memory>

/**
 * @class AIMergeResolver
 * @brief Production-ready AI-powered merge conflict resolution
 * 
 * Features:
 * - Three-way merge conflict resolution using AI analysis
 * - Semantic merge analysis with context understanding
 * - Automated conflict detection and resolution suggestions
 * - Manual override capability with audit trails
 * - Structured logging with performance metrics
 * - Configuration-driven resolution strategies
 * - GDPR-compliant data handling
 */
class AIMergeResolver : public void {

public:
    explicit AIMergeResolver(void* parent = nullptr);
    ~AIMergeResolver() override;

    // Configuration
    struct Config {
        std::string aiEndpoint;
        std::string apiKey;
        bool enableAutoResolve = false;
        int maxConflictSize = 10000;
        double minConfidenceThreshold = 0.75;
        bool enableMetrics = true;
        bool enableAuditLog = true;
        std::string auditLogPath;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Conflict representation
    struct ConflictBlock {
        std::string file;
        int startLine;
        int endLine;
        std::string baseVersion;
        std::string currentVersion;
        std::string incomingVersion;
        std::string context;
    };

    struct Resolution {
        std::string resolvedContent;
        double confidence;
        std::string strategy;
        std::string explanation;
        bool requiresManualReview;
    };

    // Core functionality
    std::vector<ConflictBlock> detectConflicts(const std::string& filePath);
    Resolution resolveConflict(const ConflictBlock& conflict);
    bool applyResolution(const std::string& filePath, const Resolution& resolution, int lineStart, int lineEnd);
    
    void* analyzeSemanticMerge(const std::string& base, const std::string& current, const std::string& incoming);
    std::vector<std::string> detectBreakingChanges(const std::string& diff);

    // Metrics
    struct Metrics {
        int64_t conflictsDetected = 0;
        int64_t conflictsResolved = 0;
        int64_t autoResolved = 0;
        int64_t manualResolved = 0;
        int64_t breakingChangesDetected = 0;
        int64_t errorCount = 0;
        double avgResolutionConfidence = 0.0;
        double avgResolutionLatencyMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

    void conflictsDetected(int count);
    void conflictResolved(const Resolution& resolution);
    void breakingChangeDetected(const std::string& change);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;

    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Helper methods
    void logStructured(const std::string& level, const std::string& message, const void*& context = void*());
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void logAudit(const std::string& action, const void*& details);
    void* makeAiRequest(const std::string& endpoint, const void*& payload);
    std::string extractConflictMarkers(const std::string& content, ConflictBlock& conflict);
    bool validateResolution(const Resolution& resolution, const ConflictBlock& conflict);
};


>>>>>>> origin/main
