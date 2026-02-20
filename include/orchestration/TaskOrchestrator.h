#pragma once
/*  TaskOrchestrator.h  -  RollarCoaster AI Orchestration (C++20, no Qt)
    
    Natural language task → task splitting → model selection → parallel execution.
*/

#include <functional>
#include <map>
#include <string>
#include <vector>

class MainWindow;

namespace RawrXD {
namespace Backend {
struct ToolResult;
struct AgenticToolExecutor;
}

struct TaskDefinition {
    std::string id;
    std::string description;
    std::string model;
    std::string type;
    int priority = 5;
    int estimatedTokens = 1000;
    std::string parameters;   // JSON-serialized (replaces QJsonObject)
    int64_t memoryLimit = 0;
    std::string memoryStrategy = "balanced";
};

struct OrchestrationResult {
    std::string taskId;
    std::string model;
    std::string result;
    bool success = false;
    std::string error;
    int executionTime = 0;
};

class TaskOrchestrator {
public:
    explicit TaskOrchestrator(MainWindow* parent = nullptr);
    ~TaskOrchestrator();

    void orchestrateTask(const std::string& naturalLanguageDescription);

    RawrXD::Backend::ToolResult executeTool(const std::string& toolName, const std::string& paramsJson);

    std::vector<std::string> getAvailableModels() const;
    bool isModelAvailable(const std::string& model) const;
    void setModelPreferences(const std::map<std::string, int>& preferences);

    std::vector<TaskDefinition> getCurrentTasks() const;
    OrchestrationResult getTaskResult(const std::string& taskId) const;
    void cancelTask(const std::string& taskId);

    void setMemoryProfile(const std::string& profileName);
    void setGlobalMemoryLimit(int64_t limitBytes);
    void setTaskMemoryStrategy(const std::string& strategy);
    int64_t getAvailableMemory() const;
    int64_t getTotalMemoryUsage() const;
    bool canAllocateMemory(int64_t requestedBytes) const;
    void allocateTaskMemory(const std::string& taskId, int64_t bytes);
    void releaseTaskMemory(const std::string& taskId);

    std::vector<TaskDefinition> decomposeComplexTask(const std::string& description);
    std::vector<TaskDefinition> createMemoryAwareSubtasks(const TaskDefinition& mainTask);
    int calculateOptimalParallelism() const;

    using TaskSplitCompletedFn = std::function<void(const std::vector<TaskDefinition>&)>;
    using ModelSelectionCompletedFn = std::function<void(const std::map<std::string, std::string>&)>;
    using TabCreatedFn = std::function<void(const std::string& tabName, const std::string& model)>;
    using TaskStartedFn = std::function<void(const std::string& taskId, const std::string& model)>;
    using TaskProgressFn = std::function<void(const std::string& taskId, int progress)>;
    using TaskCompletedFn = std::function<void(const OrchestrationResult&)>;
    using OrchestrationCompletedFn = std::function<void(const std::vector<OrchestrationResult>&)>;
    using ErrorFn = std::function<void(const std::string&)>;

    void setOnTaskSplitCompleted(TaskSplitCompletedFn fn) { m_onTaskSplitCompleted = std::move(fn); }
    void setOnModelSelectionCompleted(ModelSelectionCompletedFn fn) { m_onModelSelectionCompleted = std::move(fn); }
    void setOnTabCreated(TabCreatedFn fn) { m_onTabCreated = std::move(fn); }
    void setOnTaskStarted(TaskStartedFn fn) { m_onTaskStarted = std::move(fn); }
    void setOnTaskProgress(TaskProgressFn fn) { m_onTaskProgress = std::move(fn); }
    void setOnTaskCompleted(TaskCompletedFn fn) { m_onTaskCompleted = std::move(fn); }
    void setOnOrchestrationCompleted(OrchestrationCompletedFn fn) { m_onOrchestrationCompleted = std::move(fn); }
    void setOnErrorOccurred(ErrorFn fn) { m_onErrorOccurred = std::move(fn); }

private:
    void handleTaskSplitResponse(void* reply);
    void handleModelResponse(void* reply, const std::string& taskId);
    void handleTaskCompletion(const std::string& taskId, const std::string& result);

    std::vector<TaskDefinition> parseNaturalLanguage(const std::string& description);
    std::string determineTaskType(const std::string& description);
    int estimateTokenCount(const std::string& description);

    std::string selectModelForTask(const TaskDefinition& task);
    std::map<std::string, int> getModelCapabilities() const;
    void balanceWorkload(std::vector<TaskDefinition>& tasks);

    void executeTask(const TaskDefinition& task);
    void createExecutionTab(const TaskDefinition& task);
    void sendToRollarCoaster(const std::string& model, const std::string& prompt);

    std::string generateTaskId() const;
    std::string createRollarCoasterRequest(const std::string& model, const std::string& prompt) const;

    MainWindow* m_mainWindow = nullptr;
    void* m_networkManager = nullptr;  // WinHTTP or similar

    RawrXD::Backend::AgenticToolExecutor* m_toolExecutor = nullptr;

    std::string m_rollarCoasterEndpoint;
    int m_maxParallelTasks = 4;
    int m_taskTimeout = 60000;

    std::string m_memoryProfile;
    int64_t m_globalMemoryLimit = 0;
    std::string m_defaultMemoryStrategy;
    std::map<std::string, int64_t> m_taskMemoryUsage;
    int64_t m_totalMemoryAllocated = 0;

    std::map<std::string, TaskDefinition> m_activeTasks;
    std::map<std::string, OrchestrationResult> m_completedTasks;
    std::map<std::string, int> m_modelWorkloads;
    std::map<std::string, int> m_modelPreferences;
    std::map<std::string, std::vector<std::string>> m_modelCapabilities;
    std::map<std::string, std::map<std::string, int64_t>> m_memoryProfiles;

    TaskSplitCompletedFn m_onTaskSplitCompleted;
    ModelSelectionCompletedFn m_onModelSelectionCompleted;
    TabCreatedFn m_onTabCreated;
    TaskStartedFn m_onTaskStarted;
    TaskProgressFn m_onTaskProgress;
    TaskCompletedFn m_onTaskCompleted;
    OrchestrationCompletedFn m_onOrchestrationCompleted;
    ErrorFn m_onErrorOccurred;
};

} // namespace RawrXD
