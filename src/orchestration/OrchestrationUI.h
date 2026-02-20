#pragma once

#include "TaskOrchestrator.h"


namespace RawrXD {

class OrchestrationUI {public:
    explicit OrchestrationUI(TaskOrchestrator* orchestrator, void* parent = nullptr);

\nprivate:\n    void onOrchestrateClicked();
    void onTaskSplitCompleted(const std::vector<TaskDefinition>& tasks);
    void onModelSelectionCompleted(const std::map<std::string, std::string>& modelAssignments);
    void onTabCreated(const std::string& tabName, const std::string& model);
    void onTaskStarted(const std::string& taskId, const std::string& model);
    void onTaskProgress(const std::string& taskId, int progress);
    void onTaskCompleted(const OrchestrationResult& result);
    void onOrchestrationCompleted(const std::vector<OrchestrationResult>& results);
    void onErrorOccurred(const std::string& errorMessage);
    void onMemoryProfileChanged(int index);
    void onMemoryStrategyChanged(int index);

private:
    void setupUI();
    void* createInputSection();
    void* createProgressSection();
    void* createResultsSection();
    void* createMemorySettingsSection();
    void updateTaskList();
    void showResults(const std::vector<OrchestrationResult>& results);
    void updateMemoryUsage();
    std::string formatMemorySize(int64_t bytes) const;

    TaskOrchestrator* m_orchestrator = nullptr;
    voidEdit* m_taskInput = nullptr;
    void* m_orchestrateButton = nullptr;
    void* m_statusLabel = nullptr;
    void* m_overallProgress = nullptr;
    void* m_taskList = nullptr;  // HWND list control (was QListWidget*)
    void* m_resultsDisplay = nullptr;
    void* m_memoryProfileCombo = nullptr;
    void* m_memoryStrategyCombo = nullptr;
    void* m_memoryUsageLabel = nullptr;
    void* m_memoryUsageBar = nullptr;
};

} // namespace RawrXD

