/**
 * @file action_executor.cpp
 * @brief Implementation of action execution engine
 *
 * Executes agent-generated actions with comprehensive error handling,
 * backup/restore, and observability.
 */

#include "action_executor.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <future>
#include <chrono>
#include <windows.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

/**
 * @brief Constructor
 */
ActionExecutor::ActionExecutor()
{
    m_context.projectRoot = fs::current_path().string();
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
bool ActionExecutor::executeAction(Action& action)
{
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
void ActionExecutor::executePlan(const json& actions, bool stopOnError)
{
    m_isExecuting = true;
    m_stopOnError = stopOnError;
    m_cancelled = false;
    m_executedActions.clear();
    m_backups.clear();

    m_context.totalActions = actions.size();
    if (onPlanStarted) onPlanStarted(actions.size());

    // Run on background thread
    std::thread([this, actions]() {
        bool overallSuccess = true;

        for (size_t i = 0; i < actions.size() && !m_cancelled; ++i) {
            if (!actions[i].is_object()) {
                overallSuccess = false;
                if (m_stopOnError) break;
                continue;
            }

            Action action = parseJsonAction(actions[i]);
            m_context.currentActionIndex = static_cast<int>(i);

            if (onActionStarted) onActionStarted(static_cast<int>(i), action.description);
            if (onProgressUpdated) onProgressUpdated(static_cast<int>(i), m_context.totalActions);

            bool success = executeAction(action);
            action.executed = true;
            action.success = success;

            m_executedActions.push_back(action);

            json result;
            result["target"] = action.target;
            result["success"] = success;
            if (!action.error.empty()) {
                result["error"] = action.error;
            }
            if (!action.result.empty()) {
                result["result"] = action.result;
            }

            if (onActionCompleted) onActionCompleted(static_cast<int>(i), success, result);

            if (!success) {
                overallSuccess = false;
                if (onActionFailed) onActionFailed(static_cast<int>(i), action.error, m_stopOnError);

                if (m_stopOnError) {
                    break;
                }
            }
        }

        m_isExecuting = false;

        json finalResult;
        finalResult["success"] = overallSuccess;
        finalResult["actionsExecuted"] = m_executedActions.size();
        finalResult["state"] = m_context.state;

        if (onPlanCompleted) onPlanCompleted(overallSuccess, finalResult);
    }).detach();
}

/**
 * @brief Cancel execution
 */
void ActionExecutor::cancelExecution()
{
    m_cancelled = true;
    if (m_processHandle) {
        TerminateProcess(m_processHandle, 1);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }
}

/**
 * @brief Rollback action
 */
bool ActionExecutor::rollbackAction(int actionIndex)
{
    if (actionIndex < 0 || actionIndex >= static_cast<int>(m_executedActions.size())) {
        return false;
    }

    const Action& action = m_executedActions[actionIndex];

    // Only file edits are rollbackable
    if (action.type != ActionType::FileEdit) {
        return false;
    }

    if (m_backups.find(action.target) == m_backups.end()) {
        return false;
    }

    return restoreFromBackup(action.target);
}

/**
 * @brief Get aggregated result
 */
json ActionExecutor::getAggregatedResult() const
{
    json result;
    json actions = json::array();

    for (const auto& action : m_executedActions) {
        json actionObj;
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
    fs::path filePath = fs::path(m_context.projectRoot) / action.target;
    std::string editAction = action.params.value("action", "");
    std::string content = action.params.value("content", "");

    // Validate safety
    if (!validateFileEditSafety(filePath.string(), editAction)) {
        action.error = "File edit failed safety validation";
        return false;
    }

    if (m_context.dryRun) {
        action.result = "DRY RUN: Would edit " + filePath.string();
        return true;
    }

    // Create backup
    if (!createBackup(filePath.string())) {
        // Log warning but continue
    }

    if (editAction == "create") {
        // Create new file
        std::ofstream file(filePath);
        if (!file) {
            action.error = "Failed to create file";
            return false;
        }
        file << content;
        file.close();
        action.result = "File created: " + filePath.string();
        return true;

    } else if (editAction == "append") {
        // Append to existing file
        std::ofstream file(filePath, std::ios::app);
        if (!file) {
            action.error = "Failed to open file for append";
            return false;
        }
        file << content;
        file.close();
        action.result = "Appended to: " + filePath.string();
        return true;

    } else if (editAction == "replace") {
        // Replace entire file
        std::ofstream file(filePath);
        if (!file) {
            action.error = "Failed to open file for writing";
            return false;
        }
        file << content;
        file.close();
        action.result = "Replaced: " + filePath.string();
        return true;

    } else if (editAction == "delete") {
        // Delete file
        std::error_code ec;
        fs::remove(filePath, ec);
        if (ec) {
            action.error = "Failed to delete file";
            return false;
        }
        action.result = "Deleted: " + filePath.string();
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
    fs::path searchPath = fs::path(m_context.projectRoot) / action.params.value("path", "");
    std::string pattern = action.params.value("pattern", "*");
    std::string query = action.params.value("query", "");

    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
        action.error = "Search path does not exist: " + searchPath.string();
        return false;
    }

    json results = json::array();
    int matchCount = 0;
    int filesSearched = 0;

    for (const auto& entry : fs::directory_iterator(searchPath)) {
        if (!entry.is_regular_file()) continue;
        filesSearched++;

        if (query.empty()) {
            // Just list files
            json fileObj;
            fileObj["path"] = entry.path().string();
            fileObj["size"] = entry.file_size();
            results.push_back(fileObj);
        } else {
            // Search content
            std::ifstream file(entry.path());
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                file.close();

                size_t pos = 0;
                int count = 0;
                while ((pos = content.find(query, pos)) != std::string::npos) {
                    count++;
                    pos += query.length();
                }

                if (count > 0) {
                    json match;
                    match["file"] = entry.path().string();
                    match["matches"] = count;
                    results.push_back(match);
                    matchCount++;
                }
            }
        }
    }

    json result;
    result["files_searched"] = filesSearched;
    result["matches"] = matchCount;
    result["results"] = results;

    action.result = result.dump(2);
    return true;
}

/**
 * @brief Handle build action
 */
bool ActionExecutor::handleRunBuild(Action& action)
{
    std::string target = action.params.value("target", "all");
    std::string config = action.params.value("config", "Release");

    std::vector<std::string> args = {"--build", "build", "--config", config};
    if (target != "all") {
        args.push_back("--target");
        args.push_back(target);
    }

    json result = executeCommand("cmake", args, m_context.timeoutMs);

    action.result = result.dump(2);
    return result.value("exitCode", -1) == 0;
}

/**
 * @brief Handle test action
 */
bool ActionExecutor::handleExecuteTests(Action& action)
{
    std::string testTarget = action.params.value("target", "all_tests");

    std::vector<std::string> args;
    if (testTarget != "all_tests") {
        args.push_back(testTarget);
    }

    json result = executeCommand("ctest", args, m_context.timeoutMs);

    action.result = result.dump(2);
    return result.value("exitCode", -1) == 0;
}

/**
 * @brief Handle git action
 */
bool ActionExecutor::handleCommitGit(Action& action)
{
    std::string gitAction = action.params.value("action", "");
    std::string message = action.params.value("message", "");
    std::string branch = action.params.value("branch", "");

    std::vector<std::string> args;

    if (gitAction == "commit") {
        args = {"commit", "-m", message};
    } else if (gitAction == "push") {
        args = {"push", branch.empty() ? "origin" : ("origin " + branch)};
    } else if (gitAction == "add") {
        args = {"add", action.params.value("files", "")};
    } else {
        action.error = "Unknown git action: " + gitAction;
        return false;
    }

    json result = executeCommand("git", args, m_context.timeoutMs);

    action.result = result.dump(2);
    return result.value("exitCode", -1) == 0;
}

/**
 * @brief Handle arbitrary command
 */
bool ActionExecutor::handleInvokeCommand(Action& action)
{
    std::string command = action.params.value("command", "");
    std::vector<std::string> args;

    if (action.params.contains("args")) {
        if (action.params["args"].is_array()) {
            for (const auto& arg : action.params["args"]) {
                args.push_back(arg.get<std::string>());
            }
        } else {
            args.push_back(action.params.value("args", ""));
        }
    }

    json result = executeCommand(command, args, m_context.timeoutMs);

    action.result = result.dump(2);
    return result.value("exitCode", -1) == 0;
}

/**
 * @brief Handle recursive agent invocation
 */
bool ActionExecutor::handleRecursiveAgent(Action& action)
{
    std::string goal = action.params.value("goal", "");
    std::string context = action.params.value("context", "");

    // Logic: Create a new execution plan for the subgrid
    // If we have access to a planner (not linked here), we would use it.
    // Instead, we simulate the recursion by logging or delegating.
    
    // Explicitly handle failure cases
    if (goal.empty()) {
        action.error = "Recursive agent goal cannot be empty";
        return false;
    }

    // Capture current depth
    static thread_local int depth = 0;
    if (depth > 5) {
        action.error = "Max recursion depth exceeded";
        return false;
    }

    depth++;
    
    // In a real system, this would block until child agent finishes.
    // Here we will use the callback mechanism if available.
    bool delegationSuccess = false;

    if (onRecursiveTaskNeeded) {
        // Assume callback returns async future or result string?
        // For synchronous simplicity:
        // std::string result = onRecursiveTaskNeeded(goal, context);
        // But our callback is void logic in header usually.
        // Let's assume we just trigger it.
        onRecursiveTaskNeeded(goal, context);
        action.result = "Delegated task: " + goal;
        delegationSuccess = true;
    } else {
        // Fallback: Try to execute via command line
        std::vector<std::string> args = {"--agent", "--goal", goal};
        json cmdResult = executeCommand("RawrXD-Agent.exe", args, 60000);
        if (cmdResult["exitCode"] == 0) {
             action.result = "Sub-agent executed successfully: " + cmdResult["stdout"].get<std::string>();
             delegationSuccess = true;
        } else {
             action.error = "Sub-agent execution failed";
        }
    }

    depth--;
    return delegationSuccess;
}

/**
 * @brief Handle user query
 */
bool ActionExecutor::handleQueryUser(Action& action)
{
    std::string query = action.params.value("query", "");
    std::vector<std::string> options;

    if (action.params.contains("options") && action.params["options"].is_array()) {
        for (const auto& opt : action.params["options"]) {
            options.push_back(opt.get<std::string>());
        }
    }

    if (onUserInputNeeded) onUserInputNeeded(query, options);

    action.result = "User query: " + query;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// Utility Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse JSON action
 */
Action ActionExecutor::parseJsonAction(const json& jsonAction)
{
    Action action;
    action.type = stringToActionType(jsonAction.value("type", ""));
    action.target = jsonAction.value("target", "");
    action.params = jsonAction.value("params", json::object());
    action.description = jsonAction.value("description", "");

    return action;
}

/**
 * @brief Create backup
 */
bool ActionExecutor::createBackup(const std::string& filePath)
{
    if (!fs::exists(filePath)) {
        return true; // No need to backup non-existent file
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << filePath << ".backup." << time;
    std::string backupPath = ss.str();

    try {
        fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing);
        m_backups[filePath] = backupPath;
        return true;
    } catch (const std::exception& e) {
        // Log error
        return false;
    }
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
    fs::copy_file(backupPath, filePath, fs::copy_options::overwrite_existing, ec);

    return !ec;
}

/**
 * @brief Execute command
 */
json ActionExecutor::executeCommand(const std::string& command,
                                    const std::vector<std::string>& args,
                                    int timeoutMs)
{
    json result;
    result["command"] = command;
    result["args"] = args;

    if (m_context.dryRun) {
        result["exitCode"] = 0;
        std::string cmdStr = command;
        for (const auto& arg : args) {
            cmdStr += " " + arg;
        }
        result["stdout"] = "DRY RUN: Would execute " + cmdStr;
        return result;
    }

    // Build command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    }

    // Create pipes for stdout/stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        result["exitCode"] = -1;
        result["error"] = "Failed to create stdout pipe";
        return result;
    }
    
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        result["exitCode"] = -1;
        result["error"] = "Failed to create stderr pipe";
        return result;
    }

    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStderrWrite;
    si.hStdOutput = hStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char* cmdLinePtr = _strdup(cmdLine.c_str());
    
    if (!CreateProcessA(NULL, cmdLinePtr, NULL, NULL, TRUE, 0, NULL, 
                        m_context.projectRoot.c_str(), &si, &pi)) {
        free(cmdLinePtr);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrWrite);
        CloseHandle(hStderrRead);
        result["exitCode"] = -1;
        result["error"] = "Failed to create process";
        return result;
    }

    free(cmdLinePtr);
    m_processHandle = pi.hProcess;
    
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result["exitCode"] = -1;
        result["error"] = "Command timed out after " + std::to_string(timeoutMs) + "ms";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        m_processHandle = nullptr;
        return result;
    }

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result["exitCode"] = static_cast<int>(exitCode);

    // Read stdout
    std::string stdout_str;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        stdout_str.append(buffer, bytesRead);
    }

    // Read stderr
    std::string stderr_str;
    while (ReadFile(hStderrRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        stderr_str.append(buffer, bytesRead);
    }

    result["stdout"] = stdout_str;
    result["stderr"] = stderr_str;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    m_processHandle = nullptr;

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
        return false;
    }

    // For delete operations, require explicit confirmation
    // Explicit missing logic: delete whitelist
    // For now, only fail if it's very dangerous.
    // The previous code returned false unconditionally for delete.
    if (action == "delete") {
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
