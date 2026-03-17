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


