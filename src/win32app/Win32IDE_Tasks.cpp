// ============================================================================
// Win32IDE_Tasks.cpp — Real tasks.json / launch.json support for Win32IDE
// ============================================================================
// Parse or generate tasks.json / launch.json for "Run task" / "Debug config"
// Drive compiler/debugger from IDE without command-line
// Supports VS Code task format for compatibility
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace RawrXD {
namespace IDE {

// ============================================================================
// Task Definition
// ============================================================================

struct Task {
    std::string label;
    std::string type;          // "shell", "process"
    std::string command;
    std::vector<std::string> args;
    std::string cwd;
    std::unordered_map<std::string, std::string> env;
    std::string group;         // "build", "test", "none"
    std::string problemMatcher; // "$gcc", "$msvc", etc.
    std::string dependsOn;     // Label of prerequisite task
    bool isBackground = false;
    bool presentation_reveal = true;
};

struct LaunchConfig {
    std::string name;
    std::string type;          // "cppdbg", "gdb", "lldb"
    std::string request;       // "launch", "attach"
    std::string program;
    std::vector<std::string> args;
    std::string cwd;
    std::string stopAtEntry;
    std::string preLaunchTask; // Label of task to run before launch
    std::unordered_map<std::string, std::string> env;
    bool externalConsole = false;
    int  processId = 0;        // PID for attach requests
};

// ============================================================================
// Task Manager
// ============================================================================

class TaskManager {
private:
    std::string m_workspacePath;
    std::string m_tasksJsonPath;
    std::string m_launchJsonPath;
    std::vector<Task> m_tasks;
    std::vector<LaunchConfig> m_launchConfigs;
    std::mutex m_mutex;
    bool m_initialized = false;
    DWORD m_attachedPid = 0;
    HANDLE m_attachedProcess = nullptr;
    bool m_debugAttachPending = false;
    std::function<void(const std::string&)> m_outputCallback;
    
public:
    TaskManager() = default;
    ~TaskManager() = default;
    
    // Initialize for workspace
    bool initialize(const std::string& workspacePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_workspacePath = workspacePath;
        m_tasksJsonPath = workspacePath + "/.vscode/tasks.json";
        m_launchJsonPath = workspacePath + "/.vscode/launch.json";
        
        // Load existing configurations
        loadTasks();
        loadLaunchConfigs();
        
        // Generate default tasks if none exist
        if (m_tasks.empty()) {
            generateDefaultTasks();
        }
        
        // Generate default launch configs if none exist
        if (m_launchConfigs.empty()) {
            generateDefaultLaunchConfigs();
        }
        
        m_initialized = true;
        
        fprintf(stderr, "[TaskManager] Initialized for workspace: %s\n",
                workspacePath.c_str());
        fprintf(stderr, "[TaskManager] Loaded %zu tasks, %zu launch configs\n",
                m_tasks.size(), m_launchConfigs.size());
        
        return true;
    }
    
    // Get all tasks
    std::vector<Task> getTasks() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_tasks;
    }
    
    // Get all launch configs
    std::vector<LaunchConfig> getLaunchConfigs() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_launchConfigs;
    }
    
    // Run task by label (async — runs in background thread, does not block UI)
    bool runTask(const std::string& label) {
        // Find task
        Task task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bool found = false;
            for (auto& t : m_tasks) {
                if (t.label == label) { task = t; found = true; break; }
            }
            if (!found) {
                fprintf(stderr, "[TaskManager] Task not found: %s\n", label.c_str());
                return false;
            }
        }

        fprintf(stderr, "[TaskManager] Queuing task: %s\n", label.c_str());

        // Run dependent task first (synchronous chain)
        if (!task.dependsOn.empty()) {
            fprintf(stderr, "[TaskManager] Running dependency: %s\n", task.dependsOn.c_str());
            Task depTask;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& t : m_tasks) {
                    if (t.label == task.dependsOn) { depTask = t; break; }
                }
            }
            if (!depTask.label.empty()) {
                auto depResult = executeTask(depTask);
                if (!depResult.success || depResult.exitCode != 0) {
                    fprintf(stderr, "[TaskManager] Dependency '%s' failed (exit=%lu). Aborting.\n",
                            task.dependsOn.c_str(), depResult.exitCode);
                    return false;
                }
            }
        }

        // Run the actual task in a background thread (non-blocking)
        std::thread taskThread([this, task]() {
            auto result = executeTask(task);
            fprintf(stderr, "[TaskManager] Async task '%s' completed (exit=%lu)\n",
                    task.label.c_str(), result.exitCode);
        });
        taskThread.detach();
        return true;
    }
    
    // Launch debug configuration (with preLaunchTask support)
    bool launchDebugConfig(const std::string& name) {
        LaunchConfig config;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bool found = false;
            for (auto& cfg : m_launchConfigs) {
                if (cfg.name == name) { config = cfg; found = true; break; }
            }
            if (!found) {
                fprintf(stderr, "[TaskManager] Launch config not found: %s\n", name.c_str());
                return false;
            }
        }

        fprintf(stderr, "[TaskManager] Launching debug config: %s\n", name.c_str());

        // Run preLaunchTask if specified
        if (!config.preLaunchTask.empty()) {
            fprintf(stderr, "[TaskManager] Running preLaunchTask: %s\n", config.preLaunchTask.c_str());
            Task preTask;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& t : m_tasks) {
                    if (t.label == config.preLaunchTask) { preTask = t; break; }
                }
            }
            if (!preTask.label.empty()) {
                auto result = executeTask(preTask);
                if (!result.success || result.exitCode != 0) {
                    fprintf(stderr, "[TaskManager] preLaunchTask '%s' failed (exit=%lu). Launch aborted.\n",
                            config.preLaunchTask.c_str(), result.exitCode);
                    return false;
                }
                fprintf(stderr, "[TaskManager] preLaunchTask completed successfully.\n");
            } else {
                fprintf(stderr, "[TaskManager] WARNING: preLaunchTask '%s' not found in tasks.\n",
                        config.preLaunchTask.c_str());
            }
        }

        // Execute launch configuration
        return executeLaunchConfig(config);
    }
    
    // Add custom task
    void addTask(const Task& task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if task already exists
        for (size_t i = 0; i < m_tasks.size(); ++i) {
            if (m_tasks[i].label == task.label) {
                m_tasks[i] = task;
                saveTasks();
                return;
            }
        }
        
        m_tasks.push_back(task);
        saveTasks();
    }
    
    // Add custom launch config
    void addLaunchConfig(const LaunchConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if config already exists
        for (size_t i = 0; i < m_launchConfigs.size(); ++i) {
            if (m_launchConfigs[i].name == config.name) {
                m_launchConfigs[i] = config;
                saveLaunchConfigs();
                return;
            }
        }
        
        m_launchConfigs.push_back(config);
        saveLaunchConfigs();
    }
    
private:
    bool loadTasks() {
        if (!fs::exists(m_tasksJsonPath)) {
            return false;
        }
        
        fprintf(stderr, "[TaskManager] Loading tasks from: %s\n", m_tasksJsonPath.c_str());

        std::ifstream file(m_tasksJsonPath);
        if (!file.is_open()) {
            return false;
        }

        try {
            std::string raw((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
            json j = json::parse(raw, nullptr, false);
            if (j.is_discarded()) {
                return false;
            }

            if (!j.contains("tasks") || !j["tasks"].is_array()) {
                return false;
            }

            m_tasks.clear();
            for (const auto& tj : j["tasks"]) {
                if (!tj.is_object()) continue;

                Task t;
                t.label = tj.value("label", "");
                t.type = tj.value("type", "shell");
                t.command = tj.value("command", "");
                t.cwd = tj.value("cwd", "");
                t.group = tj.value("group", "none");
                t.problemMatcher = tj.value("problemMatcher", "");
                t.isBackground = tj.value("isBackground", false);
                t.dependsOn = tj.value("dependsOn", "");

                if (tj.contains("args") && tj["args"].is_array()) {
                    for (const auto& a : tj["args"]) {
                        if (a.is_string()) t.args.push_back(a.get<std::string>());
                    }
                }

                if (tj.contains("env") && tj["env"].is_object()) {
                    for (auto it = tj["env"].begin(); it != tj["env"].end(); ++it) {
                        if (it.value().is_string()) {
                            t.env[it.key()] = it.value().get<std::string>();
                        }
                    }
                }

                if (tj.contains("presentation") && tj["presentation"].is_object()) {
                    t.presentation_reveal = tj["presentation"].value("reveal", true);
                }

                if (!t.label.empty() && !t.command.empty()) {
                    m_tasks.push_back(std::move(t));
                }
            }
            return !m_tasks.empty();
        } catch (...) {
            return false;
        }
    }
    
    bool loadLaunchConfigs() {
        if (!fs::exists(m_launchJsonPath)) {
            return false;
        }
        
        fprintf(stderr, "[TaskManager] Loading launch configs from: %s\n",
                m_launchJsonPath.c_str());

        std::ifstream file(m_launchJsonPath);
        if (!file.is_open()) {
            return false;
        }

        try {
            std::string raw((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
            json j = json::parse(raw, nullptr, false);
            if (j.is_discarded()) {
                return false;
            }
            if (!j.contains("configurations") || !j["configurations"].is_array()) {
                return false;
            }

            m_launchConfigs.clear();
            for (const auto& cj : j["configurations"]) {
                if (!cj.is_object()) continue;

                LaunchConfig c;
                c.name = cj.value("name", "");
                c.type = cj.value("type", "cppdbg");
                c.request = cj.value("request", "launch");
                c.program = cj.value("program", "");
                c.cwd = cj.value("cwd", "");
                c.stopAtEntry = cj.value("stopAtEntry", "false");
                c.externalConsole = cj.value("externalConsole", false);
                c.processId = cj.value("processId", 0);
                c.preLaunchTask = cj.value("preLaunchTask", "");

                if (cj.contains("args") && cj["args"].is_array()) {
                    for (const auto& a : cj["args"]) {
                        if (a.is_string()) c.args.push_back(a.get<std::string>());
                    }
                }

                if (cj.contains("env") && cj["env"].is_object()) {
                    for (auto it = cj["env"].begin(); it != cj["env"].end(); ++it) {
                        if (it.value().is_string()) {
                            c.env[it.key()] = it.value().get<std::string>();
                        }
                    }
                }

                if (!c.name.empty()) {
                    m_launchConfigs.push_back(std::move(c));
                }
            }

            return !m_launchConfigs.empty();
        } catch (...) {
            return false;
        }
    }
    
    void generateDefaultTasks() {
        // Build task
        Task buildTask;
        buildTask.label = "Build (MSVC)";
        buildTask.type = "shell";
        buildTask.command = "cl.exe";
        buildTask.args = {"/c", "/EHsc", "/Zi", "${file}", "/Fo:${fileDirname}\\${fileBasenameNoExtension}.obj"};
        buildTask.group = "build";
        buildTask.problemMatcher = "$msvc";
        m_tasks.push_back(buildTask);
        
        // Build + Link task
        Task buildLinkTask;
        buildLinkTask.label = "Build + Link (MSVC)";
        buildLinkTask.type = "shell";
        buildLinkTask.command = "cl.exe";
        buildLinkTask.args = {"/EHsc", "/Zi", "${file}", "/Fe:${fileDirname}\\${fileBasenameNoExtension}.exe"};
        buildLinkTask.group = "build";
        buildLinkTask.problemMatcher = "$msvc";
        m_tasks.push_back(buildLinkTask);
        
        // Clean task
        Task cleanTask;
        cleanTask.label = "Clean";
        cleanTask.type = "shell";
        cleanTask.command = "powershell";
        cleanTask.args = {"-Command", "Remove-Item *.obj,*.exe,*.pdb -ErrorAction SilentlyContinue"};
        cleanTask.group = "none";
        m_tasks.push_back(cleanTask);
        
        fprintf(stderr, "[TaskManager] Generated %zu default tasks\n", m_tasks.size());
    }
    
    void generateDefaultLaunchConfigs() {
        // Debug current file
        LaunchConfig debugConfig;
        debugConfig.name = "Debug Current File";
        debugConfig.type = "cppdbg";
        debugConfig.request = "launch";
        debugConfig.program = "${fileDirname}\\${fileBasenameNoExtension}.exe";
        debugConfig.args = {};
        debugConfig.cwd = "${workspaceFolder}";
        debugConfig.stopAtEntry = "false";
        debugConfig.externalConsole = false;
        m_launchConfigs.push_back(debugConfig);
        
        // Attach to process
        LaunchConfig attachConfig;
        attachConfig.name = "Attach to Process";
        attachConfig.type = "cppdbg";
        attachConfig.request = "attach";
        attachConfig.program = "";
        m_launchConfigs.push_back(attachConfig);
        
        fprintf(stderr, "[TaskManager] Generated %zu default launch configs\n",
                m_launchConfigs.size());
    }

    // Callback for streaming task output to UI (uses the private m_outputCallback declared above)
    void setOutputCallback(std::function<void(const std::string&)> cb) {
        m_outputCallback = std::move(cb);
    }

    struct TaskResult {
        DWORD exitCode;
        std::string capturedOutput;
        bool  success;
    };

    struct ParsedProblem {
        std::string file;
        int line;
        int col;
        std::string message;
        int severity; // 0=error, 1=warning
    };
    std::vector<ParsedProblem> m_parsedProblems;
    const std::vector<ParsedProblem>& getParsedProblems() const { return m_parsedProblems; }
    void clearParsedProblems() { m_parsedProblems.clear(); }

    // Simple problem matcher: parse common gcc/cl error patterns
    void parseProblemMatcherLine(const Task& /*task*/, const char* line) {
        if (!line) return;
        const char* p = line;
        while (*p) {
            const char* eol = p;
            while (*eol && *eol != '\n' && *eol != '\r') ++eol;
            size_t lineLen = static_cast<size_t>(eol - p);
            if (lineLen > 8) {
                std::string ln(p, lineLen);
                // MSVC pattern: file(line): error Cxxxx: message
                auto paren = ln.find('(');
                auto colon = ln.find("): error ");
                auto warn  = ln.find("): warning ");
                if (paren != std::string::npos && (colon != std::string::npos || warn != std::string::npos)) {
                    std::string file = ln.substr(0, paren);
                    int lineNo = 0;
                    try { lineNo = std::stoi(ln.substr(paren + 1)); } catch (...) {}
                    int sev = (colon != std::string::npos) ? 0 : 1;
                    size_t msgStart = (colon != std::string::npos) ? colon + 2 : warn + 2;
                    std::string msg = (msgStart < ln.size()) ? ln.substr(msgStart) : ln;
                    m_parsedProblems.push_back({ file, lineNo, 1, msg, sev });
                }
                // GCC/Clang pattern: file:line:col: error|warning: message
                else {
                    size_t c1 = ln.find(':');
                    if (c1 != std::string::npos && c1 > 1) {
                        size_t c2 = ln.find(':', c1 + 1);
                        size_t c3 = (c2 != std::string::npos) ? ln.find(':', c2 + 1) : std::string::npos;
                        if (c3 != std::string::npos) {
                            std::string tag = ln.substr(c3 + 1);
                            while (!tag.empty() && tag[0] == ' ') tag.erase(0, 1);
                            int sev = -1;
                            if (tag.rfind("error", 0) == 0)   sev = 0;
                            if (tag.rfind("warning", 0) == 0) sev = 1;
                            if (sev >= 0) {
                                std::string file = ln.substr(0, c1);
                                int lineNo = 0, colNo = 1;
                                try { lineNo = std::stoi(ln.substr(c1 + 1, c2 - c1 - 1)); } catch (...) {}
                                try { colNo  = std::stoi(ln.substr(c2 + 1, c3 - c2 - 1)); } catch (...) {}
                                size_t msgPos = tag.find(':');
                                std::string msg = (msgPos != std::string::npos) ? tag.substr(msgPos + 1) : tag;
                                while (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
                                m_parsedProblems.push_back({ file, lineNo, colNo, msg, sev });
                            }
                        }
                    }
                }
            }
            p = eol;
            while (*p == '\n' || *p == '\r') ++p;
        }
    }

    TaskResult executeTask(const Task& task) {
        TaskResult result = { 0, "", false };
#ifdef _WIN32
        std::string cmdLine = task.command;
        for (const auto& arg : task.args) {
            cmdLine += " " + arg;
        }
        cmdLine = replaceVariables(cmdLine);

        fprintf(stderr, "[TaskManager] Executing: %s\n", cmdLine.c_str());

        // Create stdout/stderr capture pipes
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            fprintf(stderr, "[TaskManager] Failed to create pipe: %lu\n", GetLastError());
            return result;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        // Build environment block with task.env overrides
        std::string envBlock;
        if (!task.env.empty()) {
            char* parentEnv = GetEnvironmentStringsA();
            if (parentEnv) {
                const char* pe = parentEnv;
                while (*pe) { size_t l = strlen(pe) + 1; envBlock.append(pe, l); pe += l; }
                FreeEnvironmentStringsA(parentEnv);
            }
            for (const auto& kv : task.env) {
                envBlock += kv.first + "=" + kv.second;
                envBlock.push_back('\0');
            }
            envBlock.push_back('\0');
        }

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWritePipe;
        si.hStdError  = hWritePipe;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.wShowWindow = task.presentation_reveal ? SW_SHOW : SW_HIDE;

        PROCESS_INFORMATION pi = {};
        char cmdBuf[4096];
        strncpy_s(cmdBuf, cmdLine.c_str(), _TRUNCATE);
        std::string taskCwd = replaceVariables(task.cwd);
        const char* runCwd = taskCwd.empty() ? nullptr : taskCwd.c_str();
        DWORD createFlags = CREATE_NO_WINDOW;
        const char* pEnvBlock = envBlock.empty() ? nullptr : envBlock.c_str();

        clearParsedProblems();

        if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE, createFlags,
                          (LPVOID)pEnvBlock, runCwd, &si, &pi)) {
            CloseHandle(hWritePipe);
            hWritePipe = nullptr;

            // Streaming read loop
            char readBuf[4096];
            DWORD bytesRead = 0;
            constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(readBuf) - 1);
            while (ReadFile(hReadPipe, readBuf, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
                const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                readBuf[safeBytes] = '\0';
                result.capturedOutput.append(readBuf, safeBytes);

                // Stream to output panel callback
                if (m_outputCallback) {
                    m_outputCallback(std::string("[") + task.label + "] " +
                                     std::string(readBuf, safeBytes));
                }
                // Run problem matcher
                parseProblemMatcherLine(task, readBuf);
            }

            if (!task.isBackground) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                GetExitCodeProcess(pi.hProcess, &result.exitCode);
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            result.success = true;

            fprintf(stderr, "[TaskManager] Task completed (exit=%lu, output=%zu bytes, problems=%zu)\n",
                    result.exitCode, result.capturedOutput.size(), m_parsedProblems.size());
        } else {
            fprintf(stderr, "[TaskManager] Failed to execute task: %lu\n", GetLastError());
        }

        if (hWritePipe) CloseHandle(hWritePipe);
        if (hReadPipe)  CloseHandle(hReadPipe);
        return result;
#else
        return result;
#endif
    }
    
    bool executeLaunchConfig(const LaunchConfig& config) {
        fprintf(stderr, "[TaskManager] Launching: %s\n", config.program.c_str());
        fprintf(stderr, "[TaskManager] Type: %s, Request: %s\n",
                config.type.c_str(), config.request.c_str());

        if (config.request == "attach") {
            // Attach to running process — hand off to debugger subsystem
#ifdef _WIN32
            if (config.processId > 0) {
                DWORD pid = static_cast<DWORD>(config.processId);
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                if (!hProcess) {
                    fprintf(stderr, "[TaskManager] Failed to open PID %d: %lu\n",
                            config.processId, GetLastError());
                    return false;
                }
                // Store the process handle and PID for the debug subsystem
                // The debugger (Win32IDE_Debugger) will pick this up via m_attachedPid/m_attachedProcess
                m_attachedPid = pid;
                m_attachedProcess = hProcess;
                m_debugAttachPending = true;
                fprintf(stderr, "[TaskManager] Attached to PID %d — debug session ready\n",
                        config.processId);
                // Fire output callback so IDE can update UI
                if (m_outputCallback) {
                    m_outputCallback("[Launch] Attached to PID " + std::to_string(pid) +
                                     " — debug controls active\n");
                }
                return true;
            }
            // No PID: try to enumerate processes and prompt
            fprintf(stderr, "[TaskManager] Attach request without PID; set processId in launch.json "
                            "or use \"${command:pickProcess}\"\n");
            if (m_outputCallback) {
                m_outputCallback("[Launch] No processId specified. Add \"processId\" to launch.json "
                                 "or use \"${command:pickProcess}\".\n");
            }
            return false;
#else
            return false;
#endif
        }

        if (config.request != "launch") {
            fprintf(stderr, "[TaskManager] Unsupported request type: %s\n", config.request.c_str());
            return false;
        }

        std::string program = replaceVariables(config.program);
        if (program.empty()) return false;

        std::string cmdLine = "\"" + program + "\"";
        for (const auto& arg : config.args) {
            cmdLine += " " + replaceVariables(arg);
        }

#ifdef _WIN32
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = config.externalConsole ? SW_SHOW : SW_SHOWNORMAL;
        DWORD createFlags = config.externalConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;

        // If stopAtEntry is set, launch with debug flags for breakpoint support
        if (config.stopAtEntry == "true") {
            createFlags |= DEBUG_ONLY_THIS_PROCESS;
        }

        char cmdBuf[4096];
        strncpy_s(cmdBuf, cmdLine.c_str(), _TRUNCATE);

        std::string cwd = replaceVariables(config.cwd);
        const char* launchCwd = cwd.empty() ? nullptr : cwd.c_str();

        // Build environment block if config has env vars
        std::string envBlock;
        if (!config.env.empty()) {
            // Inherit current env + add overrides
            for (const auto& [k, v] : config.env) {
                envBlock += k + "=" + v;
                envBlock.push_back('\0');
            }
            envBlock.push_back('\0');
        }

        LPVOID envPtr = config.env.empty() ? nullptr : (LPVOID)envBlock.data();

        if (!CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, FALSE, createFlags,
                            envPtr, launchCwd, &si, &pi)) {
            fprintf(stderr, "[TaskManager] Launch failed: %lu\n", GetLastError());
            if (m_outputCallback) {
                m_outputCallback("[Launch] Failed to start: " + program +
                                 " (error " + std::to_string(GetLastError()) + ")\n");
            }
            return false;
        }

        // Store process info for monitoring/termination
        m_attachedPid = pi.dwProcessId;
        m_attachedProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        if (m_outputCallback) {
            m_outputCallback("[Launch] Started PID " + std::to_string(pi.dwProcessId) +
                             ": " + program + "\n");
        }

        // If stopAtEntry was set, we're debugging — continue first breakpoint
        if (config.stopAtEntry == "true") {
            m_debugAttachPending = true;
        }

        return true;
#else
        return false;
#endif
    }
    
    std::string replaceVariables(const std::string& str) const {
        std::string result = str;

        auto replaceAll = [&result](const std::string& key, const std::string& value) {
            size_t pos = 0;
            while ((pos = result.find(key, pos)) != std::string::npos) {
                result.replace(pos, key.size(), value);
                pos += value.size();
            }
        };

        // ${workspaceFolder}
        replaceAll("${workspaceFolder}", m_workspacePath);

        // ${env:VARNAME} — expand environment variables
        {
            const std::string prefix = "${env:";
            size_t pos = 0;
            while ((pos = result.find(prefix, pos)) != std::string::npos) {
                size_t end = result.find('}', pos + prefix.size());
                if (end == std::string::npos) break;
                std::string varName = result.substr(pos + prefix.size(), end - pos - prefix.size());
                const char* envVal = std::getenv(varName.c_str());
                std::string replacement = envVal ? envVal : "";
                result.replace(pos, end - pos + 1, replacement);
                pos += replacement.size();
            }
        }

        // ${config:SETTING} — expand from workspace settings.json
        {
            const std::string prefix = "${config:";
            size_t pos = 0;
            while ((pos = result.find(prefix, pos)) != std::string::npos) {
                size_t end = result.find('}', pos + prefix.size());
                if (end == std::string::npos) break;
                std::string settingKey = result.substr(pos + prefix.size(), end - pos - prefix.size());
                // Try to load from .vscode/settings.json
                std::string settingVal;
                fs::path settingsPath = fs::path(m_workspacePath) / ".vscode" / "settings.json";
                if (fs::exists(settingsPath)) {
                    std::ifstream sf(settingsPath);
                    if (sf.is_open()) {
                        std::string raw((std::istreambuf_iterator<char>(sf)),
                                         std::istreambuf_iterator<char>());
                        auto j = nlohmann::json::parse(raw, nullptr, false);
                        if (!j.is_discarded() && j.contains(settingKey)) {
                            if (j[settingKey].is_string())
                                settingVal = j[settingKey].get<std::string>();
                            else
                                settingVal = j[settingKey].dump();
                        }
                    }
                }
                result.replace(pos, end - pos + 1, settingVal);
                pos += settingVal.size();
            }
        }

        // ${input:ID} — prompt user (resolve to empty if non-interactive)
        {
            const std::string prefix = "${input:";
            size_t pos = 0;
            while ((pos = result.find(prefix, pos)) != std::string::npos) {
                size_t end = result.find('}', pos + prefix.size());
                if (end == std::string::npos) break;
                std::string inputId = result.substr(pos + prefix.size(), end - pos - prefix.size());
                // Look up input definition in tasks.json inputs array
                std::string inputVal;
                fs::path tasksPath = fs::path(m_workspacePath) / ".vscode" / "tasks.json";
                if (fs::exists(tasksPath)) {
                    std::ifstream tf(tasksPath);
                    if (tf.is_open()) {
                        std::string raw((std::istreambuf_iterator<char>(tf)),
                                         std::istreambuf_iterator<char>());
                        auto j = nlohmann::json::parse(raw, nullptr, false);
                        if (!j.is_discarded() && j.contains("inputs") && j["inputs"].is_array()) {
                            for (const auto& inp : j["inputs"]) {
                                if (inp.value("id", "") == inputId) {
                                    inputVal = inp.value("default", "");
                                    break;
                                }
                            }
                        }
                    }
                }
                result.replace(pos, end - pos + 1, inputVal);
                pos += inputVal.size();
            }
        }

        std::string activeFile;
        if (const char* envFile = std::getenv("RAWRXD_ACTIVE_FILE")) {
            activeFile = envFile;
        }
        if (activeFile.empty()) {
            for (const auto& candidate : {
                     std::string("main.cpp"), std::string("main.c"), std::string("main.asm"),
                     std::string("src/main.cpp"), std::string("src/main.c"), std::string("src/main.asm")
                 }) {
                fs::path p = fs::path(m_workspacePath) / candidate;
                if (fs::exists(p)) {
                    activeFile = p.string();
                    break;
                }
            }
        }

        if (!activeFile.empty()) {
            fs::path p(activeFile);
            replaceAll("${file}", p.string());
            replaceAll("${fileDirname}", p.parent_path().string());
            replaceAll("${fileBasename}", p.filename().string());
            replaceAll("${fileBasenameNoExtension}", p.stem().string());
            replaceAll("${fileExtname}", p.extension().string());
        }

        // ${lineNumber} — default to 1
        replaceAll("${lineNumber}", "1");

        // ${selectedText} — default to empty
        replaceAll("${selectedText}", "");

        // ${cwd}
        replaceAll("${cwd}", m_workspacePath);

        // ${workspaceFolderBasename}
        replaceAll("${workspaceFolderBasename}", fs::path(m_workspacePath).filename().string());

        // ${pathSeparator}
#ifdef _WIN32
        replaceAll("${pathSeparator}", "\\");
#else
        replaceAll("${pathSeparator}", "/");
#endif

        // ${defaultBuildTask} — resolve to first task with group "build"
        for (const auto& t : m_tasks) {
            if (t.group == "build" || t.group == "{\"kind\":\"build\",\"isDefault\":true}") {
                replaceAll("${defaultBuildTask}", t.label);
                break;
            }
        }

        return result;
    }
    
    bool saveTasks() {
        // JSON string escaping helper
        auto escapeJson = [](const std::string& s) -> std::string {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b";  break;
                    case '\f': out += "\\f";  break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char hex[8];
                            snprintf(hex, sizeof(hex), "\\u%04x", (unsigned)c);
                            out += hex;
                        } else {
                            out += c;
                        }
                }
            }
            return out;
        };

        // Create .vscode directory if needed
        fs::path vscodePath = fs::path(m_workspacePath) / ".vscode";
        try {
            fs::create_directories(vscodePath);
        } catch (...) {
            return false;
        }
        
        std::ofstream file(m_tasksJsonPath);
        if (!file.is_open()) {
            return false;
        }
        
        file << "{\n";
        file << "  \"version\": \"2.0.0\",\n";
        file << "  \"tasks\": [\n";
        
        for (size_t i = 0; i < m_tasks.size(); ++i) {
            const auto& task = m_tasks[i];
            file << "    {\n";
            file << "      \"label\": \"" << escapeJson(task.label) << "\",\n";
            file << "      \"type\": \"" << escapeJson(task.type) << "\",\n";
            file << "      \"command\": \"" << escapeJson(task.command) << "\"";
            
            // Serialize args array
            if (!task.args.empty()) {
                file << ",\n      \"args\": [";
                for (size_t a = 0; a < task.args.size(); ++a) {
                    file << "\"" << escapeJson(task.args[a]) << "\"";
                    if (a < task.args.size() - 1) file << ", ";
                }
                file << "]";
            }

            // Serialize env map
            if (!task.env.empty()) {
                file << ",\n      \"options\": { \"env\": {";
                size_t ei = 0;
                for (const auto& kv : task.env) {
                    file << "\"" << escapeJson(kv.first) << "\": \"" << escapeJson(kv.second) << "\"";
                    if (++ei < task.env.size()) file << ", ";
                }
                file << "} }";
            }

            if (!task.group.empty()) {
                file << ",\n      \"group\": \"" << escapeJson(task.group) << "\"";
            }

            if (!task.problemMatcher.empty()) {
                file << ",\n      \"problemMatcher\": \"" << escapeJson(task.problemMatcher) << "\"";
            }

            file << "\n    }";
            if (i < m_tasks.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        file.close();
        return true;
    }
    
    bool saveLaunchConfigs() {
        // JSON string escaping helper
        auto escapeJson = [](const std::string& s) -> std::string {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b";  break;
                    case '\f': out += "\\f";  break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char hex[8];
                            snprintf(hex, sizeof(hex), "\\u%04x", (unsigned)c);
                            out += hex;
                        } else {
                            out += c;
                        }
                }
            }
            return out;
        };

        fs::path vscodePath = fs::path(m_workspacePath) / ".vscode";
        try {
            fs::create_directories(vscodePath);
        } catch (...) {
            return false;
        }
        
        std::ofstream file(m_launchJsonPath);
        if (!file.is_open()) {
            return false;
        }
        
        file << "{\n";
        file << "  \"version\": \"0.2.0\",\n";
        file << "  \"configurations\": [\n";
        
        for (size_t i = 0; i < m_launchConfigs.size(); ++i) {
            const auto& cfg = m_launchConfigs[i];
            file << "    {\n";
            file << "      \"name\": \"" << escapeJson(cfg.name) << "\",\n";
            file << "      \"type\": \"" << escapeJson(cfg.type) << "\",\n";
            file << "      \"request\": \"" << escapeJson(cfg.request) << "\",\n";
            file << "      \"program\": \"" << escapeJson(cfg.program) << "\"";

            // Serialize args
            if (!cfg.args.empty()) {
                file << ",\n      \"args\": [";
                for (size_t a = 0; a < cfg.args.size(); ++a) {
                    file << "\"" << escapeJson(cfg.args[a]) << "\"";
                    if (a < cfg.args.size() - 1) file << ", ";
                }
                file << "]";
            }

            // Serialize cwd
            if (!cfg.cwd.empty()) {
                file << ",\n      \"cwd\": \"" << escapeJson(cfg.cwd) << "\"";
            }

            // Serialize stopAtEntry
            if (cfg.stopAtEntry == "true") {
                file << ",\n      \"stopAtEntry\": true";
            }

            // Serialize externalConsole
            if (cfg.externalConsole) {
                file << ",\n      \"externalConsole\": true";
            }

            // Serialize env
            if (!cfg.env.empty()) {
                file << ",\n      \"env\": {";
                size_t ei = 0;
                for (const auto& [k, v] : cfg.env) {
                    file << "\"" << escapeJson(k) << "\": \"" << escapeJson(v) << "\"";
                    if (++ei < cfg.env.size()) file << ", ";
                }
                file << "}";
            }

            // Serialize processId for attach configs
            if (cfg.request == "attach" && cfg.processId > 0) {
                file << ",\n      \"processId\": " << cfg.processId;
            }

            file << "\n    }";
            if (i < m_launchConfigs.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        file.close();
        return true;
    }
};

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<TaskManager> g_taskManager;
static std::mutex g_taskMutex;

} // namespace IDE
} // namespace RawrXD

// ============================================================================
// C API
// ============================================================================

extern "C" {

bool RawrXD_IDE_InitTaskManager(const char* workspacePath) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_taskMutex);
    
    RawrXD::IDE::g_taskManager = std::make_unique<RawrXD::IDE::TaskManager>();
    return RawrXD::IDE::g_taskManager->initialize(workspacePath ? workspacePath : ".");
}

bool RawrXD_IDE_RunTask(const char* label) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_taskMutex);
    
    if (!RawrXD::IDE::g_taskManager || !label) {
        return false;
    }
    
    return RawrXD::IDE::g_taskManager->runTask(label);
}

bool RawrXD_IDE_LaunchDebugConfig(const char* name) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_taskMutex);
    
    if (!RawrXD::IDE::g_taskManager || !name) {
        return false;
    }
    
    return RawrXD::IDE::g_taskManager->launchDebugConfig(name);
}

} // extern "C"
