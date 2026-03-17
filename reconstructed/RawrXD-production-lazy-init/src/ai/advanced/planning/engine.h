// Advanced Planning Engine - Enterprise-grade recursive task decomposition
// with intelligent dependency resolution and execution optimization
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QThread>
#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QStringList>
#include <QDateTime>
#include <QUuid>
#include <QSet>
#include <QQueue>
#include <QStack>
#include <QGraphicsItem>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

// Forward declarations
class AgenticExecutor;
class InferenceEngine;
class PerformanceMonitor;
class ErrorAnalysisSystem;
class DependencyDetector;
class DistributedTracer;
class MemoryPersistence;

/**
 * @brief Task priority levels for intelligent scheduling
 */
enum class TaskPriority : int {
    Critical = 0,    // System-critical, execute immediately
    High = 1,        // High priority, minimal delay
    Normal = 2,      // Standard priority
    Low = 3,         // Background processing
    Deferred = 4     // Execute when resources available
};

/**
 * @brief Task execution states with detailed tracking
 */
enum class TaskState : int {
    Pending,         // Waiting for dependencies
    Ready,           // Dependencies satisfied, ready to execute
    Running,         // Currently executing
    Completed,       // Successfully completed
    Failed,          // Execution failed
    Cancelled,       // Explicitly cancelled
    Paused,          // Temporarily paused
    Retrying,        // Retrying after failure
    Optimizing       // Being optimized/restructured
};

/**
 * @brief Dependency relationship types
 */
enum class DependencyType : int {
    Sequential,      // Must execute after dependency
    Parallel,        // Can execute concurrently
    Resource,        // Shares resources, needs coordination
    Data,            // Requires data output from dependency
    Conditional,     // Execution depends on dependency result
    Optional         // Weak dependency, can proceed without
};

/**
 * @brief Execution context for tasks
 */
struct TaskExecutionContext {
    QJsonObject environment;        // Environment variables and settings
    QStringList requiredTools;      // Tools needed for execution
    QVariantMap inputData;          // Input data for task
    QVariantMap outputData;         // Output data from task
    QString workingDirectory;       // Working directory for execution
    int maxRetries = 3;            // Maximum retry attempts
    qint64 timeoutMs = 300000;     // Timeout in milliseconds (5 minutes)
    TaskPriority priority = TaskPriority::Normal;
    QDateTime deadline;             // Optional deadline
    QString executorId;             // ID of executor handling task
    QJsonObject metrics;            // Performance and execution metrics
};

/**
 * @brief Task dependency specification
 */
struct TaskDependency {
    QString dependencyId;           // ID of dependency task
    DependencyType type;            // Type of dependency
    bool isRequired = true;         // Whether dependency is required
    QJsonObject conditions;         // Conditions for conditional dependencies
    qint64 delayMs = 0;            // Minimum delay after dependency completion
    QString description;            // Human-readable description
};

/**
 * @brief Intelligent task decomposition result
 */
struct TaskDecomposition {
    QStringList subtasks;           // List of subtask IDs
    QJsonObject dependencies;       // Dependency graph
    QJsonObject optimizations;      // Suggested optimizations
    int estimatedDuration = 0;      // Estimated total duration (ms)
    double complexityScore = 0.0;   // Complexity assessment (0-1)
    QStringList suggestedTools;     // Recommended tools
    QJsonObject resourceRequirements; // Resource requirements
};

/**
 * @brief Comprehensive task definition with full enterprise capabilities
 */
class PlanningTask : public QObject {
    Q_OBJECT

public:
    explicit PlanningTask(const QString& id, QObject* parent = nullptr);
    ~PlanningTask();

    // Core properties
    QString id() const { return m_id; }
    QString parentId() const { return m_parentId; }
    QString name() const { return m_name; }
    QString description() const { return m_description; }
    TaskState state() const { return m_state; }
    TaskPriority priority() const { return m_context.priority; }
    
    void setParentId(const QString& parentId) { m_parentId = parentId; }
    void setName(const QString& name) { m_name = name; }
    void setDescription(const QString& description) { m_description = description; }
    void setState(TaskState state);
    void setPriority(TaskPriority priority) { m_context.priority = priority; }

    // Execution context
    TaskExecutionContext& context() { return m_context; }
    const TaskExecutionContext& context() const { return m_context; }

    // Dependencies
    void addDependency(const TaskDependency& dependency);
    void removeDependency(const QString& dependencyId);
    QList<TaskDependency> dependencies() const { return m_dependencies; }
    bool hasDependency(const QString& dependencyId) const;

    // Subtasks
    void addSubtask(const QString& subtaskId);
    void removeSubtask(const QString& subtaskId);
    QStringList subtasks() const { return m_subtasks; }
    bool hasSubtasks() const { return !m_subtasks.isEmpty(); }

    // Execution tracking
    void startExecution();
    void completeExecution(const QVariantMap& result = QVariantMap());
    void failExecution(const QString& error);
    void pauseExecution();
    void resumeExecution();
    void cancelExecution();

    // Metrics and monitoring
    qint64 executionTime() const;
    qint64 totalTime() const;
    QDateTime createdAt() const { return m_createdAt; }
    QDateTime startedAt() const { return m_startedAt; }
    QDateTime completedAt() const { return m_completedAt; }
    int retryCount() const { return m_retryCount; }
    QJsonObject metrics() const;

    // Serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

signals:
    void stateChanged(TaskState oldState, TaskState newState);
    void progressChanged(double progress);
    void errorOccurred(const QString& error);
    void metricsUpdated(const QJsonObject& metrics);

private:
    QString m_id;
    QString m_parentId;
    QString m_name;
    QString m_description;
    TaskState m_state = TaskState::Pending;
    TaskExecutionContext m_context;
    QList<TaskDependency> m_dependencies;
    QStringList m_subtasks;
    
    // Execution tracking
    QDateTime m_createdAt;
    QDateTime m_startedAt;
    QDateTime m_completedAt;
    QElapsedTimer m_executionTimer;
    int m_retryCount = 0;
    QString m_lastError;
    QVariantMap m_result;
    
    mutable QReadWriteLock m_lock;
};

/**
 * @brief Advanced Planning Engine with enterprise-grade capabilities
 */
class AdvancedPlanningEngine : public QObject {
    Q_OBJECT

public:
    explicit AdvancedPlanningEngine(QObject* parent = nullptr);
    ~AdvancedPlanningEngine();

    // Initialization
    void initialize(AgenticExecutor* executor, InferenceEngine* inference);
    bool isInitialized() const { return m_initialized; }

    // Task management
    QString createTask(const QString& name, const QString& description,
                      TaskPriority priority = TaskPriority::Normal);
    bool removeTask(const QString& taskId);
    PlanningTask* getTask(const QString& taskId);
    QStringList getAllTasks() const;
    QStringList getTasksByState(TaskState state) const;
    QStringList getTasksByPriority(TaskPriority priority) const;

    // Recursive task decomposition
    TaskDecomposition decomposeTask(const QString& taskId);
    bool applyDecomposition(const QString& taskId, const TaskDecomposition& decomp);
    QJsonObject analyzeTaskComplexity(const QString& taskId);
    QStringList suggestOptimizations(const QString& taskId);

    // Dependency management
    bool addTaskDependency(const QString& taskId, const TaskDependency& dependency);
    bool removeTaskDependency(const QString& taskId, const QString& dependencyId);
    QJsonObject analyzeDependencyGraph();
    QStringList detectCircularDependencies();
    QStringList getExecutionOrder();
    
    // Intelligent planning algorithms
    QJsonObject generateExecutionPlan(const QStringList& taskIds);
    QJsonObject optimizeExecutionPlan(const QJsonObject& plan);
    QStringList findCriticalPath(const QStringList& taskIds);
    QJsonObject estimateResourceRequirements(const QStringList& taskIds);
    double calculateParallelizationPotential(const QStringList& taskIds);

    // Execution engine integration
    bool executePlannedTasks(const QJsonObject& plan);
    bool pauseExecution();
    bool resumeExecution();
    bool stopExecution();
    void setMaxConcurrentTasks(int maxTasks) { m_maxConcurrentTasks = maxTasks; }

    // Real-time monitoring and adaptation
    QJsonObject getExecutionStatus();
    QJsonObject getPerformanceMetrics();
    void adaptExecutionStrategy();
    void rebalanceWorkload();
    QStringList identifyBottlenecks();

    // Machine learning integration
    void learnFromExecutionHistory();
    QJsonObject predictTaskDuration(const QString& taskId);
    double predictSuccessProbability(const QString& taskId);
    QStringList recommendOptimizations();

    // Configuration and settings
    void loadConfiguration(const QJsonObject& config);
    QJsonObject saveConfiguration() const;
    void setDecompositionDepth(int maxDepth) { m_maxDecompositionDepth = maxDepth; }
    void setLearningEnabled(bool enabled) { m_learningEnabled = enabled; }

    // Export and visualization
    QString exportExecutionPlan(const QJsonObject& plan, const QString& format = "json");
    QJsonObject generateVisualizationData();
    QString generateExecutionReport();

public slots:
    void onTaskStateChanged(TaskState oldState, TaskState newState);
    void onTaskError(const QString& taskId, const QString& error);
    void onExecutionTimeout();
    void optimizePerformance();

signals:
    void taskCreated(const QString& taskId);
    void taskUpdated(const QString& taskId);
    void taskRemoved(const QString& taskId);
    void planGenerated(const QJsonObject& plan);
    void executionStarted(const QJsonObject& plan);
    void executionCompleted(const QJsonObject& results);
    void executionFailed(const QString& error);
    void performanceMetricsUpdated(const QJsonObject& metrics);
    void optimizationSuggested(const QStringList& suggestions);
    void bottleneckDetected(const QString& description);

private slots:
    void processExecutionQueue();
    void updateExecutionMetrics();
    void handleTaskCompletion(const QString& taskId);

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    PerformanceMonitor* m_performanceMonitor = nullptr;
    ErrorAnalysisSystem* m_errorAnalysis = nullptr;
    DependencyDetector* m_dependencyDetector = nullptr;
    DistributedTracer* m_tracer = nullptr;
    MemoryPersistence* m_persistence = nullptr;

    // Task management
    std::unordered_map<QString, std::unique_ptr<PlanningTask>> m_tasks;
    QQueue<QString> m_executionQueue;
    QSet<QString> m_runningTasks;
    mutable QReadWriteLock m_tasksLock;

    // Configuration
    bool m_initialized = false;
    int m_maxDecompositionDepth = 5;
    int m_maxConcurrentTasks = 4;
    bool m_learningEnabled = true;
    bool m_adaptiveOptimization = true;
    qint64 m_defaultTimeoutMs = 300000;

    // Execution state
    bool m_executionActive = false;
    bool m_executionPaused = false;
    QElapsedTimer m_executionTimer;
    QTimer* m_executionProcessorTimer;
    QTimer* m_metricsUpdateTimer;

    // Performance tracking
    QJsonObject m_executionMetrics;
    QJsonObject m_historicalData;
    double m_averageTaskDuration = 0.0;
    double m_successRate = 0.0;
    int m_totalTasksExecuted = 0;

    // Internal methods
    void initializeComponents();
    void setupTimers();
    void connectSignals();
    
    // Task decomposition algorithms
    QStringList decomposeTaskRecursively(const QString& taskId, int currentDepth = 0);
    QJsonObject analyzeTaskDependencies(const QString& taskId);
    QJsonObject calculateResourceRequirements(const QString& taskId);
    
    // Execution optimization
    QJsonObject optimizeTaskOrder(const QStringList& taskIds);
    QJsonObject parallelizeExecution(const QStringList& taskIds);
    void redistributeResources();
    
    // Learning and adaptation
    void recordExecutionData(const QString& taskId, const QJsonObject& data);
    QJsonObject analyzeExecutionPatterns();
    void updatePredictionModels();
    
    // Utility methods
    QString generateTaskId();
    bool validateTaskId(const QString& taskId) const;
    QStringList getReadyTasks() const;
    void updateTaskMetrics(PlanningTask* task);
    QJsonObject createExecutionContext();
};