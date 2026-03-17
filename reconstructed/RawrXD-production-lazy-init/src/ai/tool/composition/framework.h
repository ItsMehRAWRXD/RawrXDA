// Tool Composition Framework - Advanced function calling with dynamic tool chaining
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QDateTime>
#include <QUuid>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

// Forward declarations
class AgenticExecutor;
class AdvancedPlanningEngine;
class ErrorAnalysisSystem;
class DistributedTracer;

/**
 * @brief Tool execution result with comprehensive metadata
 */
struct ToolExecutionResult {
    QString toolId;
    QString executionId;
    bool success = false;
    QVariantMap outputData;
    QString errorMessage;
    qint64 executionTimeMs = 0;
    QDateTime timestamp;
    QJsonObject metadata;
    QVariantMap context;
    double confidenceScore = 0.0;
    QStringList warnings;
    QJsonObject performanceMetrics;
    
    // Resource usage tracking
    qint64 memoryUsedBytes = 0;
    double cpuUsagePercent = 0.0;
    int threadCount = 0;
    
    // Chain tracking
    QString parentExecutionId;
    QStringList childExecutionIds;
    int chainDepth = 0;
};

/**
 * @brief Tool execution context for maintaining state
 */
struct ToolExecutionContext {
    QString contextId;
    QJsonObject environment;
    QVariantMap sharedData;
    QStringList availableTools;
    QJsonObject permissions;
    qint64 timeoutMs = 30000;
    int maxChainDepth = 10;
    bool allowDynamicChaining = true;
    QJsonObject constraints;
    QString workingDirectory;
    QVariantMap customProperties;
};

/**
 * @brief Tool chain execution plan
 */
struct ToolChain {
    QString chainId;
    QString name;
    QString description;
    QStringList toolSequence;
    QJsonObject dataFlow;
    QJsonObject conditionalLogic;
    QJsonObject parallelismConfig;
    QJsonObject errorHandling;
    bool isTemplate = false;
    QDateTime createdAt;
    QStringList tags;
    double successProbability = 0.0;
    qint64 estimatedDurationMs = 0;
};

/**
 * @brief Individual tool definition with metadata
 */
class ComposableTool : public QObject {
    Q_OBJECT

public:
    explicit ComposableTool(const QString& id, QObject* parent = nullptr);
    virtual ~ComposableTool();

    // Basic properties
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString description() const { return m_description; }
    QString category() const { return m_category; }
    QString version() const { return m_version; }
    
    void setName(const QString& name) { m_name = name; }
    void setDescription(const QString& description) { m_description = description; }
    void setCategory(const QString& category) { m_category = category; }
    void setVersion(const QString& version) { m_version = version; }

    // Capabilities and constraints
    QStringList inputParameters() const { return m_inputParameters; }
    QStringList outputParameters() const { return m_outputParameters; }
    QJsonObject parameterSchema() const { return m_parameterSchema; }
    QStringList requiredDependencies() const { return m_requiredDependencies; }
    QStringList supportedFormats() const { return m_supportedFormats; }
    
    void setInputParameters(const QStringList& params) { m_inputParameters = params; }
    void setOutputParameters(const QStringList& params) { m_outputParameters = params; }
    void setParameterSchema(const QJsonObject& schema) { m_parameterSchema = schema; }
    void setRequiredDependencies(const QStringList& deps) { m_requiredDependencies = deps; }
    void setSupportedFormats(const QStringList& formats) { m_supportedFormats = formats; }

    // Execution configuration
    bool isChainable() const { return m_isChainable; }
    bool supportsParallelExecution() const { return m_supportsParallelExecution; }
    qint64 defaultTimeoutMs() const { return m_defaultTimeoutMs; }
    int maxConcurrentInstances() const { return m_maxConcurrentInstances; }
    
    void setChainable(bool chainable) { m_isChainable = chainable; }
    void setSupportsParallelExecution(bool parallel) { m_supportsParallelExecution = parallel; }
    void setDefaultTimeoutMs(qint64 timeout) { m_defaultTimeoutMs = timeout; }
    void setMaxConcurrentInstances(int max) { m_maxConcurrentInstances = max; }

    // Execution methods
    virtual ToolExecutionResult execute(const QVariantMap& input, 
                                       const ToolExecutionContext& context);
    virtual bool validateInput(const QVariantMap& input) const;
    virtual QJsonObject getExecutionMetrics() const;
    virtual void cleanup();

    // Chaining support
    virtual bool canChainWith(const ComposableTool* nextTool) const;
    virtual QVariantMap transformOutputForChaining(const QVariantMap& output, 
                                                  const ComposableTool* nextTool) const;
    virtual QVariantMap preprocessChainedInput(const QVariantMap& input, 
                                              const ComposableTool* previousTool) const;

    // Serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

signals:
    void executionStarted(const QString& executionId);
    void executionProgress(const QString& executionId, double progress);
    void executionCompleted(const QString& executionId, const ToolExecutionResult& result);
    void executionFailed(const QString& executionId, const QString& error);
    void chainingRequested(const QString& nextToolId, const QVariantMap& data);

protected:
    // Override this method in derived classes for actual tool implementation
    virtual ToolExecutionResult doExecute(const QVariantMap& input, 
                                         const ToolExecutionContext& context) = 0;

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QString m_category;
    QString m_version;
    
    // Configuration
    QStringList m_inputParameters;
    QStringList m_outputParameters;
    QJsonObject m_parameterSchema;
    QStringList m_requiredDependencies;
    QStringList m_supportedFormats;
    
    // Execution properties
    bool m_isChainable = true;
    bool m_supportsParallelExecution = false;
    qint64 m_defaultTimeoutMs = 30000;
    int m_maxConcurrentInstances = 1;
    
    // Runtime tracking
    QStringList m_activeExecutions;
    QJsonObject m_executionMetrics;
    mutable QReadWriteLock m_lock;
};

/**
 * @brief Advanced Tool Composition Framework
 */
class ToolCompositionFramework : public QObject {
    Q_OBJECT

public:
    explicit ToolCompositionFramework(QObject* parent = nullptr);
    ~ToolCompositionFramework();

    // Initialization
    void initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner);
    bool isInitialized() const { return m_initialized; }

    // Tool registration and management
    bool registerTool(std::unique_ptr<ComposableTool> tool);
    bool unregisterTool(const QString& toolId);
    ComposableTool* getTool(const QString& toolId);
    const ComposableTool* getTool(const QString& toolId) const;
    QStringList getAvailableTools() const;
    QStringList getToolsByCategory(const QString& category) const;
    QJsonObject getToolMetadata(const QString& toolId) const;

    // Tool discovery and analysis
    QStringList discoverCompatibleTools(const QString& toolId) const;
    QJsonObject analyzeToolCapabilities(const QString& toolId) const;
    double calculateChainCompatibility(const QStringList& toolSequence) const;
    QStringList suggestToolsForTask(const QString& taskDescription) const;

    // Execution context management
    QString createExecutionContext(const QJsonObject& environment = QJsonObject());
    bool updateExecutionContext(const QString& contextId, const QJsonObject& updates);
    ToolExecutionContext getExecutionContext(const QString& contextId) const;
    void destroyExecutionContext(const QString& contextId);

    // Single tool execution
    QString executeTool(const QString& toolId, const QVariantMap& input,
                       const QString& contextId = QString());
    QString executeToolAsync(const QString& toolId, const QVariantMap& input,
                           const QString& contextId = QString());
    ToolExecutionResult getExecutionResult(const QString& executionId) const;
    bool isExecutionComplete(const QString& executionId) const;

    // Dynamic tool chaining
    QString executeToolChain(const ToolChain& chain, const QVariantMap& initialInput,
                           const QString& contextId = QString());
    QString createDynamicChain(const QStringList& toolIds, const QJsonObject& config = QJsonObject());
    bool addToolToChain(const QString& chainId, const QString& toolId, int position = -1);
    bool removeToolFromChain(const QString& chainId, int position);

    // Chain templates and patterns
    bool saveChainTemplate(const ToolChain& chain);
    ToolChain loadChainTemplate(const QString& templateId) const;
    QStringList getChainTemplates() const;
    QStringList suggestChainPatterns(const QString& objective) const;

    // Parallel execution support
    QString executeToolsInParallel(const QStringList& toolIds, 
                                 const QList<QVariantMap>& inputs,
                                 const QString& contextId = QString());
    QJsonObject getParallelExecutionResults(const QString& batchExecutionId) const;

    // Conditional and flow control
    QString executeConditionalChain(const QString& conditionToolId,
                                  const ToolChain& trueBranch,
                                  const ToolChain& falseBranch,
                                  const QVariantMap& input,
                                  const QString& contextId = QString());
    QString executeLoopedChain(const ToolChain& chain, const QJsonObject& loopConfig,
                             const QVariantMap& input, const QString& contextId = QString());

    // Error handling and recovery
    void setErrorRecoveryStrategy(const QString& strategy);
    QString getErrorRecoveryStrategy() const { return m_errorRecoveryStrategy; }
    bool retryFailedExecution(const QString& executionId);
    QStringList getFailedExecutions() const;

    // Performance monitoring and optimization
    QJsonObject getPerformanceMetrics() const;
    QJsonObject getToolUsageStatistics() const;
    QStringList identifyPerformanceBottlenecks() const;
    void optimizeToolAllocation();

    // Advanced features
    QString scheduleDelayedExecution(const QString& toolId, const QVariantMap& input,
                                   qint64 delayMs, const QString& contextId = QString());
    bool cancelExecution(const QString& executionId);
    bool pauseExecution(const QString& executionId);
    bool resumeExecution(const QString& executionId);

    // Learning and adaptation
    void learnFromExecutionHistory();
    QJsonObject predictExecutionOutcome(const QString& toolId, const QVariantMap& input) const;
    QStringList recommendChainOptimizations(const QString& chainId) const;

    // Configuration and settings
    void loadConfiguration(const QJsonObject& config);
    QJsonObject saveConfiguration() const;
    void setMaxConcurrentExecutions(int max) { m_maxConcurrentExecutions = max; }
    void setDefaultTimeout(qint64 timeoutMs) { m_defaultTimeoutMs = timeoutMs; }

    // Export and visualization
    QString exportExecutionHistory(const QString& format = "json") const;
    QJsonObject generateVisualizationData() const;
    QString generateChainDiagram(const QString& chainId) const;

public slots:
    void onToolExecutionCompleted(const QString& executionId, const ToolExecutionResult& result);
    void onToolExecutionFailed(const QString& executionId, const QString& error);
    void optimizePerformance();
    void cleanupCompletedExecutions();

signals:
    void toolRegistered(const QString& toolId);
    void toolUnregistered(const QString& toolId);
    void executionStarted(const QString& executionId);
    void executionCompleted(const QString& executionId, const ToolExecutionResult& result);
    void executionFailed(const QString& executionId, const QString& error);
    void chainCompleted(const QString& chainId, const QJsonObject& results);
    void performanceMetricsUpdated(const QJsonObject& metrics);
    void bottleneckDetected(const QString& toolId, const QString& description);
    void chainOptimizationSuggested(const QString& chainId, const QStringList& suggestions);

private slots:
    void processExecutionQueue();
    void updatePerformanceMetrics();
    void processScheduledExecutions();

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    AdvancedPlanningEngine* m_planningEngine = nullptr;
    ErrorAnalysisSystem* m_errorAnalysis = nullptr;
    DistributedTracer* m_tracer = nullptr;

    // Tool registry and management
    std::unordered_map<QString, std::unique_ptr<ComposableTool>> m_tools;
    std::unordered_map<QString, ToolExecutionContext> m_contexts;
    std::unordered_map<QString, ToolExecutionResult> m_executionResults;
    std::unordered_map<QString, ToolChain> m_chainTemplates;
    mutable QReadWriteLock m_toolsLock;
    mutable QReadWriteLock m_contextsLock;
    mutable QReadWriteLock m_resultsLock;

    // Execution management
    QQueue<QString> m_executionQueue;
    QSet<QString> m_runningExecutions;
    QQueue<QString> m_scheduledExecutions;
    std::unordered_map<QString, QDateTime> m_executionSchedule;
    mutable QMutex m_executionMutex;

    // Configuration
    bool m_initialized = false;
    int m_maxConcurrentExecutions = 10;
    qint64 m_defaultTimeoutMs = 30000;
    QString m_errorRecoveryStrategy = "retry_once";
    bool m_learningEnabled = true;
    bool m_adaptiveOptimization = true;

    // Performance tracking
    QJsonObject m_performanceMetrics;
    QJsonObject m_usageStatistics;
    QJsonObject m_executionHistory;
    QElapsedTimer m_uptimeTimer;

    // Timers and automation
    QTimer* m_executionProcessorTimer;
    QTimer* m_metricsUpdateTimer;
    QTimer* m_scheduledExecutionTimer;
    QTimer* m_cleanupTimer;

    // Internal methods
    void initializeComponents();
    void setupTimers();
    void connectSignals();
    
    // Execution management
    QString generateExecutionId();
    QString generateContextId();
    QString generateChainId();
    void processToolExecution(const QString& executionId);
    void processChainExecution(const QString& chainId);
    
    // Chain processing
    bool validateToolChain(const ToolChain& chain) const;
    QVariantMap executeChainStep(const ToolChain& chain, int stepIndex, 
                                const QVariantMap& input, const QString& contextId);
    bool evaluateChainCondition(const QJsonObject& condition, const QVariantMap& data) const;
    
    // Performance optimization
    void redistributeExecutions();
    void analyzeExecutionPatterns();
    void updateToolPriorities();
    
    // Learning algorithms
    void recordExecutionData(const QString& executionId, const ToolExecutionResult& result);
    QJsonObject analyzeToolPerformance(const QString& toolId) const;
    double calculateToolReliability(const QString& toolId) const;
    
    // Utility methods
    bool validateExecutionId(const QString& executionId) const;
    bool validateContextId(const QString& contextId) const;
    QStringList topologicalSort(const QStringList& tools, const QJsonObject& dependencies) const;
    QJsonObject mergeExecutionResults(const QList<ToolExecutionResult>& results) const;
};