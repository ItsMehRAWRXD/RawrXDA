#pragma once

#include "backend/agentic_tools.h"

class MainWindow;


namespace RawrXD {

struct TaskDefinition {
    std::string id;
    std::string description;
    std::string type;
    int priority = 0;
    int estimatedTokens = 0;
    std::string model;
    int64_t memoryLimit = 0;
    std::string memoryStrategy;
    std::map<std::string, std::any> parameters;
};

struct OrchestrationResult {
    std::string taskId;
    std::string model;
    bool success = false;
    std::string result;
    std::string error;
    int64_t executionTime = 0;
};

class TaskOrchestrator  {public:
    explicit TaskOrchestrator(::MainWindow* parent);
    ~TaskOrchestrator() override;

    RawrXD::Backend::ToolResult executeTool(const std::string& toolName, const void*& params);

    void orchestrateTask(const std::string& naturalLanguageDescription);
    std::vector<TaskDefinition> parseNaturalLanguage(const std::string& description);
    std::string determineTaskType(const std::string& description);
    int estimateTokenCount(const std::string& description);
    std::string selectModelForTask(const TaskDefinition& task);
    void balanceWorkload(std::vector<TaskDefinition>& tasks);
    void executeTask(const TaskDefinition& task);
    void createExecutionTab(const TaskDefinition& task);
    void handleModelResponse(void** reply, const std::string& taskId);
    void* createRollarCoasterRequest(const std::string& model, const std::string& prompt) const;
    std::string generateTaskId() const;
    std::vector<std::string> getAvailableModels() const;
    bool isModelAvailable(const std::string& model) const;
    void setModelPreferences(const std::map<std::string, int>& preferences);
    std::vector<TaskDefinition> getCurrentTasks() const;
    OrchestrationResult getTaskResult(const std::string& taskId) const;
    void cancelTask(const std::string& taskId);
    void setRollarCoasterEndpoint(const std::string& endpoint);
    void setMaxParallelTasks(int maxTasks);
    void setTaskTimeout(int timeoutMs);

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
    bool shouldDecomposeFurther(const TaskDefinition& task) const;
    int64_t calculateMemoryForTask(const TaskDefinition& task) const;
    void applyMemoryConstraints(TaskDefinition& task);
    void balanceWorkloadWithMemory(std::vector<TaskDefinition>& tasks);
    bool canExecuteTask(const TaskDefinition& task) const;
    std::stringList splitDescription(const std::string& description) const;
\npublic:\n    void taskSplitCompleted(const std::vector<TaskDefinition>& tasks);
    void modelSelectionCompleted(const std::map<std::string, std::string>& modelAssignments);
    void tabCreated(const std::string& tabName, const std::string& model);
    void taskStarted(const std::string& taskId, const std::string& model);
    void taskProgress(const std::string& taskId, int progress);
    void taskCompleted(const OrchestrationResult& result);
    void orchestrationCompleted(const std::vector<OrchestrationResult>& results);
    void errorOccurred(const std::string& errorMessage);

private:
    ::MainWindow* m_mainWindow = nullptr;
    void** m_networkManager = nullptr;
    std::string m_rollarCoasterEndpoint;
    int m_maxParallelTasks = 0;
    int m_taskTimeout = 0;

    std::map<std::string, std::stringList> m_modelCapabilities;
    std::map<std::string, int> m_modelWorkloads;
    std::map<std::string, int> m_modelPreferences;

    std::map<std::string, std::map<std::string, int64_t>> m_memoryProfiles;
    std::map<std::string, TaskDefinition> m_activeTasks;
    std::map<std::string, OrchestrationResult> m_completedTasks;
    std::map<std::string, int64_t> m_taskMemoryUsage;

    std::string m_memoryProfile;
    int64_t m_globalMemoryLimit = 0;
    std::string m_defaultMemoryStrategy;
    int64_t m_totalMemoryAllocated = 0;

    RawrXD::Backend::AgenticToolExecutor m_toolExecutor;
};

} // namespace RawrXD




