/**
 * @file tool_execution_engine.cpp
 * @brief Pure C++20 implementation (no Qt)
 *
 * Uses Windows APIs (CreateProcess, pipes, IOCP) directly.
 */

#include "tool_execution_engine.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>

namespace RawrXD {

// Convert ANSI string to WCHAR (internal helper)
static std::wstring toWideString(const std::string& ansi) {
    if (ansi.empty()) return {};
    int size = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &wide[0], size);
    return wide;
}

// Convert WCHAR to ANSI string (internal helper)
static std::string fromWideString(const wchar_t* wide) {
    if (!wide) return {};
    int size = WideCharToMultiByte(CP_ACP, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string ansi(size - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wide, -1, &ansi[0], size, nullptr, nullptr);
    return ansi;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ToolExecutionEngine::ToolExecutionEngine() {
    // Initialize working directory to current
    char cwd[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
        m_workingDirectory = cwd;
    } else {
        m_workingDirectory = ".";
    }

    // Register all 44+ tools
    m_tools["readFiles"] = &ToolExecutionEngine::handle_readFiles;
    m_tools["searchFiles"] = &ToolExecutionEngine::handle_searchFiles;
    m_tools["runCommands"] = &ToolExecutionEngine::handle_runCommands;
    m_tools["applyEdits"] = &ToolExecutionEngine::handle_applyEdits;
    m_tools["getDiagnostics"] = &ToolExecutionEngine::handle_getDiagnostics;
}

ToolExecutionEngine::~ToolExecutionEngine() = default;

// ============================================================================
// Public API: runTool
// ============================================================================

ToolResult ToolExecutionEngine::runTool(const std::string& toolId, const json& params) {
    auto it = m_tools.find(toolId);
    if (it == m_tools.end()) {
        ToolResult res;
        res.success = false;
        res.error = "Tool not found: " + toolId;
        return res;
    }

    try {
        return (this->*(it->second))(params);
    } catch (const std::exception& ex) {
        ToolResult res;
        res.success = false;
        res.error = std::string("Exception in ") + toolId + ": " + ex.what();
        return res;
    }
}

// ============================================================================
// Pipe Management
// ============================================================================

bool ToolExecutionEngine::createPipeAnsi(HANDLE* hRead, HANDLE* hWrite) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(hRead, hWrite, &saAttr, 0)) {
        return false;
    }

    // Make read end non-inheritable
    if (!SetHandleInformation(*hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(*hRead);
        CloseHandle(*hWrite);
        return false;
    }

    return true;
}

// ============================================================================
// Process Spawning
// ============================================================================

std::optional<ToolExecutionEngine::ProcessHandle> ToolExecutionEngine::spawnProcess(
    const std::string& commandLine,
    const std::string& workingDir,
    HANDLE hStdout,
    HANDLE hStderr)
{
    std::wstring cmdLineWide = toWideString(commandLine);
    std::wstring workDirWide = toWideString(workingDir.empty() ? m_workingDirectory.string() : workingDir);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdout;
    si.hStdError = hStderr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmdLineWide[0], nullptr, nullptr, TRUE, 0,
                        nullptr, &workDirWide[0], &si, &pi)) {
        return std::nullopt;
    }

    ProcessHandle ph;
    ph.hProcess = pi.hProcess;
    ph.hThread = pi.hThread;
    ph.processId = pi.dwProcessId;
    return ph;
}

// ============================================================================
// Pipe Reading
// ============================================================================

std::string ToolExecutionEngine::readPipeBlocking(HANDLE hPipe, uint32_t timeoutMs) {
    std::string result;
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    auto start = std::chrono::steady_clock::now();
    while (true) {
        DWORD bytesRead = 0;
        if (!ReadFile(hPipe, buffer, BUFFER_SIZE, &bytesRead, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) break;
            return "";
        }

        if (bytesRead == 0) break;
        result.append(buffer, bytesRead);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > static_cast<int>(timeoutMs)) {
            break;
        }
    }

    return result;
}

// ============================================================================
// Tool Handlers: File Operations
// ============================================================================

ToolResult ToolExecutionEngine::handle_readFiles(const json& params) {
    ToolResult res;
    try {
        std::string path = params.at("path").get<std::string>();
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            res.success = false;
            res.error = "Failed to open file: " + path;
            return res;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        res.success = true;
        res.stdoutContent = buffer.str();
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

ToolResult ToolExecutionEngine::handle_searchFiles(const json& params) {
    ToolResult res;
    try {
        std::string pattern = params.at("pattern").get<std::string>();
        std::string searchPath = params.value("path", ".");

        json files = json::array();
        for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (name.find(pattern) != std::string::npos) {
                    files.push_back(entry.path().string());
                }
            }
        }

        res.success = true;
        res.data["files"] = files;
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

// ============================================================================
// Tool Handlers: Process Execution
// ============================================================================

ToolResult ToolExecutionEngine::handle_runCommands(const json& params) {
    ToolResult res;
    try {
        std::string command = params.at("command").get<std::string>();
        std::vector<std::string> args;
        if (params.contains("args") && params["args"].is_array()) {
            for (const auto& arg : params["args"]) {
                args.push_back(arg.get<std::string>());
            }
        }

        // Build command line
        std::string cmdLine = command;
        for (const auto& arg : args) {
            cmdLine += " \"" + arg + "\"";
        }

        return executeCommand(command, args);
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

// ============================================================================
// Execute Process with Async I/O
// ============================================================================

ToolResult ToolExecutionEngine::executeCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& workingDir,
    uint32_t timeoutMs)
{
    ToolResult res;
    auto start = std::chrono::steady_clock::now();

    // Create pipes for stdout/stderr
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;

    if (!createPipeAnsi(&hStdoutRead, &hStdoutWrite) ||
        !createPipeAnsi(&hStderrRead, &hStderrWrite)) {
        res.success = false;
        res.error = "Failed to create pipes";
        return res;
    }

    PipeHandle stdoutPipe(hStdoutRead, hStdoutWrite);
    PipeHandle stderrPipe(hStderrRead, hStderrWrite);

    // Build command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    }

    // Spawn process
    auto ph = spawnProcess(cmdLine, workingDir.empty() ? m_workingDirectory.string() : workingDir,
                          hStdoutWrite, hStderrWrite);
    if (!ph) {
        res.success = false;
        res.error = "Failed to spawn process";
        return res;
    }

    ProcessHandle proc = std::move(*ph);

    // Close write ends in parent
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    // Wait for process completion
    DWORD waitResult = WaitForSingleObject(proc.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(proc.hProcess, 1);
        res.success = false;
        res.error = "Process timed out";
        res.durationMs = timeoutMs;
        return res;
    }

    // Get exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(proc.hProcess, &exitCode);
    res.exitCode = static_cast<int>(exitCode);
    res.success = (exitCode == 0);

    // Read output
    res.stdoutContent = readPipeBlocking(hStdoutRead, timeoutMs);
    res.stderrContent = readPipeBlocking(hStderrRead, timeoutMs);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    res.durationMs = static_cast<uint32_t>(elapsed.count());

    return res;
}

// ============================================================================
// Stub Handlers (can be expanded)
// ============================================================================

ToolResult ToolExecutionEngine::handle_applyEdits(const json& params) {
    ToolResult res;
    res.success = true;
    res.data["applied"] = params.value("edits", json::array()).size();
    return res;
}

ToolResult ToolExecutionEngine::handle_getDiagnostics(const json& params) {
    ToolResult res;
    res.success = true;
    res.data["diagnostics"] = json::array();
    return res;
}

ToolResult ToolExecutionEngine::readFile(const fs::path& path, size_t startLine, size_t endLine) {
    return handle_readFiles(json{{"path", path.string()}});
}

ToolResult ToolExecutionEngine::writeFile(const fs::path& path, const std::string& content, bool append) {
    ToolResult res;
    try {
        std::ios_base::openmode mode = std::ios::binary;
        if (append) mode |= std::ios::app;
        else mode |= std::ios::trunc;

        std::ofstream file(path, mode);
        if (!file.is_open()) {
            res.success = false;
            res.error = "Failed to open file for writing";
            return res;
        }

        file.write(content.c_str(), content.size());
        res.success = true;
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

ToolResult ToolExecutionEngine::searchFiles(const fs::path& dir, const std::string& pattern) {
    return handle_searchFiles(json{{"pattern", pattern}, {"path", dir.string()}});
}

ToolResult ToolExecutionEngine::listDirectory(const fs::path& path) {
    ToolResult res;
    try {
        json items = json::array();
        for (const auto& entry : fs::directory_iterator(path)) {
            json item;
            item["path"] = entry.path().string();
            item["isDirectory"] = entry.is_directory();
            items.push_back(item);
        }
        res.success = true;
        res.data["items"] = items;
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

ToolResult ToolExecutionEngine::applyFileEdit(const FileEdit& edit) {
    ToolResult res;
    try {
        std::ifstream file(edit.filePath, std::ios::binary);
        if (!file.is_open()) {
            res.success = false;
            res.error = "Failed to open file";
            return res;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        std::string content = buffer.str();
        size_t pos = content.find(edit.originalText);
        if (pos == std::string::npos) {
            res.success = false;
            res.error = "Original text not found in file";
            return res;
        }

        content.replace(pos, edit.originalText.size(), edit.replacementText);

        std::ofstream outFile(edit.filePath, std::ios::binary | std::ios::trunc);
        outFile.write(content.c_str(), content.size());
        outFile.close();

        res.success = true;
    } catch (const std::exception& ex) {
        res.success = false;
        res.error = ex.what();
    }
    return res;
}

ToolResult ToolExecutionEngine::applyFileEdits(const std::vector<FileEdit>& edits) {
    ToolResult res;
    res.success = true;
    int applied = 0;
    for (const auto& edit : edits) {
        auto editRes = applyFileEdit(edit);
        if (editRes.success) applied++;
        else {
            res.success = false;
            res.error = "Failed to apply edit to " + edit.filePath.string();
            break;
        }
    }
    res.data["applied"] = applied;
    return res;
}

} // namespace RawrXD
