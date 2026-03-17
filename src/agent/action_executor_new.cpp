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
#include <windows.h>
#include <vector>
#include <string>

extern "C" {
    uint64_t AgentRouter_Initialize();
    uint64_t AgentRouter_ExecuteTask(void* Task, void* Context);
    uint64_t AgentRouter_SelfHeal(void* FunctionAddr);
}

namespace fs = std::filesystem;
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
    case ActionType::AgentRouterTask:
        return handleAgentRouterTask(action);
    default:
        action.error = "Unknown action type";
        return false;
    }
}

/**
 * @brief Execute complete plan (asynchronous)
 */
void ActionExecutor::executePlan(const JsonValue& actions, bool stopOnError)
{
    m_isExecuting = true;
    m_stopOnError = stopOnError;
    m_cancelled = false;
    m_executedActions.clear();
    m_backups.clear();

    m_context.totalActions = static_cast<int>(actions.size());
    notifyPlanStarted(static_cast<int>(actions.size()));

    // Run on background thread
    std::thread([this, actions]() {
        bool overallSuccess = true;

        for (size_t i = 0; i < actions.size() && !m_cancelled; ++i) {
            if (!actions[i].isObject()) {
                overallSuccess = false;
                if (m_stopOnError) break;
                continue;
            }

            Action action = parseJsonAction(actionJson);
            m_context.currentActionIndex = static_cast<int>(i);

            notifyActionStarted(static_cast<int>(i), action.description);
            notifyProgressUpdated(static_cast<int>(i), m_context.totalActions);

            bool success = executeAction(action);
            action.executed = true;
            action.success = success;
            m_executedActions.push_back(action);

            JsonValue res = JsonValue::createObject();
            res["target"] = action.target;
            res["success"] = success;
            if (!action.error.empty()) {
                res["error"] = action.error;
            }
            if (!action.result.empty()) {
                res["result"] = action.result;
            }

            notifyActionCompleted(static_cast<int>(i), success, res);

            if (!success) {
                overallSuccess = false;
                notifyActionFailed(static_cast<int>(i), action.error, m_stopOnError);

                if (m_stopOnError) {
                    break;
                }
            }
        }

        m_isExecuting = false;

        JsonValue finalResult = JsonValue::object();
        finalResult["success"] = overallSuccess;
        finalResult["actionsExecuted"] = (int)m_executedActions.size();
        finalResult["state"] = m_context.state;

void ActionExecutor::notifyPlanStarted(int totalActions)
{
    // Implementation for notifyPlanStarted...
}

void ActionExecutor::notifyActionStarted(int index, const std::string& description)
{
    // Implementation for notifyActionStarted...
}

void ActionExecutor::notifyActionCompleted(int index, bool success, const JsonValue& res)
{
    // Implementation for notifyActionCompleted...
}

void ActionExecutor::notifyActionFailed(int index, const std::string& error, bool recoverable)
{
    // Implementation for notifyActionFailed...
}

void ActionExecutor::notifyProgressUpdated(int current, int total)
{
    // Implementation for notifyProgressUpdated...
}

void ActionExecutor::notifyPlanCompleted(bool success, const JsonValue& result)
{
    // Implementation for notifyPlanCompleted...
}

void ActionExecutor::notifyUserInputNeeded(const std::string& query, const std::vector<std::string>& options)
{
    // Implementation for notifyUserInputNeeded...
}
    if (m_currentProcessHandle != INVALID_HANDLE_VALUE) {
        TerminateProcess(m_currentProcessHandle, 1);
        // We don't close handle here, it should be closed in runProcess when it finishes or fails
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
JsonValue ActionExecutor::getAggregatedResult() const
{
    JsonValue result = JsonValue::object();
    JsonValue actions = JsonValue::array();

    for (const auto& action : m_executedActions) {
        JsonValue actionObj = JsonValue::object();
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
    std::string editAction = action.params.value("action").toString("");
    std::string content = action.params.value("content").toString("");

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
    fs::path searchPath = fs::path(m_context.projectRoot) / action.params.value("path").toString("");
    std::string pattern = action.params.value("pattern").toString("*");
    std::string query = action.params.value("query").toString("");

    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
        action.error = "Search path does not exist: " + searchPath.string();
        return false;
    }

    JsonValue results = JsonValue::array();
    int matchCount = 0;
    int filesSearched = 0;

    for (const auto& entry : fs::directory_iterator(searchPath)) {
        if (!entry.is_regular_file()) continue;
        filesSearched++;

        if (query.empty()) {
            // Just list files
            JsonValue fileObj = JsonValue::object();
            fileObj["path"] = entry.path().string();
            fileObj["size"] = (int64_t)entry.file_size();
            results.append(fileObj);
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
                    JsonValue match = JsonValue::object();
                    match["file"] = entry.path().string();
                    match["matches"] = count;
                    results.append(match);
                    matchCount++;
                }
            }
        }
    }

    JsonValue result = JsonValue::object();
    result["files_searched"] = filesSearched;
    result["matches"] = matchCount;
    result["results"] = results;

    action.result = result.toJsonString();
    return true;
}

/**
 * @brief Handle build action
 */
bool ActionExecutor::handleRunBuild(Action& action)
{
    std::string target = action.params.value("target").toString("all");
    std::string config = action.params.value("config").toString("Release");

    std::vector<std::string> args = {"--build", "build", "--config", config};
    if (target != "all") {
        args.push_back("--target");
        args.push_back(target);
    }

    JsonValue result = executeCommand("cmake", args, m_context.timeoutMs);

    action.result = result.toJsonString();
    return result.value("exitCode").toInt(-1) == 0;
}

/**
 * @brief Handle test action
 */
bool ActionExecutor::handleExecuteTests(Action& action)
{
    std::string testTarget = action.params.value("target").toString("all_tests");

    std::vector<std::string> args;
    if (testTarget != "all_tests") {
        args.push_back(testTarget);
    }

    JsonValue result = executeCommand("ctest", args, m_context.timeoutMs);

    action.result = result.toJsonString();
    return result.value("exitCode").toInt(-1) == 0;
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
    // Placeholder for recursive agent call
    action.result = "Recursive agent invocation not yet implemented";
    return false;
}

/**
 * @brief Handle user query
 */
bool ActionExecutor::handleQueryUser(Action& action)
{
    std::string query = action.params.value("query").toString("");
    std::vector<std::string> options;

    if (action.params.contains("options") && action.params.value("options").isArray()) {
        JsonValue optsArr = action.params.value("options");
        for (size_t i = 0; i < optsArr.size(); ++i) {
            options.push_back(optsArr[i].toString(""));
        }
    }

    notifyUserInputNeeded(query, options);

    action.result = "User query: " + query;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// Utility Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse JSON action
 */
Action ActionExecutor::parseJsonAction(const JsonValue& jsonAction)
{
    Action action;
    action.type = stringToActionType(jsonAction.value("type").toString(""));
    action.target = jsonAction.value("target").toString("");
    action.params = jsonAction.value("params");
    action.description = jsonAction.value("description").toString("");

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

    std::error_code ec;
    fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing, ec);
    
    if (!ec) {
        m_backups[filePath] = backupPath;
    }

    return !ec;
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
JsonValue ActionExecutor::executeCommand(const std::string& command,
                                    const std::vector<std::string>& args,
                                    int timeoutMs)
{
    JsonValue result = JsonValue::object();
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
    if (typeStr == "agent_router") return ActionType::AgentRouterTask;

    return ActionType::Unknown;
}

/**
 * @brief Handle agentic routing/dispatch (ASM)
 */
bool ActionExecutor::handleAgentRouterTask(Action& action)
{
    // Initialize ASM Router on first use
    static bool initialized = false;
    if (!initialized) {
        AgentRouter_Initialize();
        initialized = true;
    }

    // Execute via ASM layer
    // We pass the action object as raw pointer for now
    // Future: Proper serialization
    uint64_t result = AgentRouter_ExecuteTask(&action, &m_context);

    if (result == 1) { // 1 = Tool Execution needed
        action.result = "ASM Router: Dispatched to Tool Handler";
        return true; 
    }

    action.result = "ASM Router: Processed via Hardware-Accelerated Inference";
    return true;
}
