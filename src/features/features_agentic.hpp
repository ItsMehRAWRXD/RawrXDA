#pragma once
/**
 * features_agentic.hpp - Features 19-28: Agentic Capabilities
 *
 * 19. Autonomous Multi-File Editing
 * 20. Composer/Agent Mode
 * 21. Background Agents
 * 22. Parallel Cloud Agents
 * 23. Terminal Command Execution
 * 24. Test-Driven Iteration
 * 25. Pull Request Creation
 * 26. Issue-to-Implementation
 * 27. Plan/Act Mode Separation
 * 28. Boomerang Tasks
 */

#include "ai_ide_features.hpp"
#include <queue>
#include <condition_variable>

namespace rawrxd {

// Forward declarations
class Feature_CodebaseIndex;

//=============================================================================
// FEATURE 19: Autonomous Multi-File Editing
//=============================================================================

class Feature_MultiFileEdit {
public:
    struct EditPlan {
        std::string filePath;
        std::vector<TextEdit> edits;
        std::string reason;
        uint32_t priority = 0;
    };

    struct EditResult {
        bool success = false;
        std::string filePath;
        std::string error;
        std::string diff;
    };

    std::vector<EditPlan> planEdits(const std::string& goal,
                                     const std::vector<std::string>& filePaths);
    std::vector<EditResult> executeEdits(const std::vector<EditPlan>& plans);
    std::vector<EditResult> previewEdits(const std::vector<EditPlan>& plans);

    void setEditor(EditorIntegration* editor);
    void setProvider(std::shared_ptr<AIProvider> provider);
    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index);

private:
    EditorIntegration* editor_ = nullptr;
    std::shared_ptr<AIProvider> provider_;
    std::shared_ptr<Feature_CodebaseIndex> index_;
};

//=============================================================================
// FEATURE 20: Composer/Agent Mode
//=============================================================================

class Feature_ComposerMode {
public:
    struct Task {
        std::string id;
        std::string description;
        std::string status;
        uint32_t progress = 0;
        std::vector<std::string> filesModified;
        std::string output;
    };

    std::future<Task> executeTask(const std::string& description);
    Task getTaskStatus(const std::string& taskId);
    void cancelTask(const std::string& taskId);

    void setEditor(EditorIntegration* editor) { editor_ = editor; }
    void setTerminal(TerminalInterface* terminal) { terminal_ = terminal; }
    void setProvider(std::shared_ptr<AIProvider> provider) { provider_ = provider; }
    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index) { index_ = index; }

private:
    EditorIntegration* editor_ = nullptr;
    TerminalInterface* terminal_ = nullptr;
    std::shared_ptr<AIProvider> provider_;
    std::shared_ptr<Feature_CodebaseIndex> index_;

    std::unordered_map<std::string, Task> tasks_;
    std::mutex tasksMutex_;
};

//=============================================================================
// FEATURE 21: Background Agents
//=============================================================================

class Feature_BackgroundAgents {
public:
    struct AgentConfig {
        std::string name;
        std::string task;
        uint32_t timeoutMs = 300000;
        bool continueOnError = false;
    };

    std::string spawnAgent(const AgentConfig& config);
    std::future<std::string> waitForAgent(const std::string& agentId);
    void cancelAgent(const std::string& agentId);
    std::vector<std::string> getActiveAgents();

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::unordered_map<std::string, std::thread> agents_;
    std::mutex mutex_;
};

//=============================================================================
// FEATURE 22: Parallel Cloud Agents
//=============================================================================

class Feature_ParallelAgents {
public:
    struct AgentResult {
        std::string agentId;
        bool success = false;
        std::string output;
        std::chrono::milliseconds duration{0};
    };

    std::vector<std::string> spawnParallel(
        const std::vector<std::string>& tasks);
    std::vector<AgentResult> waitForAll();
    std::vector<AgentResult> waitForAny();

    void setMaxParallel(uint32_t max);
    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    uint32_t maxParallel_ = 3;
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 23: Terminal Command Execution
//=============================================================================

class Feature_TerminalExecution {
public:
    struct CommandResult {
        int exitCode = 0;
        std::string standardOut;
        std::string standardErr;
        std::chrono::milliseconds duration{0};
    };

    std::future<CommandResult> execute(const std::string& command);
    std::future<CommandResult> executeWithRetry(
        const std::string& command,
        uint32_t maxRetries = 3);
    void interrupt();

    void setTerminal(TerminalInterface* terminal);

private:
    TerminalInterface* terminal_ = nullptr;
};

//=============================================================================
// FEATURE 24: Test-Driven Iteration
//=============================================================================

class Feature_TestIteration {
public:
    struct Iteration {
        uint32_t attempt = 0;
        TestResult testResult;
        std::string fixApplied;
        bool converged = false;
    };

    std::vector<Iteration> iterate(
        const std::string& testCommand,
        const std::string& codePath,
        uint32_t maxIterations = 10);

    void setProvider(std::shared_ptr<AIProvider> provider);
    void setEditor(EditorIntegration* editor);

private:
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;
};

//=============================================================================
// FEATURE 25: Pull Request Creation
//=============================================================================

class Feature_PRCreation {
public:
    struct PRInfo {
        std::string title;
        std::string body;
        std::string branch;
        std::vector<std::string> files;
    };

    PRInfo generatePR(const std::string& branch, const std::string& baseBranch = "main");
    void createPR(const PRInfo& pr);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 26: Issue-to-Implementation
//=============================================================================

class Feature_IssueToImplementation {
public:
    std::future<std::vector<TextEdit>> implementIssue(
        const std::string& issueTitle,
        const std::string& issueDescription);

    void setProvider(std::shared_ptr<AIProvider> provider);
    void setIndex(std::shared_ptr<Feature_CodebaseIndex> index);
    void setEditor(EditorIntegration* editor);

private:
    std::shared_ptr<AIProvider> provider_;
    std::shared_ptr<Feature_CodebaseIndex> index_;
    EditorIntegration* editor_ = nullptr;
};

//=============================================================================
// FEATURE 27: Plan/Act Mode Separation
//=============================================================================

class Feature_PlanActMode {
public:
    enum class Mode { Plan, Act };

    struct Plan {
        std::vector<std::string> steps;
        std::vector<std::string> filesToRead;
        std::vector<std::string> filesToModify;
        std::string summary;
    };

    Plan createPlan(const std::string& goal);
    void switchMode(Mode mode);
    Mode getCurrentMode() const;
    void executePlan(const Plan& plan);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    Mode currentMode_ = Mode::Plan;
    std::shared_ptr<AIProvider> provider_;
    Plan currentPlan_;
};

//=============================================================================
// FEATURE 28: Boomerang Tasks
//=============================================================================

class Feature_BoomerangTasks {
public:
    enum class Stage { Architect, Code, Test, Review };

    struct BoomerangResult {
        Stage stage = Stage::Architect;
        std::string output;
        std::vector<TextEdit> edits;
        bool success = false;
    };

    std::future<BoomerangResult> execute(
        const std::string& goal,
        std::function<bool(const BoomerangResult&)> shouldContinue);

    void setProvider(std::shared_ptr<AIProvider> provider);
    void setEditor(EditorIntegration* editor);

private:
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;
};

} // namespace rawrxd
