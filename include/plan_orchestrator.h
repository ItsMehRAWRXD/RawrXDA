<<<<<<< HEAD
/**
 * plan_orchestrator.h — C++20, no Qt. AI-driven multi-file edit coordinator.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace RawrXD {

class LSPClient;
class InferenceEngine;

struct EditTask {
    std::string filePath;
    int startLine = 0;
    int endLine = 0;
    std::string operation;
    std::string oldText;
    std::string newText;
    std::string symbolName;
    std::string newSymbolName;
    std::string description;
    int priority = 0;
};

struct PlanningResult {
    std::vector<EditTask> tasks;
    std::string planDescription;
    std::vector<std::string> affectedFiles;
    int estimatedChanges = 0;
    bool success = false;
    std::string errorMessage;
};

struct ExecutionResult {
    int successCount = 0;
    int failureCount = 0;
    std::vector<std::string> successfulFiles;
    std::vector<std::string> failedFiles;
    std::string errorMessage;
    bool success = false;
};

class PlanOrchestrator
{
public:
    using PlanningStartedFn   = std::function<void(const std::string& prompt)>;
    using PlanningCompletedFn = std::function<void(const PlanningResult&)>;
    using ExecutionStartedFn  = std::function<void(int taskCount)>;
    using TaskExecutedFn      = std::function<void(int taskIndex, bool success, const std::string& description)>;
    using ExecutionCompletedFn = std::function<void(const ExecutionResult&)>;
    using ErrorOccurredFn     = std::function<void(const std::string&)>;

    PlanOrchestrator() = default;
    ~PlanOrchestrator() = default;

    void setOnPlanningStarted(PlanningStartedFn f)     { m_onPlanningStarted = std::move(f); }
    void setOnPlanningCompleted(PlanningCompletedFn f)   { m_onPlanningCompleted = std::move(f); }
    void setOnExecutionStarted(ExecutionStartedFn f)    { m_onExecutionStarted = std::move(f); }
    void setOnTaskExecuted(TaskExecutedFn f)            { m_onTaskExecuted = std::move(f); }
    void setOnExecutionCompleted(ExecutionCompletedFn f) { m_onExecutionCompleted = std::move(f); }
    void setOnErrorOccurred(ErrorOccurredFn f)          { m_onErrorOccurred = std::move(f); }

    void initialize();
    void setLSPClient(LSPClient* client);
    void setInferenceEngine(InferenceEngine* engine);

    PlanningResult generatePlan(const std::string& prompt,
                                const std::string& workspaceRoot,
                                const std::vector<std::string>& contextFiles = {});

    ExecutionResult executePlan(const PlanningResult& plan, bool dryRun = false);
    ExecutionResult planAndExecute(const std::string& prompt,
                                   const std::string& workspaceRoot,
                                   bool dryRun = false);

    std::string workspaceRoot() const { return m_workspaceRoot; }
    void setWorkspaceRoot(const std::string& root);

private:
    std::string buildPlanningPrompt(const std::string& userPrompt, const std::vector<std::string>& contextFiles);
    PlanningResult parsePlanningResponse(const std::string& response);
    bool executeTask(const EditTask& task, bool dryRun);
    bool applyReplace(const EditTask& task, bool dryRun);
    bool applyInsert(const EditTask& task, bool dryRun);
    bool applyDelete(const EditTask& task, bool dryRun);
    bool applyRename(const EditTask& task, bool dryRun);
    std::vector<std::string> gatherContextFiles(const std::string& workspaceRoot, int maxFiles = 50);
    std::string readFileContent(const std::string& filePath);
    bool writeFileContent(const std::string& filePath, const std::string& content);

    LSPClient* m_lspClient = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::string m_workspaceRoot;
    std::map<std::string, std::string> m_originalFileContents;
    bool m_initialized = false;

    PlanningStartedFn   m_onPlanningStarted;
    PlanningCompletedFn m_onPlanningCompleted;
    ExecutionStartedFn  m_onExecutionStarted;
    TaskExecutedFn      m_onTaskExecuted;
    ExecutionCompletedFn m_onExecutionCompleted;
    ErrorOccurredFn     m_onErrorOccurred;
};

} // namespace RawrXD
=======
/**
 * plan_orchestrator.h — C++20, no Qt. AI-driven multi-file edit coordinator.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace RawrXD {

class LSPClient;
class InferenceEngine;

struct EditTask {
    std::string filePath;
    int startLine = 0;
    int endLine = 0;
    std::string operation;
    std::string oldText;
    std::string newText;
    std::string symbolName;
    std::string newSymbolName;
    std::string description;
    int priority = 0;
};

struct PlanningResult {
    std::vector<EditTask> tasks;
    std::string planDescription;
    std::vector<std::string> affectedFiles;
    int estimatedChanges = 0;
    bool success = false;
    std::string errorMessage;
};

struct ExecutionResult {
    int successCount = 0;
    int failureCount = 0;
    std::vector<std::string> successfulFiles;
    std::vector<std::string> failedFiles;
    std::string errorMessage;
    bool success = false;
};

class PlanOrchestrator
{
public:
    using PlanningStartedFn   = std::function<void(const std::string& prompt)>;
    using PlanningCompletedFn = std::function<void(const PlanningResult&)>;
    using ExecutionStartedFn  = std::function<void(int taskCount)>;
    using TaskExecutedFn      = std::function<void(int taskIndex, bool success, const std::string& description)>;
    using ExecutionCompletedFn = std::function<void(const ExecutionResult&)>;
    using ErrorOccurredFn     = std::function<void(const std::string&)>;

    PlanOrchestrator() = default;
    ~PlanOrchestrator() = default;

    void setOnPlanningStarted(PlanningStartedFn f)     { m_onPlanningStarted = std::move(f); }
    void setOnPlanningCompleted(PlanningCompletedFn f)   { m_onPlanningCompleted = std::move(f); }
    void setOnExecutionStarted(ExecutionStartedFn f)    { m_onExecutionStarted = std::move(f); }
    void setOnTaskExecuted(TaskExecutedFn f)            { m_onTaskExecuted = std::move(f); }
    void setOnExecutionCompleted(ExecutionCompletedFn f) { m_onExecutionCompleted = std::move(f); }
    void setOnErrorOccurred(ErrorOccurredFn f)          { m_onErrorOccurred = std::move(f); }

    void initialize();
    void setLSPClient(LSPClient* client);
    void setInferenceEngine(InferenceEngine* engine);

    PlanningResult generatePlan(const std::string& prompt,
                                const std::string& workspaceRoot,
                                const std::vector<std::string>& contextFiles = {});

    ExecutionResult executePlan(const PlanningResult& plan, bool dryRun = false);
    ExecutionResult planAndExecute(const std::string& prompt,
                                   const std::string& workspaceRoot,
                                   bool dryRun = false);

    std::string workspaceRoot() const { return m_workspaceRoot; }
    void setWorkspaceRoot(const std::string& root);

private:
    std::string buildPlanningPrompt(const std::string& userPrompt, const std::vector<std::string>& contextFiles);
    PlanningResult parsePlanningResponse(const std::string& response);
    bool executeTask(const EditTask& task, bool dryRun);
    bool applyReplace(const EditTask& task, bool dryRun);
    bool applyInsert(const EditTask& task, bool dryRun);
    bool applyDelete(const EditTask& task, bool dryRun);
    bool applyRename(const EditTask& task, bool dryRun);
    std::vector<std::string> gatherContextFiles(const std::string& workspaceRoot, int maxFiles = 50);
    std::string readFileContent(const std::string& filePath);
    bool writeFileContent(const std::string& filePath, const std::string& content);

    LSPClient* m_lspClient = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::string m_workspaceRoot;
    std::map<std::string, std::string> m_originalFileContents;
    bool m_initialized = false;

    PlanningStartedFn   m_onPlanningStarted;
    PlanningCompletedFn m_onPlanningCompleted;
    ExecutionStartedFn  m_onExecutionStarted;
    TaskExecutedFn      m_onTaskExecuted;
    ExecutionCompletedFn m_onExecutionCompleted;
    ErrorOccurredFn     m_onErrorOccurred;
};

} // namespace RawrXD
>>>>>>> origin/main
