#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <functional>

// Forward declarations
class CodebaseContextAnalyzer;
class IntelligentCodebaseEngine;
class DynamicComplexityEngine;
struct SymbolInfo;
struct Symbol;
struct RefactoringOpportunity;
struct BugReport;
struct Optimization;

/**
 * @brief Decision types for automated audit system
 */
enum class DecisionType {
    SYMBOL_INDEXING,           // Should symbol be indexed?
    DEPENDENCY_TRACKING,       // Should dependency be tracked?
    REFACTORING_APPLY,         // Should refactoring be applied?
    BUG_FIX_APPLY,            // Should bug fix be applied?
    OPTIMIZATION_APPLY,        // Should optimization be applied?
    FILE_ANALYSIS_PRIORITY,    // What priority for file analysis?
    SCOPE_ANALYSIS_DEPTH,      // How deep to analyze scope?
    PATTERN_MATCHING,          // Should pattern be matched?
    ARCHITECTURE_VALIDATION,   // Should architecture be validated?
    QUALITY_THRESHOLD,         // What quality threshold to enforce?
    REAL_TIME_UPDATE,          // Should update in real-time?
    BATCH_PROCESSING,          // Should batch process?
    CIRCULAR_DEPENDENCY,       // How to handle circular dependency?
    COMPLEXITY_REDUCTION,      // Should reduce complexity?
    MEMORY_OPTIMIZATION,       // Should optimize memory?
    SECURITY_HARDENING,        // Should apply security fix?
    CODE_GENERATION,           // Should generate code?
    TEST_GENERATION,           // Should generate tests?
    DOCUMENTATION_UPDATE       // Should update documentation?
};

/**
 * @brief Decision outcome with reasoning
 */
struct AuditDecision {
    DecisionType type;
    QString description;
    bool approved;
    double confidence;
    QString reasoning;
    QJsonObject context;
    QDateTime timestamp;
    QString triggeredBy;
    QVector<QString> affectedFiles;
    QJsonObject metrics;
    QString auditTrail;
    int hotnessLevel;  // 0=cold (safe), 1=warm (review), 2=hot (critical), 3=NASA (mission-critical)
};

/**
 * @brief Audit record for complete traceability
 */
struct AuditRecord {
    QString auditId;
    QDateTime startTime;
    QDateTime endTime;
    QString projectPath;
    QString auditType;
    QVector<AuditDecision> decisions;
    QJsonObject summary;
    double overallScore;
    int totalDecisions;
    int approvedDecisions;
    int rejectedDecisions;
    QVector<QString> recommendations;
    QJsonObject statistics;
};

/**
 * @brief Decision criteria for automation rules
 */
struct DecisionCriteria {
    double minConfidence = 0.7;
    double maxComplexity = 20.0;
    int maxImpactScope = 10;
    bool requireHumanApproval = false;
    bool enableAutoFix = true;
    bool enableAutoRefactor = false;
    QVector<QString> whitelist;
    QVector<QString> blacklist;
    QHash<QString, double> weights;
};

/**
 * @brief Automation policy for decision-making
 */
struct AutomationPolicy {
    QString policyName;
    QHash<DecisionType, DecisionCriteria> criteria;
    bool enableAggressive = false;
    bool enableConservative = true;
    bool enableLearning = true;
    double learningRate = 0.1;
    QJsonObject customRules;
};

/**
 * @brief Automated Audit Coordinator
 * 
 * Bridges CodebaseContextAnalyzer and IntelligentCodebaseEngine with
 * fully automated decision-making for every audit operation.
 */
class AutomatedAuditCoordinator : public QObject {
    Q_OBJECT
    
private:
    // Engine references
    std::shared_ptr<CodebaseContextAnalyzer> contextAnalyzer;
    std::shared_ptr<IntelligentCodebaseEngine> intelligentEngine;
    
    // Decision history
    QVector<AuditRecord> auditHistory;
    QHash<QString, QVector<AuditDecision>> decisionsByFile;
    QHash<DecisionType, int> decisionStatistics;
    
    // Automation policy
    AutomationPolicy currentPolicy;
    QHash<DecisionType, std::function<AuditDecision(QJsonObject)>> decisionHandlers;
    
    // Learning system
    QHash<DecisionType, QVector<double>> decisionSuccessRates;
    QHash<QString, double> fileRiskScores;
    QHash<QString, int> symbolAccessFrequency;
    
    // Real-time coordination
    bool coordinationActive = false;
    QDateTime lastSyncTime;
    int pendingDecisions = 0;
    
public:
    explicit AutomatedAuditCoordinator(QObject* parent = nullptr);
    ~AutomatedAuditCoordinator();
    
    // Initialization
    bool initialize(
        std::shared_ptr<CodebaseContextAnalyzer> contextAnalyzer,
        std::shared_ptr<IntelligentCodebaseEngine> intelligentEngine
    );
    
    bool initializeDynamicMetrics(std::shared_ptr<DynamicComplexityEngine> metricsEngine);
    
    bool setAutomationPolicy(const AutomationPolicy& policy);
    AutomationPolicy getAutomationPolicy() const;
    
    // Dynamic Configuration Management
    void setConfigValue(const QString& category, const QString& key, const QVariant& value);
    QVariant getConfigValue(const QString& category, const QString& key) const;
    QJsonObject getAllConfig() const;
    void resetConfigToDefaults();
    void loadConfigFromFile(const QString& filePath);
    void saveConfigToFile(const QString& filePath);
    
    // Hotness level management (0=cold, 1=warm, 2=hot, 3=NASA)
    void setHotnessLevel(int level);  // Set global hotness level
    int getHotnessLevel() const;
    int calculateHotnessForDecision(const AuditDecision& decision);
    QVector<AuditDecision> getDecisionsByHotness(int hotnessLevel);
    void escalateHotness(const QString& filePath);  // Escalate file from 0->1->2->3
    void escalateToNASA(const QString& filePath);    // Direct escalation to NASA level
    
    // Automated audit operations
    AuditRecord performFullAudit(const QString& projectPath);
    AuditRecord performIncrementalAudit(const QVector<QString>& modifiedFiles);
    AuditRecord performTargetedAudit(const QString& filePath, DecisionType focusType);
    
    // Decision automation
    AuditDecision makeDecision(DecisionType type, const QJsonObject& context);
    QVector<AuditDecision> makeDecisionBatch(const QVector<QPair<DecisionType, QJsonObject>>& contexts);
    bool applyDecision(const AuditDecision& decision);
    bool rollbackDecision(const QString& decisionId);
    
    // Symbol indexing decisions
    AuditDecision decideSymbolIndexing(const Symbol& symbol);
    AuditDecision decideSymbolIndexing(const SymbolInfo& symbol);
    bool shouldIndexSymbol(const QString& symbolName, const QString& symbolType);
    
    // Dependency tracking decisions
    AuditDecision decideDependencyTracking(const QString& fromFile, const QString& toFile);
    bool shouldTrackDependency(const QString& dependencyType, double confidence);
    
    // Refactoring decisions
    AuditDecision decideRefactoringApplication(const RefactoringOpportunity& opportunity);
    bool shouldApplyRefactoring(const QString& refactorType, double confidence, double impact);
    
    // Bug fix decisions
    AuditDecision decideBugFixApplication(const BugReport& bug);
    bool shouldApplyBugFix(const QString& bugType, const QString& severity, double confidence);
    
    // Optimization decisions
    AuditDecision decideOptimizationApplication(const Optimization& optimization);
    bool shouldApplyOptimization(const QString& optimizationType, double improvement, double confidence);
    
    // Analysis priority decisions
    AuditDecision decideAnalysisPriority(const QString& filePath);
    int calculateFilePriority(const QString& filePath);
    
    // Scope analysis decisions
    AuditDecision decideScopeAnalysisDepth(const QString& filePath, int requestedDepth);
    int determineOptimalScopeDepth(const QString& filePath);
    
    // Pattern matching decisions
    AuditDecision decidePatternMatching(const QString& pattern, const QString& language);
    bool shouldMatchPattern(const QString& pattern, int estimatedMatches);
    
    // Architecture validation decisions
    AuditDecision decideArchitectureValidation(const QString& expectedPattern);
    bool shouldValidateArchitecture(double currentConfidence);
    
    // Quality threshold decisions
    AuditDecision decideQualityThreshold(double currentScore);
    double determineAcceptableQualityThreshold(const QString& projectType);
    
    // Real-time update decisions
    AuditDecision decideRealTimeUpdate(const QString& filePath);
    bool shouldUpdateRealTime(const QString& filePath, int changeFrequency);
    
    // Batch processing decisions
    AuditDecision decideBatchProcessing(int fileCount, int totalSize);
    int determineBatchSize(int fileCount, int availableMemory);
    
    // Circular dependency decisions
    AuditDecision decideCircularDependencyHandling(const QVector<QString>& cycle);
    QString determineCircularDependencyResolution(const QVector<QString>& cycle);
    
    // Complexity reduction decisions
    AuditDecision decideComplexityReduction(const QString& functionName, double complexity);
    bool shouldReduceComplexity(double complexity, int functionSize);
    
    // Memory optimization decisions
    AuditDecision decideMemoryOptimization(const QString& functionName, int estimatedSavings);
    bool shouldOptimizeMemory(int estimatedSavings, int codeChurn);
    
    // Security hardening decisions
    AuditDecision decideSecurityHardening(const QString& vulnerabilityType, const QString& severity);
    bool shouldApplySecurityFix(const QString& vulnerabilityType, double exploitability);
    
    // Code generation decisions
    AuditDecision decideCodeGeneration(const QString& generationType, const QJsonObject& spec);
    bool shouldGenerateCode(const QString& generationType, double specCompleteness);
    
    // Test generation decisions
    AuditDecision decideTestGeneration(const QString& targetFunction, int existingTestCount);
    bool shouldGenerateTests(int existingTestCount, double coverage);
    
    // Documentation update decisions
    AuditDecision decideDocumentationUpdate(const QString& symbolName, bool hasExisting);
    bool shouldUpdateDocumentation(bool hasExisting, double docQuality);
    
    // Coordination between engines
    void synchronizeEngines();
    void propagateSymbolToIntelligent(const Symbol& symbol);
    void propagateSymbolToContext(const SymbolInfo& symbol);
    void reconcileDependencyGraphs();
    void mergeAnalysisResults();
    
    // Learning and adaptation
    void recordDecisionOutcome(const QString& decisionId, bool successful);
    void updateSuccessRates();
    void adaptPolicy();
    double calculateDecisionConfidence(DecisionType type, const QJsonObject& context);
    
    // Audit reporting
    QJsonObject generateAuditReport(const AuditRecord& audit);
    QJsonObject generateDecisionStatistics();
    QJsonObject generateRecommendations();
    QVector<QString> identifyHighRiskFiles();
    QVector<QString> identifyLowQualityAreas();
    
    // Query and analysis
    AuditRecord getAuditById(const QString& auditId);
    QVector<AuditRecord> getAuditHistory(const QDateTime& since);
    QVector<AuditDecision> getDecisionsByFile(const QString& filePath);
    QVector<AuditDecision> getDecisionsByType(DecisionType type);
    double getDecisionSuccessRate(DecisionType type);
    
    // Policy management
    void loadPolicyFromFile(const QString& filePath);
    void savePolicyToFile(const QString& filePath);
    void resetToDefaultPolicy();
    void enableAggressiveMode();
    void enableConservativeMode();
    
signals:
    void decisionMade(const AuditDecision& decision);
    void auditStarted(const QString& auditId, const QString& projectPath);
    void auditProgress(const QString& auditId, int percentage, const QString& currentFile);
    void auditCompleted(const QString& auditId, const AuditRecord& record);
    void decisionApplied(const QString& decisionId, bool success);
    void policyUpdated(const AutomationPolicy& newPolicy);
    void coordinationSynced();
    void highRiskFileDetected(const QString& filePath, double riskScore);
    void qualityThresholdViolated(const QString& filePath, double score);
    
private:
    // Helper methods
    QString generateAuditId();
    QString generateDecisionId();
    void initializeDecisionHandlers();
    void initializeDefaultPolicy();
    void updateDecisionStatistics(const AuditDecision& decision);
    void calculateFileRiskScore(const QString& filePath);
    
    // Hotness calculation helpers
    int calculateBaseHotness(const QJsonObject& metrics);
    void updateHotnessTracking(const QString& filePath, int newHotness);
    QVector<QString> getHotFiles() const;  // Returns files with hotness=2
    QVector<QString> getNASAFiles() const;  // Returns files with hotness=3
    
    // Decision evaluation
    bool evaluateCriteria(const DecisionCriteria& criteria, const QJsonObject& context);
    double calculateImpactScore(const AuditDecision& decision);
    QVector<QString> extractAffectedFiles(DecisionType type, const QJsonObject& context);
    
    // Context extraction
    QJsonObject extractSymbolContext(const Symbol& symbol);
    QJsonObject extractSymbolContext(const SymbolInfo& symbol);
    QJsonObject extractRefactoringContext(const RefactoringOpportunity& opportunity);
    QJsonObject extractBugContext(const BugReport& bug);
    QJsonObject extractOptimizationContext(const Optimization& optimization);
    
    // Application logic
    bool applySymbolIndexing(const AuditDecision& decision);
    bool applyDependencyTracking(const AuditDecision& decision);
    bool applyRefactoring(const AuditDecision& decision);
    bool applyBugFix(const AuditDecision& decision);
    bool applyOptimization(const AuditDecision& decision);
    bool applyCodeGeneration(const AuditDecision& decision);
    bool applyTestGeneration(const AuditDecision& decision);
    bool applyDocumentationUpdate(const AuditDecision& decision);
    
    // Learning algorithms
    double calculateBayesianConfidence(DecisionType type, const QJsonObject& context);
    void updateWeights(DecisionType type, bool successful);
    double predictSuccessProbability(DecisionType type, const QJsonObject& context);
    
    // Conflict resolution
    AuditDecision resolveConflictingDecisions(const QVector<AuditDecision>& conflicts);
    bool detectDecisionConflict(const AuditDecision& d1, const AuditDecision& d2);
    
    // Audit trail management
    void recordAuditTrail(const AuditDecision& decision, const QString& action);
    QString generateAuditTrailEntry(const QString& action, const QJsonObject& details);
    
    // Member variables
    std::shared_ptr<CodebaseContextAnalyzer> contextAnalyzer;
    std::shared_ptr<IntelligentCodebaseEngine> intelligentEngine;
    std::shared_ptr<DynamicComplexityEngine> dynamicMetrics;
    
    AutomationPolicy currentPolicy;
    bool coordinationActive;
    int pendingDecisions;
    int globalHotnessLevel;  // 0=cold, 1=warm, 2=hot, 3=NASA
    QDateTime lastSyncTime;
    
    QVector<AuditRecord> auditHistory;
    QHash<QString, QVector<AuditDecision>> decisionsByFile;
    QHash<DecisionType, int> decisionStatistics;
    QHash<DecisionType, QVector<double>> decisionSuccessRates;
    QHash<QString, double> fileRiskScores;
    QHash<QString, int> fileHotnessLevels;  // Track hotness per file (0-3)
    QHash<DecisionType, std::function<AuditDecision(QJsonObject)>> decisionHandlers;
};

/**
 * @brief Decision rule engine for complex decision logic
 */
class DecisionRuleEngine {
public:
    DecisionRuleEngine();
    
    bool evaluateRule(const QString& ruleName, const QJsonObject& context);
    void addCustomRule(const QString& ruleName, std::function<bool(QJsonObject)> rule);
    void removeCustomRule(const QString& ruleName);
    QVector<QString> getApplicableRules(DecisionType type);
    
private:
    QHash<QString, std::function<bool(QJsonObject)>> customRules;
    QHash<DecisionType, QVector<QString>> rulesByType;
};

/**
 * @brief Decision history analyzer for pattern recognition
 */
class DecisionHistoryAnalyzer {
public:
    DecisionHistoryAnalyzer();
    
    void analyzeHistory(const QVector<AuditRecord>& history);
    QJsonObject identifyPatterns();
    QVector<QString> suggestPolicyImprovements();
    double predictDecisionOutcome(DecisionType type, const QJsonObject& context);
    
private:
    QHash<DecisionType, QVector<QPair<QJsonObject, bool>>> decisionPatterns;
    QHash<QString, double> patternConfidence;
};
