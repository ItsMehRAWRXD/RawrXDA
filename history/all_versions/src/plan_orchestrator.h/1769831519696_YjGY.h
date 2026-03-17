/**
 * \file plan_orchestrator.h
 * \brief AI-driven multi-file edit coordinator with planning and execution
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>


namespace RawrXD {

class LSPClient;
class InferenceEngine;

/**
 * \brief Represents a single edit task in a multi-file refactoring plan
 */
struct EditTask {
    std::string filePath;           // Absolute path to file
    int startLine;              // Start line (0-indexed)
    int endLine;                // End line (0-indexed)
    std::string operation;          // "replace", "insert", "delete", "rename"
    std::string oldText;            // Text to replace (for "replace")
    std::string newText;            // Replacement text (for "replace", "insert")
    std::string symbolName;         // Symbol name (for "rename")
    std::string newSymbolName;      // New symbol name (for "rename")
    std::string description;        // Human-readable description
    int priority = 0;           // Execution priority (higher first)
};

/**
 * \brief Planning result with tasks and metadata
 */
struct PlanningResult {
    std::vector<EditTask> tasks;    // List of edit tasks
    std::string planDescription;    // Overall plan description
    std::vector<std::string> affectedFiles;  // List of files to be modified
    int estimatedChanges = 0;   // Estimated number of changes
    bool success = false;       // Planning success flag
    std::string errorMessage;       // Error message if planning failed
};

/**
 * \brief Execution result for a multi-file edit session
 */
struct ExecutionResult {
    int successCount = 0;       // Number of successful edits
    int failureCount = 0;       // Number of failed edits
    std::vector<std::string> successfulFiles; // Files successfully edited
    std::vector<std::string> failedFiles;    // Files that failed to edit
    std::string errorMessage;       // Error message if execution failed
    bool success = false;       // Overall execution success
};

/**
 * \brief AI-driven multi-file edit orchestrator
 * 
 * Features:
 * - Generates edit plans from natural language prompts
 * - Uses InferenceEngine to analyze codebase and create task lists
 * - Executes multi-file edits via LSP workspace/applyEdit
 * - Provides rollback on failure
 * - Supports atomic multi-file refactoring
 * - Two-phase initialization pattern
 */
class PlanOrchestrator
{

public:
    explicit PlanOrchestrator();
    ~PlanOrchestrator() = default;

    /**
     * Two-phase initialization
     * Call after void is ready
     */
    void initialize();

    /**
     * Set LSP client for workspace edits
     */
    void setLSPClient(LSPClient* client);

    /**
     * Set inference engine for AI planning
     */
    void setInferenceEngine(InferenceEngine* engine);

    /**
     * Generate edit plan from user prompt
     * 
     * @param prompt Natural language refactoring request
     * @param workspaceRoot Root directory of workspace
     * @param contextFiles Optional list of files to analyze for context
     * @return Planning result with task list
     */
    PlanningResult generatePlan(const std::string& prompt, 
                                 const std::string& workspaceRoot,
                                 const std::vector<std::string>& contextFiles = std::vector<std::string>());

    /**
     * Execute edit plan
     * 
     * @param plan Planning result from generatePlan()
     * @param dryRun If true, simulate execution without making changes
     * @return Execution result with success/failure counts
     */
    ExecutionResult executePlan(const PlanningResult& plan, bool dryRun = false);

    /**
     * Generate and execute plan in one call
     * 
     * @param prompt Natural language refactoring request
     * @param workspaceRoot Root directory of workspace
     * @param dryRun If true, simulate execution without making changes
     * @return Execution result
     */
    ExecutionResult planAndExecute(const std::string& prompt,
                                    const std::string& workspaceRoot,
                                    bool dryRun = false);

    /**
     * Get current workspace root
     */
    std::string workspaceRoot() const { return m_workspaceRoot; }

    /**
     * Set workspace root directory
     */
    void setWorkspaceRoot(const std::string& root);


    /**
     * Emitted when planning starts
     */
    void planningStarted(const std::string& prompt);

    /**
     * Emitted when planning completes
     */
    void planningCompleted(const PlanningResult& result);

    /**
     * Emitted when execution starts
     */
    void executionStarted(int taskCount);

    /**
     * Emitted for each task execution
     */
    void taskExecuted(int taskIndex, bool success, const std::string& description);

    /**
     * Emitted when execution completes
     */
    void executionCompleted(const ExecutionResult& result);

    /**
     * Emitted on error
     */
    void errorOccurred(const std::string& error);

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

    LSPClient* m_lspClient{};
    InferenceEngine* m_inferenceEngine{};
    std::string m_workspaceRoot;
    
    std::map<std::string, std::string> m_originalFileContents;  // Backup for rollback
    bool m_initialized = false;
};

} // namespace RawrXD

