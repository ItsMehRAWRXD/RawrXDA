// Error Analysis System - Intelligent error diagnosis with automatic fix generation
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QQueue>
#include <QSet>
#include <QHash>
#include <QRegularExpression>
#include <QUuid>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

// Forward declarations
class AgenticExecutor;
class AdvancedPlanningEngine;
class ToolCompositionFramework;
class InferenceEngine;

namespace Analysis {

/**
 * @brief Error severity levels for intelligent prioritization
 */
enum class ErrorSeverity : int {
    Critical = 0,    // System-breaking errors
    High = 1,        // Feature-breaking errors
    Medium = 2,      // Functional issues
    Low = 3,         // Minor issues
    Warning = 4,     // Potential issues
    Info = 5         // Informational messages
};

/**
 * @brief Error categories for pattern recognition
 */
enum class ErrorCategory : int {
    Syntax,          // Syntax errors
    Runtime,         // Runtime exceptions
    Logic,           // Logical errors
    Performance,     // Performance issues
    Memory,          // Memory-related errors
    Network,         // Network/connectivity issues
    FileSystem,      // File system errors
    Database,        // Database-related errors
    Security,        // Security vulnerabilities
    Configuration,   // Configuration errors
    Dependency,      // Dependency conflicts
    Concurrency,     // Threading/concurrency issues
    Unknown          // Unclassified errors
};

} // namespace Analysis

// NOTE: Do NOT add using declarations here as they conflict with
// enums defined in error_recovery_system.h
// Use Analysis::ErrorSeverity and Analysis::ErrorCategory explicitly

/**
 * @brief Error context for comprehensive analysis
 */
struct ErrorContext {
    QString contextId;
    QString sourceFile;
    int lineNumber = -1;
    int columnNumber = -1;
    QString function;
    QString className;
    QStringList callStack;
    QVariantMap variables;
    QJsonObject environment;
    QString workingDirectory;
    QDateTime timestamp;
    QString threadId;
    qint64 memoryUsage = 0;
    double cpuUsage = 0.0;
    QStringList relatedFiles;
    QJsonObject systemState;
};

/**
 * @brief Detailed error information with analysis
 */
struct ErrorInfo {
    QString errorId;
    QString message;
    QString detailedDescription;
    Analysis::ErrorSeverity severity = Analysis::ErrorSeverity::Medium;
    Analysis::ErrorCategory category = Analysis::ErrorCategory::Unknown;
    ErrorContext context;
    QStringList tags;
    QJsonObject metadata;
    
    // Analysis results
    double confidenceScore = 0.0;
    QStringList possibleCauses;
    QStringList suggestedFixes;
    QStringList preventionStrategies;
    QStringList relatedErrors;
    
    // Pattern matching
    QStringList matchedPatterns;
    QJsonObject patternData;
    double patternConfidence = 0.0;
    
    // Auto-fix information
    bool isAutoFixable = false;
    QJsonObject autoFixStrategy;
    QStringList requiredPermissions;
    QStringList riskAssessment;
    
    // Tracking
    QDateTime firstOccurrence;
    QDateTime lastOccurrence;
    int occurrenceCount = 1;
    QStringList affectedComponents;
    bool isResolved = false;
    QString resolutionMethod;
    QDateTime resolvedAt;
};

/**
 * @brief Error pattern for machine learning-based recognition
 */
struct ErrorPattern {
    QString patternId;
    QString name;
    QString description;
    QRegularExpression regex;
    QStringList keywords;
    Analysis::ErrorCategory category;
    Analysis::ErrorSeverity defaultSeverity;
    QJsonObject characteristics;
    
    // Statistical data
    int matchCount = 0;
    double successRate = 0.0;
    QDateTime lastMatched;
    QStringList commonCauses;
    QStringList effectiveFixes;
    
    // Learning parameters
    double weight = 1.0;
    bool isActive = true;
    QDateTime createdAt;
    QDateTime updatedAt;
};

/**
 * @brief Auto-fix strategy definition
 */
struct AutoFixStrategy {
    QString strategyId;
    QString name;
    QString description;
    QStringList applicablePatterns;
    QStringList requiredTools;
    QJsonObject implementation;
    
    // Validation
    QStringList preConditions;
    QStringList postConditions;
    QStringList rollbackSteps;
    
    // Risk assessment
    double riskLevel = 0.5; // 0.0 = safe, 1.0 = dangerous
    QStringList potentialSideEffects;
    bool requiresUserApproval = true;
    bool requiresBackup = true;
    
    // Performance tracking
    int applicationCount = 0;
    int successCount = 0;
    double averageFixTimeMs = 0.0;
    QDateTime lastUsed;
};

/**
 * @brief Error Analysis System with ML-powered diagnosis
 */
class ErrorAnalysisSystem : public QObject {
    Q_OBJECT

public:
    explicit ErrorAnalysisSystem(QObject* parent = nullptr);
    ~ErrorAnalysisSystem();

    // Initialization
    void initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner,
                   ToolCompositionFramework* toolFramework, InferenceEngine* inference);
    bool isInitialized() const { return m_initialized; }

    // Error reporting and analysis
    QString reportError(const QString& message, const ErrorContext& context = ErrorContext());
    QString reportException(const std::exception& exception, const ErrorContext& context = ErrorContext());
    QString reportCustomError(const ErrorInfo& errorInfo);
    
    // Error retrieval and management
    ErrorInfo getError(const QString& errorId) const;
    QStringList getAllErrors() const;
    QStringList getErrorsBySeverity(Analysis::ErrorSeverity severity) const;
    QStringList getErrorsByCategory(Analysis::ErrorCategory category) const;
    QStringList getRecentErrors(int maxCount = 100) const;
    QStringList getUnresolvedErrors() const;

    // Pattern recognition and learning
    bool addErrorPattern(const ErrorPattern& pattern);
    bool removeErrorPattern(const QString& patternId);
    QStringList getMatchingPatterns(const QString& errorMessage) const;
    ErrorPattern getBestMatchingPattern(const QString& errorMessage) const;
    void learnFromError(const QString& errorId, const QString& resolution);

    // Automatic diagnosis
    QJsonObject diagnoseError(const QString& errorId);
    QStringList suggestFixes(const QString& errorId);
    QStringList identifyRootCause(const QString& errorId);
    double calculateErrorImpact(const QString& errorId);
    QStringList findRelatedErrors(const QString& errorId);

    // Auto-fix capabilities
    bool addAutoFixStrategy(const AutoFixStrategy& strategy);
    bool removeAutoFixStrategy(const QString& strategyId);
    QStringList getApplicableFixStrategies(const QString& errorId) const;
    QString generateAutoFix(const QString& errorId);
    bool applyAutoFix(const QString& errorId, const QString& strategyId);
    QString validateAutoFix(const QString& errorId, const QString& strategyId);

    // Error prevention and monitoring
    QStringList predictPotentialErrors(const QJsonObject& codeContext);
    void setupErrorMonitoring(const QStringList& watchPatterns);
    void enableProactiveDetection(bool enabled) { m_proactiveDetection = enabled; }
    QJsonObject getErrorStatistics() const;

    // Advanced analysis
    QJsonObject performRootCauseAnalysis(const QString& errorId);
    QStringList generatePreventionStrategies(const QString& errorId);
    QJsonObject analyzeErrorTrends();
    QStringList identifySystemicIssues();
    double calculateSystemHealth() const;

    // Configuration and settings
    void loadConfiguration(const QJsonObject& config);
    QJsonObject saveConfiguration() const;
    void setAutoFixEnabled(bool enabled) { m_autoFixEnabled = enabled; }
    void setLearningEnabled(bool enabled) { m_learningEnabled = enabled; }
    void setMaxErrorHistory(int maxErrors) { m_maxErrorHistory = maxErrors; }

    // Export and reporting
    QString exportErrorReport(const QString& format = "json") const;
    QString generateSystemHealthReport() const;
    QJsonObject generateTrendAnalysis() const;
    QString exportErrorPatterns() const;

public slots:
    void onErrorDetected(const QString& source, const QString& message);
    void onSystemStateChanged(const QJsonObject& systemState);
    void performPeriodicAnalysis();
    void cleanupOldErrors();
    void updateErrorPatterns();

signals:
    void errorReported(const QString& errorId, Analysis::ErrorSeverity severity);
    void errorResolved(const QString& errorId, const QString& method);
    void criticalErrorDetected(const QString& errorId, const QString& message);
    void patternDetected(const QString& patternId, const QString& errorId);
    void autoFixApplied(const QString& errorId, const QString& strategyId, bool success);
    void systemHealthChanged(double healthScore);
    void errorTrendDetected(const QString& description);
    void preventionSuggested(const QStringList& strategies);

private slots:
    void processErrorQueue();
    void updateStatistics();
    void performLearningUpdate();

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    AdvancedPlanningEngine* m_planningEngine = nullptr;
    ToolCompositionFramework* m_toolFramework = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;

    // Error storage and management
    std::unordered_map<QString, ErrorInfo> m_errors;
    std::unordered_map<QString, ErrorPattern> m_patterns;
    std::unordered_map<QString, AutoFixStrategy> m_autoFixStrategies;
    QQueue<QString> m_errorQueue;
    mutable QReadWriteLock m_errorsLock;
    mutable QReadWriteLock m_patternsLock;
    mutable QReadWriteLock m_strategiesLock;

    // Configuration
    bool m_initialized = false;
    bool m_autoFixEnabled = true;
    bool m_learningEnabled = true;
    bool m_proactiveDetection = false;
    int m_maxErrorHistory = 10000;
    double m_patternMatchThreshold = 0.7;
    double m_autoFixRiskThreshold = 0.3;

    // Analysis state
    QJsonObject m_systemState;
    QJsonObject m_errorStatistics;
    QElapsedTimer m_uptimeTimer;
    QDateTime m_lastAnalysis;

    // Timers
    QTimer* m_processingTimer;
    QTimer* m_statisticsTimer;
    QTimer* m_learningTimer;
    QTimer* m_cleanupTimer;

    // Internal methods
    void initializeComponents();
    void setupTimers();
    void connectSignals();
    
    // Error processing
    QString generateErrorId();
    void processError(const QString& errorId);
    void categorizeError(ErrorInfo& error);
    void analyzeErrorContext(ErrorInfo& error);
    
    // Pattern matching
    QStringList matchErrorPatterns(const QString& message) const;
    double calculatePatternMatch(const ErrorPattern& pattern, const ErrorInfo& error) const;
    void updatePatternStatistics(const QString& patternId, bool success);
    
    // Machine learning
    void trainPatternRecognition();
    void updateErrorPredictionModel();
    QJsonObject extractFeatures(const ErrorInfo& error) const;
    
    // Pattern persistence
    void initializeBuiltInPatterns();
    void saveLearnedPatterns(const QString& filePath);
    void loadLearnedPatterns(const QString& filePath);
    
    // Auto-fix implementation
    bool validateFixStrategy(const AutoFixStrategy& strategy, const ErrorInfo& error) const;
    QString executeAutoFix(const QString& errorId, const AutoFixStrategy& strategy);
    bool rollbackAutoFix(const QString& errorId, const QString& strategyId);
    
    // Analysis algorithms
    QStringList performSimilarityAnalysis(const QString& errorId) const;
    QJsonObject calculateErrorMetrics(const ErrorInfo& error) const;
    QStringList identifyErrorClusters() const;
    
    // Utility methods
    Analysis::ErrorSeverity assessErrorSeverity(const QString& message, const ErrorContext& context) const;
    Analysis::ErrorCategory classifyError(const QString& message, const ErrorContext& context) const;
    QJsonObject createErrorMetadata(const ErrorInfo& error) const;
    void cleanupErrorHistory();
};