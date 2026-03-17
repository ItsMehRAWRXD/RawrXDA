#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDir>
#include <QMap>
#include <QVariant>
#include <QtGlobal>
#include <memory>
#include <vector>
#include "alert_dispatcher.h"
#include "cloud_config.h"

// Enterprise Framework Forward Declarations
class ProductionFramework;
class RefactoringCoordinator;
class TestCoordinator;
class CloudOrchestrator;

// Namespace forward declarations for enterprise platform classes
namespace RawrXD { 
namespace Agentic {
    class MonitoringCoordinator;
    class CollaborationCoordinator;
    class DeploymentOrchestrator;
}
}

// Enterprise Type Forward Declarations  
// CloudProvider is now defined in cloud_config.h
enum DeploymentStrategy { BLUE_GREEN, CANARY, ROLLING, RECREATE };

struct AgenticCloudConfig {
    CloudProvider provider;
    QString projectName;
    QString region;
    QString environment;
    QMap<QString, QString> credentials;
    QJsonObject settings;
};

struct DeploymentConfig {
    QString applicationName;
    QString image;
    QString imageTag;
    int replicas;
    QString strategy;
    QMap<QString, QString> environment;
};

class AgenticEngine;
class InferenceEngine;
class ModelTrainer;
class SettingsManager;

/**
 * @class AgenticExecutor
 * @brief Real agentic execution engine with full autonomous capabilities
 * 
 * This production-ready agent executor provides:
 * - Multi-stage task decomposition with recursive planning
 * - Real file system operations with transactional support
 * - Compiler integration with multi-backend detection
 * - Advanced function calling with tool composition
 * - Model training with fine-tuning capabilities
 * - Sophisticated error handling with automatic recovery
 * - Distributed tracing and structured logging
 * - Memory management with persistence
 * - Self-correcting execution with adaptive strategies
 */
class AgenticExecutor : public QObject {
    Q_OBJECT

public:
    explicit AgenticExecutor(QObject* parent = nullptr);
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    // Main agentic execution entry points
    QJsonObject executeUserRequest(const QString& request);
    QJsonObject executeWithMetrics(const QString& request);
    QJsonArray executeParallel(const QJsonArray& requests);

    // Advanced planning and error handling
    QJsonArray decomposeTaskRecursively(const QString& goal, int depth = 0);
    QJsonArray decomposeTask(const QString& goal);
    QJsonObject analyzeAndFixError(const QString& errorMessage);
    QJsonObject detectDependencies(const QJsonArray& steps);
    QJsonArray optimizeExecutionPlan(const QJsonArray& steps);
    int estimateStepComplexity(const QJsonObject& step);

    // Step execution with tracking
    bool executeStep(const QJsonObject& step);
    bool executeStepWithTimeout(const QJsonObject& step, int timeoutMs);
    bool verifyStepCompletion(const QJsonObject& step, const QString& result);
    QJsonObject executeStepAndGetMetrics(const QJsonObject& step);

    // File system operations (transactional)
    bool createDirectory(const QString& path);
    bool createFile(const QString& path, const QString& content);
    bool writeFile(const QString& path, const QString& content);
    QString readFile(const QString& path);
    bool deleteFile(const QString& path);
    bool deleteDirectory(const QString& path);
    QStringList listDirectory(const QString& path);
    bool backupFile(const QString& path);
    bool restoreFromBackup(const QString& backupPath);

    // Compiler integration (multi-backend)
    QJsonObject compileProject(const QString& projectPath, const QString& compiler = "g++");
    QJsonObject runExecutable(const QString& executablePath, const QStringList& args = QStringList());
    QJsonObject compileWithOptimizations(const QString& projectPath);
    QJsonObject detectAndFixCompilationErrors(const QJsonObject& compileResult);

    // Advanced function calling
    QJsonArray getAvailableTools();
    QJsonObject callTool(const QString& toolName, const QJsonObject& params);
    bool validateToolParams(const QString& toolName, const QJsonObject& params);
    QJsonObject composeFunctionCalls(const QJsonArray& toolCalls);
    QJsonArray planToolSequence(const QString& goal);
    
    // Model training and fine-tuning
    QJsonObject trainModel(const QString& datasetPath, const QString& modelPath, const QJsonObject& config);
    bool isTrainingModel() const;
    QJsonObject validateTrainedModel(const QString& modelPath, const QString& testDataPath);

    // Unified intelligent workflows
    QJsonObject executeFullWorkflow(const QString& specification, 
                                   const QString& outputPath,
                                   const QString& compilerType = "auto",
                                   const QString& modelPath = QString());
    QString detectBestCompiler(const QString& fileType);
    QStringList listAvailableCompilers() const;
    QString selectCompilerForFile(const QString& filePath, const QString& preferredCompiler = QString()) const;
    QJsonObject executeIntelligentWorkflow(const QString& request, const QJsonObject& context);
    
    // Advanced memory and context
    void addToMemory(const QString& key, const QVariant& value);
    QVariant getFromMemory(const QString& key);
    void clearMemory();
    QString getFullContext();
    QJsonObject getMemoryStatistics();
    bool persistMemorySnapshot(const QString& filePath);
    bool loadMemorySnapshot(const QString& filePath);

    // Intelligent error handling
    bool detectFailure(const QString& output);
    QString generateCorrectionPlan(const QString& failureReason);
    QJsonObject retryWithCorrection(const QJsonObject& failedStep);
    
    // Performance metrics and tracing
    QJsonObject getExecutionMetrics() const;
    void enableDetailedLogging(bool enable);
    void enableDistributedTracing(bool enable);
    QString exportExecutionTrace() const;

    // ========== ENTERPRISE FRAMEWORK OPERATIONS ==========
    
    // Code Refactoring Operations
    QJsonObject analyzeAndRefactorCode(const QString& filePath);
    int improveCodeQuality(const QString& projectPath);
    QJsonArray detectCodeIssues(const QString& filePath);
    
    // Automated Testing Operations
    int generateComprehensiveTests(const QString& projectPath);
    int runAllTests(const QString& projectPath);
    double measureCodeCoverage(const QString& projectPath);
    
    // Cloud Deployment Operations
    bool deployToCloud(const QString& applicationName, const QString& version, CloudProvider provider = AWS);
    bool deployMultiCloud(const QString& applicationName, const QString& version);
    bool configureCloudEnvironment(const CloudConfig& config);
    
    // Monitoring & Observability
    void setupMonitoring(const QString& applicationName);
    QJsonObject getSystemHealth();
    bool configureAlerts(const QString& alertName, const QString& condition);
    
    // Team Collaboration
    QString createCodeReview(const QString& prTitle, const QString& sourceBranch);
    bool addReviewComment(const QString& prId, const QString& filePath, int lineNumber, const QString& comment);
    bool approveAndMergePR(const QString& prId);
    
    // Deployment Management
    bool deployApplication(const DeploymentConfig& config, DeploymentStrategy strategy = BLUE_GREEN);
    bool rollbackDeployment(const QString& deploymentId, const QString& targetVersion);
    QString getDeploymentStatus(const QString& applicationName);
    
    // Integrated Enterprise Workflows
    QJsonObject executeEnterpriseWorkflow(const QString& specificationFile);
    QJsonObject completeProductionRelease(const QString& applicationName, const QString& version);
    QString generateProductionReport();


signals:
    void stepStarted(const QString& description);
    void stepCompleted(const QString& description, bool success);
    void taskProgress(int current, int total);
    void executionPhaseChanged(const QString& phase);
    void executionComplete(const QJsonObject& result);
    void errorOccurred(const QString& error);
    void logMessage(const QString& message);
    void workflowStatusChanged(const QString& phase, const QString& detail, int step, int total);
    void workflowCompleted(const QJsonObject& result);
    void metricsUpdated(const QJsonObject& metrics);
    void traceEvent(const QString& eventName, const QJsonObject& data);
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const QString& modelPath, float finalPerplexity);

private:
    // Agent reasoning using model
    QString planNextAction(const QString& currentState, const QString& goal);
    QJsonObject generateCode(const QString& specification);
    QString analyzeError(const QString& errorOutput);
    QString improveCode(const QString& code, const QString& issue);

    // Internal helpers
    QJsonObject buildToolCallPrompt(const QString& goal, const QJsonArray& tools);
    QString extractCodeFromResponse(const QString& response);
    bool validateGeneratedCode(const QString& code);
    QJsonObject compileGeneratedSourceInternal(const QString& sourcePath, const QString& compilerId);
    bool ensureModelLoadedForWorkflow(const QString& requestedModelPath);
    QString resolveCompilerCommand(const QString& compilerId) const;
    QMap<QString, QString> compilerCatalog() const;
    bool compilerProbe(const QString& command, const QStringList& args) const;
    void emitTraceEvent(const QString& eventName, const QJsonObject& data);

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<ModelTrainer> m_modelTrainer;
    
    // Enterprise Framework Components
    std::unique_ptr<ProductionFramework> m_productionFramework;
    std::unique_ptr<RefactoringCoordinator> m_refactoringCoordinator;
    std::unique_ptr<TestCoordinator> m_testCoordinator;
    std::unique_ptr<CloudOrchestrator> m_cloudOrchestrator;
    std::unique_ptr<RawrXD::Agentic::MonitoringCoordinator> m_monitoringCoordinator;
    std::unique_ptr<RawrXD::Agentic::CollaborationCoordinator> m_collaborationCoordinator;
    std::unique_ptr<RawrXD::Agentic::DeploymentOrchestrator> m_deploymentOrchestrator;
    
    // Dependency injections
    SettingsManager* m_settingsManager = nullptr;
    AlertDispatcher* m_alertDispatcher = nullptr;
    
    QMap<QString, QVariant> m_memory;
    QJsonArray m_executionHistory;
    QString m_currentWorkingDirectory;
    
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;
    mutable QMap<QString, QString> m_compilerCatalog;
    mutable bool m_compilerCatalogScanned = false;
    QMap<QString, QString> m_fileBackups;
    bool m_detailedLoggingEnabled = false;
    bool m_distributedTracingEnabled = false;
    struct ExecutionMetrics {
        int totalSteps = 0;
        int successfulSteps = 0;
        qint64 startTime = 0;
        qint64 endTime = 0;
        double successRate = 0.0;
        QJsonArray toolCalls;
        QMap<QString, qint64> stepDurations;
    } m_metrics;
};
