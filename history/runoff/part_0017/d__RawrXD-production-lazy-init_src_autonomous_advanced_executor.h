#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <memory>

/**
 * @class AutonomousAdvancedExecutor
 * @brief Advanced autonomous execution with sophisticated planning and learning
 * 
 * Provides high-level autonomous capabilities:
 * - Intelligent task planning and decomposition
 * - Multi-agent coordination
 * - Adaptive strategy selection
 * - Continuous learning and improvement
 * - Advanced error recovery
 * - Performance optimization
 */
class AutonomousAdvancedExecutor : public QObject {
    Q_OBJECT

public:
    explicit AutonomousAdvancedExecutor(QObject* parent = nullptr);
    ~AutonomousAdvancedExecutor();

    // Core autonomous operations
    QJsonObject executeDynamicTask(const QString& userRequest);
    QJsonObject executeContextualTask(const QString& task, const QJsonObject& context);
    QJsonArray planIntelligentSequence(const QString& goal, const QJsonObject& constraints);
    
    // Adaptive strategy management
    QString selectBestStrategy(const QString& task, const QJsonArray& availableStrategies);
    QJsonObject evaluateStrategyEffectiveness(const QString& strategy, const QJsonObject& results);
    void adaptStrategySelectionFromHistory();
    
    // Multi-stage planning
    QJsonArray generateInitialPlan(const QString& goal);
    QJsonArray refinePlan(const QJsonArray& initialPlan, int iterations);
    QJsonObject executePlan(const QJsonArray& plan);
    
    // Advanced learning
    void recordExecutionOutcome(const QString& taskId, const QJsonObject& outcome);
    QJsonObject predictTaskOutcome(const QString& task);
    void updateLearningModel(const QString& feedback);
    QJsonArray suggestImprovements();
    
    // Constraint management
    QJsonObject validateConstraints(const QString& task, const QJsonObject& constraints);
    QJsonArray suggestConstraintRelaxation(const QString& task);
    
    // Parallel execution
    QJsonArray executeTasksInParallel(const QJsonArray& tasks);
    QJsonArray executeWithDependencies(const QJsonArray& tasks);
    
    // Fallback and recovery strategies
    QJsonObject executeWithFallbacks(const QString& primaryTask, const QJsonArray& fallbackTasks);
    QJsonObject recoverFromPartialFailure(const QString& taskId, const QJsonObject& failureInfo);
    
    // Performance analytics
    QJsonObject getStrategyPerformanceAnalytics();
    QJsonObject analyzePlanExecutionMetrics();
    QString identifyPerformanceBottlenecks();
    
    // Configuration
    void enableLearning(bool enable);
    void setAdaptationAggressiveness(double factor);  // 0.0 - 1.0
    void setConstraintStrictness(double factor);      // 0.0 - 1.0
    void setParallelizationThreshold(int taskCount);

signals:
    void taskStarted(const QString& taskId);
    void stageCompleted(const QString& stageName, const QJsonObject& result);
    void strategyChanged(const QString& newStrategy, const QString& reason);
    void learningUpdated(const QJsonObject& improvements);
    void performanceOptimized(const QString& optimization);
    void taskCompleted(const QString& taskId, bool success);
    void adaptationOccurred(const QString& change);

private:
    // Strategy management
    struct Strategy {
        QString name;
        QString description;
        double successRate = 0.0;
        int timesUsed = 0;
        int timesSucceeded = 0;
        QJsonObject parameters;
        QDateTime lastUsed;
    };
    
    // Task history for learning
    struct TaskRecord {
        QString taskId;
        QString taskDescription;
        Strategy usedStrategy;
        QJsonObject context;
        QJsonObject outcome;
        qint64 executionTime;
        bool successful = false;
        QDateTime timestamp;
    };
    
    // Internal methods
    QString analyzeTaskCharacteristics(const QString& task);
    Strategy findOptimalStrategy(const QString& taskCharacteristics);
    QJsonArray decomposeComplexTask(const QString& goal, int maxDepth);
    bool validatePlanFeasibility(const QJsonArray& plan);
    QJsonArray optimizePlanSequence(const QJsonArray& plan);
    
    // Learning methods
    void updateStrategyPerformance(const Strategy& strategy, bool successful);
    double calculateStrategyScore(const Strategy& strategy) const;
    QJsonObject generatePrediction(const QString& task);
    
    // Parallelization
    bool canParallelize(const QJsonArray& steps) const;
    QJsonArray identifyDependencies(const QJsonArray& steps) const;
    
    // Recovery
    QJsonObject analyzeFailure(const QJsonObject& failureInfo);
    QString suggestRecoveryAction(const QJsonObject& failureAnalysis);
    
    // Members
    QMap<QString, Strategy> m_strategies;
    QVector<TaskRecord> m_taskHistory;
    QMap<QString, double> m_performanceMetrics;
    
    bool m_learningEnabled = true;
    double m_adaptationAggressiveness = 0.7;
    double m_constraintStrictness = 0.8;
    int m_parallelizationThreshold = 3;
    
    QJsonObject m_learningModel;
    QJsonArray m_strategyHistory;
};
