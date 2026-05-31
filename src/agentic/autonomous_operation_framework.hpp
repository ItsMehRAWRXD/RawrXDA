#pragma once

#include "workflow_state_manager.hpp"
#include "multi_step_tool_execution_engine.hpp"
#include "enhanced_memory_system.hpp"
#include "task_integration_system.hpp"
#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QThread>

namespace RawrXD {
namespace Agentic {

/**
 * @class AutonomousOperationFramework
 * @brief Central autonomous agent orchestrator integrating all RawrXD agent systems
 * 
 * This framework provides the highest level of autonomous operation by coordinating
 * workflow state management, multi-step tool execution, memory systems, and task
 * integration to create a fully autonomous agent capable of:
 * 
 * - Autonomous decision making based on memory and context analysis
 * - Dynamic workflow adaptation and optimization
 * - Intelligent tool selection and execution planning
 * - Self-monitoring, error detection, and recovery
 * - Continuous learning from execution patterns
 * - Independent goal completion without human intervention
 * 
 * The framework operates through multiple autonomous loops:
 * - Decision Loop: Analyzes context and makes strategic decisions
 * - Execution Loop: Manages tool execution and task completion
 * - Learning Loop: Continuously learns from outcomes and optimizes
 * - Recovery Loop: Handles errors and implements recovery strategies
 */
class AutonomousOperationFramework : public QObject
{
    Q_OBJECT

public:
    enum class AutonomyLevel {
        Manual = 0,           // Human-controlled operation
        Assisted = 1,         // AI assistance with human oversight
        Supervised = 2,       // Autonomous with human monitoring
        FullyAutonomous = 3   // Complete autonomous operation
    };

    enum class OperationMode {
        Reactive = 0,         // React to external inputs
        Proactive = 1,        // Anticipate and prepare
        Adaptive = 2,         // Adapt strategies based on context
        Creative = 3          // Generate novel solutions
    };

    enum class AgentState {
        Initializing = 0,
        Idle = 1,
        Planning = 2,
        Executing = 3,
        Learning = 4,
        Recovering = 5,
        Optimizing = 6,
        Sleeping = 7
    }; 

    struct AutonomousConfiguration {
        AutonomyLevel autonomyLevel = AutonomyLevel::Supervised;
        OperationMode operationMode = OperationMode::Adaptive;
        
        // Decision making parameters
        double decisionConfidenceThreshold = 0.75;
        double learningRate = 0.1;
        double explorationRate = 0.2;  // Balance exploration vs exploitation
        
        // Execution parameters
        int maxConcurrentTasks = 5;
        int maxToolExecutionDepth = 10;
        double errorToleranceThreshold = 0.1;
        
        // Learning parameters
        bool enableContinuousLearning = true;
        bool enablePatternRecognition = true;
        bool enableStrategyOptimization = true;
        
        // Safety parameters
        bool enableSafetyChecks = true;
        double maxAutonomousRuntime = 3600.0;  // seconds
        QStringList restrictedOperations;      // Operations requiring human approval
        
        // Performance parameters
        double targetEfficiencyScore = 0.8;
        double maxMemoryUsageMB = 1000.0;
        int performanceEvaluationInterval = 300; // seconds
        
        static AutonomousConfiguration defaultConfig();
        QJsonObject toJson() const;
        static AutonomousConfiguration fromJson(const QJsonObject& json);
    };

    struct DecisionContext {
        QString contextId;
        QDateTime timestamp;
        QString workflowId;
        QString currentGoal;
        
        // Current state
        AgentState agentState;
        QStringList activeTasks;
        QStringList availableTools;
        QJsonObject environmentState;
        
        // Memory context
        QStringList relevantMemories;
        QStringList recentExperiences;
        double contextConfidence = 0.0;
        
        // Decision factors
        QJsonObject knownConstraints;
        QStringList priorityFactors;
        double urgencyScore = 0.0;
        double riskScore = 0.0;
        
        // Decision outcome
        QString recommendedAction;
        QStringList alternativeActions;
        QJsonObject actionParameters;
        double decisionConfidence = 0.0;
        QString reasoning;
        
        bool isValid() const;
        QJsonObject toJson() const;
        static DecisionContext fromJson(const QJsonObject& json);
    };

    struct ExecutionPlan {
        QString planId;
        QString goalDescription;
        QDateTime createdAt;
        QDateTime targetCompletion;
        
        // Plan structure
        QStringList taskSequence;           // Ordered task IDs
        QMap<QString, QStringList> taskDependencies;
        QMap<QString, ToolExecutionRequest> toolRequests;
        
        // Execution parameters
        AutonomyLevel requiredAutonomyLevel;
        double estimatedDuration = 0.0;     // minutes
        double confidenceScore = 0.0;
        QStringList requiredResources;
        
        // Adaptation parameters
        bool allowDynamicAdaptation = true;
        double adaptationThreshold = 0.3;
        QStringList fallbackStrategies;
        
        // Monitoring
        int executionStep = 0;
        bool isActive = false;
        QDateTime startTime;
        double progressPercentage = 0.0;
        QStringList executionLog;
        
        bool isValid() const;
        QJsonObject toJson() const;
        static ExecutionPlan fromJson(const QJsonObject& json);
    };

    struct PerformanceMetrics {
        // Efficiency metrics
        double taskCompletionRate = 0.0;    // 0.0 to 1.0
        double averageTaskDuration = 0.0;   // minutes
        double goalAchievementRate = 0.0;   // 0.0 to 1.0
        double resourceUtilization = 0.0;   // 0.0 to 1.0
        
        // Learning metrics
        double learningEffectiveness = 0.0; // 0.0 to 1.0
        double decisionAccuracy = 0.0;      // 0.0 to 1.0
        double adaptationSpeed = 0.0;       // adaptations per hour
        
        // Quality metrics
        double errorRate = 0.0;             // 0.0 to 1.0
        double recoverySuccessRate = 0.0;   // 0.0 to 1.0
        double userSatisfactionScore = 0.0; // 0.0 to 1.0
        
        // Autonomy metrics
        double autonomyLevel = 0.0;         // 0.0 to 1.0
        double humanInterventionRate = 0.0; // interventions per hour
        double independentOperationTime = 0.0; // seconds
        
        QDateTime lastEvaluation;
        QJsonObject detailedBreakdown;
        
        double calculateOverallScore() const;
    };

    explicit AutonomousOperationFramework(QObject* parent = nullptr);
    virtual ~AutonomousOperationFramework();

    // Framework lifecycle
    bool initialize(const AutonomousConfiguration& config = AutonomousConfiguration::defaultConfig());
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // System integration
    bool connectWorkflowManager(WorkflowStateManager* workflowManager);
    bool connectToolExecutionEngine(MultiStepToolExecutionEngine* executionEngine);
    bool connectMemorySystem(EnhancedMemorySystem* memorySystem);
    bool connectTaskSystem(TaskIntegrationSystem* taskSystem);
    
    // Phase 1: Workflow Persistence integration
    bool registerAgenticExecutor(void* executor); // Accepts AgenticExecutor pointer
    bool saveWorkflowState(const QString& workflowId);
    bool restoreWorkflowState(const QString& workflowId);

    // Autonomous operation control
    bool startAutonomousOperation();
    bool pauseAutonomousOperation();
    bool resumeAutonomousOperation();
    void stopAutonomousOperation();
    
    AgentState getCurrentState() const { return m_currentState; }
    bool isAutonomouslyOperating() const { return m_isOperating; }
    
    // Goal and task management
    QString setGoal(const QString& goalDescription, const QJsonObject& goalParameters = QJsonObject());
    bool updateGoal(const QString& goalId, const QString& newDescription, const QJsonObject& parameters);
    bool cancelGoal(const QString& goalId);
    QStringList getActiveGoals() const;
    
    // Decision making
    DecisionContext analyzeContext(const QString& workflowId = QString());
    QString makeDecision(const DecisionContext& context);
    bool executeDecision(const QString& decisionId, const QJsonObject& parameters = QJsonObject());
    
    // Planning and execution
    QString createExecutionPlan(const QString& goalId, const DecisionContext& context);
    bool executeAutonom ousPlan(const QString& planId);
    bool adaptPlan(const QString& planId, const QJsonObject& adaptationParameters);
    QList<ExecutionPlan> getActivePlans() const;
    
    // Learning and optimization
    bool learnFromExecution(const QString& planId, const QJsonObject& outcomeData);
    void optimizeStrategy(const QString& domainArea);
    bool updateLearningModel(const QJsonObject& trainingData);
    
    // Monitoring and recovery
    PerformanceMetrics evaluatePerformance();
    QStringList detectAnomalies() const;
    bool triggerRecovery(const QString& errorContext, const QJsonObject& errorData);
    bool implementSelfCorrection(const QString& correctionStrategy);
    
    // Configuration management
    void updateConfiguration(const AutonomousConfiguration& config);
    AutonomousConfiguration getConfiguration() const { return m_config; }
    
    // Introspection and reporting
    QJsonObject generateStatusReport() const;
    QJsonObject generateLearningReport() const;
    QJsonObject generatePerformanceReport() const;
    QString explainLastDecision() const;
    QStringList getRecentActions(int count = 10) const;

signals:
    // State change signals
    void stateChanged(AgentState oldState, AgentState newState);
    void autonomousOperationStarted();
    void autonomousOperationStopped();
    void autonomousOperationPaused();
    
    // Decision and execution signals
    void decisionMade(const QString& decisionId, double confidence);
    void planCreated(const QString& planId, const QString& goalId);
    void planExecutionStarted(const QString& planId);
    void planExecutionCompleted(const QString& planId, bool success);
    void planAdapted(const QString& planId, const QString& reason);
    
    // Learning and optimization signals
    void learningCompleted(const QString& domain, double improvement);
    void strategyOptimized(const QString& strategy, double efficiency);
    void patternRecognized(const QString& patternType, double confidence);
    
    // Monitoring and recovery signals
    void anomalyDetected(const QString& anomalyType, double severity);
    void recoveryTriggered(const QString& errorType, const QString& strategy);
    void performanceEvaluated(double overallScore, const QJsonObject& breakdown);
    void humanInterventionRequired(const QString& reason, const QJsonObject& context);

private slots:
    void onDecisionLoop();
    void onExecutionLoop();
    void onLearningLoop();
    void onMonitoringLoop();
    void onPerformanceEvaluation();

private:
    // Core systems
    WorkflowStateManager* m_workflowManager = nullptr;
    MultiStepToolExecutionEngine* m_executionEngine = nullptr;
    EnhancedMemorySystem* m_memorySystem = nullptr;
    TaskIntegrationSystem* m_taskSystem = nullptr;
    void* m_agenticExecutorPtr = nullptr;
    bool m_initialized = false;
    bool m_isOperating = false;
    AgentState m_currentState = AgentState::Initializing;
    
    // Framework state
    bool m_initialized = false;
    bool m_isOperating = false;
    AgentState m_currentState = AgentState::Idle;
    AutonomousConfiguration m_config;
    
    // Execution state
    QMap<QString, ExecutionPlan> m_activePlans;
    QMap<QString, DecisionContext> m_recentDecisions;
    QStringList m_activeGoals;
    QJsonObject m_currentEnvironmentState;
    
    // Autonomous loops
    QTimer* m_decisionLoopTimer;
    QTimer* m_executionLoopTimer;
    QTimer* m_learningLoopTimer;
    QTimer* m_monitoringLoopTimer;
    QTimer* m_performanceTimer;
    
    // Performance tracking
    PerformanceMetrics m_currentMetrics;
    QList<PerformanceMetrics> m_performanceHistory;
    QDateTime m_operationStartTime;
    
    // Thread safety
    mutable QMutex m_frameworkMutex;
    
    // Decision making algorithms
    DecisionContext buildDecisionContext(const QString& workflowId);
    QString selectBestAction(const DecisionContext& context);
    double calculateActionScore(const QString& action, const DecisionContext& context);
    QStringList generateActionAlternatives(const DecisionContext& context);
    
    // Planning algorithms
    ExecutionPlan generateOptimalPlan(const QString& goalId, const DecisionContext& context);
    QStringList optimizeTaskSequence(const QStringList& tasks, const QJsonObject& constraints);
    bool validatePlan(const ExecutionPlan& plan);
    ExecutionPlan adaptPlanToContext(const ExecutionPlan& originalPlan, const DecisionContext& newContext);
    
    // Learning algorithms
    void updateDecisionModel(const QString& decisionId, const QJsonObject& outcome);
    void identifySuccessPatterns();
    void optimizeParameterWeights();
    double calculateLearningProgress();
    
    // Monitoring algorithms
    void updatePerformanceMetrics();
    QStringList analyzeExecutionTrends();
    bool detectPerformanceDegradation();
    double calculateEfficiencyTrend();
    
    // Recovery algorithms
    QString selectRecoveryStrategy(const QString& errorType, const QJsonObject& errorData);
    bool executeRecoveryPlan(const QString& strategy, const QJsonObject& parameters);
    void preventRecurringErrors(const QString& errorPattern);
    
    // Utility methods
    QString generatePlanId() const;
    QString generateDecisionId() const;
    void logAction(const QString& action, const QJsonObject& parameters);
    void updateState(AgentState newState);
    bool checkSafetyConstraints(const QString& action, const QJsonObject& parameters);
    
    // Data persistence
    bool saveFrameworkState();
    bool loadFrameworkState();
    QString getFrameworkStoragePath() const;
};

} // namespace Agentic
} // namespace RawrXD

Q_DECLARE_METATYPE(RawrXD::Agentic::AutonomousOperationFramework::AutonomyLevel)
Q_DECLARE_METATYPE(RawrXD::Agentic::AutonomousOperationFramework::OperationMode)
Q_DECLARE_METATYPE(RawrXD::Agentic::AutonomousOperationFramework::AgentState)
