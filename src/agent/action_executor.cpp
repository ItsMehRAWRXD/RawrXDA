/**
 * @file action_executor.cpp
 * @brief Implementation of action execution engine
 *
 * Executes agent-generated actions with comprehensive error handling,
 * backup/restore, and observability.
 *
 * Architecture: C++20, no Qt, no exceptions
 * Process execution: CreateProcessA (Windows) / fork+exec (POSIX)
 */

#include "action_executor.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Constructor
 */
ActionExecutor::ActionExecutor()
{
    m_context.projectRoot = fs::current_path().string();
}

/**
 * @brief Destructor — joins background thread if active
 */
ActionExecutor::~ActionExecutor()
{
    cancelExecution();
    std::lock_guard<std::mutex> lock(m_threadMutex);
    if (m_executionThread.joinable()) {
        m_executionThread.join();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Set execution context
 */
void ActionExecutor::setContext(const ExecutionContext& context)
{
    m_context = context;
    fprintf(stderr, "[ActionExecutor] Context set - projectRoot: %s\n",
            m_context.projectRoot.c_str());
}

/**
 * @brief Execute single action (synchronous)
 */
bool ActionExecutor::executeAction(Action& action)
{
    fprintf(stderr, "[ActionExecutor] Executing action: %s\n", action.description.c_str());

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
void ActionExecutor::executePlan(const JsonValue& actions, bool stopOnError)
{
    m_isExecuting.store(true);
    m_stopOnError = stopOnError;
    m_cancelled.store(false);
    m_executedActions.clear();
    m_backups.clear();

    m_context.totalActions = static_cast<int>(actions.size());
    notifyPlanStarted(static_cast<int>(actions.size()));

    // Join any previous background thread
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        if (m_executionThread.joinable()) {
            m_executionThread.join();
        }
    }

    // Run on background thread
    std::lock_guard<std::mutex> lock(m_threadMutex);
    m_executionThread = std::thread([this, actions]() {
        bool overallSuccess = true;

        for (size_t i = 0; i < actions.size() && !m_cancelled.load(); ++i) {
            if (!actions[i].isObject()) {
                fprintf(stderr, "[ActionExecutor] Invalid action at index %zu\n", i);
                overallSuccess = false;
                if (m_stopOnError) break;
                continue;
            }

            Action action = parseJsonAction(actions[i]);
            m_context.currentActionIndex = static_cast<int>(i);

            notifyActionStarted(static_cast<int>(i), action.description);
            notifyProgressUpdated(static_cast<int>(i), m_context.totalActions);

            bool success = executeAction(action);
            action.executed = true;
            action.success = success;

            m_executedActions.push_back(action);

            JsonValue result;
            result["target"] = action.target;
            result["success"] = success;
            if (!action.error.empty()) {
                result["error"] = action.error;
            }
            if (!action.result.empty()) {
                result["result"] = action.result;
            }

            notifyActionCompleted(static_cast<int>(i), success, result);

            if (!success) {
                overallSuccess = false;
                notifyActionFailed(static_cast<int>(i), action.error, m_stopOnError);

                if (m_stopOnError) {
                    fprintf(stderr, "[ActionExecutor] Stopping due to error\n");
                    break;
                }
            }
        }

        m_isExecuting.store(false);

        JsonValue finalResult;
        finalResult["success"] = overallSuccess;
        finalResult["actionsExecuted"] = static_cast<int>(m_executedActions.size());
        finalResult["state"] = m_context.state;

        notifyPlanCompleted(overallSuccess, finalResult);
    });
}

/**
 * @brief Cancel execution
 */
void ActionExecutor::cancelExecution()
{
    m_cancelled.store(true);
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(m_processMutex);
        if (m_currentProcessHandle != INVALID_HANDLE_VALUE) {
            TerminateProcess(m_currentProcessHandle, 1);
            WaitForSingleObject(m_currentProcessHandle, 5000);
        }
    }
#endif
    fprintf(stderr, "[ActionExecutor] Execution cancelled\n");
}

/**
 * @brief Rollback action
 */
bool ActionExecutor::rollbackAction(int actionIndex)
{
    if (actionIndex < 0 || actionIndex >= static_cast<int>(m_executedActions.size())) {
        return false;
    }

    const Action& action = m_executedActions[static_cast<size_t>(actionIndex)];

    // Only file edits are rollbackable
    if (action.type != ActionType::FileEdit) {
        fprintf(stderr, "[ActionExecutor] Action type not rollbackable\n");
        return false;
    }

    if (m_backups.find(action.target) == m_backups.end()) {
        fprintf(stderr, "[ActionExecutor] No backup found for %s\n", action.target.c_str());
        return false;
    }

    return restoreFromBackup(action.target);
}

/**
 * @brief Get aggregated result
 */
JsonValue ActionExecutor::getAggregatedResult() const
{
    JsonValue result;
    JsonValue actions = JsonValue::makeArray();

    for (const auto& action : m_executedActions) {
        JsonValue actionObj;
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

// ═══════════════════════════════════════════════════════════════════════════
// Action Handlers
// ═══════════════════════════════════════════════════════════════════════════

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
        fprintf(stderr, "[ActionExecutor] Failed to backup %s\n", filePath.c_str());
    }

    if (editAction == "create") {
        // Create new file — ensure parent directory exists
        fs::path fp(filePath);
        if (fp.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(fp.parent_path(), ec);
        }
        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            action.error = "Failed to create file: " + filePath;
            return false;
        }
        ofs << content;
        ofs.close();
        action.result = "File created: " + filePath;
        return true;

    } else if (editAction == "append") {
        // Append to existing file
        std::ofstream ofs(filePath, std::ios::out | std::ios::app);
        if (!ofs.is_open()) {
            action.error = "Failed to open file for append: " + filePath;
            return false;
        }
        ofs << content;
        ofs.close();
        action.result = "Appended to: " + filePath;
        return true;

    } else if (editAction == "replace") {
        // Replace entire file
        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            action.error = "Failed to open file for writing: " + filePath;
            return false;
        }
        ofs << content;
        ofs.close();
        action.result = "Replaced: " + filePath;
        return true;

    } else if (editAction == "delete") {
        // Delete file
        std::error_code ec;
        if (!fs::remove(filePath, ec)) {
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

    std::error_code ec;
    if (!fs::exists(searchPath, ec)) {
        action.error = "Search path does not exist: " + searchPath;
        return false;
    }

    // Split pattern by comma for multiple extensions
    auto patterns = strutil::split(pattern, ',');

    JsonValue results = JsonValue::makeArray();
    int matchCount = 0;
    int filesSearched = 0;

    for (auto& entry : fs::directory_iterator(searchPath, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();

        // Check if file matches any pattern (simple glob: *.ext)
        bool matchesPattern = patterns.empty();
        for (const auto& pat : patterns) {
            std::string trimmedPat = pat;
            // Trim whitespace
            while (!trimmedPat.empty() && trimmedPat.front() == ' ') trimmedPat.erase(trimmedPat.begin());
            while (!trimmedPat.empty() && trimmedPat.back() == ' ') trimmedPat.pop_back();

            if (trimmedPat.empty() || trimmedPat == "*") {
                matchesPattern = true;
                break;
            }
            // Simple wildcard match: *.ext
            if (trimmedPat.starts_with("*.")) {
                std::string ext = trimmedPat.substr(1); // ".ext"
                if (filename.size() >= ext.size() &&
                    filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
                    matchesPattern = true;
                    break;
                }
            } else if (filename.find(trimmedPat) != std::string::npos) {
                matchesPattern = true;
                break;
            }
        }

        if (!matchesPattern) continue;
        ++filesSearched;

        if (query.empty()) {
            // Just list files
            JsonValue fileObj;
            fileObj["path"] = entry.path().string();
            fileObj["size"] = static_cast<int>(entry.file_size(ec));
            results.append(fileObj);
        } else {
            // Search content
            std::ifstream ifs(entry.path(), std::ios::in);
            if (ifs.is_open()) {
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());
                ifs.close();

                if (content.find(query) != std::string::npos) {
                    JsonValue match;
                    match["file"] = entry.path().string();
                    match["matches"] = static_cast<int>(strutil::countOccurrences(content, query));
                    results.append(match);
                    matchCount++;
                }
            }
        }
    }

    JsonValue resultObj;
    resultObj["files_searched"] = filesSearched;
    resultObj["matches"] = matchCount;
    resultObj["results"] = results;

    action.result = resultObj.toJsonString();
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
    return result.value("exitCode").toInt() == 0;
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
    return result.value("exitCode").toInt() == 0;
}

/**
 * @brief Handle git action
 */
bool ActionExecutor::handleCommitGit(Action& action)
{
    std::string gitAction = action.params.value("action").toString();
    std::string message = action.params.value("message").toString();
    std::string branch = action.params.value("branch").toString();

    std::vector<std::string> args;

    if (gitAction == "commit") {
        args = {"commit", "-m", message};
    } else if (gitAction == "push") {
        args = {"push", branch.empty() ? std::string("origin") : "origin " + branch};
    } else if (gitAction == "add") {
        args = {"add", action.params.value("files").toString()};
    } else {
        action.error = "Unknown git action: " + gitAction;
        return false;
    }

    JsonValue result = executeCommand("git", args, m_context.timeoutMs);

    action.result = result.toJsonString();
    return result.value("exitCode").toInt() == 0;
}

/**
 * @brief Handle arbitrary command
 */
bool ActionExecutor::handleInvokeCommand(Action& action)
{
    std::string command = action.params.value("command").toString();
    std::vector<std::string> args;

    if (action.params.contains("args")) {
        if (action.params.value("args").isArray()) {
            for (const auto& arg : action.params.value("args").toArray()) {
                args.push_back(arg.toString());
            }
        } else {
            args.push_back(action.params.value("args").toString());
        }
    }

    JsonValue result = executeCommand(command, args, m_context.timeoutMs);

    action.result = result.toJsonString();
    return result.value("exitCode").toInt() == 0;
}

/**
 * @brief Handle recursive agent invocation
 */
bool ActionExecutor::handleRecursiveAgent(Action& action)
{
    // Recursive agent invocation: spawn a sub-agent with a new wish/goal
    std::string subWish = action.params.value("wish").toString();
    if (subWish.empty()) {
        subWish = action.params.value("query").toString();
    }
    if (subWish.empty()) {
        action.result = "No 'wish' or 'query' provided for recursive agent";
        return false;
    }

    int maxDepth = action.params.value("maxDepth").toInt(3);
    int currentDepth = action.params.value("currentDepth").toInt(0);

    if (currentDepth >= maxDepth) {
        action.result = "Recursive agent depth limit reached (" +
                        std::to_string(maxDepth) + ")";
        return false;
    }

    // Create sub-context with incremented depth
    JsonValue subParams;
    subParams["wish"] = subWish;
    subParams["currentDepth"] = currentDepth + 1;
    subParams["maxDepth"] = maxDepth;
    subParams["parentAction"] = action.id;

    // Invoke through the model invoker if available
    if (m_context.modelInvokeFunc) {
        JsonValue invokeResult = m_context.modelInvokeFunc(subWish, subParams, m_context.modelInvokeUserData);
        action.result = invokeResult.toJsonString();
        return invokeResult.value("success").toBool(false);
    }

    action.result = "ModelInvoker not available for recursive agent call";
    return false;
}

/**
 * @brief Handle user query
 */
bool ActionExecutor::handleQueryUser(Action& action)
{
    std::string query = action.params.value("query").toString();
    std::vector<std::string> options;

    if (action.params.value("options").isArray()) {
        for (const auto& opt : action.params.value("options").toArray()) {
            options.push_back(opt.toString());
        }
    }

    notifyUserInputNeeded(query, options);

    // Wait for user response (would be connected externally)
    action.result = "User query: " + query;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Utility Methods
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Parse JSON action
 */
Action ActionExecutor::parseJsonAction(const JsonValue& jsonAction)
{
    Action action;
    action.type = stringToActionType(jsonAction.value("type").toString());
    action.target = jsonAction.value("target").toString();
    action.params = jsonAction.value("params");
    // Ensure params is an object
    if (!action.params.isObject()) {
        action.params = JsonValue::makeObject();
    }
    action.description = jsonAction.value("description").toString();
    action.id = jsonAction.value("id").toString();

    return action;
}

/**
 * @brief Create backup
 */
bool ActionExecutor::createBackup(const std::string& filePath)
{
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        return true; // No need to backup non-existent file
    }

    std::string backupPath = filePath + ".backup." + strutil::currentTimestamp();

    bool success = fs::copy_file(filePath, backupPath, ec);
    if (success) {
        m_backups[filePath] = backupPath;
        fprintf(stderr, "[ActionExecutor] Backup created: %s\n", backupPath.c_str());
    }

    return success;
}

/**
 * @brief Restore from backup
 */
bool ActionExecutor::restoreFromBackup(const std::string& filePath)
{
    auto it = m_backups.find(filePath);
    if (it == m_backups.end()) {
        return false;
    }

    std::string backupPath = it->second;

    std::error_code ec;
    if (!fs::copy_file(backupPath, filePath, fs::copy_options::overwrite_existing, ec)) {
        return false;
    }

    fprintf(stderr, "[ActionExecutor] Restored from backup: %s\n", backupPath.c_str());
    return true;
}

/**
 * @brief Execute command and wrap result as JsonValue
 */
JsonValue ActionExecutor::executeCommand(const std::string& command,
                                          const std::vector<std::string>& args,
                                          int timeoutMs)
{
    JsonValue result;
    result["command"] = command;
    result["args"] = JsonValue::fromStringList(args);

    if (m_context.dryRun) {
        result["exitCode"] = 0;
        result["stdout"] = "DRY RUN: Would execute " + command + " " + strutil::join(args, " ");
        return result;
    }

    ProcessResult pr = runProcess(command, args, m_context.projectRoot, timeoutMs);

    if (pr.timedOut) {
        result["exitCode"] = -1;
        result["error"] = "Command timed out after " + std::to_string(timeoutMs) + "ms";
        return result;
    }

    result["exitCode"] = pr.exitCode;
    result["stdout"] = pr.stdoutStr;
    result["stderr"] = pr.stderrStr;

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
        fprintf(stderr, "[ActionExecutor] Blocked system file modification: %s\n", filePath.c_str());
        return false;
    }

    // For delete operations, require explicit confirmation
    if (action == "delete") {
        fprintf(stderr, "[ActionExecutor] File deletion requires explicit approval: %s\n", filePath.c_str());
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
    if (typeStr == "file_edit")        return ActionType::FileEdit;
    if (typeStr == "search_files")     return ActionType::SearchFiles;
    if (typeStr == "run_build")        return ActionType::RunBuild;
    if (typeStr == "execute_tests")    return ActionType::ExecuteTests;
    if (typeStr == "commit_git")       return ActionType::CommitGit;
    if (typeStr == "invoke_command")   return ActionType::InvokeCommand;
    if (typeStr == "recursive_agent")  return ActionType::RecursiveAgent;
    if (typeStr == "query_user")       return ActionType::QueryUser;

    return ActionType::Unknown;
}

// ═══════════════════════════════════════════════════════════════════════════
// Process Execution
// ═══════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
/**
 * @brief Run external process with stdout/stderr capture and timeout (Windows)
 */
ProcessResult ActionExecutor::runProcess(const std::string& command,
                                          const std::vector<std::string>& args,
                                          const std::string& workingDir,
                                          int timeoutMs)
{
    ProcessResult result;

    // Build command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " ";
        if (arg.find(' ') != std::string::npos) {
            cmdLine += "\"" + arg + "\"";
        } else {
            cmdLine += arg;
        }
    }

    // Create pipes for stdout and stderr
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;

    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        result.stderrStr = "Failed to create pipes";
        return result;
    }

    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    // Start process
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si,
        &pi
    );

    // Close write ends of pipes (parent doesn't write to child's stdout/stderr)
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    if (!ok) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        result.stderrStr = "CreateProcess failed for: " + command;
        return result;
    }

    // Track process handle for cancellation
    {
        std::lock_guard<std::mutex> lock(m_processMutex);
        m_currentProcessHandle = pi.hProcess;
    }

    // Read stdout and stderr in background threads to prevent pipe buffer deadlock
    std::string stdoutData, stderrData;
    auto readPipe = [](HANDLE h) -> std::string {
        std::string data;
        char buf[4096];
        DWORD n;
        while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0) {
            data.append(buf, n);
        }
        return data;
    };

    std::thread convergence_a([&]() { stdoutData = readPipe(hStdoutRead); });
    std::thread convergence_b([&]() { stderrData = readPipe(hStderrRead); });

    // Wait for process with timeout
    DWORD waitMs = (timeoutMs > 0) ? static_cast<DWORD>(timeoutMs) : INFINITE;
    DWORD waitResult = WaitForSingleObject(pi.hProcess, waitMs);

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timedOut = true;
        result.exitCode = -1;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    // Wait for reader threads to finish
    convergence_a.join();
    convergence_b.join();

    result.stdoutStr = std::move(stdoutData);
    result.stderrStr = std::move(stderrData);

    // Cleanup handles
    {
        std::lock_guard<std::mutex> lock(m_processMutex);
        m_currentProcessHandle = INVALID_HANDLE_VALUE;
    }

    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

#else // POSIX

ProcessResult ActionExecutor::runProcess(const std::string& command,
                                          const std::vector<std::string>& args,
                                          const std::string& workingDir,
                                          int timeoutMs)
{
    ProcessResult result;

    // Build argv
    std::vector<const char*> argv;
    argv.push_back(command.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    int stdoutPipe[2], stderrPipe[2];
    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
        result.stderrStr = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        if (!workingDir.empty()) chdir(workingDir.c_str());
        execvp(command.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    // Parent
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    auto readFd = [](int fd) -> std::string {
        std::string data;
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            data.append(buf, static_cast<size_t>(n));
        }
        return data;
    };

    std::string stdoutData, stderrData;
    std::thread t1([&]() { stdoutData = readFd(stdoutPipe[0]); });
    std::thread t2([&]() { stderrData = readFd(stderrPipe[0]); });

    // Wait with timeout (simplified)
    int status = 0;
    if (timeoutMs > 0) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            int r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r == -1) break;
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                result.timedOut = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        waitpid(pid, &status, 0);
    }

    t1.join();
    t2.join();

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    if (!result.timedOut) {
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        result.exitCode = -1;
    }

    result.stdoutStr = std::move(stdoutData);
    result.stderrStr = std::move(stderrData);

    return result;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Callback Registration & Notification
// ═══════════════════════════════════════════════════════════════════════════

void ActionExecutor::registerPlanStartedCallback(PlanStartedCallback cb, void* userData) {
    if (cb) m_planStartedCBs.push_back({cb, userData});
}
void ActionExecutor::registerActionStartedCallback(ActionStartedCallback cb, void* userData) {
    if (cb) m_actionStartedCBs.push_back({cb, userData});
}
void ActionExecutor::registerActionCompletedCallback(ActionCompletedCallback cb, void* userData) {
    if (cb) m_actionCompletedCBs.push_back({cb, userData});
}
void ActionExecutor::registerActionFailedCallback(ActionFailedCallback cb, void* userData) {
    if (cb) m_actionFailedCBs.push_back({cb, userData});
}
void ActionExecutor::registerProgressUpdatedCallback(ProgressUpdatedCallback cb, void* userData) {
    if (cb) m_progressUpdatedCBs.push_back({cb, userData});
}
void ActionExecutor::registerPlanCompletedCallback(PlanCompletedCallback cb, void* userData) {
    if (cb) m_planCompletedCBs.push_back({cb, userData});
}
void ActionExecutor::registerUserInputNeededCallback(UserInputNeededCallback cb, void* userData) {
    if (cb) m_userInputNeededCBs.push_back({cb, userData});
}

void ActionExecutor::notifyPlanStarted(int totalActions) {
    for (const auto& cb : m_planStartedCBs) cb.fn(totalActions, cb.userData);
}
void ActionExecutor::notifyActionStarted(int index, const std::string& description) {
    for (const auto& cb : m_actionStartedCBs) cb.fn(index, description, cb.userData);
}
void ActionExecutor::notifyActionCompleted(int index, bool success, const JsonValue& result) {
    for (const auto& cb : m_actionCompletedCBs) cb.fn(index, success, result, cb.userData);
}
void ActionExecutor::notifyActionFailed(int index, const std::string& error, bool recoverable) {
    for (const auto& cb : m_actionFailedCBs) cb.fn(index, error, recoverable, cb.userData);
}
void ActionExecutor::notifyProgressUpdated(int current, int total) {
    for (const auto& cb : m_progressUpdatedCBs) cb.fn(current, total, cb.userData);
}
void ActionExecutor::notifyPlanCompleted(bool success, const JsonValue& result) {
    for (const auto& cb : m_planCompletedCBs) cb.fn(success, result, cb.userData);
}
void ActionExecutor::notifyUserInputNeeded(const std::string& query, const std::vector<std::string>& options) {
    for (const auto& cb : m_userInputNeededCBs) cb.fn(query, options, cb.userData);
}

// ═══════════════════════════════════════════════════════════════════════════
// Former Qt Slot Implementations (now regular methods)
// ═══════════════════════════════════════════════════════════════════════════

void ActionExecutor::onActionTaskFinished()
{
    fprintf(stderr, "[ActionExecutor] Action task finished\n");
    // Process completion of current action task
}

void ActionExecutor::onProcessFinished(int exitCode, int exitStatus)
{
    fprintf(stderr, "[ActionExecutor] Process finished with exit code: %d status: %d\n",
            exitCode, exitStatus);
    // Process completion of external process execution
}
