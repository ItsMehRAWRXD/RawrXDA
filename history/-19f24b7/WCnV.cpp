/**
 * @file action_executor.cpp
 * @brief Implementation of action execution engine
 *
 * Executes agent-generated actions with comprehensive error handling,
 * backup/restore, and observability.
 */

#include "action_executor.hpp"
#include <iostream>
#include <algorithm>
#include <thread>

/**
 * @brief Constructor
 */
ActionExecutor::ActionExecutor()
    : m_stopRequested(false)
{
    m_context.projectRoot = "";
}

/**
 * @brief Destructor
 */
ActionExecutor::~ActionExecutor() = default;

/**
 * @brief Set execution context
 */
void ActionExecutor::setContext(const ExecutionContext& context)
{
    m_context = context;
}

/**
 * @brief Execute single action (synchronous)
 */
void ActionExecutor::executeAction(const Action& action)
{
    // Implementation would go here
    actionStarted(m_context.currentActionIndex, action.description);
    
    // Stub for now
    bool success = true;
    std::string result = "Action executed successfully";
    
    actionCompleted(m_context.currentActionIndex, success, result);
}

/**
 * @brief Execute complete plan (asynchronous)
 */
void ActionExecutor::executePlan(const std::vector<Action>& plan)
{
    m_stopRequested = false;
    planStarted(static_cast<int>(plan.size()));

    std::thread([this, plan]() {
        bool overallSuccess = true;

        for (int i = 0; i < plan.size() && !m_stopRequested; ++i) {
            m_context.currentActionIndex = i;
            executeAction(plan[i]);
            
            // if (plan[i].error != "") { overallSuccess = false; break; }
        }

        planCompleted(overallSuccess);
    }).detach();
}

/**
 * @brief Cancel execution
 */
void ActionExecutor::cancelExecution()
{
    m_stopRequested = true;
}

/**
 * @brief Rollback action
 */
bool ActionExecutor::rollbackAction(int actionIndex)
{
    if (actionIndex < 0 || actionIndex >= m_executedActions.size()) {
        return false;
    }

    const Action& action = m_executedActions[actionIndex];

    // Only file edits are rollbackable
    if (action.type != ActionType::FileEdit) {
        // // qWarning:  "[ActionExecutor] Action type not rollbackable";
        return false;
    }

    if (!m_backups.contains(action.target)) {
        // // qWarning:  "[ActionExecutor] No backup found for" << action.target;
        return false;
    }

    return restoreFromBackup(action.target);
}

/**
 * @brief Get aggregated result
 */
void* ActionExecutor::getAggregatedResult() const
{
    void* result;
    void* actions;

    for (const auto& action : m_executedActions) {
        void* actionObj;
        actionObj["description"] = action.description;
        actionObj["success"] = action.success;
        actionObj["result"] = action.result;
        if (!action.error.empty()) {
            actionObj["error"] = action.error;
        }
        actions.append(actionObj);
    }

    result["actions"] = actions;
    result["state"] = m_context.state;

    return result;
}

// ─────────────────────────────────────────────────────────────────────────
// Action Handlers
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Handle file edit action
 */
bool ActionExecutor::handleFileEdit(Action& action)
{
    std::string filePath = m_context.projectRoot + "/" + action.target;
    std::string editAction = action.params.value("action").toString();
    std::string content = action.params.value("content").toString();

    // Validate safety
    if (!validateFileEditSafety(filePath, editAction)) {
        action.error = "File edit failed safety validation";
        return false;
    }

    if (m_context.dryRun) {
        action.result = "DRY RUN: Would edit " + filePath;
        return true;
    }

    // Create backup
    if (!createBackup(filePath)) {
        // // qWarning:  "[ActionExecutor] Failed to backup" << filePath;
    }

    // File operation removed;

    if (editAction == "create") {
        // Create new file
        if (!file.open(std::iostream::WriteOnly | std::iostream::Text)) {
            action.error = "Failed to create file: " + file.errorString();
            return false;
        }
        file.write(content.toUtf8());
        file.close();
        action.result = "File created: " + filePath;
        return true;

    } else if (editAction == "append") {
        // Append to existing file
        if (!file.open(std::iostream::Append | std::iostream::Text)) {
            action.error = "Failed to open file for append: " + file.errorString();
            return false;
        }
        file.write(content.toUtf8());
        file.close();
        action.result = "Appended to: " + filePath;
        return true;

    } else if (editAction == "replace") {
        // Replace entire file
        if (!file.open(std::iostream::WriteOnly | std::iostream::Text)) {
            action.error = "Failed to open file for writing: " + file.errorString();
            return false;
        }
        file.write(content.toUtf8());
        file.close();
        action.result = "Replaced: " + filePath;
        return true;

    } else if (editAction == "delete") {
        // Delete file
        if (!std::filesystem::remove(filePath)) {
            action.error = "Failed to delete file";
            return false;
        }
        action.result = "Deleted: " + filePath;
        return true;

    } else {
        action.error = "Unknown edit action: " + editAction;
        return false;
    }
}

/**
 * @brief Handle file search action
 */
bool ActionExecutor::handleSearchFiles(Action& action)
{
    std::string searchPath = m_context.projectRoot + "/" + action.params.value("path").toString();
    std::string pattern = action.params.value("pattern").toString();
    std::string query = action.params.value("query").toString();

    // dir(searchPath);
    if (!dir.exists()) {
        action.error = "Search path does not exist: " + searchPath;
        return false;
    }

    std::vector<std::string> files = dir// Dir listing, // Dir::Files, // Dir::Name);

    void* results;
    int matchCount = 0;

    for (const // Info& fileInfo : files) {
        if (query.empty()) {
            // Just list files
            void* fileObj;
            fileObj["path"] = fileInfo.string();
            fileObj["size"] = (int)fileInfo.size();
            results.append(fileObj);
        } else {
            // Search content
            // File operation removed);
            if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
                std::string content = file.readAll();
                file.close();

                if (content.contains(query)) {
                    void* match;
                    match["file"] = fileInfo.string();
                    match["matches"] = content.count(query);
                    results.append(match);
                    matchCount++;
                }
            }
        }
    }

    void* result;
    result["files_searched"] = files.size();
    result["matches"] = matchCount;
    result["results"] = results;

    action.result = void*(result).toJson(void*::Compact);
    return true;
}

/**
 * @brief Handle build action
 */
bool ActionExecutor::handleRunBuild(Action& action)
{
    std::string target = action.params.value("target").toString("all");
    std::string config = action.params.value("config").toString("Release");

    std::stringList args = {"--build", "build", "--config", config};
    if (target != "all") {
        args << "--target" << target;
    }

    void* result = executeCommand("cmake", args, m_context.timeoutMs);

    action.result = void*(result).toJson(void*::Compact);
    return result.value("exitCode") == 0;
}

/**
 * @brief Handle test action
 */
bool ActionExecutor::handleExecuteTests(Action& action)
{
    std::string testTarget = action.params.value("target").toString("all_tests");

    std::stringList args;
    if (testTarget != "all_tests") {
        args << testTarget;
    }

    void* result = executeCommand("ctest", args, m_context.timeoutMs);

    action.result = void*(result).toJson(void*::Compact);
    return result.value("exitCode") == 0;
}

/**
 * @brief Handle git action
 */
bool ActionExecutor::handleCommitGit(Action& action)
{
    std::string gitAction = action.params.value("action").toString();
    std::string message = action.params.value("message").toString();
    std::string branch = action.params.value("branch").toString();

    std::stringList args;

    if (gitAction == "commit") {
        args << "commit" << "-m" << message;
    } else if (gitAction == "push") {
        args << "push" << (branch.empty() ? "origin" : "origin " + branch);
    } else if (gitAction == "add") {
        args << "add" << action.params.value("files").toString();
    } else {
        action.error = "Unknown git action: " + gitAction;
        return false;
    }

    void* result = executeCommand("git", args, m_context.timeoutMs);

    action.result = void*(result).toJson(void*::Compact);
    return result.value("exitCode") == 0;
}

/**
 * @brief Handle arbitrary command
 */
bool ActionExecutor::handleInvokeCommand(Action& action)
{
    std::string command = action.params.value("command").toString();
    std::stringList args;

    if (action.params.contains("args")) {
        if (action.params.value("args").isArray()) {
            for (const void*& arg : action.params.value("args").toArray()) {
                args << arg.toString();
            }
        } else {
            args << action.params.value("args").toString();
        }
    }

    void* result = executeCommand(command, args, m_context.timeoutMs);

    action.result = void*(result).toJson(void*::Compact);
    return result.value("exitCode") == 0;
}

/**
 * @brief Handle recursive agent invocation
 */
bool ActionExecutor::handleRecursiveAgent(Action& action)
{
    // Placeholder for recursive agent call
    // Would invoke ModelInvoker again with new wish
    action.result = "Recursive agent invocation not yet implemented";
    return false;
}

/**
 * @brief Handle user query
 */
bool ActionExecutor::handleQueryUser(Action& action)
{
    std::string query = action.params.value("query").toString();
    std::stringList options;

    if (action.params.value("options").isArray()) {
        for (const void*& opt : action.params.value("options").toArray()) {
            options << opt.toString();
        }
    }

    userInputNeeded(query, options);

    // Wait for user response (would be connected externally)
    action.result = "User query: " + query;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// Utility Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse JSON action
 */
Action ActionExecutor::parseJsonAction(const void*& jsonAction)
{
    Action action;
    action.type = stringToActionType(jsonAction.value("type").toString());
    action.target = jsonAction.value("target").toString();
    action.params = jsonAction.value("params").toObject();
    action.description = jsonAction.value("description").toString();

    return action;
}

/**
 * @brief Create backup
 */
bool ActionExecutor::createBackup(const std::string& filePath)
{
    if (!// Info::exists(filePath)) {
        return true; // No need to backup non-existent file
    }

    std::string backupPath = filePath + ".backup." + // DateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    bool success = std::filesystem::copy(filePath, backupPath);
    if (success) {
        m_backups[filePath] = backupPath;
        // // qDebug:  "[ActionExecutor] Backup created:" << backupPath;
    }

    return success;
}

/**
 * @brief Restore from backup
 */
bool ActionExecutor::restoreFromBackup(const std::string& filePath)
{
    if (!m_backups.contains(filePath)) {
        return false;
    }

    std::string backupPath = m_backups[filePath];

    if (!std::filesystem::copy(backupPath, filePath)) {
        return false;
    }

    // // qDebug:  "[ActionExecutor] Restored from backup:" << backupPath;
    return true;
}

/**
 * @brief Execute command
 */
void* ActionExecutor::executeCommand(const std::string& command,
                                            const std::stringList& args,
                                            int timeoutMs)
{
    void* result;
    result["command"] = command;
    result["args"] = void*::fromStringList(args);

    if (m_context.dryRun) {
        result["exitCode"] = 0;
        result["stdout"] = "DRY RUN: Would execute " + command + " " + args.join(" ");
        return result;
    }

    m_process->setWorkingDirectory(m_context.projectRoot);
    m_process->start(command, args);

    if (!m_process->waitForFinished(timeoutMs)) {
        m_process->kill();
        result["exitCode"] = -1;
        result["error"] = "Command timed out after " + std::string::number(timeoutMs) + "ms";
        return result;
    }

    result["exitCode"] = m_process->exitCode();
    result["stdout"] = std::string::fromUtf8(m_process->readAllStandardOutput());
    result["stderr"] = std::string::fromUtf8(m_process->readAllStandardError());

    return result;
}

/**
 * @brief Validate file edit safety
 */
bool ActionExecutor::validateFileEditSafety(const std::string& filePath, const std::string& action)
{
    // Prevent modifications to system files
    if (filePath.contains("C:\\Windows") || filePath.contains("/etc/") || 
        filePath.contains("/System/")) {
        // // qWarning:  "[ActionExecutor] Blocked system file modification:" << filePath;
        return false;
    }

    // For delete operations, require explicit confirmation
    if (action == "delete") {
        // // qWarning:  "[ActionExecutor] File deletion requires explicit approval:" << filePath;
        // In real implementation, would query user
        return false;
    }

    return true;
}

/**
 * @brief String to ActionType conversion
 */
ActionType ActionExecutor::stringToActionType(const std::string& typeStr) const
{
    if (typeStr == "file_edit") return ActionType::FileEdit;
    if (typeStr == "search_files") return ActionType::SearchFiles;
    if (typeStr == "run_build") return ActionType::RunBuild;
    if (typeStr == "execute_tests") return ActionType::ExecuteTests;
    if (typeStr == "commit_git") return ActionType::CommitGit;
    if (typeStr == "invoke_command") return ActionType::InvokeCommand;
    if (typeStr == "recursive_agent") return ActionType::RecursiveAgent;
    if (typeStr == "query_user") return ActionType::QueryUser;

    return ActionType::Unknown;
}

// Private  implementations - called by Qt signal/ mechanism
void ActionExecutor::onActionTaskFinished()
{
    // // qDebug:  "[ActionExecutor] Action task finished";
    // Process completion of current action task
}

void ActionExecutor::onProcessFinished(int exitCode, void*::ExitStatus exitStatus)
{
    // // qDebug:  "[ActionExecutor] Process finished with exit code:" << exitCode
             << "status:" << static_cast<int>(exitStatus);
    // Process completion of external process execution
}









