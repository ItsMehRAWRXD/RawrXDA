#include <thread>
#include <atomic>
#include <compare>
#include <string>
#include <fstream> 
#include <filesystem>
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <cctype>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "agentic_executor.h"
#include "agentic_engine.h"
#include "cpu_inference_engine.h" 
#include "model_trainer.h"

using json = nlohmann::json;

// ========== PATH / SHELL SAFETY ==========

bool AgenticExecutor::isPathSafe(const std::filesystem::path& p) const {
    if (m_workspaceRoot.empty()) return true; // no sandbox configured
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(p, ec);
    if (ec) return false;
    auto root = std::filesystem::weakly_canonical(m_workspaceRoot, ec);
    if (ec) return false;
    // p must start with workspaceRoot
    auto [rootEnd, _] = std::mismatch(root.begin(), root.end(), canonical.begin(), canonical.end());
    return rootEnd == root.end();
}

std::filesystem::path AgenticExecutor::safePath(const std::string& userPath) const {
    std::filesystem::path p(userPath);
    if (p.is_relative() && !m_workspaceRoot.empty())
        p = m_workspaceRoot / p;
    return std::filesystem::weakly_canonical(p);
}

bool AgenticExecutor::hasShellMetachars(const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '|': case '&': case ';': case '<': case '>':
            case '`': case '$': case '(': case ')': case '{':
            case '}': case '\n': case '\r':
                return true;
        }
    }
    return false;
}

bool AgenticExecutor::isCompilerAllowed(const std::string& compiler) {
    // Extract just the executable name (no path)
    std::filesystem::path p(compiler);
    std::string name = p.stem().string();
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    return name == "g++" || name == "gcc" || name == "cl" ||
           name == "clang" || name == "clang++" || name == "cmake";
}

void AgenticExecutor::setWorkspaceRoot(const std::filesystem::path& root) {
    m_workspaceRoot = std::filesystem::weakly_canonical(root);
}

void AgenticExecutor::logMessage(const std::string& msg) {
    if (m_onLogMessage)
        m_onLogMessage(msg.c_str(), m_callbackContext);
    else
        std::cerr << "[AgenticExecutor] " << msg << "\n";
}

void AgenticExecutor::errorOccurred(const std::string& msg) {
    logMessage("ERROR: " + msg);
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    m_memory["last_error"] = msg;
}

AgenticExecutor::AgenticExecutor(void* parent)
    : m_currentWorkingDirectory(std::filesystem::current_path().string())
{
    m_executionHistory = json::array();
}

AgenticExecutor::~AgenticExecutor()
{
    clearMemory();
}

void AgenticExecutor::initialize(AgenticEngine* engine, InferenceEngine* inference)
{
    m_agenticEngine = engine;
    m_inferenceEngine = inference;
    // Real training requires pointer to ModelTrainer
    m_modelTrainer = std::make_unique<ModelTrainer>(); 
}

// ========== NATIVE TITAN KERNEL HANDOFF (x64 MASM) ==========
#ifdef _WIN32
using TitanExecuteTaskFn = uint64_t (*)(const char* task_json, uint64_t length);

static uint64_t TitanExecuteTaskFallback(const char*, uint64_t)
{
    return 0xFFFFFFFFULL;
}

static TitanExecuteTaskFn ResolveTitanExecuteTask()
{
    const HMODULE self = ::GetModuleHandleW(nullptr);
    if (!self)
    {
        return &TitanExecuteTaskFallback;
    }

    const FARPROC proc = ::GetProcAddress(self, "Titan_ExecuteTask");
    if (!proc)
    {
        return &TitanExecuteTaskFallback;
    }

    return reinterpret_cast<TitanExecuteTaskFn>(proc);
}
#else
using TitanExecuteTaskFn = uint64_t (*)(const char* task_json, uint64_t length);

static uint64_t TitanExecuteTaskFallback(const char*, uint64_t)
{
    return 0xFFFFFFFFULL;
}

static TitanExecuteTaskFn ResolveTitanExecuteTask()
{
    return &TitanExecuteTaskFallback;
}
#endif

json AgenticExecutor::executeUserRequest(const std::string& request)
{
    logMessage("Routing to TITAN x64 Kernel: " + request);

    // Prepare task for Titan kernel when present; otherwise use fallback.
    const TitanExecuteTaskFn titanExecute = ResolveTitanExecuteTask();
    uint64_t status = titanExecute(request.c_str(), request.length());

    json result;
    result["request"] = request;
    result["kernel_status"] = status;
    result["routing"] = "MASM_TITAN_0x5854494A";

    auto now = std::chrono::system_clock::now();
    result["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if (status == 0) {
        logMessage("TITAN Kernel Execution Success.");
        result["overall_success"] = true;
    } else {
        errorOccurred("TITAN Kernel Reported Error: " + std::to_string(status));
        result["overall_success"] = false;
        result["error_code"] = status;
    }

    return result;
}

json AgenticExecutor::decomposeTask(const std::string& goal)
{
    // Use json::dump() to safely serialize — prevents injection via goal string
    json payload;
    payload["action"] = "DECOMPOSE";
    payload["goal"] = goal;
    return executeUserRequest(payload.dump());
}
// ========== STEP EXECUTION ==========

bool AgenticExecutor::executeStep(const json& step)
{
    if (!step.contains("action")) return false;
    
    std::string action = step["action"];
    std::string description = step.contains("description") ? step["description"].get<std::string>() : action;

    // Normalize action to lowercase for case-insensitive matching
    std::string actionLower = action;
    std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(), ::tolower);

    logMessage("Step: " + description);

    try {
        if (actionLower == "create_file") {
            std::string path = step["params"]["path"];
            std::string content = "";
            if (step["params"].contains("content"))
                content = step["params"]["content"];
            if (content.empty() && step["params"].contains("specification")) {
                json codeGen = generateCode(step["params"]["specification"]);
                if (codeGen.contains("code"))
                    content = codeGen["code"];
            }
            return createFile(path, content);
        }
        else if (actionLower == "create_directory") {
            std::string path = step["params"]["path"];
            return createDirectory(path);
        }
        else if (actionLower == "compile") {
            std::string projectPath = step["params"]["project_path"];
            std::string compiler = step["params"].contains("compiler") ? step["params"]["compiler"].get<std::string>() : "g++";
            json compileResult = compileProject(projectPath, compiler);
            return compileResult.contains("success") && compileResult["success"].get<bool>();
        }
        else if (actionLower == "run") {
            std::string executable = step["params"]["executable"];
            std::vector<std::string> args;
            if (step["params"].contains("args") && step["params"]["args"].is_array()) {
                args = step["params"]["args"].get<std::vector<std::string>>();
            }
            json runResult = runExecutable(executable, args);
            return runResult.contains("success") && runResult["success"].get<bool>();
        }
        else if (actionLower == "generate_code") {
            std::string spec = step["params"]["specification"];
            std::string outputPath = step["params"]["output_path"];
            json codeGen = generateCode(spec);
            if (codeGen.contains("code")) {
                return writeFile(outputPath, codeGen["code"]);
            }
            return false;
        }
        else if (actionLower == "tool_call") {
            std::string toolName = step["params"]["tool_name"];
            json toolParams = step["params"]["tool_params"];
            json toolResult = callTool(toolName, toolParams);
            return toolResult.contains("success") && toolResult["success"].get<bool>();
        }
        else {
            logMessage("Unknown action: " + action);
            return false;
        }

    } catch (const std::exception& e) {
        errorOccurred("Step failed: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::verifyStepCompletion(const json& step, const std::string& result)
{
    if (!step.contains("criteria")) return true;
    std::string criteria = step["criteria"];
    if (criteria.empty()) return true;

    // Use model to verify completion
    std::ostringstream prompt;
    prompt << "Verification Task:\n"
           << "Expected: " << criteria << "\n"
           << "Actual Result: " << result << "\n\n"
           << "Does the actual result meet the success criteria? Answer with ONLY 'yes' or 'no'.";

    if (m_agenticEngine) {
        std::string verification = m_agenticEngine->processQuery(prompt.str());
        std::transform(verification.begin(), verification.end(), verification.begin(), ::tolower);
        return verification.find("yes") != std::string::npos;
    }
    return true;
}

// ========== FILE SYSTEM OPERATIONS (REAL) ==========

bool AgenticExecutor::createDirectory(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return false;
    }
    try {
        std::filesystem::create_directories(safe);
        logMessage("Created directory: " + safe.string());
        {
            std::lock_guard<std::mutex> lock(m_memoryMutex);
            m_memory["last_created_dir"] = safe.string();
        }
        return true;
    } catch (const std::exception& e) {
        logMessage("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return false;
    }
    // Ensure parent directory exists
    if (safe.has_parent_path() && !std::filesystem::exists(safe.parent_path())) {
         try {
            std::filesystem::create_directories(safe.parent_path());
         } catch(...) { return false; }
    }

    std::ofstream file(safe);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    file.close();

    logMessage("Created file: " + safe.string());
    return true;
}

bool AgenticExecutor::writeFile(const std::string& path, const std::string& content)
{
    return createFile(path, content);
}

std::string AgenticExecutor::readFile(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return "";
    }
    std::ifstream file(safe);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool AgenticExecutor::deleteFile(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return false;
    }
    try {
        return std::filesystem::remove(safe);
    } catch (...) {
        return false;
    }
}

bool AgenticExecutor::deleteDirectory(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return false;
    }
    try {
        return std::filesystem::remove_all(safe) > 0;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return {};
    }
    std::vector<std::string> entries;
    try {
        if (std::filesystem::exists(safe) && std::filesystem::is_directory(safe)) {
            for (const auto& entry : std::filesystem::directory_iterator(safe)) {
                entries.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {
        return {};
    }
    return entries;
}

// Safe shell execution via CreateProcessA with piped stdout.
static std::pair<std::string, int> runShellCommand(const std::string& cmd, const std::string& directory = "") {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return {"Failed to create pipe", 1};
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    std::string cmdLine = "cmd.exe /C " + cmd;
    std::vector<char> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back('\0');

    PROCESS_INFORMATION pi{};
    LPCSTR cwd = directory.empty() ? nullptr : directory.c_str();
    BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, cwd, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        return {"CreateProcess failed (" + std::to_string(GetLastError()) + ")", 1};
    }

    std::string output;
    constexpr size_t kMaxOutput = 1024 * 1024; // 1 MB cap
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
        if (output.size() > kMaxOutput) {
            output += "\n... [output truncated at 1 MB] ...";
            break;
        }
    }
    CloseHandle(hRead);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 120000); // 2 min timeout
    DWORD exitCode = 1;
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        output += "\n[TIMEOUT: process killed after 120s]";
    } else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return {output, static_cast<int>(exitCode)};
#else
    std::string command = cmd;
    if (!directory.empty()) command = "cd " + directory + " && " + cmd;
    command += " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {"Failed to spawn shell", 1};
    std::string output;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output += buffer;
        if (output.size() > 1024 * 1024) break;
    }
    int result = pclose(pipe);
    return {output, result};
#endif
}

// ========== COMPILER INTEGRATION (REAL) ==========

json AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler)
{
    json result;
    result["compiler"] = compiler;
    result["project_path"] = projectPath;

    // Compiler whitelist check
    if (!isCompilerAllowed(compiler)) {
        result["success"] = false;
        result["error"] = "Compiler not in whitelist: " + compiler;
        errorOccurred(result["error"].get<std::string>());
        return result;
    }

    // Reject shell metacharacters in project path
    if (hasShellMetachars(projectPath)) {
        result["success"] = false;
        result["error"] = "Invalid characters in project path";
        errorOccurred(result["error"].get<std::string>());
        return result;
    }

    logMessage("Compiling with " + compiler + "...");

    // Detect build system and compile
    bool isCMake = std::filesystem::exists(projectPath + "/CMakeLists.txt");
    if (isCMake) {
        logMessage("Detected CMake project");
        
        // Create build directory
        std::string buildDir = projectPath + "/build";
        createDirectory(buildDir);
        
        // Run cmake
        auto cmakeRes = runShellCommand("cmake ..", buildDir);
        if (cmakeRes.second != 0) {
            result["success"] = false;
            result["error"] = "CMake configuration failed: " + cmakeRes.first;
            return result;
        }
        
        // Run cmake --build
        auto buildRes = runShellCommand("cmake --build .", buildDir);
        
        result["cmake_output"] = cmakeRes.first;
        result["build_output"] = buildRes.first;
        result["exit_code"] = buildRes.second;
        result["success"] = (buildRes.second == 0);
        
    } else {
        // Direct compilation
        std::vector<std::string> files;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(projectPath)) {
                if (entry.path().extension() == ".cpp" || entry.path().extension() == ".c") {
                    files.push_back(entry.path().filename().string());
                }
            }
        } catch(...) {}
        
        if (files.empty()) {
            result["success"] = false;
            result["error"] = "No source files found";
            return result;
        }
        
        std::string cmd = compiler + " -o output";
        for (const std::string& file : files) {
            cmd += " " + file;
        }
        
        auto compileRes = runShellCommand(cmd, projectPath);
        
        result["compiler_output"] = compileRes.first;
        result["exit_code"] = compileRes.second;
        result["success"] = (compileRes.second == 0);
    }

    if (result["success"].get<bool>()) {
        logMessage("Compilation successful!");
    } else {
        std::string err = result.contains("build_output") ? result["build_output"].get<std::string>() : 
                         (result.contains("compiler_output") ? result["compiler_output"].get<std::string>() : "Unknown error");
        logMessage("Compilation failed: " + err);
        errorOccurred("Compilation failed");
    }

    addToMemory("last_compilation", result.dump());
    return result;
}

json AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args)
{
    json result;
    result["executable"] = executablePath;
    result["arguments"] = args;

    // Validate inputs for shell injection
    if (hasShellMetachars(executablePath)) {
        result["success"] = false;
        result["error"] = "Invalid characters in executable path";
        return result;
    }
    for (const auto& arg : args) {
        if (hasShellMetachars(arg)) {
            result["success"] = false;
            result["error"] = "Invalid characters in argument";
            return result;
        }
    }

    logMessage("Running: " + executablePath);

    std::string cmd = "\"" + executablePath + "\"";
    for(const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }

    std::filesystem::path exePath(executablePath);
    std::string cwd = exePath.has_parent_path() ? exePath.parent_path().string() : "";

    auto runRes = runShellCommand(cmd, cwd);

    result["stdout"] = runRes.first;
    result["exit_code"] = runRes.second;
    result["success"] = (runRes.second == 0);

    if (result["success"].get<bool>()) {
        logMessage("Execution completed");
    }

    addToMemory("last_execution", result.dump());
    return result;
}

// ========== CODE GENERATION ==========

json AgenticExecutor::generateCode(const std::string& specification)
{
    json result;
    
    if (!m_agenticEngine) {
        result["error"] = "No engine available";
        result["success"] = false;
        return result;
    }

    std::ostringstream prompt;
    prompt << "Generate production-ready C++ code for the following specification:\n\n"
           << specification << "\n\n"
           << "Requirements:\n"
           << "- Complete, compilable code\n"
           << "- Include all necessary headers\n"
           << "- Add error handling\n"
           << "- Add helpful comments\n"
           << "- Follow C++17 best practices\n\n"
           << "Return ONLY the code, no explanations.";

    std::string response = m_agenticEngine->processQuery(prompt.str()); 
    std::string code = extractCodeFromResponse(response);

    result["specification"] = specification;
    result["code"] = code;
    result["success"] = !code.empty();

    return result;
}

std::string AgenticExecutor::extractCodeFromResponse(const std::string& response) {
    // Manual find-based parse — avoids std::regex stack overflow on large inputs
    const std::string fenceStart = "```";
    auto pos = response.find(fenceStart);
    if (pos == std::string::npos) return response;

    // Skip the language tag line (e.g. ```cpp\n)
    auto lineEnd = response.find('\n', pos + fenceStart.size());
    if (lineEnd == std::string::npos) return response;

    auto codeStart = lineEnd + 1;
    auto fenceEnd = response.find("```", codeStart);
    if (fenceEnd == std::string::npos) return response;

    return response.substr(codeStart, fenceEnd - codeStart);
}

// ========== FUNCTION CALLING / TOOL USE ==========

json AgenticExecutor::getAvailableTools()
{
    json tools = json::array();
    
    tools.push_back({{"name", "create_directory"}, {"description", "Create a new directory"}});
    tools.push_back({{"name", "create_file"}, {"description", "Create a file with content"}});
    tools.push_back({{"name", "read_file"}, {"description", "Read file contents"}});
    tools.push_back({{"name", "delete_file"}, {"description", "Delete a file"}});
    tools.push_back({{"name", "list_directory"}, {"description", "List directory contents"}});
    
    // Compilation tools
    tools.push_back({{"name", "compile_project"}, {"description", "Compile C++ project"}});
    tools.push_back({{"name", "run_executable"}, {"description", "Run compiled executable"}});
    
    // Model tools
    tools.push_back({{"name", "train_model"}, {"description", "Fine-tune a GGUF model with dataset"}});
    tools.push_back({{"name", "is_training"}, {"description", "Check if model training is in progress"}});
    
    return tools;
}

json AgenticExecutor::callTool(const std::string& toolName, const json& params)
{
    json result;
    result["tool"] = toolName;
    result["params"] = params;

    if (toolName == "create_directory") {
        bool success = createDirectory(params["path"]);
        result["success"] = success;
    }
    else if (toolName == "create_file") {
        bool success = createFile(params["path"], params["content"]);
        result["success"] = success;
    }
    else if (toolName == "read_file") {
        std::string content = readFile(params["path"]);
        result["success"] = !content.empty();
        result["content"] = content;
    }
    else if (toolName == "delete_file") {
        bool success = deleteFile(params["path"]);
        result["success"] = success;
    }
    else if (toolName == "list_directory") {
        std::vector<std::string> entries = listDirectory(params["path"]);
        result["success"] = true;
        result["entries"] = entries;
    }
    else if (toolName == "compile_project") {
        return compileProject(params["project_path"]);
    }
    else if (toolName == "run_executable") {
        std::string exe = params["executable"];
        std::vector<std::string> args; 
        if(params.contains("args")) args = params["args"].get<std::vector<std::string>>();
        return runExecutable(exe, args);
    }
    else if (toolName == "train_model") {
        return trainModel(params["dataset_path"], params["model_path"], params["config"]);
    }
    else if (toolName == "is_training") {
        result["success"] = true;
        result["is_training"] = isTrainingModel();
    }
    else {
        result["success"] = false;
        result["error"] = "Unknown tool: " + toolName;
    }

    return result;
}

// ========== MEMORY & CONTEXT ==========

void AgenticExecutor::addToMemory(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    m_memory[key] = value;
}

std::string AgenticExecutor::getFromMemory(const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    auto it = m_memory.find(key);
    if (it != m_memory.end()) return it->second;
    return "";
}

void AgenticExecutor::clearMemory()
{
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    m_memory.clear();
    m_executionHistory = json::array();
}

void AgenticExecutor::removeMemoryItem(const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    m_memory.erase(key);
}

std::string AgenticExecutor::getFullContext()
{
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    std::ostringstream context;
    context << "=== EXECUTION CONTEXT ===\n";
    context << "Working Directory: " << m_currentWorkingDirectory << "\n";
    context << "Memory Items: " << m_memory.size() << "\n";
    context << "Execution History: " << m_executionHistory.size() << " steps\n";
    context << "\n=== MEMORY ===\n";
    
    for (const auto& kv : m_memory) {
        context << kv.first << ": " << kv.second << "\n";
    }
    
    return context.str();
}

// ========== SELF-CORRECTION ==========

bool AgenticExecutor::detectFailure(const std::string& output)
{
    // Pattern + exclusion pairs to prevent false positives
    struct Indicator {
        const char* pattern;
        const char* exclude; // null = no exclusion
    };
    static const Indicator indicators[] = {
        {"error",                "0 error"},
        {"failed",               nullptr},
        {"exception",            nullptr},
        {"cannot",               nullptr},
        {"unable",               nullptr},
        {"undefined reference",  nullptr},
        {"segmentation fault",   nullptr},
        {"compilation terminated", nullptr},
    };
    
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& ind : indicators) {
        if (lower.find(ind.pattern) != std::string::npos) {
            // Check exclusion — if present and found, skip this indicator
            if (ind.exclude && lower.find(ind.exclude) != std::string::npos)
                continue;
            return true;
        }
    }
    
    return false;
}

std::string AgenticExecutor::generateCorrectionPlan(const std::string& failureReason)
{
    if (!m_agenticEngine) return "No correction available";

    std::ostringstream prompt;
    prompt << "An automated task failed with this error:\n" << failureReason << "\n\n"
           << "Analyze the error and provide a correction plan. Include:\n"
           << "1. Root cause of the failure\n"
           << "2. Specific steps to fix it\n"
           << "3. Code changes if needed\n"
           << "4. Verification steps\n\n"
           << "Be concise and actionable.";

    return m_agenticEngine->processQuery(prompt.str());
}

json AgenticExecutor::retryWithCorrection(const json& failedStep)
{
    json result;
    result["original_step"] = failedStep;

    for (int attempt = 1; attempt <= m_maxRetries; ++attempt) {
        result["retry_attempt"] = attempt;

        std::string failureContext = getFromMemory("last_error");
        if (failureContext.empty()) failureContext = "Unknown Error";

        std::string correctionPlan = generateCorrectionPlan(failureContext);
        logMessage("Correction attempt " + std::to_string(attempt) + ": " + correctionPlan);

        bool success = executeStep(failedStep);
        result["success"] = success;
        result["correction_plan"] = correctionPlan;

        if (success) return result;
    }

    result["success"] = false;
    return result;
}

// ========== MODEL TRAINING ==========

json AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const json& config)
{
    json result;
    
    if (!m_modelTrainer) {
        result["success"] = false;
        result["error"] = "Model trainer not initialized";
        return result;
    }

    if (m_inferenceEngine == nullptr) {
        result["success"] = false;
        result["error"] = "Inference engine not available";
        return result;
    }
    
    if (!m_modelTrainer->initialize(m_inferenceEngine, modelPath)) {
        result["success"] = false;
        result["error"] = "Failed to initialize model trainer";
        return result;
    }
    
    logMessage("Starting model training with dataset: " + datasetPath);
    
    // Configure training
    ModelTrainer::TrainingConfig trainConfig;
    trainConfig.datasetPath = datasetPath;
    trainConfig.outputPath = modelPath + ".trained";
    
    trainConfig.epochs = config.value("epochs", 10);
    trainConfig.learningRate = config.value("learning_rate", 1e-4);
    trainConfig.batchSize = config.value("batch_size", 32);
    trainConfig.sequenceLength = config.value("sequence_length", 512);
    trainConfig.gradientClip = config.value("gradient_clip", 1.0);
    trainConfig.validateEveryEpoch = config.value("validate_every_epoch", true);
    trainConfig.validationSplit = config.value("validation_split", 0.1);
    trainConfig.weightDecay = config.value("weight_decay", 0.01);
    trainConfig.warmupSteps = config.value("warmup_steps", 0.1);
    
    // Start training
    bool success = m_modelTrainer->startTraining(trainConfig);
    
    result["success"] = success;
    result["output_model_path"] = trainConfig.outputPath;
    if (!success) {
        result["error"] = "Failed to start training";
    }
    
    return result;
}


bool AgenticExecutor::isTrainingModel() const
{
    return m_modelTrainer && m_modelTrainer->isTraining();
}


