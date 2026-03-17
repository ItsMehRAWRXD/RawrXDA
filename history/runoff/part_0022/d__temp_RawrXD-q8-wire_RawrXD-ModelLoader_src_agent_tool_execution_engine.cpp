/**
 * @file tool_execution_engine.cpp
 * @brief Implementation of tool execution engine (Pure Win32/C++20)
 */

#include "tool_execution_engine.hpp"
#include "license_enforcement.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <cctype>
#include <regex>

namespace RawrXD {

ToolExecutionEngine::ToolExecutionEngine()
{
    try {
        m_workingDirectory = std::filesystem::current_path();
    } catch (...) {
        m_workingDirectory = std::filesystem::path{"."};
    }

    m_defaultTimeoutMs = 30000;
    m_maxOutputSize = 10 * 1024 * 1024;

    // Register MCP-style built-in tools
    registerTool("read_file", [this](const std::map<std::string, std::string>& params) {
        return handleReadFile(params);
    });

    registerTool("write_file", [this](const std::map<std::string, std::string>& params) {
        return handleWriteFile(params);
    });

    registerTool("search_files", [this](const std::map<std::string, std::string>& params) {
        return handleSearchFiles(params);
    });

    registerTool("grep_search", [this](const std::map<std::string, std::string>& params) {
        return handleGrepSearch(params);
    });

    registerTool("edit_file", [this](const std::map<std::string, std::string>& params) {
        return handleEditFile(params);
    });

    registerTool("list_dir", [this](const std::map<std::string, std::string>& params) {
        return handleListDirectory(params);
    });

    registerTool("get_diagnostics", [this](const std::map<std::string, std::string>& params) {
        return handleGetDiagnostics(params);
    });

    registerTool("run_in_terminal", [this](const std::map<std::string, std::string>& params) {
        return handleRunTerminal(params);
    });
}

ToolExecutionEngine::~ToolExecutionEngine()
{
}

ExecutionResult ToolExecutionEngine::executeCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& workingDir,
    uint32_t timeoutMs)
{
    ExecutionResult result;
    result.success = false;
    result.exitCode = -1;
    
    // Build full command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    }
    
    // Create pipes for stdout/stderr
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;
    
    // Create stdout pipe
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &saAttr, 0)) {
        result.errorMessage = "Failed to create stdout pipe";
        return result;
    }
    
    // Create stderr pipe
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &saAttr, 0)) {
        result.errorMessage = "Failed to create stderr pipe";
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return result;
    }
    
    // Prevent pipe handles from being inherited
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create process
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    // Convert strings for CreateProcessA
    std::string workDir = workingDir.empty() ? "." : workingDir;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLine.c_str()),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir.c_str(),
        &si,
        &pi)) {
        
        result.errorMessage = "CreateProcessA failed";
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        return result;
    }
    
    // Close write ends in parent
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // Read stdout
    const DWORD READ_SIZE = 4096;
    char buffer[READ_SIZE];
    DWORD bytesRead = 0;
    
    while (ReadFile(hStdoutRead, buffer, READ_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
        result.stdoutContent.append(buffer, bytesRead);
    }
    
    // Read stderr
    bytesRead = 0;
    while (ReadFile(hStderrRead, buffer, READ_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
        result.stderrContent.append(buffer, bytesRead);
    }
    
    // Wait for process to complete with timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    if (waitResult == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(pi.hProcess, -1);
        result.errorMessage = "Process timeout";
    } else if (waitResult == WAIT_OBJECT_0) {
        // Get exit code
        DWORD exitCode = -1;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            result.exitCode = static_cast<int>(exitCode);
            result.success = (exitCode == 0);
        }
    } else {
        result.errorMessage = "WaitForSingleObject failed";
    }
    
    // Cleanup
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    
    return result;
}

ExecutionResult ToolExecutionEngine::executePowerShell(
    const std::string& script,
    const std::map<std::string, std::string>& args,
    uint32_t timeoutMs)
{
    // Escape PowerShell script
    std::string escapedScript = script;
    
    // Replace " with \"
    size_t pos = 0;
    while ((pos = escapedScript.find('"', pos)) != std::string::npos) {
        escapedScript.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    std::vector<std::string> psArgs;
    psArgs.push_back("-NoProfile");
    psArgs.push_back("-Command");
    psArgs.push_back(escapedScript);
    
    return executeCommand("powershell.exe", psArgs, "", timeoutMs);
}

ExecutionResult ToolExecutionEngine::executeBatchFile(
    const std::filesystem::path& batchFile,
    const std::vector<std::string>& args,
    uint32_t timeoutMs)
{
    std::vector<std::string> cmdArgs = args;
    cmdArgs.insert(cmdArgs.begin(), batchFile.string());
    
    return executeCommand("cmd.exe", cmdArgs, "", timeoutMs);
}

void ToolExecutionEngine::registerTool(const std::string& toolId, ToolHandler handler)
{
    m_toolRegistry[toolId] = handler;
}

ExecutionResult ToolExecutionEngine::invokeTool(const ToolInvocation& invocation)
{
    ExecutionResult result;
    result.success = false;
    
    auto it = m_toolRegistry.find(invocation.toolId);
    if (it == m_toolRegistry.end()) {
        result.errorMessage = "Tool not found: " + invocation.toolId;
        return result;
    }
    
    return it->second(invocation.parameters);
}

ExecutionResult ToolExecutionEngine::readFile(const std::filesystem::path& path, size_t startLine, size_t endLine)
{
    ExecutionResult result;
    result.success = false;
    
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open file: " + path.string();
            return result;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // If line filtering requested
        if (startLine > 0 || endLine < SIZE_MAX) {
            std::istringstream iss(content);
            std::string line;
            std::ostringstream oss;
            size_t lineNum = 1;
            
            while (std::getline(iss, line)) {
                if (lineNum >= startLine && lineNum <= endLine) {
                    oss << line << "\n";
                }
                lineNum++;
            }
            result.stdoutContent = oss.str();
        } else {
            result.stdoutContent = content;
        }
        
        result.success = true;
        result.exitCode = 0;
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Exception: ") + e.what();
        return result;
    }
}

ExecutionResult ToolExecutionEngine::writeFile(const std::filesystem::path& path, const std::string& content, bool append)
{
    ExecutionResult result;
    result.success = false;
    
    try {
        std::ios_base::openmode mode = std::ios::binary;
        if (append) {
            mode |= std::ios::app;
        }
        
        std::ofstream file(path, mode);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open file for writing: " + path.string();
            return result;
        }
        
        file.write(content.c_str(), content.size());
        file.close();
        
        result.success = true;
        result.exitCode = 0;
        result.stdoutContent = "File written: " + path.string();
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Exception: ") + e.what();
        return result;
    }
}

ExecutionResult ToolExecutionEngine::listDirectory(const std::filesystem::path& path, const std::string& pattern)
{
    ExecutionResult result;
    result.success = false;
    
    try {
        if (!std::filesystem::is_directory(path)) {
            result.errorMessage = "Path is not a directory: " + path.string();
            return result;
        }
        
        std::ostringstream oss;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            oss << entry.path().filename().string();
            if (entry.is_directory()) {
                oss << "/";
            }
            oss << "\n";
        }
        
        result.stdoutContent = oss.str();
        result.success = true;
        result.exitCode = 0;
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Exception: ") + e.what();
        return result;
    }
}

ExecutionResult ToolExecutionEngine::applyFileEdit(const FileEdit& edit)
{
    ExecutionResult result;
    result.success = false;
    
    // Read original file
    auto readResult = readFile(edit.filePath);
    if (!readResult.success) {
        return readResult;
    }
    
    std::string originalContent = readResult.stdoutContent;
    
    // Perform replacement
    size_t pos = originalContent.find(edit.originalText);
    if (pos == std::string::npos) {
        result.errorMessage = "Original text not found in file";
        return result;
    }
    
    originalContent.replace(pos, edit.originalText.length(), edit.replacementText);
    
    // Write back
    return writeFile(edit.filePath, originalContent);
}

ExecutionResult ToolExecutionEngine::applyFileEdits(const std::vector<FileEdit>& edits)
{
    ExecutionResult result;
    result.success = true;
    result.exitCode = 0;
    
    // Verify all edits first
    for (const auto& edit : edits) {
        auto readResult = readFile(edit.filePath);
        if (!readResult.success) {
            result.success = false;
            result.errorMessage = "Verification failed for: " + edit.filePath.string();
            return result;
        }
        
        if (readResult.stdoutContent.find(edit.originalText) == std::string::npos) {
            result.success = false;
            result.errorMessage = "Original text not found in: " + edit.filePath.string();
            return result;
        }
    }
    
    // Apply all edits
    for (const auto& edit : edits) {
        auto applyResult = applyFileEdit(edit);
        if (!applyResult.success) {
            result.success = false;
            result.errorMessage = applyResult.errorMessage;
            break;
        }
    }
    
    return result;
}

void ToolExecutionEngine::killProcessTree(DWORD processId)
{
    // This would traverse child processes and terminate them
    // Simplified version for now
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess) {
        TerminateProcess(hProcess, -1);
        CloseHandle(hProcess);
    }
}

bool ToolExecutionEngine::isProcessRunning(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return false;
    }
    
    DWORD exitCode = 0;
    bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hProcess);
    
    return isRunning;
}

std::vector<DWORD> ToolExecutionEngine::getChildProcesses(DWORD parentId)
{
    std::vector<DWORD> children;
    // Implementation would use CreateToolhelp32Snapshot
    return children;
}

std::vector<ToolExecutionEngine::ParsedError> ToolExecutionEngine::parseCompilerErrors(const std::string& output)
{
    std::vector<ParsedError> errors;
    // Simple MSVC error pattern: file(line): error code: message
    
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Look for patterns like: file.cpp(10): error C1234: message
        size_t parenPos = line.find('(');
        if (parenPos != std::string::npos) {
            size_t closeParenPos = line.find(')', parenPos);
            if (closeParenPos != std::string::npos) {
                ParsedError err;
                err.file = line.substr(0, parenPos);
                
                // Parse line number
                std::string lineStr = line.substr(parenPos + 1, closeParenPos - parenPos - 1);
                try {
                    err.line = std::stoul(lineStr);
                } catch (...) {
                    err.line = 0;
                }
                
                // Extract severity and message
                size_t colonPos = line.find(':', closeParenPos);
                if (colonPos != std::string::npos) {
                    std::string afterColon = line.substr(colonPos + 1);
                    if (afterColon.find("error") != std::string::npos) {
                        err.severity = "error";
                    } else if (afterColon.find("warning") != std::string::npos) {
                        err.severity = "warning";
                    }
                    err.message = afterColon;
                    errors.push_back(err);
                }
            }
        }
    }
    
    return errors;
}

// ============================================================================
// Tool Handler Implementations (MCP-compatible interface)
// ============================================================================

ExecutionResult ToolExecutionEngine::handleReadFile(const std::map<std::string, std::string>& params) {
    auto it = params.find("path");
    if (it == params.end()) {
        it = params.find("filePath");
    }

    if (it == params.end()) {
        ExecutionResult result;
        result.errorMessage = "Missing required parameter: path/filePath";
        return result;
    }
    
    size_t startLine = 0;
    size_t endLine = SIZE_MAX;
    
    if (params.count("startLine")) {
        startLine = std::stoul(params.at("startLine"));
    }
    if (params.count("endLine")) {
        endLine = std::stoul(params.at("endLine"));
    }
    
    return readFile(it->second, startLine, endLine);
}

ExecutionResult ToolExecutionEngine::handleWriteFile(const std::map<std::string, std::string>& params) {
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        pathIt = params.find("filePath");
    }
    auto contentIt = params.find("content");
    
    if (pathIt == params.end() || contentIt == params.end()) {
        ExecutionResult result;
        result.errorMessage = "Missing required parameters: filePath, content";
        return result;
    }
    
    bool append = false;
    if (params.count("append")) {
        auto v = params.at("append");
        append = (v == "1" || v == "true" || v == "True");
    }
    return writeFile(pathIt->second, contentIt->second, append);
}

ExecutionResult ToolExecutionEngine::handleSearchFiles(const std::map<std::string, std::string>& params) {
    ExecutionResult result;
    
    // Support both legacy (query-only) and path+pattern calling styles
    std::filesystem::path searchRoot;
    std::string pattern = "*";

    auto pathIt = params.find("path");
    if (pathIt != params.end()) {
        searchRoot = pathIt->second;
        auto patIt = params.find("pattern");
        if (patIt != params.end()) {
            pattern = patIt->second;
        }
    } else {
        auto queryIt = params.find("query");
        if (queryIt == params.end()) {
            result.errorMessage = "Missing required parameter: path or query";
            return result;
        }
        pattern = queryIt->second;
        searchRoot = m_workingDirectory.empty() ? std::filesystem::path{"."} : m_workingDirectory;
    }

    int maxResults = 50;
    if (params.count("maxResults")) {
        maxResults = std::stoi(params.at("maxResults"));
    }
    
    std::ostringstream output;
    int count = 0;
    
    auto matchGlob = [](const std::string& name, const std::string& pattern) -> bool {
        // Handle **/ prefix
        std::string p = pattern;
        if (p.length() >= 3 && p.substr(0, 3) == "**/") {
            p = p.substr(3);
        }
        if (p == "*") return true;
        if (!p.empty() && p[0] == '*') {
            std::string suffix = p.substr(1);
            return name.length() >= suffix.length() && 
                   name.substr(name.length() - suffix.length()) == suffix;
        }
        return name.find(p) != std::string::npos;
    };
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(searchRoot)) {
            if (count >= maxResults) break;
            
            std::string filename = entry.path().filename().string();
            std::string relPath = entry.path().lexically_relative(searchRoot).string();
            
            if (matchGlob(filename, pattern) || matchGlob(relPath, pattern)) {
                output << relPath << "\n";
                count++;
            }
        }
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Search error: ") + e.what();
        return result;
    }
    
    result.success = true;
    result.exitCode = 0;
    result.stdoutContent = count == 0 ? "No files found matching: " + pattern : output.str();
    return result;
}

ExecutionResult ToolExecutionEngine::handleGrepSearch(const std::map<std::string, std::string>& params) {
    ExecutionResult result;
    
    auto queryIt = params.find("query");
    if (queryIt == params.end()) {
        result.errorMessage = "Missing required parameter: query";
        return result;
    }
    
    std::string pattern = queryIt->second;
    bool isRegex = params.count("isRegexp") && params.at("isRegexp") == "true";
    std::string includePattern = params.count("includePattern") ? params.at("includePattern") : "*";
    int maxResults = params.count("maxResults") ? std::stoi(params.at("maxResults")) : 100;
    
    std::filesystem::path searchRoot = m_workingDirectory.empty() ? "." : m_workingDirectory;
    
    std::ostringstream output;
    int matchCount = 0;
    
    try {
        std::regex regex;
        if (isRegex) {
            regex = std::regex(pattern, std::regex::icase);
        }
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(searchRoot)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;
            
            std::string filename = entry.path().filename().string();
            
            // Skip binary files
            std::string ext = entry.path().extension().string();
            if (ext == ".exe" || ext == ".dll" || ext == ".obj" || ext == ".lib" ||
                ext == ".png" || ext == ".jpg" || ext == ".gif" || ext == ".pdb") {
                continue;
            }
            
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;
            
            std::string line;
            int lineNum = 0;
            while (std::getline(file, line) && matchCount < maxResults) {
                lineNum++;
                bool matches = false;
                
                if (isRegex) {
                    matches = std::regex_search(line, regex);
                } else {
                    // Case-insensitive search
                    std::string lowerLine = line;
                    std::string lowerPattern = pattern;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
                    matches = lowerLine.find(lowerPattern) != std::string::npos;
                }
                
                if (matches) {
                    output << entry.path().lexically_relative(searchRoot).string()
                           << ":" << lineNum << ": " << line << "\n";
                    matchCount++;
                }
            }
        }
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Search error: ") + e.what();
        return result;
    }
    
    result.success = true;
    result.exitCode = 0;
    result.stdoutContent = matchCount == 0 ? "No matches found." : output.str();
    return result;
}

ExecutionResult ToolExecutionEngine::handleEditFile(const std::map<std::string, std::string>& params) {
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        pathIt = params.find("filePath");
    }

    auto oldIt = params.find("original");
    if (oldIt == params.end()) {
        oldIt = params.find("oldString");
    }

    auto newIt = params.find("replacement");
    if (newIt == params.end()) {
        newIt = params.find("newString");
    }
    
    if (pathIt == params.end() || oldIt == params.end() || newIt == params.end()) {
        ExecutionResult result;
        result.errorMessage = "Missing required parameters: path/filePath, original/oldString, replacement/newString";
        return result;
    }
    
    FileEdit edit;
    edit.filePath = pathIt->second;
    edit.originalText = oldIt->second;
    edit.replacementText = newIt->second;
    
    return applyFileEdit(edit);
}

ExecutionResult ToolExecutionEngine::handleListDirectory(const std::map<std::string, std::string>& params) {
    auto pathIt = params.find("path");
    if (pathIt == params.end()) {
        ExecutionResult result;
        result.errorMessage = "Missing required parameter: path";
        return result;
    }
    
    std::string pattern = params.count("pattern") ? params.at("pattern") : "*";
    return listDirectory(pathIt->second, pattern);
}

ExecutionResult ToolExecutionEngine::handleGetDiagnostics(const std::map<std::string, std::string>& params) {
    ExecutionResult result;
    result.success = true;
    result.exitCode = 0;
    
    std::ostringstream output;
    output << "Diagnostics check:\n";
    
    // If filePaths provided, could run compiler syntax check
    if (params.count("filePaths")) {
        output << "Requested files: " << params.at("filePaths") << "\n";
        output << "For C++: cl /c /Zs <file> to check syntax\n";
        output << "For Python: python -m py_compile <file>\n";
    } else {
        output << "No specific files requested. Provide 'filePaths' parameter.\n";
    }
    
    result.stdoutContent = output.str();
    return result;
}

ExecutionResult ToolExecutionEngine::handleRunTerminal(const std::map<std::string, std::string>& params) {
    auto cmdIt = params.find("command");
    if (cmdIt == params.end()) {
        ExecutionResult result;
        result.errorMessage = "Missing required parameter: command";
        return result;
    }
    
    std::string workDir = params.count("cwd") ? params.at("cwd") : "";
    uint32_t timeout = params.count("timeoutMs") ? std::stoul(params.at("timeoutMs")) : m_defaultTimeoutMs;
    
    return executeCommand(cmdIt->second, {}, workDir, timeout);
}

// ============================================================================
// Missing Private Methods
// ============================================================================

std::unique_ptr<ToolExecutionEngine::ProcessContext> ToolExecutionEngine::createProcess(
    const std::string& commandLine,
    const std::string& workingDir) 
{
    auto ctx = std::make_unique<ProcessContext>();
    
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    
    // Create pipes
    if (!CreatePipe(&ctx->hStdoutRead, &ctx->hStdoutWrite, &saAttr, 0)) {
        return nullptr;
    }
    if (!CreatePipe(&ctx->hStderrRead, &ctx->hStderrWrite, &saAttr, 0)) {
        ctx->cleanup();
        return nullptr;
    }
    
    SetHandleInformation(ctx->hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(ctx->hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = ctx->hStdoutWrite;
    si.hStdError = ctx->hStderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    std::string workDir = workingDir.empty() ? "." : workingDir;
    std::string cmdLine = commandLine;
    
    if (!CreateProcessA(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir.c_str(),
        &si,
        &pi)) {
        ctx->cleanup();
        return nullptr;
    }
    
    ctx->hProcess = pi.hProcess;
    ctx->hThread = pi.hThread;
    ctx->processId = pi.dwProcessId;
    ctx->startTime = std::chrono::steady_clock::now();
    
    // Close write ends in parent
    CloseHandle(ctx->hStdoutWrite);
    CloseHandle(ctx->hStderrWrite);
    ctx->hStdoutWrite = nullptr;
    ctx->hStderrWrite = nullptr;
    
    return ctx;
}

ExecutionResult ToolExecutionEngine::captureOutput(
    std::unique_ptr<ProcessContext> ctx,
    uint32_t timeoutMs) 
{
    ExecutionResult result;
    result.success = false;
    
    if (!ctx) {
        result.errorMessage = "Invalid process context";
        return result;
    }
    
    const DWORD READ_SIZE = 4096;
    char buffer[READ_SIZE];
    DWORD bytesRead = 0;
    
    // Read stdout
    while (ReadFile(ctx->hStdoutRead, buffer, READ_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
        result.stdoutContent.append(buffer, bytesRead);
        if (result.stdoutContent.size() > m_maxOutputSize) {
            result.stdoutContent = result.stdoutContent.substr(0, m_maxOutputSize) + "\n... [truncated]";
            break;
        }
    }
    
    // Read stderr
    bytesRead = 0;
    while (ReadFile(ctx->hStderrRead, buffer, READ_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
        result.stderrContent.append(buffer, bytesRead);
    }
    
    // Wait for process with timeout
    DWORD waitResult = WaitForSingleObject(ctx->hProcess, timeoutMs);
    
    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - ctx->startTime);
    
    if (waitResult == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(ctx->hProcess, -1);
        result.errorMessage = "Process timeout";
    } else if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = -1;
        if (GetExitCodeProcess(ctx->hProcess, &exitCode)) {
            result.exitCode = static_cast<int>(exitCode);
            result.success = (exitCode == 0);
        }
    }
    
    ctx->cleanup();
    return result;
}

std::string ToolExecutionEngine::readPipeAsync(HANDLE hPipe, uint32_t timeoutMs) {
    std::string result;
    const DWORD READ_SIZE = 4096;
    char buffer[READ_SIZE];
    DWORD bytesRead = 0;
    DWORD bytesAvailable = 0;
    
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > timeoutMs) break;
        
        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            break;
        }
        
        if (bytesAvailable > 0) {
            if (ReadFile(hPipe, buffer, READ_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
                result.append(buffer, bytesRead);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    return result;
}

bool ToolExecutionEngine::createPipeNonBlocking(HANDLE* hRead, HANDLE* hWrite)
{
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    
    return CreatePipe(hRead, hWrite, &saAttr, 0) != FALSE;
}

} // namespace RawrXD
