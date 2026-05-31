/**
 * \file plan_orchestrator.h
 * \brief AI-driven multi-file edit coordinator with planning and execution
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonDocument>

namespace RawrXD {

class LSPClient;
class InferenceEngine;

/**
 * \brief Represents a single edit task in a multi-file refactoring plan
 */
struct EditTask {
    QString filePath;           // Absolute path to file
    int startLine;              // Start line (0-indexed)
    int endLine;                // End line (0-indexed)
    QString operation;          // "replace", "insert", "delete", "rename"
    QString oldText;            // Text to replace (for "replace")
    QString newText;            // Replacement text (for "replace", "insert")
    QString symbolName;         // Symbol name (for "rename")
    QString newSymbolName;      // New symbol name (for "rename")
    QString description;        // Human-readable description
    int priority = 0;           // Execution priority (higher first)
};

/**
 * \brief Planning result with tasks and metadata
 */
struct PlanningResult {
    QVector<EditTask> tasks;    // List of edit tasks
    QString planDescription;    // Overall plan description
    QStringList affectedFiles;  // List of files to be modified
    int estimatedChanges = 0;   // Estimated number of changes
    bool success = false;       // Planning success flag
    QString errorMessage;       // Error message if planning failed
};

/**
 * \brief Execution result for a multi-file edit session
 */
struct ExecutionResult {
    int successCount = 0;       // Number of successful edits
    int failureCount = 0;       // Number of failed edits
    QStringList successfulFiles; // Files successfully edited
    QStringList failedFiles;    // Files that failed to edit
    QString errorMessage;       // Error message if execution failed
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
class PlanOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit PlanOrchestrator(QObject* parent = nullptr);
    ~PlanOrchestrator() override = default;

    /**
     * Two-phase initialization
     * Call after QApplication is ready
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
    PlanningResult generatePlan(const QString& prompt, 
                                 const QString& workspaceRoot,
                                 const QStringList& contextFiles = QStringList());

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
    ExecutionResult planAndExecute(const QString& prompt,
                                    const QString& workspaceRoot,
                                    bool dryRun = false);

    /**
     * Get current workspace root
     */
    QString workspaceRoot() const { return m_workspaceRoot; }

    /**
     * Set workspace root directory
     */
    void setWorkspaceRoot(const QString& root);

signals:
    /**
     * Emitted when planning starts
     */
    void planningStarted(const QString& prompt);

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
    void taskExecuted(int taskIndex, bool success, const QString& description);

    /**
     * Emitted when execution completes
     */
    void executionCompleted(const ExecutionResult& result);

    /**
     * Emitted on error
     */
    void errorOccurred(const QString& error);

private:
    QString buildPlanningPrompt(const QString& userPrompt, const QStringList& contextFiles);
    PlanningResult parsePlanningResponse(const QString& response);
    bool executeTask(const EditTask& task, bool dryRun);
    bool applyReplace(const EditTask& task, bool dryRun);
    bool applyInsert(const EditTask& task, bool dryRun);
    bool applyDelete(const EditTask& task, bool dryRun);
    bool applyRename(const EditTask& task, bool dryRun);
    QStringList gatherContextFiles(const QString& workspaceRoot, int maxFiles = 50);
    QString readFileContent(const QString& filePath);
    bool writeFileContent(const QString& filePath, const QString& content);

    LSPClient* m_lspClient{};
    InferenceEngine* m_inferenceEngine{};
    QString m_workspaceRoot;
    
    QMap<QString, QString> m_originalFileContents;  // Backup for rollback
    bool m_initialized = false;
};

} // namespace RawrXD
