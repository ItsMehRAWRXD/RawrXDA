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
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cctype>
#include <windows.h>

using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;
using RawrXD::Agent::ToolOutcome;
using RawrXD::Agent::ToolGuardrails;
using json = nlohmann::json;

static std::string ToLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}
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

// Very small semantic index: TF cosine over tokenized files
struct IndexedFile {
    std::string path;
    std::unordered_map<std::string, double> tf;
    double norm = 0.0;
};

bool IsLikelyBinary(const std::string& content) {
    if (content.empty()) return false;
    size_t nonPrintable = 0;
    size_t sample = std::min<size_t>(content.size(), 4096);
    for (size_t i = 0; i < sample; ++i) {
        unsigned char c = static_cast<unsigned char>(content[i]);
        if (c == 0) return true;
        if (c < 9 || (c > 13 && c < 32)) ++nonPrintable;
    }
    // Consider binary if more than 5% of sampled bytes are non-printable
    return (nonPrintable * 20) > sample;
}

std::vector<std::string> Tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

IndexedFile BuildIndexForFile(const std::string& path, size_t maxBytes) {
    IndexedFile f;
    f.path = path;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return f;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    if (content.size() > maxBytes && maxBytes > 0) {
        content = content.substr(0, maxBytes);
    }
    if (IsLikelyBinary(content)) return f;
    auto tokens = Tokenize(content);
    if (tokens.empty()) return f;
    for (const auto& t : tokens) {
        f.tf[t] += 1.0;
    }
    for (auto& kv : f.tf) {
        kv.second = kv.second / static_cast<double>(tokens.size());
        f.norm += kv.second * kv.second;
    }
    f.norm = std::sqrt(f.norm);
    return f;
}

double CosineScore(const IndexedFile& f, const std::unordered_map<std::string, double>& qtf, double qnorm) {
    if (f.norm == 0.0 || qnorm == 0.0) return 0.0;
    double dot = 0.0;
    for (const auto& kv : qtf) {
        auto it = f.tf.find(kv.first);
        if (it != f.tf.end()) dot += kv.second * it->second;
    }
    return dot / (f.norm * qnorm);
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
        res_metadata["command"] = command;
        result.metadata = res_metadata;
        return result;
    }

    // Bound output size to keep tool replies safe in UI surfaces
    const size_t kMaxOutput = std::max<size_t>(1024, s_guardrails.maxOutputCaptureBytes);
    bool truncated = false;
    size_t originalSize = output.size();
    if (output.size() > kMaxOutput) {
        output = output.substr(0, kMaxOutput) + "\n[truncated output]";
        truncated = true;
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["exit_code"] = exitCode;
    res_metadata["elapsed_ms"] = elapsed;
    res_metadata["truncated"] = truncated;
    res_metadata["captured_bytes"] = output.size();
    res_metadata["original_bytes"] = originalSize;
    res_metadata["command"] = command;

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
    if (query.empty()) {
        return ToolCallResult::Validation("search_code query cannot be empty");
    }
    std::string filePattern = args.value("file_pattern", "*.*");
    bool caseSensitive = args.value("case_sensitive", true);
    bool useRegex = args.value("regex", false);
    int maxResults = s_guardrails.maxSearchResults;
    if (args.contains("max_results") && args["max_results"].is_number()) {
        maxResults = std::clamp(args["max_results"].get<int>(), 1, 1000);
    }
    size_t contextBytes = static_cast<size_t>(std::max<int>(args.value("context_bytes", 200), 40));

    if (s_guardrails.allowedRoots.empty()) {
        return ToolCallResult::Error("No workspace root configured for search");
    }

    std::string searchRoot = s_guardrails.allowedRoots[0]; // Primary workspace
    if (args.contains("root") && args["root"].is_string()) {
        std::string candidate = NormalizePath(args["root"].get<std::string>());
        if (!IsPathAllowed(candidate)) {
            return ToolCallResult::Sandbox("Search root not allowed: " + candidate);
        }
        searchRoot = candidate;
    }
    size_t maxFileSize = s_guardrails.maxFileSizeBytes;
    int hitCount = 0;
    int scannedFiles = 0;
    int skippedBinary = 0;

    std::ostringstream results;

    try {
        for (auto it = fs::recursive_directory_iterator(searchRoot,
                 fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {

            if (hitCount >= maxResults) break;
            if (!it->is_regular_file()) continue;
            if (!IsPathAllowed(it->path().string())) continue;

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
            if (fsize == 0 || fsize > maxFileSize) continue;
            scannedFiles++;

            // Read and search
            std::ifstream file(it->path(), std::ios::binary);
            if (!file.is_open()) continue;

            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();
            if (IsLikelyBinary(content)) { skippedBinary++; continue; }

            if (useRegex) {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (!caseSensitive) flags |= std::regex::icase;
                std::regex re;
                try {
                    re = std::regex(query, flags);
                } catch (const std::exception& ex) {
                    return ToolCallResult::Validation(std::string("Invalid regex: ") + ex.what());
                }

                int lineNum = 1;
                size_t lineStart = 0;
                for (auto itMatch = std::sregex_iterator(content.begin(), content.end(), re);
                     itMatch != std::sregex_iterator() && hitCount < maxResults; ++itMatch) {
                    size_t found = static_cast<size_t>(itMatch->position());
                    // Count lines to found position
                    for (size_t i = lineStart; i < found; ++i) {
                        if (content[i] == '\n') ++lineNum;
                    }
                    lineStart = found;
                    size_t contextStart = content.rfind('\n', found);
                    contextStart = (contextStart == std::string::npos) ? 0 : contextStart + 1;
                    size_t contextEnd = content.find('\n', found);
                    if (contextEnd == std::string::npos) contextEnd = content.size();

                    std::string lineText = content.substr(contextStart,
                        std::min(contextEnd - contextStart, contextBytes));

                    std::string relPath = fs::relative(it->path(), searchRoot).string();
                    results << relPath << ":" << lineNum << ": " << lineText << "\n";

                    ++hitCount;
                }
                continue;
            }

            std::string lowerContent;
            std::string lowerQuery;
            if (!caseSensitive) {
                lowerContent = content;
                lowerQuery = query;
                std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            }

            size_t pos = 0;
            int lineNum = 1;
            size_t lineStart = 0;

            while (pos < content.size() && hitCount < maxResults) {
                // Track line numbers
                while (lineStart < pos) {
                    if (content[lineStart] == '\n') ++lineNum;
                    ++lineStart;
                }

                size_t found;
                found = caseSensitive ? content.find(query, pos) : lowerContent.find(lowerQuery, pos);
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
                    std::min(contextEnd - contextStart, contextBytes));

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
    res_metadata["scanned_files"] = scannedFiles;
    res_metadata["skipped_binary"] = skippedBinary;
    res_metadata["case_sensitive"] = caseSensitive;
    res_metadata["regex"] = useRegex;
    res_metadata["root"] = searchRoot;

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
// run_shell — Guarded alias of execute_command with allowlist enforcement
// ============================================================================
ToolCallResult AgentToolHandlers::RunShell(const json& args) {
    if (!args.contains("command") || !args["command"].is_string()) {
        return ToolCallResult::Validation("run_shell requires 'command' (string)");
    }
    std::string command = args["command"].get<std::string>();

    // Enforce allowlist if configured
    if (!s_guardrails.allowedCommands.empty()) {
        std::istringstream iss(command);
        std::string first;
        iss >> first;
        bool allowed = false;
        for (const auto& ac : s_guardrails.allowedCommands) {
            if (first == ac) { allowed = true; break; }
        }
        if (!allowed) {
            nlohmann::json meta = nlohmann::json::object();
            meta["command"] = command;
            meta["first_token"] = first;
            meta["policy"] = "allowedCommands";
            ToolCallResult res = ToolCallResult::Sandbox("Command not allowed by policy: " + first);
            res.metadata = meta;
            return res;
        }
    }
    return ExecuteCommand(args);
}

// ============================================================================
// semantic_search — Lightweight TF cosine search across workspace files
// ============================================================================
ToolCallResult AgentToolHandlers::SemanticSearch(const json& args) {
    if (!args.contains("query") || !args["query"].is_string()) {
        return ToolCallResult::Validation("semantic_search requires 'query' (string)");
    }
    if (s_guardrails.allowedRoots.empty()) {
        return ToolCallResult::Error("No workspace root configured for semantic search");
    }
    std::string query = args["query"].get<std::string>();
    std::string root = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string()) {
        root = NormalizePath(args["root"].get<std::string>());
        if (!IsPathAllowed(root)) {
            return ToolCallResult::Sandbox("Root not in allowlist: " + root);
        }
    }
    int topK = args.value("top_k", 5);
    if (topK <= 0) topK = 5;
    if (topK > 25) topK = 25;
    int maxFiles = s_guardrails.maxIndexFiles;
    bool includeNonCode = args.value("include_non_code", false);
    static const std::unordered_set<std::string> kCodeExt = {
        ".cpp",".c",".cc",".cxx",".h",".hpp",".hh",".hxx",
        ".cs",".java",".kt",".rs",".go",".ts",".tsx",".js",".jsx",
        ".py",".rb",".swift",".m",".mm",".scala",".sql",".json",
        ".yaml",".yml",".toml",".ini",".cfg",".cmake",".sh",".ps1"
    };

    auto qTokens = Tokenize(query);
    if (qTokens.empty()) {
        return ToolCallResult::Validation("semantic_search query produced no tokens");
    }
    std::unordered_map<std::string, double> qtf;
    for (const auto& t : qTokens) qtf[t] += 1.0;
    double qnorm = 0.0;
    for (auto& kv : qtf) {
        kv.second = kv.second / static_cast<double>(qTokens.size());
        qnorm += kv.second * kv.second;
    }
    qnorm = std::sqrt(qnorm);

    std::vector<IndexedFile> indexed;
    int scanned = 0;
    int skippedBinary = 0;
    try {
        for (auto it = fs::recursive_directory_iterator(root,
                 fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {
            if (scanned >= maxFiles) break;
            if (!it->is_regular_file()) continue;
            auto fsize = it->file_size();
            if (fsize == 0 || fsize > s_guardrails.maxFileSizeBytes) continue;
            std::string p = it->path().string();
            std::string ext = ToLowerCopy(it->path().extension().string());
            if (!includeNonCode && !ext.empty() && kCodeExt.find(ext) == kCodeExt.end()) continue;
            if (!IsPathAllowed(p)) continue;
            // Quick binary sniff to avoid heavy tokenization
            bool binaryFlag = false;
            try {
                std::ifstream sniff(p, std::ios::binary);
                if (sniff.is_open()) {
                    std::string head(512, '\0');
                    sniff.read(head.data(), static_cast<std::streamsize>(head.size()));
                    head.resize(static_cast<size_t>(sniff.gcount()));
                    binaryFlag = IsLikelyBinary(head);
                }
            } catch (...) { /* ignore */ }
            if (binaryFlag) { skippedBinary++; continue; }
            auto idx = BuildIndexForFile(p, s_guardrails.maxFileSizeBytes);
            if (!idx.tf.empty()) {
                indexed.push_back(std::move(idx));
                ++scanned;
            }
        }
    } catch (...) {
        // tolerate partial scan
    }

    if (indexed.empty()) {
        return ToolCallResult::Error("No indexable files found under " + root);
    }

    struct Scored { std::string path; double score; };
    std::vector<Scored> scores;
    for (const auto& f : indexed) {
        double s = CosineScore(f, qtf, qnorm);
        if (s > 0.0) scores.push_back({f.path, s});
    }
    std::sort(scores.begin(), scores.end(), [](const Scored& a, const Scored& b) {
        return a.score > b.score;
    });
    if ((int)scores.size() > topK) scores.resize(topK);

    nlohmann::json res = nlohmann::json::array();
    double scoreSum = 0.0;
    for (const auto& s : scores) {
        nlohmann::json row;
        row["path"] = fs::relative(s.path, root).string();
        row["score"] = s.score;
        scoreSum += s.score;
        // Optional snippet preview (first 240 chars)
        std::string snippet;
        try {
            std::ifstream preview(s.path, std::ios::binary);
            if (preview.is_open()) {
                std::string buf(240, '\0');
                preview.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                buf.resize(static_cast<size_t>(preview.gcount()));
                snippet = buf;
            }
        } catch (...) { /* ignore */ }
        if (!snippet.empty()) {
            row["preview"] = snippet;
        }
        res.push_back(row);
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["scanned_files"] = scanned;
    meta["returned"] = res.size();
    meta["root"] = root;
    meta["top_k"] = topK;
    meta["skipped_binary"] = skippedBinary;
    if (!scores.empty()) {
        meta["avg_score"] = scoreSum / static_cast<double>(scores.size());
    }
    return ToolCallResult::Ok(res.dump(2), meta);
}

// ============================================================================
// mention_lookup — Symbol-aware alias over semantic_search
// ============================================================================
ToolCallResult AgentToolHandlers::MentionLookup(const json& args) {
    json copy = args;
    if (!copy.contains("query") && copy.contains("symbol")) {
        copy["query"] = copy["symbol"];
    }
    if (!copy.contains("top_k")) {
        copy["top_k"] = 3;
    }
    // Clamp to avoid huge responses
    if (copy.contains("top_k") && copy["top_k"].is_number()) {
        int tk = std::clamp(copy["top_k"].get<int>(), 1, 10);
        copy["top_k"] = tk;
    }
    // Favor current workspace root when not provided
    if (!copy.contains("root") && !s_guardrails.allowedRoots.empty()) {
        copy["root"] = s_guardrails.allowedRoots[0];
    }
    if (args.contains("include_non_code")) {
        copy["include_non_code"] = args["include_non_code"];
    }
    return SemanticSearch(copy);
}

// ============================================================================
// next_edit_hint — Heuristic “next edit” suggestion from context
// ============================================================================
ToolCallResult AgentToolHandlers::NextEditHint(const json& args) {
    if (!args.contains("context") || !args["context"].is_string()) {
        return ToolCallResult::Validation("next_edit_hint requires 'context' (string)");
    }
    std::string ctx = args["context"].get<std::string>();
    std::vector<std::string> hints;
    std::unordered_set<std::string> seen;
    auto addHint = [&](const std::string& h) {
        if (seen.insert(h).second && hints.size() < 3) hints.push_back(h);
    };
    if (ctx.find("TODO") != std::string::npos) {
        addHint("Address the TODO with a small, testable change and add a unit test.");
    }
    if (ctx.find("function") != std::string::npos || ctx.find("def ") != std::string::npos) {
        addHint("Add docstring/comments and edge-case handling (empty input, null pointers).");
    }
    if (ctx.find("class") != std::string::npos || ctx.find("struct") != std::string::npos) {
        addHint("Ensure constructors initialize all fields and add default move/clone semantics if needed.");
    }
    if (hints.empty()) {
        addHint("Extract helper functions to simplify logic and add assertions for invariants.");
    }
    nlohmann::json meta = nlohmann::json::object();
    meta["count"] = hints.size();
    meta["context_length"] = ctx.size();
    return ToolCallResult::Ok(nlohmann::json(hints).dump(2), meta);
}

// ============================================================================
// propose_multifile_edits — Generate a structured plan for multiple files
// ============================================================================
ToolCallResult AgentToolHandlers::ProposeMultiFileEdits(const json& args) {
    if (!args.contains("files") || !args["files"].is_array()) {
        return ToolCallResult::Validation("propose_multifile_edits requires 'files' (array)");
    }
    std::string instruction = args.value("instruction", "Apply requested change across files.");
    nlohmann::json plans = nlohmann::json::array();
    size_t capped = 0;
    size_t skipped = 0;
    for (const auto& f : args["files"]) {
        if (!f.is_string()) { skipped++; continue; }
        std::string path = NormalizePath(f.get<std::string>());
        if (!IsPathAllowed(path) || !fs::exists(path)) { skipped++; continue; }
        if (plans.size() >= 20) { capped++; continue; }
        nlohmann::json step = nlohmann::json::object();
        step["file"] = path;
        step["plan"] = instruction + " (review, edit, validate)";
        plans.push_back(step);
    }
    if (plans.empty()) {
        return ToolCallResult::Error("No valid files to plan edits for.");
    }
    nlohmann::json meta = nlohmann::json::object();
    meta["files"] = plans.size();
    meta["skipped"] = skipped;
    meta["capped"] = capped;
    return ToolCallResult::Ok(plans.dump(2), meta);
}

// ============================================================================
// load_rules — Parse a .rawrrules file to seed system instructions
// ============================================================================
ToolCallResult AgentToolHandlers::LoadRules(const json& args) {
    std::string path;
    bool hasInline = args.contains("content") && args["content"].is_string();
    if (args.contains("path") && args["path"].is_string()) {
        path = NormalizePath(args["path"].get<std::string>());
    } else if (!s_guardrails.allowedRoots.empty()) {
        path = NormalizePath(s_guardrails.allowedRoots[0] + "/.rawrrules");
    } else {
        return ToolCallResult::Error("No workspace root configured to locate .rawrrules");
    }
    if (!IsPathAllowed(path)) {
        return ToolCallResult::Sandbox("Rules file not in allowlist: " + path);
    }
    if (!hasInline && !fs::exists(path)) {
        return ToolCallResult::Error("Rules file not found: " + path);
    }
    if (!path.empty()) {
        std::string ext = ToLowerCopy(fs::path(path).extension().string());
        if (!ext.empty() && ext != ".rawrrules" && ext != ".rules" && ext != ".txt") {
            return ToolCallResult::Validation("Rules path must be .rawrrules/.rules/.txt");
        }
    }

    nlohmann::json rules = nlohmann::json::object();
    auto parseLine = [&](const std::string& line) {
        if (line.empty() || line[0] == '#') return;
        auto pos = line.find(':');
        if (pos == std::string::npos) return;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (!key.empty()) rules[key] = val;
    };

    size_t lineCount = 0;
    std::string concat;
    if (hasInline) {
        std::istringstream ss(args["content"].get<std::string>());
        std::string line;
        while (std::getline(ss, line)) {
            parseLine(line);
            concat += line;
            ++lineCount;
        }
    } else {
        std::ifstream in(path);
        if (!in.is_open()) {
            return ToolCallResult::Error("Failed to open rules file: " + path);
        }
        std::string line;
        while (std::getline(in, line)) {
            parseLine(line);
            concat += line;
            ++lineCount;
        }
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["entries"] = rules.size();
    meta["source"] = hasInline ? "inline" : path;
    meta["lines"] = lineCount;
    meta["fingerprint"] = std::hash<std::string>{}(concat);
    if (rules.empty()) {
        ToolCallResult res = ToolCallResult::Validation("Rules parsed but no entries found");
        res.metadata = meta;
        return res;
    }
    return ToolCallResult::Ok(rules.dump(2), meta);
}

// ============================================================================
// plan_tasks — Lightweight deterministic plan generator
// ============================================================================
ToolCallResult AgentToolHandlers::PlanTasks(const json& args) {
    if (!args.contains("goal") || !args["goal"].is_string()) {
        return ToolCallResult::Validation("plan_tasks requires 'goal' (string)");
    }
    std::string goal = args["goal"].get<std::string>();
    int maxSteps = std::clamp(args.value("max_steps", 6), 3, 10);
    std::string deadline = args.value("deadline", "");
    std::string owner = args.value("owner", "");
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back("Understand goal: " + goal);
    plan.push_back("Search and gather context (files, rules, diagnostics).");
    plan.push_back("Apply changes incrementally with tests/diagnostics.");
    if (plan.size() < static_cast<size_t>(maxSteps)) {
        plan.push_back("Validate with unit/smoke tests and capture artifacts.");
    }
    if (plan.size() < static_cast<size_t>(maxSteps)) {
        plan.push_back("Summarize changes, risks, and follow-ups.");
    }
    if (plan.is_array() && static_cast<int>(plan.size()) > maxSteps) {
        json trimmed = json::array();
        for (int i = 0; i < maxSteps && i < static_cast<int>(plan.size()); ++i) {
            trimmed.push_back(plan[i]);
        }
        plan = std::move(trimmed);
    }
    nlohmann::json meta = nlohmann::json::object();
    meta["steps"] = plan.size();
    meta["goal_length"] = goal.size();
    meta["max_steps"] = maxSteps;
    if (!deadline.empty()) meta["deadline"] = deadline;
    if (!owner.empty()) meta["owner"] = owner;
    return ToolCallResult::Ok(plan.dump(2), meta);
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
    json sc_cs = json::object();
    sc_cs["type"] = "boolean";
    sc_cs["description"] = "Case sensitive search (default true)";
    json sc_rx = json::object();
    sc_rx["type"] = "boolean";
    sc_rx["description"] = "Treat query as regex (ECMAScript)";
    json sc_root = json::object();
    sc_root["type"] = "string";
    sc_root["description"] = "Optional search root (must be allowlisted)";

    nlohmann::json sc_metadata = nlohmann::json::object();
    sc_metadata["query"] = sc_q;
    sc_metadata["file_pattern"] = sc_pat;
    sc_metadata["case_sensitive"] = sc_cs;
    sc_metadata["regex"] = sc_rx;
    sc_metadata["root"] = sc_root;
    sc_prop["query"] = sc_metadata["query"];
    sc_prop["file_pattern"] = sc_metadata["file_pattern"];
    sc_prop["case_sensitive"] = sc_metadata["case_sensitive"];
    sc_prop["regex"] = sc_metadata["regex"];
    sc_prop["root"] = sc_metadata["root"];

    sc_p["properties"] = sc_prop;
    sc_p["required"] = jstrArr({"query"});
    sc_f["parameters"] = sc_p;
    sc["function"] = sc_f;
    tools.push_back(sc);

    // semantic_search
    json ss = json::object();
    ss["type"] = "function";
    json ss_f = json::object();
    ss_f["name"] = "semantic_search";
    ss_f["description"] = "Semantic search over workspace files using TF cosine; returns top-k file matches.";
    json ss_p = json::object();
    ss_p["type"] = "object";
    json ss_prop = json::object();
    json ss_q = json::object();
    ss_q["type"] = "string";
    ss_q["description"] = "Query text to match semantically";
    json ss_root = json::object();
    ss_root["type"] = "string";
    ss_root["description"] = "Optional root path (defaults to primary workspace)";
    json ss_topk = json::object();
    ss_topk["type"] = "integer";
    ss_topk["description"] = "Number of results to return (default 5)";
    json ss_inc = json::object();
    ss_inc["type"] = "boolean";
    ss_inc["description"] = "Include non-code files (default false)";
    ss_prop["query"] = ss_q;
    ss_prop["root"] = ss_root;
    ss_prop["top_k"] = ss_topk;
    ss_prop["include_non_code"] = ss_inc;
    ss_p["properties"] = ss_prop;
    ss_p["required"] = jstrArr({"query"});
    ss_f["parameters"] = ss_p;
    ss["function"] = ss_f;
    tools.push_back(ss);

    // mention_lookup
    json ml = json::object();
    ml["type"] = "function";
    json ml_f = json::object();
    ml_f["name"] = "mention_lookup";
    ml_f["description"] = "Find files relevant to a symbol or mention (alias of semantic_search).";
    json ml_p = json::object();
    ml_p["type"] = "object";
    json ml_prop = json::object();
    json ml_sym = json::object();
    ml_sym["type"] = "string";
    ml_sym["description"] = "Symbol or mention to resolve";
    json ml_root = json::object();
    ml_root["type"] = "string";
    ml_root["description"] = "Optional search root (allowlisted)";
    json ml_topk = json::object();
    ml_topk["type"] = "integer";
    ml_topk["description"] = "Number of results (default 3)";
    json ml_inc = json::object();
    ml_inc["type"] = "boolean";
    ml_inc["description"] = "Include non-code files";
    ml_prop["symbol"] = ml_sym;
    ml_prop["query"] = ml_sym;
    ml_prop["root"] = ml_root;
    ml_prop["top_k"] = ml_topk;
    ml_prop["include_non_code"] = ml_inc;
    ml_p["properties"] = ml_prop;
    ml_p["required"] = jstrArr({"symbol"});
    ml_f["parameters"] = ml_p;
    ml["function"] = ml_f;
    tools.push_back(ml);

    // next_edit_hint
    json neh = json::object();
    neh["type"] = "function";
    json neh_f = json::object();
    neh_f["name"] = "next_edit_hint";
    neh_f["description"] = "Suggest the next small edit based on current context.";
    json neh_p = json::object();
    neh_p["type"] = "object";
    json neh_prop = json::object();
    json neh_ctx = json::object();
    neh_ctx["type"] = "string";
    neh_ctx["description"] = "Snippet of current code or notes";
    neh_prop["context"] = neh_ctx;
    neh_p["properties"] = neh_prop;
    neh_p["required"] = jstrArr({"context"});
    neh_f["parameters"] = neh_p;
    neh["function"] = neh_f;
    tools.push_back(neh);

    // propose_multifile_edits
    json pme = json::object();
    pme["type"] = "function";
    json pme_f = json::object();
    pme_f["name"] = "propose_multifile_edits";
    pme_f["description"] = "Produce a structured plan for edits across multiple files.";
    json pme_p = json::object();
    pme_p["type"] = "object";
    json pme_prop = json::object();
    json pme_files = json::object();
    pme_files["type"] = "array";
    pme_files["description"] = "Array of absolute file paths to include in the plan";
    json pme_instr = json::object();
    pme_instr["type"] = "string";
    pme_instr["description"] = "Instruction/goal to apply across files";
    pme_prop["files"] = pme_files;
    pme_prop["instruction"] = pme_instr;
    pme_p["properties"] = pme_prop;
    pme_p["required"] = jstrArr({"files"});
    pme_f["parameters"] = pme_p;
    pme["function"] = pme_f;
    tools.push_back(pme);

    // load_rules
    json lr = json::object();
    lr["type"] = "function";
    json lr_f = json::object();
    lr_f["name"] = "load_rules";
    lr_f["description"] = "Load .rawrrules and return parsed key/value rules.";
    json lr_p = json::object();
    lr_p["type"] = "object";
    json lr_prop = json::object();
    json lr_path = json::object();
    lr_path["type"] = "string";
    lr_path["description"] = "Optional path to rules file (default: workspace/.rawrrules)";
    json lr_content = json::object();
    lr_content["type"] = "string";
    lr_content["description"] = "Inline rules content (overrides path if provided)";
    lr_prop["path"] = lr_path;
    lr_prop["content"] = lr_content;
    lr_p["properties"] = lr_prop;
    lr_f["parameters"] = lr_p;
    lr["function"] = lr_f;
    tools.push_back(lr);

    // plan_tasks
    json pt = json::object();
    pt["type"] = "function";
    json pt_f = json::object();
    pt_f["name"] = "plan_tasks";
    pt_f["description"] = "Generate a short, deterministic plan for a goal.";
    json pt_p = json::object();
    pt_p["type"] = "object";
    json pt_prop = json::object();
    json pt_goal = json::object();
    pt_goal["type"] = "string";
    pt_goal["description"] = "Goal or task to plan";
    json pt_max = json::object();
    pt_max["type"] = "integer";
    pt_max["description"] = "Maximum steps (3-10)";
    json pt_deadline = json::object();
    pt_deadline["type"] = "string";
    pt_deadline["description"] = "Optional deadline or due-by note";
    pt_prop["goal"] = pt_goal;
    pt_prop["max_steps"] = pt_max;
    pt_prop["deadline"] = pt_deadline;
    pt_p["properties"] = pt_prop;
    pt_p["required"] = jstrArr({"goal"});
    pt_f["parameters"] = pt_p;
    pt["function"] = pt_f;
    tools.push_back(pt);

    // run_shell
    json rs = json::object();
    rs["type"] = "function";
    json rs_f = json::object();
    rs_f["name"] = "run_shell";
    rs_f["description"] = "Run a shell command (allowlisted) with timeout.";
    json rs_p = json::object();
    rs_p["type"] = "object";
    json rs_prop = json::object();
    json rs_cmd = json::object();
    rs_cmd["type"] = "string";
    rs_cmd["description"] = "Command to execute";
    json rs_timeout = json::object();
    rs_timeout["type"] = "integer";
    rs_timeout["description"] = "Timeout in milliseconds (default guardrail value)";
    rs_prop["command"] = rs_cmd;
    rs_prop["timeout"] = rs_timeout;
    rs_p["properties"] = rs_prop;
    rs_p["required"] = jstrArr({"command"});
    rs_f["parameters"] = rs_p;
    rs["function"] = rs_f;
    tools.push_back(rs);

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
        "list_dir", "list_directory", "execute_command", "run_shell", "search_code", "semantic_search",
        "mention_lookup", "next_edit_hint", "propose_multifile_edits",
        "load_rules", "plan_tasks", "get_diagnostics"
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
    if (name == "list_dir" || name == "list_directory") return ListDir(args);
    if (name == "execute_command")  return ExecuteCommand(args);
    if (name == "run_shell")        return RunShell(args);
    if (name == "search_code")      return SearchCode(args);
    if (name == "semantic_search")  return SemanticSearch(args);
    if (name == "mention_lookup")   return MentionLookup(args);
    if (name == "next_edit_hint")   return NextEditHint(args);
    if (name == "propose_multifile_edits") return ProposeMultiFileEdits(args);
    if (name == "load_rules")       return LoadRules(args);
    if (name == "plan_tasks")       return PlanTasks(args);
    if (name == "get_diagnostics")  return GetDiagnostics(args);
    return ToolCallResult::NotFound(name);
}
