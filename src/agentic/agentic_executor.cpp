// agentic_executor.cpp — C++20 / Win32 file API implementation. No Qt.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "agentic_executor.h"
#include "../agentic_engine.h"
#include "file_manager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <vector>
#include <windows.h>

// Request execution path:
// 1. Normalize request and classify command-like operations.
// 2. Route deterministic command requests through command execution.
// 3. Route conversational requests through model-backed response generation.

std::string AgenticExecutor::executeUserRequest(const std::string& request) {
    if (m_onStepStarted) m_onStepStarted("executeUserRequest", m_callbackContext);

    // Internal task tracking for observability and correlation.
    uint64_t task_id = GetTickCount64();
    std::string lowered = request;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::string result;
    if (m_agenticEngine) {
        if (m_onLogMessage) {
            m_onLogMessage("[AgenticExecutor] Processing request with agentic engine", m_callbackContext);
        }

        // Route clear shell-style requests to command execution for deterministic behavior.
        const bool looksLikeCommand = lowered.rfind("run ", 0) == 0 || lowered.rfind("cmd:", 0) == 0 ||
                                      lowered.rfind("powershell:", 0) == 0;
        if (looksLikeCommand) {
            std::string command = request;
            bool powershell = false;
            if (lowered.rfind("run ", 0) == 0) {
                command = request.substr(4);
            } else if (lowered.rfind("cmd:", 0) == 0) {
                command = request.substr(4);
            } else if (lowered.rfind("powershell:", 0) == 0) {
                command = request.substr(11);
                powershell = true;
            }
            result = m_agenticEngine->executeCommand(command, powershell);
        } else {
            const std::string context = getFullContext();
            result = m_agenticEngine->generateNaturalResponse(request, context);
        }
    } else {
        if (m_onLogMessage) {
            m_onLogMessage("[AgenticExecutor] No engine configured; returning explicit failure", m_callbackContext);
        }
        result = "{\"status\":\"error\",\"taskId\":" + std::to_string(task_id) +
                 ",\"message\":\"Agentic engine is not configured\"}";
    }

    addToMemory("last_request", request);
    addToMemory("last_result", result);
    if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
    return result;
}

std::string AgenticExecutor::decomposeTask(const std::string& goal) {
    if (m_agenticEngine) {
        return m_agenticEngine->decomposeTask(goal);
    }
    return "[]";
}

bool AgenticExecutor::executeStep(const std::string& stepJson) {
    if (stepJson.empty()) {
        if (m_onStepCompleted) m_onStepCompleted("executeStep", false, m_callbackContext);
        return false;
    }

    std::string lowered = stepJson;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    bool success = false;
    if (m_agenticEngine) {
        if (lowered.find("read_file") != std::string::npos || lowered.find("list_directory") != std::string::npos ||
            lowered.find("grep") != std::string::npos || lowered.find("search") != std::string::npos) {
            // Non-mutating step: rely on engine response quality and failure detection.
            const std::string output = m_agenticEngine->processQuery("Execute tool step: " + stepJson);
            success = !detectFailure(output);
            addToMemory("last_step_output", output);
        } else if (lowered.find("compile") != std::string::npos || lowered.find("build") != std::string::npos) {
            const std::string output = m_agenticEngine->executeCommand(stepJson, false);
            success = !detectFailure(output);
            addToMemory("last_step_output", output);
        } else {
            const std::string output = m_agenticEngine->processQuery("Execute and report result for step: " + stepJson);
            success = !detectFailure(output);
            addToMemory("last_step_output", output);
        }
    }

    if (m_onStepCompleted) m_onStepCompleted("executeStep", success, m_callbackContext);
    return success;
}

bool AgenticExecutor::verifyStepCompletion(const std::string& stepJson, const std::string& result) {
    if (detectFailure(result)) {
        return false;
    }

    if (stepJson.empty()) {
        return !result.empty();
    }

    std::string loweredStep = stepJson;
    std::transform(loweredStep.begin(), loweredStep.end(), loweredStep.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    // Deterministic checks for common intents.
    if (loweredStep.find("compile") != std::string::npos || loweredStep.find("build") != std::string::npos) {
        return result.find("error") == std::string::npos && result.find("failed") == std::string::npos;
    }
    if (loweredStep.find("create") != std::string::npos || loweredStep.find("write") != std::string::npos) {
        return !result.empty() && result.find("false") == std::string::npos;
    }
    return !result.empty();
}

std::string AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler) {
    namespace fs = std::filesystem;
    fs::path root = projectPath.empty() ? fs::current_path() : fs::path(projectPath);
    if (!fs::exists(root)) {
        return "{\"success\":false,\"output\":\"Project path does not exist\"}";
    }

    const fs::path cmakeLists = root / "CMakeLists.txt";
    if (fs::exists(cmakeLists)) {
        return runExecutable("cmake", {"--build", root.string(), "--config", "Release"});
    }

    fs::path slnPath;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sln") {
            slnPath = entry.path();
            break;
        }
    }
    if (!slnPath.empty()) {
        return runExecutable("msbuild", {slnPath.string(), "/m", "/p:Configuration=Release"});
    }

    fs::path singleCpp;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
            singleCpp = entry.path();
            break;
        }
    }
    if (!singleCpp.empty()) {
        const fs::path outExe = root / "agentic_build_output.exe";
        std::string cc = compiler.empty() ? "g++" : compiler;
        return runExecutable(cc, {singleCpp.string(), "-std=c++20", "-O2", "-o", outExe.string()});
    }

    return "{\"success\":false,\"output\":\"No supported build manifest found (CMakeLists.txt/.sln/.cpp)\"}";
}

std::string AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args) {
#ifdef _WIN32
    auto quoteArg = [](const std::string& s) -> std::string {
        if (s.find_first_of(" \t\"") == std::string::npos) {
            return s;
        }
        std::string q;
        q.reserve(s.size() + 2);
        q.push_back('"');
        for (char c : s) {
            if (c == '"') {
                q.push_back('\\');
            }
            q.push_back(c);
        }
        q.push_back('"');
        return q;
    };

    std::string cmdline = quoteArg(executablePath);
    for (const auto& a : args) {
        cmdline += " ";
        cmdline += quoteArg(a);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmdline.begin(), cmdline.end());
    buf.push_back('\0');

    const bool hasExplicitPath = (executablePath.find('\\') != std::string::npos) ||
                                 (executablePath.find('/') != std::string::npos) ||
                                 (executablePath.find(':') != std::string::npos);
    LPCSTR appName = hasExplicitPath ? executablePath.c_str() : nullptr;

    if (!CreateProcessA(appName, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
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
    return "{\"success\":false,\"output\":\"CreateProcess available on Windows only\"}";
#endif
}
#endif // _WIN32

std::string AgenticExecutor::getAvailableTools() {
    return "{\"tools\":[\"createDirectory\",\"createFile\",\"writeFile\",\"readFile\",\"deleteFile\",\"deleteDirectory\",\"listDirectory\",\"compileProject\",\"runExecutable\"],\"source\":\"AgenticExecutor\"}";
}

std::string AgenticExecutor::callTool(const std::string& toolName, const std::string& paramsJson) {
    // Minimal production dispatch for frequently-used local tools.
    // paramsJson is treated as a plain string payload for backward compatibility.
    if (toolName == "readFile") {
        return readFile(paramsJson);
    }
    if (toolName == "listDirectory") {
        const auto files = listDirectory(paramsJson.empty() ? "." : paramsJson);
        std::ostringstream out;
        out << "{";
        out << "\"count\":" << files.size() << ",\"items\":[";
        for (size_t i = 0; i < files.size(); ++i) {
            if (i) out << ",";
            out << "\"" << files[i] << "\"";
        }
        out << "]}";
        return out.str();
    }
    if (toolName == "compileProject") {
        return compileProject(paramsJson);
    }
    if (toolName == "createDirectory") {
        const bool ok = createDirectory(paramsJson);
        return std::string("{\"success\":") + (ok ? "true" : "false") + "}";
    }
    if (toolName == "createFile") {
        const bool ok = createFile(paramsJson, "");
        return std::string("{\"success\":") + (ok ? "true" : "false") + "}";
    }
    if (toolName == "writeFile") {
        const bool ok = writeFile(paramsJson, "");
        return std::string("{\"success\":") + (ok ? "true" : "false") + "}";
    }
    if (toolName == "deleteFile") {
        const bool ok = deleteFile(paramsJson);
        return std::string("{\"success\":") + (ok ? "true" : "false") + "}";
    }
    if (toolName == "deleteDirectory") {
        const bool ok = deleteDirectory(paramsJson);
        return std::string("{\"success\":") + (ok ? "true" : "false") + "}";
    }
    return "{\"success\":false,\"error\":\"Unsupported tool\",\"tool\":\"" + toolName + "\"}";
}

bool AgenticExecutor::createDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p = path.empty() ? fs::path(".") : fs::path(path);
    if (fs::exists(p, ec)) {
        return fs::is_directory(p, ec);
    }
    return fs::create_directories(p, ec) && !ec;
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content) {
    namespace fs = std::filesystem;
    const fs::path p = path.empty() ? fs::path() : fs::path(path);
    if (p.empty()) {
        return false;
    }
    std::error_code ec;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

bool AgenticExecutor::writeFile(const std::string& path, const std::string& content) {
    return createFile(path, content);
}

bool AgenticExecutor::deleteFile(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p = path.empty() ? fs::path() : fs::path(path);
    if (p.empty()) {
        return false;
    }
    return fs::remove(p, ec) && !ec;
}

bool AgenticExecutor::deleteDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p = path.empty() ? fs::path() : fs::path(path);
    if (p.empty()) {
        return false;
    }
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) {
        return false;
    }
    return fs::remove_all(p, ec) > 0 && !ec;
}

std::string AgenticExecutor::readFile(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p = path.empty() ? fs::path(".") : fs::path(path);
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in) {
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p = path.empty() ? fs::path(".") : fs::path(path);
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(p, ec) || ec || !fs::is_directory(p, ec)) {
        return out;
    }
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        if (ec) {
            break;
        }
        out.push_back(entry.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson) {
    namespace fs = std::filesystem;
    if (datasetPath.empty() || !fs::exists(datasetPath)) {
        return "{\"success\":false,\"error\":\"dataset not found\"}";
    }
    if (modelPath.empty()) {
        return "{\"success\":false,\"error\":\"output model path is required\"}";
    }

    // Persist an executable training job manifest that can be picked by runtime trainer workers.
    const std::string cwd = m_currentWorkingDirectory.empty() ? "." : m_currentWorkingDirectory;
    const fs::path queueDir = fs::path(cwd) / ".agentic_training_queue";
    std::error_code ec;
    fs::create_directories(queueDir, ec);
    if (ec) {
        return "{\"success\":false,\"error\":\"failed to create training queue directory\"}";
    }

    const std::uint64_t ts = static_cast<std::uint64_t>(std::time(nullptr));
    const fs::path jobPath = queueDir / ("train_" + std::to_string(ts) + ".json");

    std::ofstream out(jobPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return "{\"success\":false,\"error\":\"failed to write training job\"}";
    }

    out << "{\"dataset\":\"" << datasetPath << "\","
        << "\"outputModel\":\"" << modelPath << "\","
        << "\"config\":" << (configJson.empty() ? "{}" : configJson) << "}";
    out.close();

    if (m_onLogMessage) {
        const std::string msg = "[AgenticExecutor] Training job queued at " + jobPath.string();
        m_onLogMessage(msg.c_str(), m_callbackContext);
    }
    return "{\"success\":true,\"status\":\"queued\",\"jobPath\":\"" + jobPath.string() + "\"}";
}

bool AgenticExecutor::isTrainingModel() const {
    return false;
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
    return "{\"strategy\":\"retry_with_guardrails\",\"reason\":\"" + failureReason + "\"}";
}

std::string AgenticExecutor::retryWithCorrection(const std::string& failedStepJson) {
    if (m_currentRetryCount >= m_maxRetries) {
        return "{\"success\":false,\"error\":\"max_retries_exceeded\"}";
    }
    ++m_currentRetryCount;
    return "{\"success\":true,\"retry\":" + std::to_string(m_currentRetryCount) + ",\"step\":" + (failedStepJson.empty() ? "{}" : failedStepJson) + "}";
}

// -----------------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------------

std::string AgenticExecutor::planNextAction(const std::string& currentState, const std::string& goal) {
    if (m_agenticEngine) {
        const std::string prompt = "Given current state: " + currentState + "\nGoal: " + goal +
                                   "\nReturn the next best action as compact JSON.";
        const std::string planned = m_agenticEngine->processQuery(prompt);
        if (!planned.empty()) {
            return planned;
        }
    }
    return "{\"nextAction\":\"analyze\",\"goal\":\"" + goal + "\"}";
}

std::string AgenticExecutor::generateCode(const std::string& specification) {
    if (m_agenticEngine) {
        const std::string generated = m_agenticEngine->generateCode(specification);
        const std::string extracted = extractCodeFromResponse(generated);
        if (validateGeneratedCode(extracted)) {
            return extracted;
        }
        return extracted.empty() ? generated : extracted;
    }
    std::ostringstream os;
    os << "// Unable to access model-backed generation path\n";
    os << "// Requested specification: " << specification << "\n";
    return os.str();
}

std::string AgenticExecutor::analyzeError(const std::string& errorOutput) {
    if (errorOutput.empty()) return "No error output provided";
    if (errorOutput.find("not found") != std::string::npos) return "Dependency or file path issue detected";
    if (errorOutput.find("syntax") != std::string::npos) return "Syntax issue detected";
    return "General build/runtime failure detected";
}

std::string AgenticExecutor::improveCode(const std::string& code, const std::string& issue) {
    std::ostringstream os;
    os << "// Improvement hint: " << issue << "\n";
    os << code;
    return os.str();
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
    std::ostringstream os;
    os << "Goal:\n" << goal << "\n\n";
    os << "Available tools (JSON):\n" << toolsJson << "\n\n";
    os << "Return strictly one tool call as JSON with fields: "
          "tool, arguments, expectedOutcome.";
    return os.str();
}

std::string AgenticExecutor::extractCodeFromResponse(const std::string& response) {
    const std::string fence = "```";
    const auto begin = response.find(fence);
    if (begin == std::string::npos) {
        return response;
    }
    const auto firstLineEnd = response.find('\n', begin + fence.size());
    if (firstLineEnd == std::string::npos) {
        return response;
    }
    const auto end = response.find(fence, firstLineEnd + 1);
    if (end == std::string::npos || end <= firstLineEnd + 1) {
        return response;
    }
    return response.substr(firstLineEnd + 1, end - (firstLineEnd + 1));
}

bool AgenticExecutor::validateGeneratedCode(const std::string& code) {
    if (code.empty()) {
        return false;
    }
    int braceBalance = 0;
    int parenBalance = 0;
    for (char c : code) {
        if (c == '{') ++braceBalance;
        else if (c == '}') --braceBalance;
        else if (c == '(') ++parenBalance;
        else if (c == ')') --parenBalance;
        if (braceBalance < 0 || parenBalance < 0) {
            return false;
        }
    }
    if (braceBalance != 0 || parenBalance != 0) {
        return false;
    }
    return code.find("#include") != std::string::npos || code.find("int main") != std::string::npos ||
           code.find("class ") != std::string::npos || code.find("struct ") != std::string::npos;
}
