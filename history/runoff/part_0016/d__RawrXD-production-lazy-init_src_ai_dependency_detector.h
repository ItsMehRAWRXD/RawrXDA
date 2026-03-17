// Dependency Detector - Automatic dependency resolution with graph analysis
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QQueue>
#include <QSet>
#include <QHash>
#include <QUuid>
#include <QVersionNumber>
#include <QUrl>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

// Forward declarations
class AgenticExecutor;
class AdvancedPlanningEngine;
class ErrorAnalysisSystem;

/**
 * @brief Dependency relationship types
 */
enum class DependencyRelationType : int {
    Required,        // Hard dependency - must be present
    Optional,        // Soft dependency - enhances functionality
    Conditional,     // Depends on runtime conditions
    Development,     // Only needed during development
    Runtime,         // Only needed at runtime
    Build,          // Only needed during build
    Test,           // Only needed for testing
    Peer,           // Expected to be provided by parent
    Bundle,         // Bundled with the package
    Conflict        // Conflicts with this dependency
};

/**
 * @brief Dependency constraint types
 */
enum class ConstraintType : int {
    Exact,          // Exact version match
    Minimum,        // Minimum version
    Maximum,        // Maximum version
    Range,          // Version range
    Compatible,     // Compatible version (semver)
    Latest,         // Latest available version
    Any            // Any version acceptable
};

/**
 * @brief Version constraint specification
 */
struct VersionConstraint {
    ConstraintType type = ConstraintType::Any;
    QVersionNumber minVersion;
    QVersionNumber maxVersion;
    QVersionNumber exactVersion;
    QString rangeSpec;          // e.g., ">=1.0.0 <2.0.0"
    bool includePrerelease = false;
    QStringList excludeVersions;
    
    bool isVersionSatisfied(const QVersionNumber& version) const;
    QString toString() const;
    static VersionConstraint fromString(const QString& constraint);
};

/**
 * @brief Platform and environment constraints
 */
struct PlatformConstraint {
    QStringList supportedOs;        // Windows, Linux, macOS, etc.
    QStringList supportedArch;      // x86, x64, arm64, etc.
    QStringList requiredFeatures;   // Optional platform features
    QVariantMap environmentVars;    // Required environment variables
    QString minimumOsVersion;       // Minimum OS version
    QStringList conflictingPackages; // Packages that conflict
};

/**
 * @brief Dependency specification with comprehensive metadata
 */
struct DependencySpec {
    QString id;                     // Unique dependency identifier
    QString name;                   // Human-readable name
    QString description;            // Description of the dependency
    DependencyRelationType type = DependencyRelationType::Required;
    QString source;                 // Where to find this dependency
    QUrl sourceUrl;                 // URL to dependency source
    
    // Version constraints
    VersionConstraint versionConstraint;
    QString currentVersion;         // Currently installed version
    QString availableVersion;       // Latest available version
    
    // Platform constraints
    PlatformConstraint platformConstraint;
    
    // Metadata
    QStringList categories;         // Categorization tags
    QStringList keywords;           // Search keywords
    QString license;                // License information
    QString author;                 // Author/maintainer
    QDateTime lastUpdated;          // Last update timestamp
    qint64 downloadSize = 0;        // Download size in bytes
    qint64 installedSize = 0;       // Installed size in bytes
    
    // Dependency tree
    QStringList dependencies;       // Direct dependencies
    QStringList dependents;         // Packages that depend on this
    QStringList optionalDeps;       // Optional dependencies
    QStringList conflicts;          // Conflicting packages
    
    // Installation metadata
    bool isInstalled = false;
    bool isAvailable = true;
    QString installationPath;
    QDateTime installedAt;
    QJsonObject installationMetadata;
    
    // Quality metrics
    double popularityScore = 0.0;   // Usage popularity (0-1)
    double securityScore = 1.0;     // Security assessment (0-1)
    double maintainanceScore = 1.0; // Maintenance quality (0-1)
    int issueCount = 0;             // Known issues count
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    bool isCompatibleWith(const DependencySpec& other) const;
};

/**
 * @brief Dependency conflict information
 */
struct DependencyConflict {
    QString conflictId;
    QString description;
    QStringList involvedDependencies;
    QString conflictType;           // "version", "platform", "circular", etc.
    QString detailedReason;
    QStringList potentialResolutions;
    double severityScore = 0.5;     // 0=minor, 1=critical
    bool isResolvable = true;
    QJsonObject conflictData;
    QDateTime detectedAt;
};

/**
 * @brief Dependency resolution strategy
 */
struct ResolutionStrategy {
    QString strategyId;
    QString name;
    QString description;
    QStringList applicableConflictTypes;
    QJsonObject implementation;
    
    // Configuration
    double priorityScore = 0.5;     // Higher priority strategies tried first
    bool isAutomaticApproved = false; // Can be applied automatically
    bool requiresUserInput = false;
    bool isReversible = true;
    QStringList requiredPermissions;
    
    // Success tracking
    int applicationCount = 0;
    int successCount = 0;
    double averageResolutionTime = 0.0;
    QDateTime lastUsed;
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

/**
 * @brief Dependency graph node for analysis
 */
struct DependencyNode {
    QString nodeId;
    DependencySpec spec;
    QSet<QString> dependencies;      // Outgoing edges (depends on)
    QSet<QString> dependents;        // Incoming edges (depended by)
    
    // Graph analysis properties
    int depth = 0;                   // Distance from root
    bool isVisited = false;          // For graph traversal
    bool isInPath = false;           // For cycle detection
    double importance = 0.0;         // Centrality measure
    QStringList criticalPaths;       // Critical paths through this node
};

/**
 * @brief Dependency resolution result
 */
struct ResolutionResult {
    QString resolutionId;
    bool success = false;
    QString errorMessage;
    QStringList resolvedDependencies;
    QStringList remainingConflicts;
    QJsonObject resolutionPlan;
    QJsonObject executionLog;
    qint64 resolutionTimeMs = 0;
    QDateTime completedAt;
    
    // Statistics
    int dependenciesInstalled = 0;
    int dependenciesUpdated = 0;
    int dependenciesRemoved = 0;
    int conflictsResolved = 0;
    qint64 totalDownloadSize = 0;
    qint64 totalInstallSize = 0;
};

/**
 * @brief Advanced Dependency Detection and Resolution System
 */
class DependencyDetector : public QObject {
    Q_OBJECT

public:
    explicit DependencyDetector(QObject* parent = nullptr);
    ~DependencyDetector();

    // Initialization
    void initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner, 
                   ErrorAnalysisSystem* errorSystem);
    bool isInitialized() const { return m_initialized; }

    // Dependency registration and management
    bool registerDependency(const DependencySpec& spec);
    bool removeDependency(const QString& dependencyId);
    DependencySpec getDependency(const QString& dependencyId) const;
    QStringList getAllDependencies() const;
    QStringList getDependenciesByType(DependencyRelationType type) const;
    bool updateDependency(const QString& dependencyId, const DependencySpec& spec);

    // Dependency discovery and analysis
    QStringList discoverDependencies(const QString& projectPath);
    QStringList analyzeDependencyTree(const QString& rootDependency);
    QJsonObject generateDependencyGraph();
    QJsonObject analyzeDependencyMetrics();
    QStringList findOrphanedDependencies();
    QStringList findUnusedDependencies();

    // Conflict detection and analysis
    QStringList detectConflicts();
    QStringList detectCircularDependencies();
    QStringList detectVersionConflicts();
    QStringList detectPlatformConflicts();
    DependencyConflict getConflictDetails(const QString& conflictId) const;
    double assessConflictSeverity(const QString& conflictId) const;

    // Resolution strategies
    bool addResolutionStrategy(const ResolutionStrategy& strategy);
    bool removeResolutionStrategy(const QString& strategyId);
    QStringList getAvailableStrategies() const;
    QStringList getApplicableStrategies(const QString& conflictId) const;
    ResolutionStrategy getBestStrategy(const QString& conflictId) const;

    // Automatic dependency resolution
    QString resolveAllConflicts();
    QString resolveConflict(const QString& conflictId);
    QString resolveConflictWithStrategy(const QString& conflictId, const QString& strategyId);
    ResolutionResult getResolutionResult(const QString& resolutionId) const;
    bool isResolutionComplete(const QString& resolutionId) const;

    // Dependency installation and management
    QString installDependency(const QString& dependencyId);
    QString updateDependency(const QString& dependencyId, const QString& targetVersion = QString());
    QString removeDependencyChain(const QString& dependencyId);
    bool verifyDependencyIntegrity(const QString& dependencyId);
    QStringList getInstallationOrder(const QStringList& dependencyIds);

    // Version management
    QStringList getAvailableVersions(const QString& dependencyId);
    QString getLatestVersion(const QString& dependencyId);
    QString getBestCompatibleVersion(const QString& dependencyId, const VersionConstraint& constraint);
    bool isVersionCompatible(const QString& dependencyId, const QString& version);
    QJsonObject compareVersions(const QString& version1, const QString& version2);

    // Platform compatibility
    bool isPlatformCompatible(const QString& dependencyId) const;
    QJsonObject getCurrentPlatformInfo() const;
    QStringList getCompatiblePlatforms(const QString& dependencyId) const;
    bool checkPlatformRequirements(const QString& dependencyId) const;

    // Advanced analysis
    QStringList findUpdateCandidates();
    QStringList findSecurityVulnerabilities();
    QJsonObject generateCompatibilityMatrix();
    QStringList suggestOptimizations();
    double calculateDependencyHealth() const;

    // Caching and optimization
    void enableCaching(bool enabled) { m_cachingEnabled = enabled; }
    void clearCache();
    void optimizeDependencyTree();
    QJsonObject getCacheStatistics() const;

    // Configuration and settings
    void loadConfiguration(const QJsonObject& config);
    QJsonObject saveConfiguration() const;
    void setResolutionTimeout(int timeoutMs) { m_resolutionTimeoutMs = timeoutMs; }
    void setAutoResolutionEnabled(bool enabled) { m_autoResolutionEnabled = enabled; }

    // Import/Export
    QString exportDependencyGraph(const QString& format = "json") const;
    QString exportResolutionReport(const QString& resolutionId) const;
    bool importDependencies(const QString& filePath);
    QString generateDependencyReport() const;

public slots:
    void onDependencyAdded(const QString& dependencyId);
    void onDependencyRemoved(const QString& dependencyId);
    void onConflictDetected(const QString& conflictId);
    void performPeriodicAnalysis();
    void updateDependencyDatabase();

signals:
    void dependencyRegistered(const QString& dependencyId);
    void dependencyUpdated(const QString& dependencyId);
    void dependencyRemoved(const QString& dependencyId);
    void conflictDetected(const QString& conflictId, const QString& description);
    void conflictResolved(const QString& conflictId, const QString& method);
    void resolutionStarted(const QString& resolutionId);
    void resolutionCompleted(const QString& resolutionId, bool success);
    void dependencyInstalled(const QString& dependencyId, const QString& version);
    void dependencyFailed(const QString& dependencyId, const QString& error);
    void vulnerabilityDetected(const QString& dependencyId, const QString& details);
    void optimizationSuggested(const QStringList& suggestions);

private slots:
    void processResolutionQueue();
    void updateMetrics();
    void performMaintenanceTasks();

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    AdvancedPlanningEngine* m_planningEngine = nullptr;
    ErrorAnalysisSystem* m_errorSystem = nullptr;

    // Dependency storage
    std::unordered_map<QString, DependencySpec> m_dependencies;
    std::unordered_map<QString, DependencyNode> m_dependencyGraph;
    std::unordered_map<QString, DependencyConflict> m_conflicts;
    std::unordered_map<QString, ResolutionStrategy> m_strategies;
    std::unordered_map<QString, ResolutionResult> m_resolutionResults;
    mutable QReadWriteLock m_dataLock;

    // Processing queues
    QQueue<QString> m_resolutionQueue;
    QSet<QString> m_activeResolutions;
    mutable QMutex m_queueMutex;

    // Configuration
    bool m_initialized = false;
    bool m_cachingEnabled = true;
    bool m_autoResolutionEnabled = true;
    int m_resolutionTimeoutMs = 300000; // 5 minutes
    QString m_defaultStrategy = "intelligent_resolution";

    // Caching and performance
    QJsonObject m_dependencyCache;
    QJsonObject m_versionCache;
    QDateTime m_lastCacheUpdate;
    int m_cacheExpiryMinutes = 60;

    // Metrics and monitoring
    QJsonObject m_metrics;
    QElapsedTimer m_uptimeTimer;
    QDateTime m_lastAnalysis;

    // Timers
    QTimer* m_processingTimer;
    QTimer* m_metricsTimer;
    QTimer* m_maintenanceTimer;

    // Internal methods
    void initializeComponents();
    void setupTimers();
    void connectSignals();
    void loadBuiltInStrategies();
    
    // Graph analysis
    QStringList topologicalSort();
    QStringList detectCycles();
    void calculateNodeImportance();
    QStringList findCriticalPath(const QString& startNode, const QString& endNode);
    
    // Conflict detection algorithms
    QStringList detectVersionConflictsInternal();
    QStringList detectCircularDependenciesInternal();
    QStringList detectPlatformConflictsInternal();
    
    // Resolution implementation
    QString executeResolutionStrategy(const QString& conflictId, const ResolutionStrategy& strategy);
    bool validateResolutionPlan(const QJsonObject& plan);
    QString applyResolutionPlan(const QJsonObject& plan);
    
    // Utility methods
    QString generateResolutionId();
    QString generateConflictId();
    bool isVersionSatisfied(const QString& version, const VersionConstraint& constraint) const;
    QJsonObject createPlatformInfo() const;
    void updateCacheIfNeeded();
    void cleanupCompletedResolutions();
};