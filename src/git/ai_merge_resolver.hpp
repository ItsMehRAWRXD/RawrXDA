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
        qint64 conflictsDetected = 0;
        qint64 conflictsResolved = 0;
        qint64 autoResolved = 0;
        qint64 manualResolved = 0;
        qint64 breakingChangesDetected = 0;
        qint64 errorCount = 0;
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

