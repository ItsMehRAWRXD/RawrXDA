// ============================================================================
// AgentToolHandlers.cpp — Agentic Tool Handler Implementations
// ============================================================================
// Concrete tools for the RawrXD autonomous coding agent.
// Every tool returns ToolCallResult. Every path is sandboxed.
// Every mutation creates a backup. Every command is timeout-enforced.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "AgentToolHandlers.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <chrono>
#include <windows.h>

using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;
using RawrXD::Agent::ToolOutcome;
using RawrXD::Agent::ToolGuardrails;
using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Static state
// ============================================================================
ToolGuardrails AgentToolHandlers::s_guardrails;

namespace {

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

int CountLines(const std::string& text) {
    int count = 0;
    for (char c : text) {
        if (c == '\n') ++count;
    }
    return count + (text.empty() ? 0 : 1);
}

} // anonymous namespace

// ============================================================================
// Guardrails
// ============================================================================

void AgentToolHandlers::SetGuardrails(const ToolGuardrails& guards) {
    s_guardrails = guards;
}

const ToolGuardrails& AgentToolHandlers::GetGuardrails() {
    return s_guardrails;
}

// ============================================================================
// Path validation
// ============================================================================

std::string AgentToolHandlers::NormalizePath(const std::string& path) {
    try {
        return fs::weakly_canonical(path).string();
    } catch (...) {
        return path;
    }
}

bool AgentToolHandlers::IsPathAllowed(const std::string& path) {
    std::string normalized = NormalizePath(path);

    // Must be under at least one allowed root
    if (s_guardrails.allowedRoots.empty()) return true; // No restrictions configured

    for (const auto& root : s_guardrails.allowedRoots) {
        std::string normRoot = NormalizePath(root);
        if (normalized.find(normRoot) == 0) {
            // Check deny patterns
            if (!MatchesDenyPattern(normalized)) {
                return true;
            }
        }
    }
    return false;
}

bool AgentToolHandlers::MatchesDenyPattern(const std::string& path) {
    for (const auto& pattern : s_guardrails.denyPatterns) {
        // Simple suffix matching for deny patterns like "*.exe"
        if (pattern.size() > 1 && pattern[0] == '*') {
            std::string suffix = pattern.substr(1);
            if (path.size() >= suffix.size() &&
                path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return true;
            }
        }
    }
    return false;
}

std::string AgentToolHandlers::CreateBackup(const std::string& path) {
    std::string backupPath = path + ".agent_bak";
    try {
        fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing);
    } catch (const std::exception& ex) {
        return std::string("Backup failed: ") + ex.what();
    }
    return "";
}

// ============================================================================
// read_file — Read file contents
// ============================================================================

ToolCallResult AgentToolHandlers::ToolReadFile(const json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolCallResult::Validation("read_file requires 'path' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path)) {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    if (!fs::exists(path)) {
        return ToolCallResult::Error("File not found: " + path);
    }

    auto fileSize = fs::file_size(path);
    if (fileSize > s_guardrails.maxFileSizeBytes) {
        return ToolCallResult::Error("File too large: " + std::to_string(fileSize) +
                                     " bytes (max " + std::to_string(s_guardrails.maxFileSizeBytes) + ")");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ToolCallResult::Error("Cannot open file: " + path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    file.close();
    std::string content = ss.str();

    ToolCallResult result = ToolCallResult::Ok(content, {
        {"lines", CountLines(content)},
        {"size_bytes", content.size()}
    });
    result.filePath = path;
    result.bytesRead = content.size();
    return result;
}

// ============================================================================
// write_file — Create or overwrite file
// ============================================================================

ToolCallResult AgentToolHandlers::WriteFile(const json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolCallResult::Validation("write_file requires 'path' (string)");
    }
    if (!args.contains("content") || !args["content"].is_string()) {
        return ToolCallResult::Validation("write_file requires 'content' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    std::string content = args["content"].get<std::string>();

    if (!IsPathAllowed(path)) {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    if (content.size() > s_guardrails.maxFileSizeBytes) {
        return ToolCallResult::Error("Content too large: " + std::to_string(content.size()) + " bytes");
    }

    // Create backup if file exists and guardrails require it
    bool existed = fs::exists(path);
    if (existed && s_guardrails.requireBackupOnWrite) {
        std::string backupError = CreateBackup(path);
        if (!backupError.empty()) {
            return ToolCallResult::Error(backupError);
        }
    }

    // Ensure parent directories exist
    try {
        fs::path parentDir = fs::path(path).parent_path();
        if (!parentDir.empty() && !fs::exists(parentDir)) {
            fs::create_directories(parentDir);
        }
    } catch (const std::exception& ex) {
        return ToolCallResult::Error(std::string("Cannot create directories: ") + ex.what());
    }

    // Write file
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        return ToolCallResult::Error("Cannot open file for writing: " + path);
    }
    file.write(content.data(), content.size());
    file.close();

    ToolCallResult result = ToolCallResult::Ok(
        existed ? "File overwritten successfully" : "File created successfully",
        {{"lines", CountLines(content)}, {"size_bytes", content.size()}, {"created", !existed}}
    );
    result.filePath = path;
    result.bytesWritten = content.size();
    result.linesAffected = CountLines(content);
    return result;
}

// ============================================================================
// replace_in_file — Search+replace text block
// ============================================================================

ToolCallResult AgentToolHandlers::ReplaceInFile(const json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolCallResult::Validation("replace_in_file requires 'path' (string)");
    }
    if (!args.contains("old_string") || !args["old_string"].is_string()) {
        return ToolCallResult::Validation("replace_in_file requires 'old_string' (string)");
    }
    if (!args.contains("new_string") || !args["new_string"].is_string()) {
        return ToolCallResult::Validation("replace_in_file requires 'new_string' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    std::string oldStr = args["old_string"].get<std::string>();
    std::string newStr = args["new_string"].get<std::string>();

    if (!IsPathAllowed(path)) {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path)) {
        return ToolCallResult::Error("File not found: " + path);
    }

    // Read file
    std::ifstream inFile(path, std::ios::binary);
    if (!inFile.is_open()) {
        return ToolCallResult::Error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << inFile.rdbuf();
    inFile.close();
    std::string content = ss.str();

    // Find the old string
    size_t pos = content.find(oldStr);
    if (pos == std::string::npos) {
        return ToolCallResult::Error("old_string not found in file. "
                                     "Ensure you're using the exact text including whitespace.");
    }

    // Check for multiple matches (warn but still replace first)
    size_t secondMatch = content.find(oldStr, pos + oldStr.size());
    bool multipleMatches = (secondMatch != std::string::npos);

    // Create backup
    if (s_guardrails.requireBackupOnWrite) {
        std::string backupError = CreateBackup(path);
        if (!backupError.empty()) {
            return ToolCallResult::Error(backupError);
        }
    }

    // Perform replacement
    std::string newContent = content.substr(0, pos) + newStr + content.substr(pos + oldStr.size());

    // Write back
    std::ofstream outFile(path, std::ios::trunc | std::ios::binary);
    if (!outFile.is_open()) {
        return ToolCallResult::Error("Cannot write file: " + path);
    }
    outFile.write(newContent.data(), newContent.size());
    outFile.close();

    int linesChanged = CountLines(newStr) - CountLines(oldStr);
    std::string msg = "Replaced " + std::to_string(oldStr.size()) + " bytes with " +
                      std::to_string(newStr.size()) + " bytes";
    if (multipleMatches) {
        msg += " (WARNING: multiple matches found, only first replaced)";
    }

    ToolCallResult result = ToolCallResult::Ok(msg, {
        {"old_length", oldStr.size()},
        {"new_length", newStr.size()},
        {"position", pos},
        {"multiple_matches", multipleMatches},
        {"line_delta", linesChanged}
    });
    result.filePath = path;
    result.bytesWritten = newContent.size();
    result.linesAffected = std::abs(linesChanged) + CountLines(newStr);
    return result;
}

// ============================================================================
// list_dir — List directory contents
// ============================================================================

ToolCallResult AgentToolHandlers::ListDir(const json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolCallResult::Validation("list_dir requires 'path' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path)) {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return ToolCallResult::Error("Directory not found: " + path);
    }

    std::ostringstream listing;
    int fileCount = 0, dirCount = 0;

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                listing << name << "/\n";
                ++dirCount;
            } else {
                auto size = entry.file_size();
                listing << name << " (" << size << " bytes)\n";
                ++fileCount;
            }
        }
    } catch (const std::exception& ex) {
        return ToolCallResult::Error(std::string("Directory listing failed: ") + ex.what());
    }

    return ToolCallResult::Ok(listing.str(), {
        {"files", fileCount},
        {"directories", dirCount},
        {"total", fileCount + dirCount}
    });
}

// ============================================================================
// execute_command — Run terminal command (sandboxed)
// ============================================================================

bool AgentToolHandlers::RunProcess(const std::wstring& cmdLine, uint32_t timeoutMs,
                                    std::string& output, uint32_t& exitCode) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        output = "Failed to create output pipe";
        return false;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdLine;
    BOOL created = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!created) {
        CloseHandle(hRead);
        output = "Failed to create process";
        return false;
    }

    std::string buffer;
    buffer.reserve(4096);
    DWORD startTick = GetTickCount();

    while (true) {
        DWORD available = 0;
        if (PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char temp[4096];
            DWORD bytesRead = 0;
            if (::ReadFile(hRead, temp, sizeof(temp) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                temp[bytesRead] = '\0';
                buffer.append(temp, bytesRead);
                if (buffer.size() > s_guardrails.maxOutputCaptureBytes) {
                    buffer.resize(s_guardrails.maxOutputCaptureBytes);
                    buffer += "\n[OUTPUT TRUNCATED]";
                    break;
                }
            }
        }

        DWORD waitResult = WaitForSingleObject(pi.hProcess, 100);
        if (waitResult == WAIT_OBJECT_0) {
            // Read remaining output
            while (true) {
                DWORD avail2 = 0;
                if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &avail2, nullptr) || avail2 == 0) break;
                char temp[4096];
                DWORD bytesRead = 0;
                if (::ReadFile(hRead, temp, sizeof(temp) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    temp[bytesRead] = '\0';
                    buffer.append(temp, bytesRead);
                } else break;
            }
            break;
        }

        if (GetTickCount() - startTick > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);
            output = buffer + "\n[TIMEOUT after " + std::to_string(timeoutMs) + "ms]";
            exitCode = WAIT_TIMEOUT;
            return false;
        }
    }

    DWORD dwExitCode = 0;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    exitCode = static_cast<uint32_t>(dwExitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
    output = buffer;
    return true;
}

ToolCallResult AgentToolHandlers::ExecuteCommand(const json& args) {
    if (!args.contains("command") || !args["command"].is_string()) {
        return ToolCallResult::Validation("execute_command requires 'command' (string)");
    }

    std::string command = args["command"].get<std::string>();
    uint32_t timeout = s_guardrails.commandTimeoutMs;
    if (args.contains("timeout") && args["timeout"].is_number()) {
        timeout = static_cast<uint32_t>(args["timeout"].get<int>());
        // Cap timeout at 5 minutes
        if (timeout > 300000) timeout = 300000;
    }

    // Build command line via cmd.exe
    std::wstring cmdLine = L"cmd.exe /C " + ToWide(command);

    std::string output;
    uint32_t exitCode = 0;

    auto startTime = std::chrono::steady_clock::now();
    bool success = RunProcess(cmdLine, timeout, output, exitCode);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    if (!success && exitCode == WAIT_TIMEOUT) {
        ToolCallResult result = ToolCallResult::TimedOut(output);
        result.metadata = {{"exit_code", exitCode}, {"elapsed_ms", elapsed}};
        return result;
    }

    ToolCallResult result = (exitCode == 0)
        ? ToolCallResult::Ok(output, {{"exit_code", 0}, {"elapsed_ms", elapsed}})
        : ToolCallResult::Error("Command exited with code " + std::to_string(exitCode) +
                                "\n" + output, ToolOutcome::ExecutionError);
    if (exitCode != 0) {
        result.metadata = {{"exit_code", exitCode}, {"elapsed_ms", elapsed}};
        result.output = output; // Include output even on error
    }
    return result;
}

// ============================================================================
// search_code — Recursive file search
// ============================================================================

ToolCallResult AgentToolHandlers::SearchCode(const json& args) {
    if (!args.contains("query") || !args["query"].is_string()) {
        return ToolCallResult::Validation("search_code requires 'query' (string)");
    }

    std::string query = args["query"].get<std::string>();
    std::string filePattern = args.value("file_pattern", "*.*");

    if (s_guardrails.allowedRoots.empty()) {
        return ToolCallResult::Error("No workspace root configured for search");
    }

    std::string searchRoot = s_guardrails.allowedRoots[0]; // Primary workspace
    int maxResults = s_guardrails.maxSearchResults;
    int hitCount = 0;

    std::ostringstream results;

    try {
        for (auto it = fs::recursive_directory_iterator(searchRoot,
                 fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {

            if (hitCount >= maxResults) break;
            if (!it->is_regular_file()) continue;

            std::string filename = it->path().filename().string();

            // Simple pattern matching for file_pattern
            if (filePattern != "*.*" && filePattern != "*") {
                // Check extension
                std::string ext = it->path().extension().string();
                if (filePattern[0] == '*' && filePattern.size() > 1) {
                    std::string reqExt = filePattern.substr(1);
                    if (ext != reqExt) continue;
                }
            }

            // Skip binary/large files
            auto fsize = it->file_size();
            if (fsize > 2 * 1024 * 1024) continue; // Skip files > 2MB
            if (fsize == 0) continue;

            // Read and search
            std::ifstream file(it->path(), std::ios::binary);
            if (!file.is_open()) continue;

            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();

            size_t pos = 0;
            int lineNum = 1;
            size_t lineStart = 0;

            while (pos < content.size() && hitCount < maxResults) {
                // Track line numbers
                while (lineStart < pos) {
                    if (content[lineStart] == '\n') ++lineNum;
                    ++lineStart;
                }

                size_t found = content.find(query, pos);
                if (found == std::string::npos) break;

                // Count lines to found position
                for (size_t i = lineStart; i < found; ++i) {
                    if (content[i] == '\n') ++lineNum;
                }
                lineStart = found;

                // Extract line context
                size_t contextStart = content.rfind('\n', found);
                contextStart = (contextStart == std::string::npos) ? 0 : contextStart + 1;
                size_t contextEnd = content.find('\n', found);
                if (contextEnd == std::string::npos) contextEnd = content.size();

                std::string lineText = content.substr(contextStart,
                    std::min(contextEnd - contextStart, static_cast<size_t>(200)));

                // Relative path from search root
                std::string relPath = fs::relative(it->path(), searchRoot).string();
                results << relPath << ":" << lineNum << ": " << lineText << "\n";

                ++hitCount;
                pos = found + query.size();
            }
        }
    } catch (const std::exception& ex) {
        if (hitCount == 0) {
            return ToolCallResult::Error(std::string("Search failed: ") + ex.what());
        }
        // Partial results are still useful
    }

    if (hitCount == 0) {
        return ToolCallResult::Ok("No matches found for: " + query, {{"matches", 0}});
    }

    std::string truncMsg;
    if (hitCount >= maxResults) {
        truncMsg = "\n[Results truncated at " + std::to_string(maxResults) + " matches]";
    }

    return ToolCallResult::Ok(results.str() + truncMsg, {
        {"matches", hitCount},
        {"truncated", hitCount >= maxResults}
    });
}

// ============================================================================
// get_diagnostics — Return compiler/LSP errors
// ============================================================================

ToolCallResult AgentToolHandlers::GetDiagnostics(const json& args) {
    if (!args.contains("file") || !args["file"].is_string()) {
        return ToolCallResult::Validation("get_diagnostics requires 'file' (string)");
    }

    std::string file = NormalizePath(args["file"].get<std::string>());

    // Run cl.exe /Zs (syntax check only) for C++ files
    std::string ext = fs::path(file).extension().string();
    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp") {
        std::wstring cmdLine = L"cl.exe /Zs /EHsc /std:c++20 /W4 /nologo \"" + ToWide(file) + L"\"";

        std::string output;
        uint32_t exitCode = 0;
        RunProcess(cmdLine, 30000, output, exitCode);

        return ToolCallResult::Ok(output.empty() ? "No diagnostics" : output, {
            {"file", file},
            {"exit_code", exitCode},
            {"has_errors", exitCode != 0}
        });
    }

    return ToolCallResult::Ok("Diagnostics not available for file type: " + ext);
}

// ============================================================================
// Schema generation — OpenAI function-calling format
// ============================================================================

json AgentToolHandlers::GetAllSchemas() {
    json tools = json::array();

    // Helper: build a JSON array of strings (avoids json::array() initializer issues)
    auto jstrArr = [](std::initializer_list<const char*> items) -> json {
        json arr = json::array();
        for (auto s : items) arr.push_back(s);
        return arr;
    };

    // read_file
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "read_file"},
            {"description", "Read the content of a file at a specific path."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Absolute path to the file"}}}
                }},
                {"required", jstrArr({"path"})}
            }}
        }}
    });

    // write_file
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "write_file"},
            {"description", "Create a new file or overwrite an existing one. Backup is created automatically."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Absolute path for the file"}}},
                    {"content", {{"type", "string"}, {"description", "Complete file content to write"}}}
                }},
                {"required", jstrArr({"path", "content"})}
            }}
        }}
    });

    // replace_in_file
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "replace_in_file"},
            {"description", "Search and replace a block of text in a file. Include 3+ lines of context in old_string for uniqueness."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Absolute path to the file"}}},
                    {"old_string", {{"type", "string"}, {"description", "Exact text to find (include surrounding context)"}}},
                    {"new_string", {{"type", "string"}, {"description", "Replacement text"}}}
                }},
                {"required", jstrArr({"path", "old_string", "new_string"})}
            }}
        }}
    });

    // list_dir
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "list_dir"},
            {"description", "List the contents of a directory."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Absolute path to the directory"}}}
                }},
                {"required", jstrArr({"path"})}
            }}
        }}
    });

    // execute_command
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "execute_command"},
            {"description", "Run a command in the terminal (cmd.exe). Use for builds, tests, git."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"command", {{"type", "string"}, {"description", "Command to execute"}}},
                    {"timeout", {{"type", "number"}, {"description", "Timeout in milliseconds (default 30000, max 300000)"}}}
                }},
                {"required", jstrArr({"command"})}
            }}
        }}
    });

    // search_code
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "search_code"},
            {"description", "Search the codebase for a text pattern. Returns file:line: context matches."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"query", {{"type", "string"}, {"description", "Text pattern to search for"}}},
                    {"file_pattern", {{"type", "string"}, {"description", "File extension filter (e.g. *.cpp, *.h). Default: *.*"}}}
                }},
                {"required", jstrArr({"query"})}
            }}
        }}
    });

    // get_diagnostics
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "get_diagnostics"},
            {"description", "Get compiler errors and warnings for a source file."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"file", {{"type", "string"}, {"description", "Absolute path to the source file"}}}
                }},
                {"required", jstrArr({"file"})}
            }}
        }}
    });

    return tools;
}

std::string AgentToolHandlers::GetSystemPrompt(const std::string& cwd,
                                                const std::vector<std::string>& openFiles) {
    std::string filesStr;
    for (const auto& f : openFiles) {
        filesStr += "- " + f + "\n";
    }

    std::ostringstream ss;
    ss << "You are RawrXD Agent, a local high-performance autonomous coding assistant.\n"
       << "You have direct access to the filesystem and terminal.\n\n"
       << "Current Directory: " << cwd << "\n"
       << "Open Files:\n" << (filesStr.empty() ? "  (none)\n" : filesStr) << "\n"
       << "Available Tools:\n"
       << GetAllSchemas().dump(2) << "\n\n"
       << "Rules:\n"
       << "1. Always read_file before editing to verify current content.\n"
       << "2. Use replace_in_file for surgical edits; write_file for new files only.\n"
       << "3. Include 3+ lines of context in old_string for uniqueness.\n"
       << "4. Run get_diagnostics after code changes to verify correctness.\n"
       << "5. Explain your reasoning before executing each tool.\n"
       << "6. Use search_code to find relevant code before making assumptions.\n"
       << "7. Do not modify files outside the workspace.\n";

    return ss.str();
}
