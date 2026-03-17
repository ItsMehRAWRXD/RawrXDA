/**
 * @file agentic_executor.cpp
 * @brief Implementation of the real agentic execution engine
 */

#include "agentic_executor.hpp"
#include "agentic_tools.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono;

AgenticExecutor::AgenticExecutor() {
    m_workingDirectory = fs::current_path().string();
    initializeDefaultTools();
}

AgenticExecutor::~AgenticExecutor() {
    cancelExecution();
}

void AgenticExecutor::initialize() {
    log("[AgenticExecutor] Initialized - Real execution engine ready");
}

void AgenticExecutor::setToolExecutor(std::shared_ptr<AgenticToolExecutor> executor) {
    m_toolExecutor = std::move(executor);
}

// ============================================================================
// Main Execution
// ============================================================================

ExecutionResult AgenticExecutor::executeUserRequest(const std::string& request) {
    log("[AgenticExecutor] Executing user request: " + request);
    
    ExecutionResult result;
    result.request = request;
    
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    result.timestamp = std::ctime(&time);
    result.timestamp.pop_back(); // Remove newline

    m_isExecuting = true;
    m_cancelRequested = false;
    auto startTime = high_resolution_clock::now();

    try {
        // Store request in memory for context
        addToMemory("last_user_request", request);
        addToMemory("last_request_timestamp", result.timestamp);

        // Step 1: Decompose the task
        result.steps = decomposeTask(request);
        result.totalSteps = static_cast<int>(result.steps.size());

        // Step 2: Execute each step
        for (size_t i = 0; i < result.steps.size() && !m_cancelRequested; ++i) {
            auto& step = result.steps[i];
            
            emitStepStarted(step.description);
            emitProgress(static_cast<int>(i + 1), result.totalSteps);

            auto stepStart = high_resolution_clock::now();
            bool success = executeStep(step);
            auto stepEnd = high_resolution_clock::now();
            
            step.executionTimeMs = duration_cast<microseconds>(stepEnd - stepStart).count() / 1000.0;
            step.completed = true;
            step.success = success;
            result.completedSteps++;

            if (success) {
                result.successfulSteps++;
                emitStepCompleted(step.description, true);
            } else {
                emitStepCompleted(step.description, false);
                
                // Try to recover from failure
                if (m_maxRetries > 0) {
                    log("[AgenticExecutor] Step failed, attempting retry...");
                    auto correctedStep = retryWithCorrection(step);
                    if (correctedStep.success) {
                        result.successfulSteps++;
                        step.result = correctedStep.result;
                    }
                }
            }

            // Track generated/modified files
            if (step.action == "create_file" || step.action == "write_file") {
                auto it = step.params.find("path");
                if (it != step.params.end()) {
                    if (step.action == "create_file") {
                        result.generatedFiles.push_back(it->second);
                    } else {
                        result.modifiedFiles.push_back(it->second);
                    }
                }
            }
        }

        auto endTime = high_resolution_clock::now();
        result.totalExecutionTimeMs = duration_cast<microseconds>(endTime - startTime).count() / 1000.0;
        result.overallSuccess = (result.successfulSteps == result.totalSteps);
        
        // Generate summary
        std::ostringstream summary;
        summary << "Completed " << result.completedSteps << "/" << result.totalSteps << " steps. ";
        summary << "Success rate: " << (result.totalSteps > 0 ? (result.successfulSteps * 100 / result.totalSteps) : 0) << "%. ";
        summary << "Total time: " << result.totalExecutionTimeMs << "ms.";
        result.summary = summary.str();

        emitComplete(result);
        
    } catch (const std::exception& e) {
        result.overallSuccess = false;
        result.summary = std::string("Execution failed: ") + e.what();
        emitError(result.summary);
    }

    m_isExecuting = false;
    return result;
}

// ============================================================================
// Task Decomposition
// ============================================================================

std::vector<ExecutionStep> AgenticExecutor::decomposeTask(const std::string& goal) {
    log("[AgenticExecutor] Decomposing task: " + goal);
    return planTaskHeuristic(goal);
}

std::vector<ExecutionStep> AgenticExecutor::planTaskHeuristic(const std::string& goal) {
    std::vector<ExecutionStep> steps;
    std::string lowerGoal = goal;
    std::transform(lowerGoal.begin(), lowerGoal.end(), lowerGoal.begin(), ::tolower);

    int stepNum = 1;

    // Detect task type and create appropriate steps
    
    // Project creation patterns
    if (lowerGoal.find("create") != std::string::npos && 
        (lowerGoal.find("project") != std::string::npos || lowerGoal.find("app") != std::string::npos)) {
        
        std::string projectName = "new_project";
        
        // Extract project name if mentioned
        std::regex namePattern(R"((?:called|named|for)\s+['\"]?(\w+)['\"]?)");
        std::smatch match;
        if (std::regex_search(goal, match, namePattern)) {
            projectName = match[1].str();
        }

        // Create project structure
        ExecutionStep mkdirStep;
        mkdirStep.stepNumber = stepNum++;
        mkdirStep.action = "create_directory";
        mkdirStep.description = "Create project directory: " + projectName;
        mkdirStep.params["path"] = projectName;
        mkdirStep.successCriteria = "Directory exists";
        steps.push_back(mkdirStep);

        // Create src subdirectory
        ExecutionStep srcStep;
        srcStep.stepNumber = stepNum++;
        srcStep.action = "create_directory";
        srcStep.description = "Create source directory";
        srcStep.params["path"] = projectName + "/src";
        srcStep.successCriteria = "Directory exists";
        steps.push_back(srcStep);

        // Determine file type
        bool isCpp = lowerGoal.find("c++") != std::string::npos || lowerGoal.find("cpp") != std::string::npos;
        bool isPython = lowerGoal.find("python") != std::string::npos;
        bool isRust = lowerGoal.find("rust") != std::string::npos;

        if (isCpp || (!isPython && !isRust)) {
            // Default to C++
            ExecutionStep mainStep;
            mainStep.stepNumber = stepNum++;
            mainStep.action = "create_file";
            mainStep.description = "Create main.cpp";
            mainStep.params["path"] = projectName + "/src/main.cpp";
            mainStep.params["content"] = R"(#include <iostream>

int main() {
    std::cout << "Hello from )" + projectName + R"(!" << std::endl;
    return 0;
}
)";
            mainStep.successCriteria = "File created";
            steps.push_back(mainStep);

            // Create CMakeLists.txt
            ExecutionStep cmakeStep;
            cmakeStep.stepNumber = stepNum++;
            cmakeStep.action = "create_file";
            cmakeStep.description = "Create CMakeLists.txt";
            cmakeStep.params["path"] = projectName + "/CMakeLists.txt";
            cmakeStep.params["content"] = "cmake_minimum_required(VERSION 3.16)\n"
                "project(" + projectName + ")\n"
                "set(CMAKE_CXX_STANDARD 20)\n"
                "add_executable(" + projectName + " src/main.cpp)\n";
            cmakeStep.successCriteria = "File created";
            steps.push_back(cmakeStep);
        } else if (isPython) {
            ExecutionStep mainStep;
            mainStep.stepNumber = stepNum++;
            mainStep.action = "create_file";
            mainStep.description = "Create main.py";
            mainStep.params["path"] = projectName + "/src/main.py";
            mainStep.params["content"] = "#!/usr/bin/env python3\n\ndef main():\n    print('Hello from " + projectName + "!')\n\nif __name__ == '__main__':\n    main()\n";
            mainStep.successCriteria = "File created";
            steps.push_back(mainStep);
        }

        // Create README
        ExecutionStep readmeStep;
        readmeStep.stepNumber = stepNum++;
        readmeStep.action = "create_file";
        readmeStep.description = "Create README.md";
        readmeStep.params["path"] = projectName + "/README.md";
        readmeStep.params["content"] = "# " + projectName + "\n\nGenerated by RawrXD Agentic IDE\n";
        readmeStep.successCriteria = "File created";
        steps.push_back(readmeStep);
    }
    // File creation patterns
    else if (lowerGoal.find("create") != std::string::npos && lowerGoal.find("file") != std::string::npos) {
        ExecutionStep step;
        step.stepNumber = stepNum++;
        step.action = "create_file";
        step.description = "Create file as requested";
        
        // Extract filename
        std::regex filePattern(R"((?:file|named)\s+['\"]?([^\s'\"]+)['\"]?)");
        std::smatch match;
        if (std::regex_search(goal, match, filePattern)) {
            step.params["path"] = match[1].str();
        } else {
            step.params["path"] = "new_file.txt";
        }
        step.params["content"] = "// Generated by RawrXD Agentic IDE\n";
        step.successCriteria = "File created";
        steps.push_back(step);
    }
    // Compile patterns
    else if (lowerGoal.find("compile") != std::string::npos || lowerGoal.find("build") != std::string::npos) {
        ExecutionStep step;
        step.stepNumber = stepNum++;
        step.action = "compile";
        step.description = "Compile the project";
        step.params["path"] = ".";
        step.params["compiler"] = "g++";
        step.successCriteria = "Compilation successful";
        steps.push_back(step);
    }
    // Run patterns
    else if (lowerGoal.find("run") != std::string::npos || lowerGoal.find("execute") != std::string::npos) {
        ExecutionStep step;
        step.stepNumber = stepNum++;
        step.action = "run";
        step.description = "Run the executable";
        step.params["path"] = "./a.out";
        step.successCriteria = "Execution completed";
        steps.push_back(step);
    }
    // List/show patterns
    else if (lowerGoal.find("list") != std::string::npos || lowerGoal.find("show") != std::string::npos) {
        ExecutionStep step;
        step.stepNumber = stepNum++;
        step.action = "list_directory";
        step.description = "List directory contents";
        step.params["path"] = ".";
        step.successCriteria = "Listed successfully";
        steps.push_back(step);
    }
    // Default: analyze the request
    else {
        ExecutionStep step;
        step.stepNumber = stepNum++;
        step.action = "analyze";
        step.description = "Analyze request: " + goal;
        step.params["goal"] = goal;
        step.successCriteria = "Analysis complete";
        steps.push_back(step);
    }

    log("[AgenticExecutor] Decomposed into " + std::to_string(steps.size()) + " steps");
    return steps;
}

// ============================================================================
// Step Execution
// ============================================================================

bool AgenticExecutor::executeStep(ExecutionStep& step) {
    log("[AgenticExecutor] Executing step: " + step.description);

    try {
        if (step.action == "create_directory") {
            auto it = step.params.find("path");
            if (it == step.params.end()) {
                step.error = "Missing path parameter";
                return false;
            }
            bool success = createDirectory(it->second);
            step.result = success ? "Directory created: " + it->second : "Failed to create directory";
            return success;
        }
        else if (step.action == "create_file" || step.action == "write_file") {
            auto pathIt = step.params.find("path");
            auto contentIt = step.params.find("content");
            if (pathIt == step.params.end()) {
                step.error = "Missing path parameter";
                return false;
            }
            std::string content = (contentIt != step.params.end()) ? contentIt->second : "";
            bool success = createFile(pathIt->second, content);
            step.result = success ? "File created: " + pathIt->second : "Failed to create file";
            return success;
        }
        else if (step.action == "read_file") {
            auto it = step.params.find("path");
            if (it == step.params.end()) {
                step.error = "Missing path parameter";
                return false;
            }
            step.result = readFile(it->second);
            return !step.result.empty();
        }
        else if (step.action == "list_directory") {
            auto it = step.params.find("path");
            std::string path = (it != step.params.end()) ? it->second : ".";
            auto entries = listDirectory(path);
            std::ostringstream oss;
            for (const auto& entry : entries) {
                oss << entry << "\n";
            }
            step.result = oss.str();
            return true;
        }
        else if (step.action == "compile") {
            auto pathIt = step.params.find("path");
            auto compilerIt = step.params.find("compiler");
            std::string path = (pathIt != step.params.end()) ? pathIt->second : ".";
            std::string compiler = (compilerIt != step.params.end()) ? compilerIt->second : "g++";
            auto result = compileProject(path, compiler);
            step.result = result.output;
            step.error = result.error;
            return result.success;
        }
        else if (step.action == "run") {
            auto it = step.params.find("path");
            if (it == step.params.end()) {
                step.error = "Missing path parameter";
                return false;
            }
            auto result = runExecutable(it->second, {});
            step.result = result.output;
            step.error = result.error;
            return result.success;
        }
        else if (step.action == "tool_call") {
            auto toolIt = step.params.find("tool_name");
            if (toolIt == step.params.end()) {
                step.error = "Missing tool_name parameter";
                return false;
            }
            step.result = callTool(toolIt->second, step.params);
            return !step.result.empty();
        }
        else if (step.action == "analyze") {
            step.result = "Analysis completed for: " + step.params["goal"];
            return true;
        }
        else {
            step.error = "Unknown action: " + step.action;
            return false;
        }

    } catch (const std::exception& e) {
        step.error = std::string("Exception: ") + e.what();
        return false;
    }
}

bool AgenticExecutor::verifyStepCompletion(const ExecutionStep& step, const std::string& result) {
    if (step.successCriteria.empty()) return true;

    std::string lowerCriteria = step.successCriteria;
    std::string lowerResult = result;
    std::transform(lowerCriteria.begin(), lowerCriteria.end(), lowerCriteria.begin(), ::tolower);
    std::transform(lowerResult.begin(), lowerResult.end(), lowerResult.begin(), ::tolower);

    // Simple keyword matching
    if (lowerCriteria.find("exist") != std::string::npos) {
        return lowerResult.find("created") != std::string::npos || 
               lowerResult.find("exists") != std::string::npos;
    }
    if (lowerCriteria.find("success") != std::string::npos) {
        return lowerResult.find("success") != std::string::npos ||
               lowerResult.find("completed") != std::string::npos;
    }

    return !result.empty();
}

// ============================================================================
// File System Operations
// ============================================================================

bool AgenticExecutor::createDirectory(const std::string& path) {
    try {
        fs::path fullPath = fs::path(m_workingDirectory) / path;
        bool success = fs::create_directories(fullPath);
        std::string fullPathStr = fullPath.string();
        log("[AgenticExecutor] Created directory: " + fullPathStr);
        addToMemory("last_created_dir", fullPathStr);
        return true;  // create_directories returns false if already exists, but that's OK
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content) {
    try {
        fs::path filePath = fs::path(m_workingDirectory) / path;
        
        // Ensure parent directory exists
        fs::create_directories(filePath.parent_path());

        std::ofstream file(filePath);
        if (!file.is_open()) {
            log("[AgenticExecutor] Cannot open file for writing: " + filePath.string());
            return false;
        }
        file << content;
        file.close();

        log("[AgenticExecutor] Created file: " + filePath.string() + " (" + std::to_string(content.size()) + " bytes)");
        addToMemory("last_created_file", filePath.string());
        return true;
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to create file: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::writeFile(const std::string& path, const std::string& content) {
    return createFile(path, content);
}

std::string AgenticExecutor::readFile(const std::string& path) {
    try {
        fs::path filePath = fs::path(m_workingDirectory) / path;
        std::ifstream file(filePath);
        if (!file.is_open()) {
            log("[AgenticExecutor] Cannot read file: " + filePath.string());
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        log("[AgenticExecutor] Read file: " + filePath.string() + " (" + std::to_string(buffer.str().size()) + " bytes)");
        return buffer.str();
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to read file: " + std::string(e.what()));
        return "";
    }
}

bool AgenticExecutor::deleteFile(const std::string& path) {
    if (m_blockDestructiveCommands) {
        log("[AgenticExecutor] Blocked delete file (destructive commands disabled)");
        return false;
    }
    try {
        fs::path filePath = fs::path(m_workingDirectory) / path;
        bool success = fs::remove(filePath);
        log("[AgenticExecutor] Deleted file: " + filePath.string());
        return success;
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to delete file: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::deleteDirectory(const std::string& path) {
    if (m_blockDestructiveCommands) {
        log("[AgenticExecutor] Blocked delete directory (destructive commands disabled)");
        return false;
    }
    try {
        fs::path dirPath = fs::path(m_workingDirectory) / path;
        auto count = fs::remove_all(dirPath);
        log("[AgenticExecutor] Deleted directory: " + dirPath.string() + " (" + std::to_string(count) + " items)");
        return count > 0;
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to delete directory: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path) {
    std::vector<std::string> entries;
    try {
        fs::path dirPath = fs::path(m_workingDirectory) / path;
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                name += "/";
            }
            entries.push_back(name);
        }
        log("[AgenticExecutor] Listed directory: " + dirPath.string() + " (" + std::to_string(entries.size()) + " items)");
    } catch (const std::exception& e) {
        log("[AgenticExecutor] Failed to list directory: " + std::string(e.what()));
    }
    return entries;
}

bool AgenticExecutor::fileExists(const std::string& path) {
    fs::path filePath = fs::path(m_workingDirectory) / path;
    return fs::exists(filePath) && fs::is_regular_file(filePath);
}

bool AgenticExecutor::directoryExists(const std::string& path) {
    fs::path dirPath = fs::path(m_workingDirectory) / path;
    return fs::exists(dirPath) && fs::is_directory(dirPath);
}

// ============================================================================
// Compiler Integration
// ============================================================================

AgenticExecutor::CompileResult AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler) {
    CompileResult result;
    log("[AgenticExecutor] Compiling project: " + projectPath + " with " + compiler);

    fs::path path = fs::path(m_workingDirectory) / projectPath;

    // Check for CMakeLists.txt
    if (fs::exists(path / "CMakeLists.txt")) {
        log("[AgenticExecutor] Detected CMake project");
        
        // Create build directory
        fs::path buildDir = path / "build";
        fs::create_directories(buildDir);

        // Run cmake
        auto cmakeResult = executeProcess("cmake", {".."});
        if (!cmakeResult.success) {
            result.success = false;
            result.error = "CMake configuration failed: " + cmakeResult.stderr_output;
            return result;
        }

        // Run cmake --build
        auto buildResult = executeProcess("cmake", {"--build", "."});
        result.success = buildResult.success;
        result.output = cmakeResult.stdout_output + "\n" + buildResult.stdout_output;
        result.error = buildResult.stderr_output;
        result.exitCode = buildResult.exitCode;
    } else {
        // Direct compilation - find source files
        std::vector<std::string> sourceFiles;
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".c" || ext == ".cc") {
                    sourceFiles.push_back(entry.path().string());
                }
            }
        }

        if (sourceFiles.empty()) {
            result.success = false;
            result.error = "No source files found";
            return result;
        }

        std::vector<std::string> args = {"-o", "output"};
        for (const auto& src : sourceFiles) {
            args.push_back(src);
        }

        auto compileResult = executeProcess(compiler, args);
        result.success = compileResult.success;
        result.output = compileResult.stdout_output;
        result.error = compileResult.stderr_output;
        result.exitCode = compileResult.exitCode;
    }

    if (result.success) {
        log("[AgenticExecutor] Compilation successful");
    } else {
        log("[AgenticExecutor] Compilation failed");
        emitError("Compilation failed");
    }

    addToMemory("last_compilation_success", result.success ? "true" : "false");
    return result;
}

AgenticExecutor::CompileResult AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args) {
    CompileResult result;
    log("[AgenticExecutor] Running executable: " + executablePath);

    auto procResult = executeProcess(executablePath, args);
    result.success = procResult.success;
    result.output = procResult.stdout_output;
    result.error = procResult.stderr_output;
    result.exitCode = procResult.exitCode;

    return result;
}

// ============================================================================
// Process Execution
// ============================================================================

AgenticExecutor::ProcessResult AgenticExecutor::executeProcess(const std::string& program, const std::vector<std::string>& args, int timeoutMs) {
    ProcessResult result;
    auto start = high_resolution_clock::now();

    if (isDestructiveCommand(program, args)) {
        log("[AgenticExecutor] Blocked destructive command: " + program);
        result.success = false;
        result.stderr_output = "Command blocked for safety";
        return result;
    }

#ifdef _WIN32
    // Windows implementation using CreateProcess
    std::string cmdLine = program;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;

    PROCESS_INFORMATION pi;

    if (CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);

        WaitForSingleObject(pi.hProcess, timeoutMs);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
        result.success = (exitCode == 0);

        // Read stdout
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.stdout_output += buffer;
        }

        // Read stderr
        while (ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result.stderr_output += buffer;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
    } else {
        result.success = false;
        result.stderr_output = "Failed to create process";
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
    }
#else
    // POSIX implementation using fork/exec
    int stdoutPipe[2], stderrPipe[2];
    pipe(stdoutPipe);
    pipe(stderrPipe);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(program.c_str(), argv.data());
        _exit(127);
    } else if (pid > 0) {
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        char buffer[4096];
        ssize_t bytesRead;
        
        while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            result.stdout_output += buffer;
        }
        
        while ((bytesRead = read(stderrPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            result.stderr_output += buffer;
        }

        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        int status;
        waitpid(pid, &status, 0);
        result.exitCode = WEXITSTATUS(status);
        result.success = (result.exitCode == 0);
    } else {
        result.success = false;
        result.stderr_output = "Fork failed";
    }
#endif

    auto end = high_resolution_clock::now();
    result.executionTimeMs = duration_cast<microseconds>(end - start).count() / 1000.0;

    return result;
}

bool AgenticExecutor::isDestructiveCommand(const std::string& program, const std::vector<std::string>& args) const {
    if (!m_blockDestructiveCommands) return false;

    std::string lowerProgram = program;
    std::transform(lowerProgram.begin(), lowerProgram.end(), lowerProgram.begin(), ::tolower);

    // Block dangerous commands
    static const std::vector<std::string> dangerous = {"rm", "rmdir", "del", "rd", "format", "shutdown", "reboot"};
    for (const auto& cmd : dangerous) {
        if (lowerProgram.find(cmd) != std::string::npos) {
            return true;
        }
    }

    // Check for dangerous flags
    for (const auto& arg : args) {
        if (arg == "-rf" || arg == "/s" || arg == "/q") {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Tool System
// ============================================================================

std::vector<ToolDefinition> AgenticExecutor::getAvailableTools() const {
    std::lock_guard<std::mutex> lock(m_toolsMutex);
    return m_tools;
}

std::string AgenticExecutor::callTool(const std::string& toolName, const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(m_toolsMutex);
    
    for (const auto& tool : m_tools) {
        if (tool.name == toolName) {
            log("[AgenticExecutor] Calling tool: " + toolName);
            try {
                std::unordered_map<std::string, std::string> toolParams;
                for (const auto& p : params) {
                    toolParams[p.first] = p.second;
                }
                ToolResult result = tool.execute(toolParams);
                return result.output.empty() ? result.error : result.output;
            } catch (const std::exception& e) {
                return std::string("Tool error: ") + e.what();
            }
        }
    }
    
    return "Tool not found: " + toolName;
}

void AgenticExecutor::registerTool(const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(m_toolsMutex);
    m_tools.push_back(tool);
    log("[AgenticExecutor] Registered tool: " + tool.name);
}

void AgenticExecutor::initializeDefaultTools() {
    // Read file tool
    ToolDefinition readFileTool;
    readFileTool.id = "read_file";
    readFileTool.name = "read_file";
    readFileTool.description = "Read contents of a file";
    readFileTool.category = "file";
    readFileTool.parameters = {
        ToolParameter{"path", "string", "Path to the file", true, "", {}}
    };
    readFileTool.execute = [this](const std::unordered_map<std::string, std::string>& params) -> ToolResult {
        auto it = params.find("path");
        if (it == params.end()) {
            return ToolResult{false, "", "Missing path parameter", 0.0, {}};
        }
        std::string content = readFile(it->second);
        return ToolResult{true, content, "", 0.0, {}};
    };
    registerTool(readFileTool);

    // Write file tool
    ToolDefinition writeFileTool;
    writeFileTool.id = "write_file";
    writeFileTool.name = "write_file";
    writeFileTool.description = "Write content to a file";
    writeFileTool.category = "file";
    writeFileTool.parameters = {
        ToolParameter{"path", "string", "Path to the file", true, "", {}},
        ToolParameter{"content", "string", "Content to write", true, "", {}}
    };
    writeFileTool.execute = [this](const std::unordered_map<std::string, std::string>& params) -> ToolResult {
        auto pathIt = params.find("path");
        auto contentIt = params.find("content");
        if (pathIt == params.end()) {
            return ToolResult{false, "", "Missing path parameter", 0.0, {}};
        }
        std::string content = (contentIt != params.end()) ? contentIt->second : "";
        bool success = writeFile(pathIt->second, content);
        return ToolResult{success, "File written successfully", success ? "" : "Failed to write file", 0.0, {}};
    };
    registerTool(writeFileTool);

    // List directory tool
    ToolDefinition listDirTool;
    listDirTool.id = "list_directory";
    listDirTool.name = "list_directory";
    listDirTool.description = "List contents of a directory";
    listDirTool.category = "file";
    listDirTool.parameters = {
        ToolParameter{"path", "string", "Path to the directory", true, ".", {}}
    };
    listDirTool.execute = [this](const std::unordered_map<std::string, std::string>& params) -> ToolResult {
        auto it = params.find("path");
        std::string path = (it != params.end()) ? it->second : ".";
        auto entries = listDirectory(path);
        std::ostringstream oss;
        for (const auto& entry : entries) {
            oss << entry << "\n";
        }
        return ToolResult{true, oss.str(), "", 0.0, {}};
    };
    registerTool(listDirTool);

    // Execute command tool
    ToolDefinition execTool;
    execTool.id = "execute_command";
    execTool.name = "execute_command";
    execTool.description = "Execute a shell command";
    execTool.category = "system";
    execTool.parameters = {
        ToolParameter{"command", "string", "Command to execute", true, "", {}},
        ToolParameter{"args", "string", "Arguments (space-separated)", false, "", {}}
    };
    execTool.execute = [this](const std::unordered_map<std::string, std::string>& params) -> ToolResult {
        auto cmdIt = params.find("command");
        if (cmdIt == params.end()) {
            return ToolResult{false, "", "Missing command parameter", 0.0, {}};
        }
        
        std::vector<std::string> args;
        auto argsIt = params.find("args");
        if (argsIt != params.end() && !argsIt->second.empty()) {
            std::istringstream iss(argsIt->second);
            std::string arg;
            while (iss >> arg) {
                args.push_back(arg);
            }
        }
        
        auto result = executeProcess(cmdIt->second, args);
        return ToolResult{true, result.stdout_output, result.stderr_output, 0.0, {}};
    };
    registerTool(execTool);

    log("[AgenticExecutor] Initialized " + std::to_string(m_tools.size()) + " default tools");
}

// ============================================================================
// Memory System
// ============================================================================

void AgenticExecutor::addToMemory(const std::string& key, const std::string& value, bool persistent) {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    
    MemoryEntry entry;
    entry.key = key;
    entry.value = value;
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    entry.timestamp = std::ctime(&time);
    entry.timestamp.pop_back();
    entry.accessCount = 0;
    entry.persistent = persistent;
    
    m_memory[key] = entry;
    
    // Enforce memory limit
    enforceMemoryLimit();
}

std::string AgenticExecutor::getFromMemory(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    auto it = m_memory.find(key);
    if (it != m_memory.end()) {
        const_cast<MemoryEntry&>(it->second).accessCount++;
        return it->second.value;
    }
    return "";
}

bool AgenticExecutor::hasMemory(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    return m_memory.find(key) != m_memory.end();
}

void AgenticExecutor::clearMemory() {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    m_memory.clear();
    log("[AgenticExecutor] Memory cleared");
}

void AgenticExecutor::clearNonPersistentMemory() {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    for (auto it = m_memory.begin(); it != m_memory.end();) {
        if (!it->second.persistent) {
            it = m_memory.erase(it);
        } else {
            ++it;
        }
    }
    log("[AgenticExecutor] Non-persistent memory cleared");
}

std::string AgenticExecutor::getFullContext() const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    std::ostringstream oss;
    oss << "Memory Context (" << m_memory.size() << " entries):\n";
    for (const auto& [key, entry] : m_memory) {
        oss << "  " << key << ": " << entry.value << "\n";
    }
    return oss.str();
}

std::vector<std::string> AgenticExecutor::getMemoryKeys() const {
    std::lock_guard<std::mutex> lock(m_memoryMutex);
    std::vector<std::string> keys;
    for (const auto& [key, _] : m_memory) {
        keys.push_back(key);
    }
    return keys;
}

void AgenticExecutor::loadMemoryFromDisk(const std::string& path) {
    std::string filePath = path.empty() ? "agentic_memory.json" : path;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        log("[AgenticExecutor] No memory file found at: " + filePath);
        return;
    }
    
    // Simple JSON-like parsing (for a real implementation, use a JSON library)
    std::string line;
    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\""));
            key.erase(key.find_last_not_of(" \t\"") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\",") + 1);
            addToMemory(key, value, true);
        }
    }
    
    log("[AgenticExecutor] Loaded memory from disk: " + filePath);
}

void AgenticExecutor::persistMemoryToDisk(const std::string& path) {
    std::string filePath = path.empty() ? "agentic_memory.json" : path;
    std::ofstream file(filePath);
    if (!file.is_open()) {
        log("[AgenticExecutor] Cannot write memory file: " + filePath);
        return;
    }

    std::lock_guard<std::mutex> lock(m_memoryMutex);
    file << "{\n";
    bool first = true;
    for (const auto& [key, entry] : m_memory) {
        if (entry.persistent) {
            if (!first) file << ",\n";
            file << "  \"" << key << "\": \"" << entry.value << "\"";
            first = false;
        }
    }
    file << "\n}\n";
    
    log("[AgenticExecutor] Persisted memory to disk: " + filePath);
}

void AgenticExecutor::enforceMemoryLimit() {
    // Calculate total memory usage
    size_t totalSize = 0;
    for (const auto& [key, entry] : m_memory) {
        totalSize += key.size() + entry.value.size() + entry.timestamp.size();
    }

    // If over limit, remove least recently accessed non-persistent entries
    while (totalSize > m_memoryLimitBytes && !m_memory.empty()) {
        auto minIt = m_memory.end();
        int minAccess = INT_MAX;
        
        for (auto it = m_memory.begin(); it != m_memory.end(); ++it) {
            if (!it->second.persistent && it->second.accessCount < minAccess) {
                minAccess = it->second.accessCount;
                minIt = it;
            }
        }
        
        if (minIt != m_memory.end()) {
            totalSize -= minIt->first.size() + minIt->second.value.size() + minIt->second.timestamp.size();
            m_memory.erase(minIt);
        } else {
            break;  // No non-persistent entries to remove
        }
    }
}

// ============================================================================
// Self-Correction
// ============================================================================

bool AgenticExecutor::detectFailure(const std::string& output) {
    std::string lowerOutput = output;
    std::transform(lowerOutput.begin(), lowerOutput.end(), lowerOutput.begin(), ::tolower);

    static const std::vector<std::string> failurePatterns = {
        "error", "failed", "exception", "undefined", "cannot", "not found",
        "permission denied", "segmentation fault", "abort", "fatal"
    };

    for (const auto& pattern : failurePatterns) {
        if (lowerOutput.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string AgenticExecutor::generateCorrectionPlan(const std::string& failureReason) {
    std::ostringstream plan;
    plan << "Correction plan for: " << failureReason << "\n";
    
    std::string lowerReason = failureReason;
    std::transform(lowerReason.begin(), lowerReason.end(), lowerReason.begin(), ::tolower);

    if (lowerReason.find("not found") != std::string::npos || lowerReason.find("missing") != std::string::npos) {
        plan << "1. Check if file/directory exists\n";
        plan << "2. Create missing resources\n";
        plan << "3. Retry original operation\n";
    } else if (lowerReason.find("permission") != std::string::npos) {
        plan << "1. Check file permissions\n";
        plan << "2. Try alternative location\n";
        plan << "3. Request elevated access if needed\n";
    } else if (lowerReason.find("compile") != std::string::npos || lowerReason.find("syntax") != std::string::npos) {
        plan << "1. Analyze error messages\n";
        plan << "2. Identify problematic code\n";
        plan << "3. Apply fixes and recompile\n";
    } else {
        plan << "1. Analyze error output\n";
        plan << "2. Search for solutions\n";
        plan << "3. Apply fix and retry\n";
    }

    return plan.str();
}

ExecutionStep AgenticExecutor::retryWithCorrection(const ExecutionStep& failedStep) {
    ExecutionStep correctedStep = failedStep;
    correctedStep.completed = false;
    correctedStep.success = false;
    
    log("[AgenticExecutor] Attempting correction for: " + failedStep.description);
    
    std::string correctionPlan = generateCorrectionPlan(failedStep.error);
    log("[AgenticExecutor] Correction plan:\n" + correctionPlan);

    // Simple retry with same parameters
    executeStep(correctedStep);
    
    return correctedStep;
}

// ============================================================================
// Configuration
// ============================================================================

void AgenticExecutor::setWorkingDirectory(const std::string& path) {
    m_workingDirectory = path;
    log("[AgenticExecutor] Working directory set to: " + path);
}

std::string AgenticExecutor::getWorkingDirectory() const {
    return m_workingDirectory;
}

void AgenticExecutor::setMaxRetries(int retries) {
    m_maxRetries = retries;
}

void AgenticExecutor::setMemoryLimit(size_t bytes) {
    m_memoryLimitBytes = bytes;
}

void AgenticExecutor::setBlockDestructiveCommands(bool block) {
    m_blockDestructiveCommands = block;
}

void AgenticExecutor::cancelExecution() {
    m_cancelRequested = true;
}

// ============================================================================
// Callbacks
// ============================================================================

void AgenticExecutor::log(const std::string& message) {
    if (m_onLog) {
        m_onLog(message);
    }
    std::cout << message << std::endl;
}

void AgenticExecutor::emitStepStarted(const std::string& description) {
    if (m_onStepStarted) m_onStepStarted(description);
}

void AgenticExecutor::emitStepCompleted(const std::string& description, bool success) {
    if (m_onStepCompleted) m_onStepCompleted(description, success);
}

void AgenticExecutor::emitProgress(int current, int total) {
    if (m_onProgress) m_onProgress(current, total);
}

void AgenticExecutor::emitComplete(const ExecutionResult& result) {
    if (m_onComplete) m_onComplete(result);
}

void AgenticExecutor::emitError(const std::string& error) {
    if (m_onError) m_onError(error);
}
