#ifndef ML_WORKFLOW_OPTIMIZER_HPP
#define ML_WORKFLOW_OPTIMIZER_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <memory>

struct WorkflowStep {
    std::string id;
    std::string name;
    std::string tool;
    std::unordered_map<std::string, std::string> parameters;
    std::vector<std::string> dependencies;
    int timeoutMs;
    bool parallel;
    double estimatedDuration;
    double actualDuration;
    bool success;
};

struct Workflow {
    std::string id;
    std::string name;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<WorkflowStep> steps;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point completedAt;
    std::string status;
    std::unordered_map<std::string, std::string> results;
    double totalDuration;
    bool optimized;
};

struct MLPrediction {
    std::string workflowId;
    double optimizationScore;
    std::unordered_map<std::string, std::string> suggestedOptimizations;
    double predictedPerformanceGain;
    double confidence;
    std::chrono::system_clock::time_point predictionTime;
};

class MLWorkflowOptimizer {
    
public:
    explicit MLWorkflowOptimizer();
    ~MLWorkflowOptimizer();
    
    // Workflow optimization
    std::unordered_map<std::string, std::string> optimizeWorkflow(const std::unordered_map<std::string, std::string>& originalWorkflow);
    std::unordered_map<std::string, std::string> predictWorkflowPerformance(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> suggestWorkflowImprovements(const std::unordered_map<std::string, std::string>& workflow);
    
    // Machine learning training
    void trainOnHistoricalData(const std::vector<std::unordered_map<std::string, std::string>>& workflowData);
    void updateModelWithNewData(const std::unordered_map<std::string, std::string>& workflowResult);
    void retrainModel();
    
    // Pattern recognition
    std::vector<std::unordered_map<std::string, std::string>> identifyCommonPatterns(const std::vector<std::unordered_map<std::string, std::string>>& workflows);
    std::unordered_map<std::string, std::string> detectAntiPatterns(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> suggestPatternBasedOptimizations(const std::unordered_map<std::string, std::string>& workflow);
    
    // Performance prediction
    double predictExecutionTime(const std::unordered_map<std::string, std::string>& workflow);
    double predictResourceUsage(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> predictBottlenecks(const std::unordered_map<std::string, std::string>& workflow);
    
    // Parallelization optimization
    std::unordered_map<std::string, std::string> optimizeParallelization(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> identifyParallelOpportunities(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> calculateParallelSpeedup(const std::unordered_map<std::string, std::string>& workflow);
    
    // Dependency optimization
    std::unordered_map<std::string, std::string> optimizeDependencies(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> identifyCriticalPath(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> suggestDependencyReduction(const std::unordered_map<std::string, std::string>& workflow);
    
    // Resource optimization
    std::unordered_map<std::string, std::string> optimizeResourceAllocation(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> predictResourceContention(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> suggestResourceOptimizations(const std::unordered_map<std::string, std::string>& workflow);
    
    // ML model management
    std::unordered_map<std::string, std::string> getModelStatus();
    std::unordered_map<std::string, std::string> getTrainingMetrics();
    void exportModel(const std::string& filePath);
    void importModel(const std::string& filePath);
    
    // Enterprise features
    std::unordered_map<std::string, std::string> generateOptimizationReport(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> compareWorkflowVersions(const std::unordered_map<std::string, std::string>& original, const std::unordered_map<std::string, std::string>& optimized);
    std::unordered_map<std::string, std::string> calculateROI(const std::unordered_map<std::string, std::string>& optimization);
    
    // Real-time optimization
    std::unordered_map<std::string, std::string> optimizeInRealTime(const std::unordered_map<std::string, std::string>& workflow, const std::unordered_map<std::string, std::string>& runtimeMetrics);
    std::unordered_map<std::string, std::string> adaptToRuntimeConditions(const std::unordered_map<std::string, std::string>& workflow);
    std::unordered_map<std::string, std::string> suggestDynamicOptimizations(const std::unordered_map<std::string, std::string>& workflow);
    
    // Event callbacks (replacing Qt signals)
    std::function<void(const std::string& workflowId, const std::unordered_map<std::string, std::string>& optimization)> onWorkflowOptimized;
    std::function<void(const std::string& workflowId, double predictedTime)> onPerformancePredictionReady;
    std::function<void(const std::string& workflowId, const std::unordered_map<std::string, std::string>& suggestions)> onOptimizationSuggestionReady;
    std::function<void(const std::unordered_map<std::string, std::string>& trainingResults)> onModelTrainingComplete;
    std::function<void(const std::string& patternType, const std::unordered_map<std::string, std::string>& details)> onPatternDetected;
    std::function<void(const std::string& workflowId, const std::unordered_map<std::string, std::string>& optimization)> onRealTimeOptimizationApplied;
    
private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

#endif // ML_WORKFLOW_OPTIMIZER_HPP