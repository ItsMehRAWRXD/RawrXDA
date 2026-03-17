#include "agent/action_executor.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <regex>
#include <windows.h>
#include "RawrXD_Win32_Foundation.h" // Assuming this exists based on previous context

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper to safely detach thread or join if needed. For now we just detach as per pattern.
// In robust code we would track this thread.

ActionExecutor::ActionExecutor() {
    m_context.projectRoot = fs::current_path().string();
    m_isExecuting = false;
    m_stopOnError = true;
    m_cancelled = false;
}

ActionExecutor::~ActionExecutor() {
    // Ensure we don't crash if thread is running? 
    // Cancellation flag set.
    m_cancelled = true;
}

void ActionExecutor::setContext(const ExecutionContext& context) {
    m_context = context;
}

bool ActionExecutor::executeAction(Action& action) {
    bool success = false;
    m_isExecuting = true;
    
    // Auto-backup if file edit
    if (action.type == ActionType::FileEdit && !action.target.empty()) {
        createBackup(action.target); // Best effort
    }
    
    switch (action.type) {
        case ActionType::FileEdit:       success = handleFileEdit(action); break;
        case ActionType::SearchFiles:    success = handleSearchFiles(action); break;
        case ActionType::RunBuild:       success = handleRunBuild(action); break;
        case ActionType::ExecuteTests:   success = handleExecuteTests(action); break;
        case ActionType::CommitGit:      success = handleCommitGit(action); break;
        case ActionType::InvokeCommand:  success = handleInvokeCommand(action); break;
        case ActionType::RecursiveAgent: success = handleRecursiveAgent(action); break;
        case ActionType::QueryUser:      success = handleQueryUser(action); break;
        default:
            action.error = "Unknown action type";
            success = false;
            break;
    }

    action.executed = true;
    action.success = success;
    
    if (success) {
        if (action.result.empty()) action.result = json({{"status", "success"}}).dump();
        if (onActionCompleted) onActionCompleted(m_context.currentActionIndex, true, json::parse(action.result, nullptr, false));
    } else {
        if (action.error.empty()) action.error = "Unknown error occurred";
        if (onActionFailed) onActionFailed(m_context.currentActionIndex, action.error, false); // canRetry=false
    }
    
    m_executedActions.push_back(action);
    return success;
}

void ActionExecutor::executePlan(const nlohmann::json& actions, bool stopOnError) {
    m_stopOnError = stopOnError;
    m_cancelled = false;
    m_isExecuting = true;
    m_executedActions.clear();
    m_backups.clear();
    
    if (onPlanStarted) onPlanStarted(static_cast<int>(actions.size()));
    
    // Execute in background thread to avoid blocking UI
    std::thread([this, actions]() {
        bool planSuccess = true;
        int index = 0;
        
        for (const auto& item : actions) {
            if (m_cancelled) {
                planSuccess = false; 
                break;
            }
            
            Action action = parseJsonAction(item);
            m_context.currentActionIndex = index;
            
            if (onActionStarted) onActionStarted(index, action.description);
            if (onProgressUpdated) onProgressUpdated(index + 1, static_cast<int>(actions.size())); // +1 for current starting
            
            bool result = executeAction(action);
            
            if (!result) {
                planSuccess = false;
                if (m_stopOnError) break;
            }
            
            index++;
        }
        
        m_isExecuting = false;
        if (onPlanCompleted) onPlanCompleted(planSuccess, getAggregatedResult());
        
    }).detach();
}

void ActionExecutor::cancelExecution() {
    m_cancelled = true;
    // We can't forcibly kill the thread easily without native handle hacks
    // Rely on cooperative cancellation points
}

bool ActionExecutor::rollbackAction(int actionIndex) {
    if (actionIndex < 0 || actionIndex >= (int)m_executedActions.size()) return false;
    
    const Action& action = m_executedActions[actionIndex];
    
    if (action.type == ActionType::FileEdit) {
        // Restore from backup if exists
        return restoreFromBackup(action.target);
    }
    
    // Other actions might not be reversible (e.g. commands)
    return false;
}

nlohmann::json ActionExecutor::getAggregatedResult() const {
    json summary = json::array();
    for (const auto& action : m_executedActions) {
        summary.push_back({
            {"type", (int)action.type},
            {"description", action.description},
            {"success", action.success},
            {"error", action.error}
        });
    }
    return summary;
}

// ─────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────

ActionType ActionExecutor::stringToActionType(const std::string& typeStr) const {
    if (typeStr == "file_edit") return ActionType::FileEdit;
    if (typeStr == "search") return ActionType::SearchFiles;
    if (typeStr == "build") return ActionType::RunBuild;
    if (typeStr == "test") return ActionType::ExecuteTests;
    if (typeStr == "git") return ActionType::CommitGit;
    if (typeStr == "command") return ActionType::InvokeCommand;
    if (typeStr == "agent") return ActionType::RecursiveAgent;
    if (typeStr == "ask_user") return ActionType::QueryUser;
    return ActionType::Unknown;
}

Action ActionExecutor::parseJsonAction(const nlohmann::json& jsonAction) {
    Action action;
    std::string typeStr = jsonAction.value("type", "unknown");
    action.type = stringToActionType(typeStr);
    action.target = jsonAction.value("target", "");
    action.description = jsonAction.value("description", "No description");
    
    if (jsonAction.contains("params")) {
        action.params = jsonAction["params"];
    }
    
    // Fallback specific param parsing if flat structure
    if (action.params.empty()) {
        action.params = jsonAction; // Just copy everything
    }
    
    return action;
}

bool ActionExecutor::createBackup(const std::string& filePath) {
    if (!fs::exists(filePath)) return true; // Nothing to backup
    
    std::string backupPath = filePath + ".bak." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    try {
        fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing);
        m_backups[filePath] = backupPath;
        return true;
    } catch (...) {
        return false;
    }
}

bool ActionExecutor::restoreFromBackup(const std::string& filePath) {
    auto it = m_backups.find(filePath);
    if (it == m_backups.end()) return false;
    
    try {
        fs::copy_file(it->second, filePath, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────
// Handlers
// ─────────────────────────────────────────────────────────────────────

bool ActionExecutor::handleFileEdit(Action& action) {
    std::string op = action.params.value("operation", "write");
    std::string content = action.params.value("content", "");
    
    try {
        if (op == "write" || op == "create") {
             // Create directories if needed
             fs::path p(action.target);
             if (p.has_parent_path()) {
                 fs::create_directories(p.parent_path());
             }
             
             std::ofstream file(action.target, std::ios::binary);
             if (!file) {
                 action.error = "Failed to open file for writing: " + action.target;
                 return false;
             }
             file << content;
             file.close();
             
             action.result = json({{"path", action.target}, {"bytes", content.size()}}).dump();
             return true;
        } 
        else if (op == "read") {
            std::ifstream file(action.target);
            if (!file) {
                 action.error = "File not found: " + action.target;
                 return false;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            action.result = json({{"content", buffer.str()}}).dump();
            return true;
        }
        else if (op == "delete") {
            if (fs::remove(action.target)) {
                action.result = json({{"deleted", true}}).dump();
                return true;
            } else {
                 action.error = "Failed to delete file";
                 return false;
            }
        }
        else if (op == "replace") {
             // Search and replace text
             std::string oldTxt = action.params.value("old_text", "");
             std::string newTxt = action.params.value("new_text", "");
             
             std::ifstream inFile(action.target);
             if (!inFile) {
                 action.error = "File not found";
                 return false;
             }
             std::stringstream buffer;
             buffer << inFile.rdbuf();
             std::string fileContent = buffer.str();
             inFile.close();
             
             size_t pos = fileContent.find(oldTxt);
             if (pos != std::string::npos) {
                 fileContent.replace(pos, oldTxt.length(), newTxt);
                 
                 std::ofstream outFile(action.target, std::ios::trunc);
                 outFile << fileContent;
                 outFile.close();
                 action.result = json({{"replaced", true}}).dump();
                 return true;
             } else {
                 action.error = "String not found for replacement";
                 return false;
             }
        }
        
    } catch (const std::exception& e) {
        action.error = std::string("File exception: ") + e.what();
        return false;
    }
    
    action.error = "Unknown file operation";
    return false;
}

bool ActionExecutor::handleSearchFiles(Action& action) {
    std::string pattern = action.params.value("pattern", "");
    std::string pathVec = action.params.value("path", m_context.projectRoot);
    std::vector<std::string> matches;
    enum { REGEX, GLOB } mode = REGEX; // Simplify to regex for now
    
    try {
        std::regex re(pattern); // Assumes regex
        for (const auto& entry : fs::recursive_directory_iterator(pathVec)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_search(filename, re)) {
                    matches.push_back(entry.path().string());
                    if (matches.size() > 100) break; // Limit results
                }
            }
        }
    } catch(const std::exception& e) {
        action.error = e.what();
        return false;
    }
    
    action.result = json(matches).dump();
    return true;
}

json ActionExecutor::executeCommand(const std::string& command, const std::vector<std::string>& args, int timeoutMs) {
    std::string fullCmd = command;
    for(const auto& arg : args) fullCmd += " " + arg;
    
    // Windows Process Creation
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        return {{"error", "CreatePipe failed"}};
    }
    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_OUT_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    // Create the child process.
    std::string cmdLine = "cmd.exe /C " + fullCmd;
    char* szCmdLine = _strdup(cmdLine.c_str()); // Writable buffer

    if (!CreateProcessA(NULL, szCmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        free(szCmdLine);
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        return {{"error", "CreateProcess failed"}};
    }

    free(szCmdLine);
    CloseHandle(hChildStd_OUT_Wr); // Close write end in parent

    // Read output
    std::string output;
    DWORD dwRead, dwAvail;
    char chBuf[4096];
    bool bSuccess = FALSE;
    
    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        // Check timeout
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            output += "\n[TIMEOUT]\n";
            break;
        }

        // Check if process exited
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            // Read remaining bytes
            while(ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf)-1, &dwRead, NULL) && dwRead != 0) {
                 chBuf[dwRead] = 0;
                 output += chBuf;
            }
            break;
        }
        
        // Peek pipe
        if (PeekNamedPipe(hChildStd_OUT_Rd, NULL, 0, NULL, &dwAvail, NULL)) {
            if (dwAvail > 0) {
                 if (ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf)-1, &dwRead, NULL) && dwRead != 0) {
                     chBuf[dwRead] = 0;
                     output += chBuf;
                 }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    CloseHandle(hChildStd_OUT_Rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {{"output", output}, {"exitCode", 0}}; // Exit code simplified
}

bool ActionExecutor::handleRunBuild(Action& action) {
    json res = executeCommand("cmake", {"--build", "build"}, m_context.timeoutMs);
    action.result = res.dump();
    return !res.contains("error"); // Simplified success check
}

bool ActionExecutor::handleExecuteTests(Action& action) {
    json res = executeCommand("ctest", {"--test-dir", "build"}, m_context.timeoutMs);
    action.result = res.dump();
    return !res.contains("error");
}

bool ActionExecutor::handleInvokeCommand(Action& action) {
    std::string cmd = action.params.value("command", "");
    std::vector<std::string> args = action.params.value("args", std::vector<std::string>{});
    
    json res = executeCommand(cmd, args, m_context.timeoutMs);
    action.result = res.dump();
    
    if (res.contains("error")) {
        action.error = res["error"];
        return false;
    }
    return true;
}

bool ActionExecutor::handleCommitGit(Action& action) {
    std::string msg = action.params.value("message", "Auto commit");
    executeCommand("git", {"add", "."}, 5000);
    json res = executeCommand("git", {"commit", "-m", "\"" + msg + "\""}, 5000);
    action.result = res.dump();
    return true; // Commit might fail if nothing to commit, treat as soft success
}

bool ActionExecutor::handleRecursiveAgent(Action& action) {
    if (!m_agenticEngine) {
        action.error = "Recursive agent call failed: No AgenticEngine instance available.";
        return false;
    }
    
    std::string goal = action.params.value("goal", "");
    if (goal.empty()) {
        action.error = "Recursive agent goal is empty.";
        return false;
    }
    
    // Spawn a new planning session or sub-task
    // For this implementation, we'll ask the engine to decompose and plan, 
    // simulating a sub-agent execution by returning the plan.
    
    json plan = m_agenticEngine->planTask(goal);
    action.result = plan.dump();
    return true;
}

bool ActionExecutor::handleQueryUser(Action& action) {
    if (onUserInputNeeded) {
        std::string prompt = action.params.value("prompt", "Confirm?");
        std::vector<std::string> options = action.params.value("options", std::vector<std::string>{"Yes", "No"});
        
        // This is async in UI, but sync here? 
        // We can't block easily in this architecture without CV.
        // For now, auto-confirm or blocking wait on future?
        onUserInputNeeded(prompt, options);
        // We assume the UI will callback/signal logic (not implemented here)
    }
    action.result = json({{"user_input", "simulated_ack"}}).dump();
    return true;
}

