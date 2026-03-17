// Win32IDE_TasksDebugUI.cpp - Tasks/Debug Config UI Binding
// Connects tasks.json/launch.json to Run/Debug menu + UX
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include <algorithm>

// Forward declarations of existing APIs
extern "C" {
    // From Win32IDE_Tasks.cpp
    const char* TaskManager_runTask(const char* workspaceFolder, const char* taskLabel);
    const char* TaskManager_executeLaunchConfig(const char* workspaceFolder, const char* configName);
    const char* TaskManager_listTasks(const char* workspaceFolder);
    const char* TaskManager_listLaunchConfigs(const char* workspaceFolder);
}

namespace {
    std::mutex g_uiMutex;
    
    struct TaskResult {
        std::string label;
        int exitCode = -1;
        std::string output;
        DWORD processId = 0;
    };
    
    std::map<std::string, TaskResult> g_taskResults;
    
    struct DebugSession {
        std::string configName;
        DWORD processId = 0;
        bool attached = false;
    };
    
    std::map<std::string, DebugSession> g_debugSessions;
    
    // Variable resolver: ${workspaceFolder}, ${file}, ${fileBasename}, ${fileDirname}
    std::string resolveVariables(const std::string& input, const std::string& workspaceFolder, const std::string& currentFile) {
        std::string result = input;
        
        // ${workspaceFolder}
        size_t pos = 0;
        while ((pos = result.find("${workspaceFolder}", pos)) != std::string::npos) {
            if (!workspaceFolder.empty()) {
                result.replace(pos, 18, workspaceFolder);
                pos += workspaceFolder.length();
            } else {
                result.replace(pos, 18, "<WORKSPACE_NOT_SET>");
                pos += 21;
            }
        }
        
        // ${file}
        pos = 0;
        while ((pos = result.find("${file}", pos)) != std::string::npos) {
            if (!currentFile.empty()) {
                result.replace(pos, 7, currentFile);
                pos += currentFile.length();
            } else {
                result.replace(pos, 7, "<FILE_NOT_SET>");
                pos += 15;
            }
        }
        
        // ${fileBasename} - e.g., main.cpp
        pos = 0;
        while ((pos = result.find("${fileBasename}", pos)) != std::string::npos) {
            std::string basename = "<FILE_NOT_SET>";
            if (!currentFile.empty()) {
                size_t lastSlash = currentFile.find_last_of("/\\");
                basename = (lastSlash != std::string::npos) ? currentFile.substr(lastSlash + 1) : currentFile;
            }
            result.replace(pos, 15, basename);
            pos += basename.length();
        }
        
        // ${fileDirname} - directory of current file
        pos = 0;
        while ((pos = result.find("${fileDirname}", pos)) != std::string::npos) {
            std::string dirname = "<FILE_NOT_SET>";
            if (!currentFile.empty()) {
                size_t lastSlash = currentFile.find_last_of("/\\");
                dirname = (lastSlash != std::string::npos) ? currentFile.substr(0, lastSlash) : ".";
            }
            result.replace(pos, 14, dirname);
            pos += dirname.length();
        }
        
        return result;
    }
}

extern "C" {

// UI API: Show Run Task picker (populates menu/combobox)
const char* RawrXD_UI_ListTasksForPicker(const char* workspaceFolder) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!workspaceFolder || !workspaceFolder[0]) {
        static std::string err = "No workspace folder set";
        return err.c_str();
    }
    
    const char* tasksJson = TaskManager_listTasks(workspaceFolder);
    if (!tasksJson) {
        static std::string empty = "[]";
        return empty.c_str();
    }
    
    // Return JSON array of {label, type, command} objects
    static std::string result;
    result = tasksJson;
    return result.c_str();
}

// UI API: Run selected task (from menu/toolbar button)
const char* RawrXD_UI_RunTask(const char* workspaceFolder, const char* taskLabel, const char* currentFile) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!workspaceFolder || !taskLabel) {
        static std::string err = "{\"error\": \"Missing parameters\"}";
        return err.c_str();
    }
    
    // Resolve variables in task context
    std::string resolvedWorkspace = workspaceFolder;
    std::string resolvedFile = currentFile ? currentFile : "";
    
    // Execute task
    const char* resultJson = TaskManager_runTask(workspaceFolder, taskLabel);
    if (!resultJson) {
        static std::string err = "{\"error\": \"Task execution failed\"}";
        return err.c_str();
    }
    
    // Parse result to extract exit code + PID
    TaskResult tr;
    tr.label = taskLabel;
    
    std::string resultStr = resultJson;
    size_t exitCodePos = resultStr.find("\"exitCode\":");
    if (exitCodePos != std::string::npos) {
        tr.exitCode = std::atoi(resultStr.c_str() + exitCodePos + 11);
    }
    
    size_t pidPos = resultStr.find("\"processId\":");
    if (pidPos != std::string::npos) {
        tr.processId = std::atol(resultStr.c_str() + pidPos + 12);
    }
    
    g_taskResults[taskLabel] = tr;
    
    static std::string result;
    result = resultJson;
    return result.c_str();
}

// UI API: Get task result (exit code + output)
const char* RawrXD_UI_GetTaskResult(const char* taskLabel) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!taskLabel) {
        static std::string err = "{\"error\": \"Missing taskLabel\"}";
        return err.c_str();
    }
    
    auto it = g_taskResults.find(taskLabel);
    if (it == g_taskResults.end()) {
        static std::string notFound = "{\"error\": \"Task not found\"}";
        return notFound.c_str();
    }
    
    const TaskResult& tr = it->second;
    static std::string json;
    json = "{\"label\": \"" + tr.label + "\", \"exitCode\": " + std::to_string(tr.exitCode) + 
           ", \"processId\": " + std::to_string(tr.processId) + "}";
    return json.c_str();
}

// UI API: List debug/launch configurations
const char* RawrXD_UI_ListLaunchConfigs(const char* workspaceFolder) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!workspaceFolder || !workspaceFolder[0]) {
        static std::string err = "No workspace folder set";
        return err.c_str();
    }
    
    const char* configsJson = TaskManager_listLaunchConfigs(workspaceFolder);
    if (!configsJson) {
        static std::string empty = "[]";
        return empty.c_str();
    }
    
    static std::string result;
    result = configsJson;
    return result.c_str();
}

// UI API: Start debug session (from Run > Start Debugging menu)
const char* RawrXD_UI_StartDebugging(const char* workspaceFolder, const char* configName, const char* currentFile) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!workspaceFolder || !configName) {
        static std::string err = "{\"error\": \"Missing parameters\"}";
        return err.c_str();
    }
    
    // Execute launch config (spawns process + debugger attach)
    const char* resultJson = TaskManager_executeLaunchConfig(workspaceFolder, configName);
    if (!resultJson) {
        static std::string err = "{\"error\": \"Launch config execution failed\"}";
        return err.c_str();
    }
    
    // Parse result
    DebugSession ds;
    ds.configName = configName;
    
    std::string resultStr = resultJson;
    size_t pidPos = resultStr.find("\"processId\":");
    if (pidPos != std::string::npos) {
        ds.processId = std::atol(resultStr.c_str() + pidPos + 12);
        ds.attached = (ds.processId != 0);
    }
    
    g_debugSessions[configName] = ds;
    
    static std::string result;
    result = resultJson;
    return result.c_str();
}

// UI API: Get active debug session info
const char* RawrXD_UI_GetDebugSession(const char* configName) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!configName) {
        static std::string err = "{\"error\": \"Missing configName\"}";
        return err.c_str();
    }
    
    auto it = g_debugSessions.find(configName);
    if (it == g_debugSessions.end()) {
        static std::string notFound = "{\"error\": \"Session not found\"}";
        return notFound.c_str();
    }
    
    const DebugSession& ds = it->second;
    static std::string json;
    json = "{\"configName\": \"" + ds.configName + "\", \"processId\": " + std::to_string(ds.processId) + 
           ", \"attached\": " + (ds.attached ? "true" : "false") + "}";
    return json.c_str();
}

// UI API: Resolve variables (for preview/validation in settings UI)
const char* RawrXD_UI_ResolveVariables(const char* input, const char* workspaceFolder, const char* currentFile) {
    if (!input) {
        static std::string err = "<NULL_INPUT>";
        return err.c_str();
    }
    
    std::string ws = workspaceFolder ? workspaceFolder : "";
    std::string cf = currentFile ? currentFile : "";
    
    static std::string resolved;
    resolved = resolveVariables(input, ws, cf);
    return resolved.c_str();
}

// UI API: Get task exit code (quick accessor for build status icon)
int RawrXD_UI_GetLastTaskExitCode(const char* taskLabel) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!taskLabel) return -1;
    
    auto it = g_taskResults.find(taskLabel);
    if (it == g_taskResults.end()) return -1;
    
    return it->second.exitCode;
}

// UI API: Clear task history (for cleanup)
void RawrXD_UI_ClearTaskResults() {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    g_taskResults.clear();
}

// UI API: Stop/detach debug session
const char* RawrXD_UI_StopDebugging(const char* configName) {
    std::lock_guard<std::mutex> lock(g_uiMutex);
    
    if (!configName) {
        static std::string err = "{\"error\": \"Missing configName\"}";
        return err.c_str();
    }
    
    auto it = g_debugSessions.find(configName);
    if (it == g_debugSessions.end()) {
        static std::string notFound = "{\"error\": \"Session not found\"}";
        return notFound.c_str();
    }
    
    // Detach/terminate process
    DWORD pid = it->second.processId;
    if (pid != 0) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc) {
            TerminateProcess(hProc, 1);
            CloseHandle(hProc);
        }
    }
    
    g_debugSessions.erase(it);
    
    static std::string success = "{\"status\": \"stopped\"}";
    return success.c_str();
}

} // extern "C"
