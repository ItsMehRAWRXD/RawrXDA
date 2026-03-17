#ifndef ML_WORKFLOW_OPTIMIZER_HPP
#define ML_WORKFLOW_OPTIMIZER_HPP

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

struct WorkflowStep {
    QString id;
    QString name;
    QString tool;
    QJsonObject parameters;
    QStringList dependencies;
    int timeoutMs;
    bool parallel;
    double estimatedDuration;
    double actualDuration;
    bool success;
};

struct Workflow {
    QString id;
    QString name;
    QJsonObject metadata;
    QList<WorkflowStep> steps;
    QDateTime createdAt;
    QDateTime completedAt;
    QString status;
    QJsonObject results;
    double totalDuration;
    bool optimized;
};

struct MLPrediction {
    QString workflowId;
    double optimizationScore;
    QJsonObject suggestedOptimizations;
    double predictedPerformanceGain;
    double confidence;
    QDateTime predictionTime;
};

class MLWorkflowOptimizer : public QObject {
    Q_OBJECT
    
public:
    explicit MLWorkflowOptimizer(QObject *parent = nullptr);
    ~MLWorkflowOptimizer();
    
    // Workflow optimization
    QJsonObject optimizeWorkflow(const QJsonObject& originalWorkflow);
    QJsonObject predictWorkflowPerformance(const QJsonObject& workflow);
    QJsonObject suggestWorkflowImprovements(const QJsonObject& workflow);
    
    // Machine learning training
    void trainOnHistoricalData(const QJsonArray& workflowData);
    void updateModelWithNewData(const QJsonObject& workflowResult);
    void retrainModel();
    
    // Pattern recognition
    QJsonArray identifyCommonPatterns(const QJsonArray& workflows);
    QJsonObject detectAntiPatterns(const QJsonObject& workflow);
    QJsonObject suggestPatternBasedOptimizations(const QJsonObject& workflow);
    
    // Performance prediction
    double predictExecutionTime(const QJsonObject& workflow);
    double predictResourceUsage(const QJsonObject& workflow);
    QJsonObject predictBottlenecks(const QJsonObject& workflow);
    
    // Parallelization optimization
    QJsonObject optimizeParallelization(const QJsonObject& workflow);
    QJsonObject identifyParallelOpportunities(const QJsonObject& workflow);
    QJsonObject calculateParallelSpeedup(const QJsonObject& workflow);
    
    // Dependency optimization
    QJsonObject optimizeDependencies(const QJsonObject& workflow);
    QJsonObject identifyCriticalPath(const QJsonObject& workflow);
    QJsonObject suggestDependencyReduction(const QJsonObject& workflow);
    
    // Resource optimization
    QJsonObject optimizeResourceAllocation(const QJsonObject& workflow);
    QJsonObject predictResourceContention(const QJsonObject& workflow);
    QJsonObject suggestResourceOptimizations(const QJsonObject& workflow);
    
    // ML model management
    QJsonObject getModelStatus();
    QJsonObject getTrainingMetrics();
    void exportModel(const QString& filePath);
    void importModel(const QString& filePath);
    
    // Enterprise features
    QJsonObject generateOptimizationReport(const QJsonObject& workflow);
    QJsonObject compareWorkflowVersions(const QJsonObject& original, const QJsonObject& optimized);
    QJsonObject calculateROI(const QJsonObject& optimization);
    
    // Real-time optimization
    QJsonObject optimizeInRealTime(const QJsonObject& workflow, const QJsonObject& runtimeMetrics);
    QJsonObject adaptToRuntimeConditions(const QJsonObject& workflow);
    QJsonObject suggestDynamicOptimizations(const QJsonObject& workflow);
    
signals:
    void workflowOptimized(const QString& workflowId, const QJsonObject& optimization);
    void performancePredictionReady(const QString& workflowId, double predictedTime);
    void optimizationSuggestionReady(const QString& workflowId, const QJsonObject& suggestions);
    void modelTrainingComplete(const QJsonObject& trainingResults);
    void patternDetected(const QString& patternType, const QJsonObject& details);
    void realTimeOptimizationApplied(const QString& workflowId, const QJsonObject& optimization);
    
private:
    class Private;
    QScopedPointer<Private> d_ptr;
};

#endif // ML_WORKFLOW_OPTIMIZER_HPP