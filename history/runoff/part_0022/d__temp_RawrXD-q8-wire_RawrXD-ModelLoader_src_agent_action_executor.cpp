/**
 * @file action_executor.cpp
 * @brief Implementation of action execution engine
 *
 * Executes agent-generated actions with comprehensive error handling,
 * backup/restore, and observability.
 */

#include "action_executor.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace RawrXD;

/**
 * @brief Constructor
 */
ActionExecutor::ActionExecutor()
    : m_process(std::make_unique<ToolExecutionEngine>())
{
    m_context.projectRoot = std::filesystem::current_path().string();
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
    std::cout << "[ActionExecutor] Context set - projectRoot: " << m_context.projectRoot << std::endl;
}

/**
 * @brief Execute single action (synchronous)
 */
bool ActionExecutor::executeAction(Action& action)
{
    std::cout << "[ActionExecutor] Executing action: " << action.description << std::endl;

    switch (action.type) {
    case ActionType::FileEdit:
        return handleFileEdit(action);
    case ActionType::SearchFiles:
        return handleSearchFiles(action);
    case ActionType::RunBuild:
        return handleRunBuild(action);
    case ActionType::ExecuteTests:
        return handleExecuteTests(action);
    case ActionType::CommitGit:
        return handleCommitGit(action);
    case ActionType::InvokeCommand:
        return handleInvokeCommand(action);
    case ActionType::RecursiveAgent:
        return handleRecursiveAgent(action);
    case ActionType::QueryUser:
        return handleQueryUser(action);
    default:
        action.error = "Unknown action type";
        return false;
    }
}

/**
 * @brief Execute complete plan (asynchronous)
 */
void ActionExecutor::executePlan(const nlohmann::json& actions, bool stopOnError)
{
    m_isExecuting = true;
    m_stopOnError = stopOnError;
    m_cancelled = false;
    m_executedActions.clear();
    m_backups.clear();

    m_context.totalActions = static_cast<int>(actions.size());
    if (m_onPlanStarted) {
        m_onPlanStarted(static_cast<int>(actions.size()));
    }

    // Run on background thread
    std::async(std::launch::async, [this, actions]() {
        bool overallSuccess = true;

        for (int i = 0; i < static_cast<int>(actions.size()) && !m_cancelled; ++i) {
            if (!actions[i].is_object()) {
                std::cerr << "[ActionExecutor] Invalid action at index " << i << std::endl;
                overallSuccess = false;
                if (m_stopOnError) break;
                continue;
            }

            Action action = parseJsonAction(actions[i]);
            m_context.currentActionIndex = i;

            if (m_onActionStarted) {
                m_onActionStarted(i, action.description);
            }
            if (m_onProgressUpdated) {
                m_onProgressUpdated(i, m_context.totalActions);
            }

            bool success = executeAction(action);
            action.executed = true;
            action.success = success;

            m_executedActions.push_back(action);

            nlohmann::json result;
            result["target"] = action.target;
            result["success"] = success;
            if (!action.error.empty()) {
                result["error"] = action.error;
            }
            if (!action.result.empty()) {
                result["result"] = action.result;
            }

            if (m_onActionCompleted) {
                m_onActionCompleted(i, success, result);
            }

            if (!success) {
                overallSuccess = false;
                if (m_onActionFailed) {
                    m_onActionFailed(i, action.error, m_stopOnError);
                }

                if (m_stopOnError) {
                    std::cerr << "[ActionExecutor] Stopping due to error" << std::endl;
                    break;
                }
            }
        }

        m_isExecuting = false;

        nlohmann::json finalResult;
        finalResult["success"] = overallSuccess;
        finalResult["actionsExecuted"] = m_executedActions.size();
        finalResult["state"] = m_context.state;

        if (m_onPlanCompleted) {
            m_onPlanCompleted(overallSuccess, finalResult);
        }
    });
}

/**
 * @brief Cancel execution
 */
void ActionExecutor::cancelExecution()
{
    m_cancelled = true;
    std::cout << "[ActionExecutor] Execution cancelled" << std::endl;
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
        std::cerr << "[ActionExecutor] Action type not rollbackable" << std::endl;
        return false;
    }

    if (m_backups.find(action.target) == m_backups.end()) {
        std::cerr << "[ActionExecutor] No backup found for " << action.target << std::endl;
        return false;
    }

    return restoreFromBackup(action.target);
}

/**
 * @brief Get aggregated result
 */
nlohmann::json ActionExecutor::getAggregatedResult() const
{
    nlohmann::json result;
    nlohmann::json actions = nlohmann::json::array();

    for (const auto& action : m_executedActions) {
        nlohmann::json actionObj;
        actionObj["description"] = action.description;
        actionObj["success"] = action.success;
        actionObj["result"] = action.result;
        if (!action.error.empty()) {
            actionObj["error"] = action.error;
        }
        actions.push_back(actionObj);
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
    std::string editAction = action.params.contains("action") ? action.params["action"].get<std::string>() : "";
    std::string content = action.params.contains("content") ? action.params["content"].get<std::string>() : "";

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
        std::cerr << "[ActionExecutor] Failed to backup " << filePath << std::endl;
    }

    if (editAction == "create") {
        // Create new file
        std::ofstream file(filePath);
        if (!file.is_open()) {
            action.error = "Failed to create file: " + filePath;
            return false;
        }
        file << content;
        file.close();
        action.result = "File created: " + filePath;
        return true;

    } else if (editAction == "append") {
        // Append to existing file
        std::ofstream file(filePath, std::ios::app);
        if (!file.is_open()) {
            action.error = "Failed to open file for append: " + filePath;
            return false;
        }
        file << content;
        file.close();
        action.result = "Appended to: " + filePath;
        return true;

    } else if (editAction == "replace") {
        // Replace entire file
        std::ofstream file(filePath, std::ios::trunc);
        if (!file.is_open()) {
            action.error = "Failed to open file for writing: " + filePath;
            return false;
        }
        file << content;
        file.close();
        action.result = "Replaced: " + filePath;
        return true;

    } else if (editAction == "delete") {
        // Delete file
        std::error_code ec;
        if (!std::filesystem::remove(filePath, ec)) {
            action.error = "Failed to delete file: " + ec.message();
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
    std::string searchPath = m_context.projectRoot + "/" + (action.params.contains("path") ? action.params["path"].get<std::string>() : ".");
    std::string pattern = action.params.contains("pattern") ? action.params["pattern"].get<std::string>() : "*";
    std::string query = action.params.contains("query") ? action.params["query"].get<std::string>() : "";

    if (!std::filesystem::exists(searchPath)) {
        action.error = "Search path does not exist: " + searchPath;
        return false;
    }

    nlohmann::json results = nlohmann::json::array();
    int matchCount = 0;
    int filesSearched = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
            if (!entry.is_regular_file()) continue;

            filesSearched++;
            std::string path = entry.path().string();

            // Simple pattern matching (basic)
            if (pattern != "*" && path.find(pattern) == std::string::npos) {
                continue;
            }

            if (query.empty()) {
                // Just list files
                nlohmann::json fileObj;
                fileObj["path"] = path;
                fileObj["size"] = static_cast<int>(entry.file_size());
                results.push_back(fileObj);
            } else {
                // Search content
                std::ifstream file(path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string content = buffer.str();
                    file.close();

                    size_t pos = content.find(query, 0);
                    int count = 0;
                    while (pos != std::string::npos) {
                        count++;
                        pos = content.find(query, pos + query.length());
                    }

                    if (count > 0) {
                        nlohmann::json match;
                        match["file"] = path;
                        match["matches"] = count;
                        results.push_back(match);
                        matchCount++;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        action.error = std::string("Search failed: ") + e.what();
        return false;
    }

    nlohmann::json result;
    result["files_searched"] = filesSearched;
    result["matches"] = matchCount;
    result["results"] = results;

    action.result = result.dump();
    return true;
}

/**
 * @brief Handle build action
 */
bool ActionExecutor::handleRunBuild(Action& action)
{
    std::string target = action.params.contains("target") ? action.params["target"].get<std::string>() : "all";
    std::string config = action.params.contains("config") ? action.params["config"].get<std::string>() : "Release";

    std::vector<std::string> args = {"--build", "build", "--config", config};
    if (target != "all") {
        args.push_back("--target");
        args.push_back(target);
    }

    nlohmann::json result = executeCommand("cmake", args, m_context.timeoutMs);

    action.result = result.dump();
    return result.contains("exitCode") && result["exitCode"].get<int>() == 0;
}

/**
 * @brief Handle test action
 */
bool ActionExecutor::handleExecuteTests(Action& action)
{
    std::string testTarget = action.params.contains("target") ? action.params["target"].get<std::string>() : "all_tests";

    std::vector<std::string> args;
    if (testTarget != "all_tests") {
        args.push_back(testTarget);
    }

    nlohmann::json result = executeCommand("ctest", args, m_context.timeoutMs);

    action.result = result.dump();
    return result.contains("exitCode") && result["exitCode"].get<int>() == 0;
}

/**
 * @brief Handle git action
 */
bool ActionExecutor::handleCommitGit(Action& action)
{
    std::string gitAction = action.params.contains("action") ? action.params["action"].get<std::string>() : "";
    std::string message = action.params.contains("message") ? action.params["message"].get<std::string>() : "";
    std::string branch = action.params.contains("branch") ? action.params["branch"].get<std::string>() : "";

    std::vector<std::string> args;

    if (gitAction == "commit") {
        args.push_back("commit");
        args.push_back("-m");
        args.push_back(message);
    } else if (gitAction == "push") {
        args.push_back("push");
        args.push_back(branch.empty() ? "origin" : "origin " + branch);
    } else if (gitAction == "add") {
        args.push_back("add");
        args.push_back(action.params.contains("files") ? action.params["files"].get<std::string>() : ".");
    } else {
        action.error = "Unknown git action: " + gitAction;
        return false;
    }

    nlohmann::json result = executeCommand("git", args, m_context.timeoutMs);

    action.result = result.dump();
    return result.contains("exitCode") && result["exitCode"].get<int>() == 0;
}

/**
 * @brief Handle arbitrary command
 */
bool ActionExecutor::handleInvokeCommand(Action& action)
{
    std::string command = action.params.contains("command") ? action.params["command"].get<std::string>() : "";
    std::vector<std::string> args;

    if (action.params.contains("args")) {
        if (action.params["args"].is_array()) {
            for (const auto& arg : action.params["args"]) {
                if (arg.is_string()) {
                    args.push_back(arg.get<std::string>());
                }
            }
        } else if (action.params["args"].is_string()) {
            args.push_back(action.params["args"].get<std::string>());
        }
    }

    nlohmann::json result = executeCommand(command, args, m_context.timeoutMs);

    action.result = result.dump();
    return result.contains("exitCode") && result["exitCode"].get<int>() == 0;
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
    std::string query = action.params.contains("query") ? action.params["query"].get<std::string>() : "";
    std::vector<std::string> options;

    if (action.params.contains("options") && action.params["options"].is_array()) {
        for (const auto& opt : action.params["options"]) {
            if (opt.is_string()) {
                options.push_back(opt.get<std::string>());
            }
        }
    }

    if (m_onUserInputNeeded) {
        m_onUserInputNeeded(query, options);
    }

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
Action ActionExecutor::parseJsonAction(const nlohmann::json& jsonAction)
{
    Action action;
    if (jsonAction.contains("type")) {
        action.type = stringToActionType(jsonAction["type"].get<std::string>());
    }
    if (jsonAction.contains("target")) {
        action.target = jsonAction["target"].get<std::string>();
    }
    if (jsonAction.contains("params")) {
        action.params = jsonAction["params"];
    }
    if (jsonAction.contains("description")) {
        action.description = jsonAction["description"].get<std::string>();
    }

    return action;
}

/**
 * @brief Create backup
 */
bool ActionExecutor::createBackup(const std::string& filePath)
{
    if (!std::filesystem::exists(filePath)) {
        return true; // No need to backup non-existent file
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    
    std::string backupPath = filePath + ".backup." + ss.str();

    std::error_code ec;
    std::filesystem::copy(filePath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) {
        m_backups[filePath] = backupPath;
        std::cout << "[ActionExecutor] Backup created: " << backupPath << std::endl;
        return true;
    }

    return false;
}

/**
 * @brief Restore from backup
 */
bool ActionExecutor::restoreFromBackup(const std::string& filePath)
{
    if (m_backups.find(filePath) == m_backups.end()) {
        return false;
    }

    std::string backupPath = m_backups[filePath];

    std::error_code ec;
    std::filesystem::copy(backupPath, filePath, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) {
        std::cout << "[ActionExecutor] Restored from backup: " << backupPath << std::endl;
        return true;
    }

    return false;
}

/**
 * @brief Execute command
 */
nlohmann::json ActionExecutor::executeCommand(const std::string& command,
                                            const std::vector<std::string>& args,
                                            int timeoutMs)
{
    nlohmann::json result;
    result["command"] = command;
    result["args"] = args;

    if (m_context.dryRun) {
        result["exitCode"] = 0;
        std::string cmdStr = command;
        for (const auto& arg : args) cmdStr += " " + arg;
        result["stdout"] = "DRY RUN: Would execute " + cmdStr;
        return result;
    }

    ExecutionResult execResult = m_process->executeCommand(command, args, m_context.projectRoot, static_cast<uint32_t>(timeoutMs));

    result["exitCode"] = execResult.exitCode;
    result["stdout"] = execResult.stdoutContent;
    result["stderr"] = execResult.stderrContent;
    if (!execResult.errorMessage.empty()) {
        result["error"] = execResult.errorMessage;
    }
    result["timedOut"] = execResult.timedOut;

    return result;
}

/**
 * @brief Validate file edit safety
 */
bool ActionExecutor::validateFileEditSafety(const std::string& filePath, const std::string& action)
{
    // Prevent modifications to system files
    if (filePath.find("C:\\Windows") != std::string::npos || 
        filePath.find("/etc/") != std::string::npos || 
        filePath.find("/System/") != std::string::npos) {
        std::cerr << "[ActionExecutor] Blocked system file modification: " << filePath << std::endl;
        return false;
    }

    // For delete operations, require explicit confirmation
    if (action == "delete") {
        std::cerr << "[ActionExecutor] File deletion requires explicit approval: " << filePath << std::endl;
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

/**
 * @brief Handle background task completion
 */
void ActionExecutor::onActionTaskFinished()
{
    std::cout << "[ActionExecutor] Background action task finished" << std::endl;
    // Process next action in queue if available
    if (m_isExecuting && m_context.currentActionIndex < m_context.totalActions - 1) {
        m_context.currentActionIndex++;
        // Continue with next action
    }
}

/**
 * @brief Handle process completion
 */
void ActionExecutor::onProcessFinished(int exitCode, int exitStatus)
{
    std::cout << "[ActionExecutor] Process finished with exit code: " << exitCode 
             << " status: " << exitStatus << std::endl;
    
    if (m_process) {
        std::string stdout_data = m_process->readAllStandardOutput();
        std::string stderr_data = m_process->readAllStandardError();
        
        std::cout << "[ActionExecutor] stdout: " << stdout_data << std::endl;
        if (!stderr_data.empty()) {
            std::cout << "[ActionExecutor] stderr: " << stderr_data << std::endl;
        }
    }
}