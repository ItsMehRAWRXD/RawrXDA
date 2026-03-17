#pragma once
/*  TaskOrchestrator.h  -  RollarCoaster AI Orchestration System
    
    This orchestrator enables natural language task description → automatic task splitting → 
    model selection → parallel execution across multiple IDE tabs.
    
    Features:
    - Natural language parsing and intent recognition
    - Intelligent task decomposition into subtasks
    - Model selection based on task type (codellama, deepseek-coder, your-custom-model, mock-model)
    - Dynamic tab creation for parallel execution
    - Workload balancing across available models
    - Real-time progress tracking and result aggregation
    
    Integration with RollarCoaster AI system via ports 11438-11439
*/

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../backend/agentic_tools.h"

// Agentic tool executor for autonomous operations (backend implementation)
// #include "../../src/backend/agentic_tools.h" // TODO: Re-enable when backend is available

class MainWindow;

namespace RawrXD {

struct TaskDefinition {
    QString id;
    QString description;
    QString model;           // Selected model: codellama, deepseek-coder, your-custom-model, mock-model
    QString type;           // Task type: code_generation, explanation, refactoring, analysis, etc.
    int priority;           // Priority level (1-10)
    int estimatedTokens;    // Estimated token count
    QJsonObject parameters; // Task-specific parameters
    
    // Memory management
    qint64 memoryLimit;     // Memory limit in bytes (0 = unlimited)
    QString memoryStrategy; // Memory allocation strategy: "conservative", "balanced", "aggressive"
    
    TaskDefinition() : priority(5), estimatedTokens(1000), memoryLimit(0), memoryStrategy("balanced") {}
};

struct OrchestrationResult {
    QString taskId;
    QString model;
    QString result;
    bool success;
    QString error;
    int executionTime;      // Execution time in milliseconds
    
    OrchestrationResult() : success(false), executionTime(0) {}
};

class TaskOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit TaskOrchestrator(MainWindow* parent = nullptr);
    ~TaskOrchestrator();

    // Main orchestration method
    void orchestrateTask(const QString& naturalLanguageDescription);

    // Configuration
    void setRollarCoasterEndpoint(const QString& endpoint);
    void setMaxParallelTasks(int maxTasks);
    void setTaskTimeout(int timeoutMs);

    // Expose tool execution (agentic/autonomous) similar to AgenticChat
    RawrXD::Backend::ToolResult executeTool(const QString& toolName, const QJsonObject& params);
    
    // Model management
    QList<QString> getAvailableModels() const;
    bool isModelAvailable(const QString& model) const;
    void setModelPreferences(const QHash<QString, int>& preferences);
    
    // Task management
    QList<TaskDefinition> getCurrentTasks() const;
    OrchestrationResult getTaskResult(const QString& taskId) const;
    void cancelTask(const QString& taskId);
    
    // Memory management
    void setMemoryProfile(const QString& profileName);
    void setGlobalMemoryLimit(qint64 limitBytes);
    void setTaskMemoryStrategy(const QString& strategy);
    qint64 getAvailableMemory() const;
    qint64 getTotalMemoryUsage() const;
    bool canAllocateMemory(qint64 requestedBytes) const;
    void allocateTaskMemory(const QString& taskId, qint64 bytes);
    void releaseTaskMemory(const QString& taskId);
    
    // Enhanced task decomposition
    QList<TaskDefinition> decomposeComplexTask(const QString& description);
    QList<TaskDefinition> createMemoryAwareSubtasks(const TaskDefinition& mainTask);
    int calculateOptimalParallelism() const;

signals:
    void taskSplitCompleted(const QList<TaskDefinition>& tasks);
    void modelSelectionCompleted(const QHash<QString, QString>& modelAssignments);
    void tabCreated(const QString& tabName, const QString& model);
    void taskStarted(const QString& taskId, const QString& model);
    void taskProgress(const QString& taskId, int progress);
    void taskCompleted(const OrchestrationResult& result);
    void orchestrationCompleted(const QList<OrchestrationResult>& results);
    void errorOccurred(const QString& errorMessage);

private slots:
    void handleTaskSplitResponse(QNetworkReply* reply);
    void handleModelResponse(QNetworkReply* reply, const QString& taskId);
    void handleTaskCompletion(const QString& taskId, const QString& result);

private:
    // Task parsing and splitting
    QList<TaskDefinition> parseNaturalLanguage(const QString& description);
    QString determineTaskType(const QString& description);
    int estimateTokenCount(const QString& description);
    
    // Model selection
    QString selectModelForTask(const TaskDefinition& task);
    QHash<QString, int> getModelCapabilities() const;
    void balanceWorkload(QList<TaskDefinition>& tasks);
    
    // Execution management
    void executeTask(const TaskDefinition& task);
    void createExecutionTab(const TaskDefinition& task);
    void sendToRollarCoaster(const QString& model, const QString& prompt);
    
    // Utility methods
    QString generateTaskId() const;
    QJsonObject createRollarCoasterRequest(const QString& model, const QString& prompt) const;
    
    MainWindow* m_mainWindow;
    QNetworkAccessManager* m_networkManager;

    // Agentic tool executor for autonomous tool access
    RawrXD::Backend::AgenticToolExecutor m_toolExecutor;
    
    // Configuration
    QString m_rollarCoasterEndpoint;
    int m_maxParallelTasks;
    int m_taskTimeout;
    
    // Memory management
    QString m_memoryProfile;           // Current memory profile: "minimal", "standard", "large", "unlimited"
    qint64 m_globalMemoryLimit;        // Global memory limit in bytes
    QString m_defaultMemoryStrategy;   // Default memory strategy
    QHash<QString, qint64> m_taskMemoryUsage; // Memory usage per task
    qint64 m_totalMemoryAllocated;     // Total memory currently allocated
    
    // State management
    QHash<QString, TaskDefinition> m_activeTasks;
    QHash<QString, OrchestrationResult> m_completedTasks;
    QHash<QString, int> m_modelWorkloads;
    QHash<QString, int> m_modelPreferences;
    
    // Model capabilities mapping
    QHash<QString, QList<QString>> m_modelCapabilities;
    
    // Memory profiles (KB to GB ranges)
    QHash<QString, QHash<QString, qint64>> m_memoryProfiles;
};

} // namespace RawrXD