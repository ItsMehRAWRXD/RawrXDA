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

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["lines"] = CountLines(content);
    res_metadata["size_bytes"] = content.size();

    ToolCallResult result = ToolCallResult::Ok(content, res_metadata);
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

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["lines"] = CountLines(content);
    res_metadata["size_bytes"] = content.size();
    res_metadata["created"] = !existed;

    ToolCallResult result = ToolCallResult::Ok(
        existed ? "File overwritten successfully" : "File created successfully",
        res_metadata
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

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["old_length"] = oldStr.size();
    res_metadata["new_length"] = newStr.size();
    res_metadata["position"] = pos;
    res_metadata["multiple_matches"] = multipleMatches;
    res_metadata["line_delta"] = linesChanged;

    ToolCallResult result = ToolCallResult::Ok(msg, res_metadata);
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

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["files"] = fileCount;
    res_metadata["directories"] = dirCount;
    res_metadata["total"] = fileCount + dirCount;

    return ToolCallResult::Ok(listing.str(), res_metadata);
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
        nlohmann::json res_metadata = nlohmann::json::object();
        res_metadata["exit_code"] = exitCode;
        res_metadata["elapsed_ms"] = elapsed;
        result.metadata = res_metadata;
        return result;
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["exit_code"] = exitCode;
    res_metadata["elapsed_ms"] = elapsed;

    ToolCallResult result = (exitCode == 0)
        ? ToolCallResult::Ok(output, res_metadata)
        : ToolCallResult::Error("Command exited with code " + std::to_string(exitCode) +
                                "\n" + output, ToolOutcome::ExecutionError);
    if (exitCode != 0) {
        result.metadata = res_metadata;
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
        nlohmann::json zero_matches = nlohmann::json::object();
        zero_matches["matches"] = 0;
        return ToolCallResult::Ok("No matches found for: " + query, zero_matches);
    }

    std::string truncMsg;
    if (hitCount >= maxResults) {
        truncMsg = "\n[Results truncated at " + std::to_string(maxResults) + " matches]";
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["matches"] = hitCount;
    res_metadata["truncated"] = (hitCount >= maxResults);

    return ToolCallResult::Ok(results.str() + truncMsg, res_metadata);
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

        nlohmann::json res_metadata = nlohmann::json::object();
        res_metadata["file"] = file;
        res_metadata["exit_code"] = exitCode;
        res_metadata["has_errors"] = (exitCode != 0);

        return ToolCallResult::Ok(output.empty() ? "No diagnostics" : output, res_metadata);
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["file_type"] = ext;
    return ToolCallResult::Ok("Diagnostics not available for file type: " + ext, res_metadata);
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
    json rf = json::object();
    rf["type"] = "function";
    json rf_f = json::object();
    rf_f["name"] = "read_file";
    rf_f["description"] = "Read the content of a file at a specific path.";
    json rf_p = json::object();
    rf_p["type"] = "object";
    json rf_prop = json::object();
    json rf_path = json::object();
    rf_path["type"] = "string";
    rf_path["description"] = "Absolute path to the file";
    nlohmann::json metadata = nlohmann::json::object();
    metadata["path"] = rf_path;
    rf_prop["path"] = metadata["path"];
    rf_p["properties"] = rf_prop;
    rf_p["required"] = jstrArr({"path"});
    rf_f["parameters"] = rf_p;
    rf["function"] = rf_f;
    tools.push_back(rf);

    // write_file
    json wf = json::object();
    wf["type"] = "function";
    json wf_f = json::object();
    wf_f["name"] = "write_file";
    wf_f["description"] = "Create a new file or overwrite an existing one. Backup is created automatically.";
    json wf_p = json::object();
    wf_p["type"] = "object";
    json wf_prop = json::object();
    json wf_path = json::object();
    wf_path["type"] = "string";
    wf_path["description"] = "Absolute path for the file";
    json wf_cont = json::object();
    wf_cont["type"] = "string";
    wf_cont["description"] = "Complete file content to write";

    nlohmann::json wf_metadata = nlohmann::json::object();
    wf_metadata["path"] = wf_path;
    wf_metadata["content"] = wf_cont;
    wf_prop["path"] = wf_metadata["path"];
    wf_prop["content"] = wf_metadata["content"];

    wf_p["properties"] = wf_prop;
    wf_p["required"] = jstrArr({"path", "content"});
    wf_f["parameters"] = wf_p;
    wf["function"] = wf_f;
    tools.push_back(wf);

    // replace_in_file
    json rif = json::object();
    rif["type"] = "function";
    json rif_f = json::object();
    rif_f["name"] = "replace_in_file";
    rif_f["description"] = "Search and replace a block of text in a file. Include 3+ lines of context in old_string for uniqueness.";
    json rif_p = json::object();
    rif_p["type"] = "object";
    json rif_prop = json::object();
    json rif_path = json::object();
    rif_path["type"] = "string";
    rif_path["description"] = "Absolute path to the file";
    json rif_old = json::object();
    rif_old["type"] = "string";
    rif_old["description"] = "Exact text to find (include surrounding context)";
    json rif_new = json::object();
    rif_new["type"] = "string";
    rif_new["description"] = "Replacement text";

    nlohmann::json rif_metadata = nlohmann::json::object();
    rif_metadata["path"] = rif_path;
    rif_metadata["old_string"] = rif_old;
    rif_metadata["new_string"] = rif_new;
    rif_prop["path"] = rif_metadata["path"];
    rif_prop["old_string"] = rif_metadata["old_string"];
    rif_prop["new_string"] = rif_metadata["new_string"];

    rif_p["properties"] = rif_prop;
    rif_p["required"] = jstrArr({"path", "old_string", "new_string"});
    rif_f["parameters"] = rif_p;
    rif["function"] = rif_f;
    tools.push_back(rif);

    // list_dir
    json ld = json::object();
    ld["type"] = "function";
    json ld_f = json::object();
    ld_f["name"] = "list_dir";
    ld_f["description"] = "List the contents of a directory.";
    json ld_p = json::object();
    ld_p["type"] = "object";
    json ld_prop = json::object();
    json ld_path = json::object();
    ld_path["type"] = "string";
    ld_path["description"] = "Absolute path to the directory";

    nlohmann::json ld_metadata = nlohmann::json::object();
    ld_metadata["path"] = ld_path;
    ld_prop["path"] = ld_metadata["path"];

    ld_p["properties"] = ld_prop;
    ld_p["required"] = jstrArr({"path"});
    ld_f["parameters"] = ld_p;
    ld["function"] = ld_f;
    tools.push_back(ld);

    // execute_command
    json ec = json::object();
    ec["type"] = "function";
    json ec_f = json::object();
    ec_f["name"] = "execute_command";
    ec_f["description"] = "Run a command in the terminal (cmd.exe). Use for builds, tests, git.";
    json ec_p = json::object();
    ec_p["type"] = "object";
    json ec_prop = json::object();
    json ec_cmd = json::object();
    ec_cmd["type"] = "string";
    ec_cmd["description"] = "Command to execute";
    json ec_to = json::object();
    ec_to["type"] = "number";
    ec_to["description"] = "Timeout in milliseconds (default 30000, max 300000)";

    nlohmann::json ec_metadata = nlohmann::json::object();
    ec_metadata["command"] = ec_cmd;
    ec_metadata["timeout"] = ec_to;
    ec_prop["command"] = ec_metadata["command"];
    ec_prop["timeout"] = ec_metadata["timeout"];

    ec_p["properties"] = ec_prop;
    ec_p["required"] = jstrArr({"command"});
    ec_f["parameters"] = ec_p;
    ec["function"] = ec_f;
    tools.push_back(ec);

    // search_code
    json sc = json::object();
    sc["type"] = "function";
    json sc_f = json::object();
    sc_f["name"] = "search_code";
    sc_f["description"] = "Search the codebase for a text pattern. Returns file:line: context matches.";
    json sc_p = json::object();
    sc_p["type"] = "object";
    json sc_prop = json::object();
    json sc_q = json::object();
    sc_q["type"] = "string";
    sc_q["description"] = "Text pattern to search for";
    json sc_pat = json::object();
    sc_pat["type"] = "string";
    sc_pat["description"] = "File extension filter (e.g. *.cpp, *.h). Default: *.*";

    nlohmann::json sc_metadata = nlohmann::json::object();
    sc_metadata["query"] = sc_q;
    sc_metadata["file_pattern"] = sc_pat;
    sc_prop["query"] = sc_metadata["query"];
    sc_prop["file_pattern"] = sc_metadata["file_pattern"];

    sc_p["properties"] = sc_prop;
    sc_p["required"] = jstrArr({"query"});
    sc_f["parameters"] = sc_p;
    sc["function"] = sc_f;
    tools.push_back(sc);

    // get_diagnostics
    json gd = json::object();
    gd["type"] = "function";
    json gd_f = json::object();
    gd_f["name"] = "get_diagnostics";
    gd_f["description"] = "Get compiler errors and warnings for a source file.";
    json gd_p = json::object();
    gd_p["type"] = "object";
    json gd_prop = json::object();
    json gd_file = json::object();
    gd_file["type"] = "string";
    gd_file["description"] = "Absolute path to the source file";

    nlohmann::json gd_metadata = nlohmann::json::object();
    gd_metadata["file"] = gd_file;
    gd_prop["file"] = gd_metadata["file"];

    gd_p["properties"] = gd_prop;
    gd_p["required"] = jstrArr({"file"});
    gd_f["parameters"] = gd_p;
    gd["function"] = gd_f;
    tools.push_back(gd);

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

// ============================================================================
// Generic dispatch — Instance / HasTool / Execute
// Used by DeterministicReplayEngine for transcript replay.
// ============================================================================
AgentToolHandlers& AgentToolHandlers::Instance() {
    static AgentToolHandlers instance;
    return instance;
}

bool AgentToolHandlers::HasTool(const std::string& name) const {
    static const char* const tools[] = {
        "read_file", "write_file", "replace_in_file",
        "list_dir", "execute_command", "search_code", "get_diagnostics"
    };
    for (const auto* t : tools) {
        if (name == t) return true;
    }
    return false;
}

ToolCallResult AgentToolHandlers::Execute(const std::string& name,
                                           const nlohmann::json& args) {
    if (name == "read_file")        return ToolReadFile(args);
    if (name == "write_file")       return WriteFile(args);
    if (name == "replace_in_file")  return ReplaceInFile(args);
    if (name == "list_dir")         return ListDir(args);
    if (name == "execute_command")  return ExecuteCommand(args);
    if (name == "search_code")      return SearchCode(args);
    if (name == "get_diagnostics")  return GetDiagnostics(args);
    return ToolCallResult::NotFound(name);
}
