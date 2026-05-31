#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD {

class InferenceEngine;
class LSPClient;
class UniversalModelRouter;

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
    std::vector<std::string> observations;
    std::vector<std::string> loopMemory;
    int iterations = 0;
    std::string errorMessage;
    bool success = false;
};

class PlanOrchestrator {
public:
    struct Step {
        std::string description;
        bool isComplete = false;
        std::string result;
    };

    PlanOrchestrator();
    ~PlanOrchestrator();

    void initialize();
    void setLSPClient(LSPClient* client);
    void setInferenceEngine(InferenceEngine* engine);
    void setModelRouter(UniversalModelRouter* router);
    void setTerminalCommandSink(std::function<void(const std::string&)> sink);

    void createPlan(const std::string& goal);
    void executeNextStep();
    std::vector<Step> getPlan() const;
    bool isComplete() const;

    PlanningResult generatePlan(const std::string& prompt,
                                const std::string& workspaceRoot,
                                const std::vector<std::string>& contextFiles = {});
    ExecutionResult executePlan(const PlanningResult& plan, bool dryRun = false);
    ExecutionResult planAndExecute(const std::string& prompt,
                                   const std::string& workspaceRoot,
                                   bool dryRun = false);
    
    ExecutionResult runAutonomousLoop(const std::string& userGoal, int maxSteps = 10);
    void observeExternalEvent(const std::string& message);
    void observeTerminalOutput(const std::string& source, const std::string& chunk, bool isError = false);

    // Stop a running plan: clears all event callbacks so in-flight background threads
    // stop emitting to the UI. The PlanOrchestrator instance remains valid for potential
    // re-initialization via initializePlanOrchestrator().
    void requestStop();

    const std::string& workspaceRoot() const { return m_workspaceRoot; }
    void setWorkspaceRoot(const std::string& root);
    void notifyWorkspaceChanged();

    std::function<void(const std::string&)> onStepCompleted;
    std::function<void(const std::string&)> onPlanCompleted;
    std::function<void(const std::string&)> onPlanningStarted;
    std::function<void(const std::string&)> onPlanningChunk;
    std::function<void(const PlanningResult&)> onPlanningCompleted;
    std::function<void(int)> onExecutionStarted;
    std::function<void(int, bool, const std::string&)> onTaskExecuted;
    std::function<void(const ExecutionResult&)> onExecutionCompleted;
    std::function<void(const std::string&)> onErrorOccurred;
    std::function<bool(const EditTask&)> onTaskApprovalRequired;
    std::function<void()> onWorkspaceChanged;
private:
    void decomposeGoal(const std::string& goal);
    std::string buildPlanningPrompt(const std::string& userPrompt,
                                    const std::vector<std::string>& contextFiles);
    std::string generateModelBackedPlan(const std::string& userPrompt,
                                        const std::vector<std::string>& contextFiles);
    std::string generateInferenceBackedPlan(const std::string& userPrompt,
                                            const std::vector<std::string>& contextFiles);
    void recordExecutionObservation(const std::string& message);
    void rememberLoopEvent(const std::string& message);
    std::string consumeExecutionObservations();
    std::string buildLoopMemoryContext() const;
    std::vector<std::string> snapshotLoopMemory() const;
    ExecutionResult runExecutionLoop(const std::string& userGoal,
                                     const std::string& workspaceRoot,
                                     int maxSteps,
                                     bool dryRun);
    PlanningResult parsePlanningResponse(const std::string& response);
    void validatePlanSymbols(PlanningResult& plan, const std::vector<std::string>& contextFiles);
    bool executeTask(const EditTask& task, bool dryRun);
    bool executeCommandTask(const EditTask& task, bool dryRun);
    bool executeReadTask(const EditTask& task, bool dryRun);
    bool executeListTask(const EditTask& task, bool dryRun);
    bool executeSearchTask(const EditTask& task, bool dryRun);
    bool applyWrite(const EditTask& task, bool dryRun);
    bool applyReplace(const EditTask& task, bool dryRun);
    bool applyInsert(const EditTask& task, bool dryRun);
    bool applyDelete(const EditTask& task, bool dryRun);
    bool applyRename(const EditTask& task, bool dryRun);
    std::vector<std::string> gatherContextFiles(const std::string& workspaceRoot, int maxFiles = 50);
    std::string readFileContent(const std::string& filePath);
    bool writeFileContent(const std::string& filePath, const std::string& content);
    std::string resolvePath(const std::string& filePath) const;
    std::string selectPlanningModel() const;

    std::vector<Step> m_steps;
    int m_currentStepIndex = 0;
    mutable std::mutex m_mutex;

    LSPClient* m_lspClient = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    UniversalModelRouter* m_modelRouter = nullptr;
    std::function<void(const std::string&)> m_terminalCommandSink;
    std::vector<std::string> m_executionObservations;
    std::vector<std::string> m_loopMemory;
    std::map<std::string, std::string> m_terminalObservationBuffers;
    std::string m_workspaceRoot;
    bool m_initialized = false;
};

} // namespace RawrXD

