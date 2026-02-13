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
// Execution (stubs / minimal — delegate to inference when available)
// -----------------------------------------------------------------------------

std::string AgenticExecutor::executeUserRequest(const std::string& request) {
    if (m_onStepStarted) m_onStepStarted("executeUserRequest", m_callbackContext);
    if (m_onLogMessage) m_onLogMessage("[AgenticExecutor] executeUserRequest (stub)", m_callbackContext);
    std::string result = "{\"status\":\"ok\",\"message\":\"stub\"}";
    if (m_onExecutionComplete) m_onExecutionComplete(result.c_str(), m_callbackContext);
    return result;
}

std::string AgenticExecutor::decomposeTask(const std::string& goal) {
    (void)goal;
    return "[]";
}

bool AgenticExecutor::executeStep(const std::string& stepJson) {
    (void)stepJson;
    if (m_onStepCompleted) m_onStepCompleted("executeStep", true, m_callbackContext);
    return true;
}

bool AgenticExecutor::verifyStepCompletion(const std::string& stepJson, const std::string& result) {
    (void)stepJson;
    (void)result;
    return true;
}

std::string AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler) {
    (void)projectPath;
    (void)compiler;
    return "{\"success\":false,\"output\":\"stub\"}";
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
    return "[]";
}

std::string AgenticExecutor::callTool(const std::string& toolName, const std::string& paramsJson) {
    (void)toolName;
    (void)paramsJson;
    return "{}";
}

std::string AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson) {
    (void)datasetPath;
    (void)modelPath;
    (void)configJson;
    return "{\"success\":false}";
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
