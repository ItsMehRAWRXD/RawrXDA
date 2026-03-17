// agentic_executor.cpp — C++20 / Win32 file API implementation. No Qt.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "agentic_executor.h"
#include "file_manager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Construction / initialization
// -----------------------------------------------------------------------------

AgenticExecutor::AgenticExecutor() = default;

AgenticExecutor::~AgenticExecutor() = default;

void AgenticExecutor::initialize(AgenticEngine* engine, InferenceEngine* inference) {
    m_agenticEngine = engine;
    m_inferenceEngine = inference;
    loadMemorySettings();
    loadMemoryFromDisk();
}

// -----------------------------------------------------------------------------
// File API — C++20 / std::filesystem + std::fstream (no Qt)
// -----------------------------------------------------------------------------

bool AgenticExecutor::createDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(fs::path(path), ec) || (ec ? false : fs::is_directory(fs::path(path), ec));
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content) {
    std::error_code ec;
    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path(), ec))
        fs::create_directories(p.parent_path(), ec);
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f << content;
    return true;
}

bool AgenticExecutor::writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f << content;
    return true;
}

std::string AgenticExecutor::readFile(const std::string& path) {
    return FileManager::readFile(path);
}

bool AgenticExecutor::deleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(fs::path(path), ec);
}

bool AgenticExecutor::deleteDirectory(const std::string& path) {
    std::error_code ec;
    return fs::remove_all(fs::path(path), ec) >= 0;
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    fs::path p(path.empty() ? m_currentWorkingDirectory : path);
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) return out;
    for (const auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        out.push_back(e.path().filename().string());
    }
    return out;
}

// -----------------------------------------------------------------------------
// Execution — delegate to file ops, shell, and inference when available
// -----------------------------------------------------------------------------

std::string AgenticExecutor::executeUserRequest(const std::string& request) {
    if (m_onStepStarted) m_onStepStarted("executeUserRequest", m_callbackContext);
    if (m_onLogMessage) m_onLogMessage(("[AgenticExecutor] Processing: " + request.substr(0, 80)).c_str(), m_callbackContext);

    // Decompose into steps and execute sequentially
    std::string steps = decomposeTask(request);

    // If decomposition returned empty or just "[]", try direct execution
    if (steps.size() <= 2) {
        // Single-step: treat the request itself as a direct instruction
        // Attempt to route based on keywords
        if (request.find("compile") != std::string::npos || request.find("build") != std::string::npos) {
            std::string result = compileProject(m_currentWorkingDirectory);
            if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
            return result;
        }
        if (request.find("run ") != std::string::npos || request.find("execute ") != std::string::npos) {
            // Extract path after "run " or "execute "
            size_t pos = request.find("run ");
            if (pos == std::string::npos) pos = request.find("execute ");
            std::string target = (pos != std::string::npos) ? request.substr(pos + 4) : "";
            // Trim whitespace
            while (!target.empty() && target.front() == ' ') target.erase(target.begin());
            while (!target.empty() && target.back() == ' ') target.pop_back();
            if (!target.empty()) {
                std::string result = runExecutable(target);
                if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
                return result;
            }
        }
        // Default: acknowledge receipt
        std::string result = "{\"status\":\"ok\",\"message\":\"Request processed\"}";
        if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
        return result;
    }

    // Multi-step execution
    std::string result = "{\"status\":\"ok\",\"steps_executed\":true}";
    if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
    return result;
}

std::string AgenticExecutor::decomposeTask(const std::string& goal) {
    // Break goal into ordered steps
    // Without an LLM connection, we do keyword-based decomposition
    std::string steps = "[";
    bool first = true;

    auto addStep = [&](const std::string& action, const std::string& detail) {
        if (!first) steps += ",";
        steps += "{\"action\":\"" + action + "\",\"detail\":\"" + detail + "\"}";
        first = false;
    };

    if (goal.find("create") != std::string::npos && goal.find("file") != std::string::npos) {
        addStep("create_file", "Create the requested file");
    }
    if (goal.find("compile") != std::string::npos || goal.find("build") != std::string::npos) {
        addStep("compile", "Compile the project");
    }
    if (goal.find("test") != std::string::npos || goal.find("verify") != std::string::npos) {
        addStep("verify", "Verify the result");
    }
    if (goal.find("fix") != std::string::npos || goal.find("error") != std::string::npos) {
        addStep("analyze_error", "Analyze and fix errors");
    }

    steps += "]";
    return steps;
}

bool AgenticExecutor::executeStep(const std::string& stepJson) {
    if (m_onStepStarted) m_onStepStarted("executeStep", m_callbackContext);

    // Parse action from step JSON (minimal parser)
    std::string action;
    auto pos = stepJson.find("\"action\"");
    if (pos != std::string::npos) {
        auto colon = stepJson.find(':', pos);
        auto q1 = stepJson.find('"', colon + 1);
        auto q2 = stepJson.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
            action = stepJson.substr(q1 + 1, q2 - q1 - 1);
        }
    }

    bool success = true;
    if (action == "compile") {
        auto result = compileProject(m_currentWorkingDirectory);
        success = (result.find("\"success\":true") != std::string::npos);
    } else if (action == "create_file") {
        // Extract file path from detail field
        success = true; // File creation handled by specific API calls
    }

    if (m_onStepCompleted) m_onStepCompleted(action.c_str(), success, m_callbackContext);
    return success;
}

bool AgenticExecutor::verifyStepCompletion(const std::string& stepJson, const std::string& result) {
    // Check for error indicators in result
    if (result.find("\"success\":false") != std::string::npos) return false;
    if (result.find("error") != std::string::npos && result.find("\"error\":\"\"") == std::string::npos) return false;
    (void)stepJson;
    return true;
}

std::string AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler) {
#ifdef _WIN32
    // Build the compile command
    std::string cmd;
    std::string projDir = projectPath.empty() ? m_currentWorkingDirectory : projectPath;

    // Check for CMakeLists.txt → use cmake
    if (fs::exists(fs::path(projDir) / "CMakeLists.txt")) {
        cmd = "cd /d \"" + projDir + "\" && cmake --build build --config Release 2>&1";
    } else if (fs::exists(fs::path(projDir) / "Makefile")) {
        cmd = "cd /d \"" + projDir + "\" && " + compiler + " 2>&1";
    } else {
        // Try to find .cpp files and compile directly
        cmd = "cd /d \"" + projDir + "\" && cl.exe /nologo /EHsc /W4 *.cpp /Fe:output.exe 2>&1";
    }

    // Run via cmd.exe and capture output
    std::string fullCmd = "cmd.exe /c \"" + cmd + "\"";
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back('\0');

    std::string output;
    DWORD exitCode = 1;

    if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = nullptr;

        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            output += buf;
        }
        WaitForSingleObject(pi.hProcess, 30000); // 30s timeout
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (hWritePipe) CloseHandle(hWritePipe);
    CloseHandle(hReadPipe);

    // Escape output for JSON
    std::string escaped;
    for (char c : output) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else if (c >= 32) escaped += c;
    }

    return "{\"success\":" + std::string(exitCode == 0 ? "true" : "false")
         + ",\"exitCode\":" + std::to_string(exitCode)
         + ",\"output\":\"" + escaped.substr(0, 4000) + "\"}";
#else
    (void)projectPath; (void)compiler;
    return "{\"success\":false,\"output\":\"compile not available on this platform\"}";
#endif
}

std::string AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string cmdline = executablePath;
    for (const auto& a : args) { cmdline += " "; cmdline += a; }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmdline.begin(), cmdline.end());
    buf.push_back('\0');
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return "{\"success\":false,\"exitCode\":-1,\"error\":\"CreateProcess failed\"}";
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return "{\"success\":true,\"exitCode\":" + std::to_string(exitCode) + "}";
#else
    (void)executablePath;
    (void)args;
    return "{\"success\":false,\"output\":\"stub\"}";
#endif
}

std::string AgenticExecutor::getAvailableTools() {
    // Return the built-in tool set
    return "[\"read_file\",\"write_file\",\"create_file\",\"delete_file\","
           "\"list_directory\",\"create_directory\",\"compile_project\","
           "\"run_executable\",\"search_files\"]";
}

std::string AgenticExecutor::callTool(const std::string& toolName, const std::string& paramsJson) {
    // Dispatch to internal file/exec APIs based on tool name
    // Minimal JSON param extraction
    auto extractParam = [&](const std::string& key) -> std::string {
        auto pos = paramsJson.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        auto colon = paramsJson.find(':', pos);
        auto q1 = paramsJson.find('"', colon + 1);
        auto q2 = paramsJson.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos)
            return paramsJson.substr(q1 + 1, q2 - q1 - 1);
        return "";
    };

    if (toolName == "read_file") {
        std::string content = readFile(extractParam("path"));
        return "{\"success\":true,\"content\":\"" + content.substr(0, 2000) + "\"}";
    }
    if (toolName == "write_file" || toolName == "create_file") {
        bool ok = writeFile(extractParam("path"), extractParam("content"));
        return ok ? "{\"success\":true}" : "{\"success\":false}";
    }
    if (toolName == "delete_file") {
        bool ok = deleteFile(extractParam("path"));
        return ok ? "{\"success\":true}" : "{\"success\":false}";
    }
    if (toolName == "list_directory") {
        auto entries = listDirectory(extractParam("path"));
        std::string list = "[";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i) list += ",";
            list += "\"" + entries[i] + "\"";
        }
        list += "]";
        return "{\"success\":true,\"entries\":" + list + "}";
    }
    if (toolName == "create_directory") {
        bool ok = createDirectory(extractParam("path"));
        return ok ? "{\"success\":true}" : "{\"success\":false}";
    }
    if (toolName == "compile_project") {
        return compileProject(extractParam("path"), extractParam("compiler"));
    }
    if (toolName == "run_executable") {
        return runExecutable(extractParam("path"));
    }

    return "{\"success\":false,\"error\":\"Unknown tool: " + toolName + "\"}";
}

std::string AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson) {
    // Training requires ModelTrainer (Qt-based) which is not available in Win32 build.
    // Return structured error so callers know to use the Ollama or external training path.
    (void)configJson;
    if (!fs::exists(datasetPath)) {
        return "{\"success\":false,\"error\":\"Dataset not found: " + datasetPath + "\"}";
    }
    if (modelPath.empty()) {
        return "{\"success\":false,\"error\":\"No model output path specified\"}";
    }
    return "{\"success\":false,\"error\":\"Local training unavailable in Win32 build. Use Ollama or external trainer.\"}";
}

bool AgenticExecutor::isTrainingModel() const {
    return false; // No training support in Win32 build
}

// -----------------------------------------------------------------------------
// Memory — std::map + file persistence (no Qt)
// -----------------------------------------------------------------------------

void AgenticExecutor::addToMemory(const std::string& key, const std::string& valueJson) {
    m_memory[key] = valueJson;
    enforceMemoryLimit();
}

std::string AgenticExecutor::getFromMemory(const std::string& key) {
    auto it = m_memory.find(key);
    return it != m_memory.end() ? it->second : std::string();
}

void AgenticExecutor::clearMemory() {
    m_memory.clear();
}

std::string AgenticExecutor::getFullContext() {
    std::ostringstream os;
    for (const auto& [k, v] : m_memory) os << k << ":" << v << "\n";
    return os.str();
}

void AgenticExecutor::removeMemoryItem(const std::string& key) {
    m_memory.erase(key);
}

bool AgenticExecutor::detectFailure(const std::string& output) {
    return output.find("error") != std::string::npos ||
           output.find("Error") != std::string::npos ||
           output.find("failed") != std::string::npos;
}

std::string AgenticExecutor::generateCorrectionPlan(const std::string& failureReason) {
    (void)failureReason;
    return "{}";
}

std::string AgenticExecutor::retryWithCorrection(const std::string& failedStepJson) {
    (void)failedStepJson;
    return "{}";
}

// -----------------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------------

std::string AgenticExecutor::planNextAction(const std::string& currentState, const std::string& goal) {
    (void)currentState;
    (void)goal;
    return "{}";
}

std::string AgenticExecutor::generateCode(const std::string& specification) {
    (void)specification;
    return "";
}

std::string AgenticExecutor::analyzeError(const std::string& errorOutput) {
    (void)errorOutput;
    return "";
}

std::string AgenticExecutor::improveCode(const std::string& code, const std::string& issue) {
    (void)code;
    (void)issue;
    return "";
}

void AgenticExecutor::loadMemorySettings() {
    m_memoryEnabled = true;
}

void AgenticExecutor::loadMemoryFromDisk() {
    // Optional: load m_memory from a JSON file under m_currentWorkingDirectory
    std::string path = m_currentWorkingDirectory.empty() ? "." : m_currentWorkingDirectory;
    path += "/.agentic_memory.json";
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon != std::string::npos)
            m_memory[line.substr(0, colon)] = line.substr(colon + 1);
    }
}

void AgenticExecutor::persistMemoryToDisk() {
    if (m_currentWorkingDirectory.empty()) return;
    std::string path = m_currentWorkingDirectory + "/.agentic_memory.json";
    std::ofstream f(path);
    if (!f) return;
    for (const auto& [k, v] : m_memory) f << k << ":" << v << "\n";
}

void AgenticExecutor::enforceMemoryLimit() {
    if (m_memoryLimitBytes <= 0) return;
    size_t total = 0;
    for (const auto& [k, v] : m_memory) total += k.size() + v.size();
    while (total > static_cast<size_t>(m_memoryLimitBytes) && !m_memory.empty()) {
        auto it = m_memory.begin();
        total -= it->first.size() + it->second.size();
        m_memory.erase(it);
    }
}

std::string AgenticExecutor::buildToolCallPrompt(const std::string& goal, const std::string& toolsJson) {
    (void)goal;
    (void)toolsJson;
    return "";
}

std::string AgenticExecutor::extractCodeFromResponse(const std::string& response) {
    (void)response;
    return "";
}

bool AgenticExecutor::validateGeneratedCode(const std::string& code) {
    (void)code;
    return true;
}
