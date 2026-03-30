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
#include "../agent_history.h"
#include "agentic_workspace_analyzer.hpp"
#include "DiskRecoveryAgent.h"
#include "../core/rawrxd_subsystem_api.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../dae/tool_abi.h"
#include "../dae/trace_log.h"
#include "../dae/shadow_fs.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;
using RawrXD::Agent::ToolGuardrails;
using json = nlohmann::json;

extern "C" int AgentWorkflow_Resume(const char* checkpointPath);

static std::string ToLowerCopy(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
namespace fs = std::filesystem;

// ============================================================================
// Static state
// ============================================================================
ToolGuardrails AgentToolHandlers::s_guardrails;
static AgentHistoryRecorder* s_historyRecorder = nullptr;

namespace
{

std::wstring ToWide(const std::string& s)
{
    if (s.empty())
        return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

std::string ToUtf8(const std::wstring& s)
{
    if (s.empty())
        return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}

int CountLines(const std::string& text)
{
    int count = 0;
    for (char c : text)
    {
        if (c == '\n')
            ++count;
    }
    return count + (text.empty() ? 0 : 1);
}

bool WriteFileAtomic(const std::string& path, const std::string& content, std::string& error)
{
    std::string tempPath =
        path + ".tmp." +
        std::to_string(std::hash<std::string>{}(
            path + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())));
    std::ofstream tmp(tempPath, std::ios::binary | std::ios::trunc);
    if (!tmp.is_open())
    {
        error = "Failed to open temporary file " + tempPath;
        return false;
    }
    tmp.write(content.data(), content.size());
    tmp.close();
    if (!tmp.good())
    {
        error = "Failed to write temporary file " + tempPath;
        fs::remove(tempPath);
        return false;
    }
    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec)
    {
        error = "Failed to rename tmp file " + tempPath + " to " + path + ": " + ec.message();
        fs::remove(tempPath);
        return false;
    }
    return true;
}

struct PatchJournalEntry
{
    std::string path;
    std::string before;
    std::string after;
    bool applied{false};
};

class PatchJournal
{
  public:
    void record(const std::string& path, const std::string& before, const std::string& after)
    {
        entries.emplace_back(PatchJournalEntry{path, before, after, false});
    }
    bool rollback()
    {
        bool allOk = true;
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            std::error_code ec;
            if (it->before.empty())
            {
                fs::remove(it->path, ec);
                if (ec)
                {
                    allOk = false;
                }
            }
            else
            {
                std::ofstream file(it->path, std::ios::binary | std::ios::trunc);
                if (!file.is_open())
                {
                    allOk = false;
                    continue;
                }
                file.write(it->before.data(), it->before.size());
                file.close();
                it->applied = !file.fail();
                if (file.fail())
                    allOk = false;
            }
        }
        entries.clear();
        return allOk;
    }
    nlohmann::json toJson() const
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : entries)
        {
            nlohmann::json item;
            item["path"] = e.path;
            item["applied"] = e.applied;
            item["before_len"] = e.before.size();
            item["after_len"] = e.after.size();
            arr.push_back(item);
        }
        return arr;
    }

  private:
    std::vector<PatchJournalEntry> entries;
};

static PatchJournal s_patchJournal;

struct SemanticSearchCacheEntry
{
    std::string body;
    nlohmann::json metadata;
    std::chrono::steady_clock::time_point timestamp;
};
static std::unordered_map<std::string, SemanticSearchCacheEntry> s_semanticSearchCache;
static std::mutex s_semanticSearchCacheMutex;

// Shared iteration progress state for long-running model/agent work.
static std::mutex s_iterationStatusMutex;
static bool s_iterationBusy = false;
static int s_iterationCurrent = 0;
static int s_iterationTotal = 0;
static std::string s_iterationPhase = "idle";
static std::string s_iterationMessage;

// Very small semantic index: TF cosine over tokenized files
struct IndexedFile
{
    std::string path;
    std::unordered_map<std::string, double> tf;
    double norm = 0.0;
};

bool IsLikelyBinary(const std::string& content)
{
    if (content.empty())
        return false;
    size_t nonPrintable = 0;
    size_t sample = std::min<size_t>(content.size(), 4096);
    for (size_t i = 0; i < sample; ++i)
    {
        unsigned char c = static_cast<unsigned char>(content[i]);
        if (c == 0)
            return true;
        if (c < 9 || (c > 13 && c < 32))
            ++nonPrintable;
    }
    // Consider binary if more than 5% of sampled bytes are non-printable
    return (nonPrintable * 20) > sample;
}

std::vector<std::string> Tokenize(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : text)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
        {
            cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        else if (!cur.empty())
        {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty())
        tokens.push_back(cur);
    return tokens;
}

IndexedFile BuildIndexForFile(const std::string& path, size_t maxBytes)
{
    IndexedFile f;
    f.path = path;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return f;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    if (content.size() > maxBytes && maxBytes > 0)
    {
        content = content.substr(0, maxBytes);
    }
    if (IsLikelyBinary(content))
        return f;
    auto tokens = Tokenize(content);
    if (tokens.empty())
        return f;
    for (const auto& t : tokens)
    {
        f.tf[t] += 1.0;
    }
    for (auto& kv : f.tf)
    {
        kv.second = kv.second / static_cast<double>(tokens.size());
        f.norm += kv.second * kv.second;
    }
    f.norm = std::sqrt(f.norm);
    return f;
}

double CosineScore(const IndexedFile& f, const std::unordered_map<std::string, double>& qtf, double qnorm)
{
    if (f.norm == 0.0 || qnorm == 0.0)
        return 0.0;
    double dot = 0.0;
    for (const auto& kv : qtf)
    {
        auto it = f.tf.find(kv.first);
        if (it != f.tf.end())
            dot += kv.second * it->second;
    }
    return dot / (f.norm * qnorm);
}

std::string EscapeRegex(const std::string& s)
{
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s)
    {
        switch (c)
        {
            case '.':
            case '^':
            case '$':
            case '|':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '*':
            case '+':
            case '?':
            case '\\':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::regex GlobToRegex(const std::string& glob, bool caseSensitive, std::string& error)
{
    std::string rx;
    rx.reserve(glob.size() * 2);
    rx.push_back('^');
    for (size_t i = 0; i < glob.size(); ++i)
    {
        char c = glob[i];
        if (c == '*')
        {
            rx.append(".*");
        }
        else if (c == '?')
        {
            rx.push_back('.');
        }
        else
        {
            rx.append(EscapeRegex(std::string(1, c)));
        }
    }
    rx.push_back('$');
    try
    {
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (!caseSensitive)
            flags |= std::regex::icase;
        return std::regex(rx, flags);
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return std::regex();
    }
}

}  // anonymous namespace

// ============================================================================
// Guardrails
// ============================================================================

void AgentToolHandlers::SetGuardrails(const ToolGuardrails& guards)
{
    s_guardrails = guards;
}

const ToolGuardrails& AgentToolHandlers::GetGuardrails()
{
    return s_guardrails;
}

void AgentToolHandlers::SetHistoryRecorder(AgentHistoryRecorder* recorder)
{
    s_historyRecorder = recorder;
}

// ============================================================================
// Path validation
// ============================================================================

std::string AgentToolHandlers::NormalizePath(const std::string& path)
{
    try
    {
        return fs::weakly_canonical(path).string();
    }
    catch (...)
    {
        return path;
    }
}

bool AgentToolHandlers::IsPathAllowed(const std::string& path)
{
    std::string normalized = NormalizePath(path);

    // Must be under at least one allowed root
    if (s_guardrails.allowedRoots.empty())
        return true;  // No restrictions configured

    for (const auto& root : s_guardrails.allowedRoots)
    {
        std::string normRoot = NormalizePath(root);
        if (normalized.find(normRoot) == 0)
        {
            // Check deny patterns
            if (!MatchesDenyPattern(normalized))
            {
                return true;
            }
        }
    }
    return false;
}

bool AgentToolHandlers::MatchesDenyPattern(const std::string& path)
{
    for (const auto& pattern : s_guardrails.denyPatterns)
    {
        // Simple suffix matching for deny patterns like "*.exe"
        if (pattern.size() > 1 && pattern[0] == '*')
        {
            std::string suffix = pattern.substr(1);
            if (path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

std::string AgentToolHandlers::CreateBackup(const std::string& path)
{
    std::string backupPath = path + ".agent_bak";
    try
    {
        fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing);
    }
    catch (const std::exception& ex)
    {
        return std::string("Backup failed: ") + ex.what();
    }
    return "";
}

// ============================================================================
// read_file — Read file contents
// ============================================================================

ToolCallResult AgentToolHandlers::ToolReadFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("read_file requires 'path' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    if (!fs::exists(path))
    {
        return ToolCallResult::Error("File not found: " + path);
    }

    auto fileSize = fs::file_size(path);
    if (fileSize > s_guardrails.maxFileSizeBytes)
    {
        return ToolCallResult::Error("File too large: " + std::to_string(fileSize) + " bytes (max " +
                                     std::to_string(s_guardrails.maxFileSizeBytes) + ")");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
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

ToolCallResult AgentToolHandlers::WriteFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("write_file requires 'path' (string)");
    }
    if (!args.contains("content") || !args["content"].is_string())
    {
        return ToolCallResult::Validation("write_file requires 'content' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    std::string content = args["content"].get<std::string>();

    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    if (content.size() > s_guardrails.maxFileSizeBytes)
    {
        return ToolCallResult::Error("Content too large: " + std::to_string(content.size()) + " bytes");
    }

    // Create backup if file exists and guardrails require it
    bool existed = fs::exists(path);
    std::string beforeContent;
    if (existed)
    {
        std::ifstream read(path, std::ios::binary);
        if (read.is_open())
        {
            std::ostringstream ss;
            ss << read.rdbuf();
            beforeContent = ss.str();
        }
        if (s_guardrails.requireBackupOnWrite)
        {
            std::string backupError = CreateBackup(path);
            if (!backupError.empty())
            {
                return ToolCallResult::Error(backupError);
            }
        }
    }

    // Ensure parent directories exist
    try
    {
        fs::path parentDir = fs::path(path).parent_path();
        if (!parentDir.empty() && !fs::exists(parentDir))
        {
            fs::create_directories(parentDir);
        }
    }
    catch (const std::exception& ex)
    {
        return ToolCallResult::Error(std::string("Cannot create directories: ") + ex.what());
    }

    // Record patch journal before applying
    s_patchJournal.record(path, beforeContent, content);

    // Write atomically with rollback support
    std::string writeErr;
    if (!WriteFileAtomic(path, content, writeErr))
    {
        s_patchJournal.rollback();
        return ToolCallResult::Error(writeErr);
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["lines"] = CountLines(content);
    res_metadata["size_bytes"] = content.size();
    res_metadata["created"] = !existed;

    ToolCallResult result =
        ToolCallResult::Ok(existed ? "File overwritten successfully" : "File created successfully", res_metadata);
    result.filePath = path;
    result.bytesWritten = content.size();
    result.linesAffected = CountLines(content);
    return result;
}

// ============================================================================
// replace_in_file — Search+replace text block
// ============================================================================

ToolCallResult AgentToolHandlers::ReplaceInFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("replace_in_file requires 'path' (string)");
    }
    if (!args.contains("old_string") || !args["old_string"].is_string())
    {
        return ToolCallResult::Validation("replace_in_file requires 'old_string' (string)");
    }
    if (!args.contains("new_string") || !args["new_string"].is_string())
    {
        return ToolCallResult::Validation("replace_in_file requires 'new_string' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    std::string oldStr = args["old_string"].get<std::string>();
    std::string newStr = args["new_string"].get<std::string>();

    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path))
    {
        return ToolCallResult::Error("File not found: " + path);
    }

    // Read file
    std::ifstream inFile(path, std::ios::binary);
    if (!inFile.is_open())
    {
        return ToolCallResult::Error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << inFile.rdbuf();
    inFile.close();
    std::string content = ss.str();

    // Find the old string
    size_t pos = content.find(oldStr);
    if (pos == std::string::npos)
    {
        return ToolCallResult::Error("old_string not found in file. "
                                     "Ensure you're using the exact text including whitespace.");
    }

    // Check for multiple matches (warn but still replace first)
    size_t secondMatch = content.find(oldStr, pos + oldStr.size());
    bool multipleMatches = (secondMatch != std::string::npos);

    // Create backup
    if (s_guardrails.requireBackupOnWrite)
    {
        std::string backupError = CreateBackup(path);
        if (!backupError.empty())
        {
            return ToolCallResult::Error(backupError);
        }
    }

    // Perform replacement
    std::string newContent = content.substr(0, pos) + newStr + content.substr(pos + oldStr.size());

    // Record patch journal before applying
    s_patchJournal.record(path, content, newContent);

    // Write back atomically
    std::string writeErr;
    if (!WriteFileAtomic(path, newContent, writeErr))
    {
        s_patchJournal.rollback();
        return ToolCallResult::Error(writeErr);
    }

    int linesChanged = CountLines(newStr) - CountLines(oldStr);
    std::string msg =
        "Replaced " + std::to_string(oldStr.size()) + " bytes with " + std::to_string(newStr.size()) + " bytes";
    if (multipleMatches)
    {
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

ToolCallResult AgentToolHandlers::ListDir(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("list_dir requires 'path' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path) || !fs::is_directory(path))
    {
        return ToolCallResult::Error("Directory not found: " + path);
    }

    std::ostringstream listing;
    int fileCount = 0, dirCount = 0;

    try
    {
        for (const auto& entry : fs::directory_iterator(path))
        {
            std::string name = entry.path().filename().string();
            if (entry.is_directory())
            {
                listing << name << "/\n";
                ++dirCount;
            }
            else
            {
                auto size = entry.file_size();
                listing << name << " (" << size << " bytes)\n";
                ++fileCount;
            }
        }
    }
    catch (const std::exception& ex)
    {
        return ToolCallResult::Error(std::string("Directory listing failed: ") + ex.what());
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["files"] = fileCount;
    res_metadata["directories"] = dirCount;
    res_metadata["total"] = fileCount + dirCount;

    return ToolCallResult::Ok(listing.str(), res_metadata);
}

// ============================================================================
// delete_file — Remove a file from workspace
// ============================================================================
ToolCallResult AgentToolHandlers::DeleteFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("delete_file requires 'path' (string)");
    }

    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path))
    {
        return ToolCallResult::Error("Path not found: " + path);
    }
    if (!fs::is_regular_file(path))
    {
        return ToolCallResult::Validation("delete_file only supports regular files");
    }

    std::error_code ec;
    const bool removed = fs::remove(path, ec);
    if (!removed || ec)
    {
        return ToolCallResult::Error("delete_file failed: " + (ec ? ec.message() : std::string("unknown error")));
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["path"] = path;
    meta["deleted"] = true;
    return ToolCallResult::Ok("File deleted", meta);
}

// ============================================================================
// rename_file / move_file — Move or rename a file
// ============================================================================
ToolCallResult AgentToolHandlers::RenameFile(const json& args)
{
    std::string source;
    std::string destination;
    if (args.contains("source") && args["source"].is_string())
        source = args["source"].get<std::string>();
    if (args.contains("path") && args["path"].is_string() && source.empty())
        source = args["path"].get<std::string>();
    if (args.contains("destination") && args["destination"].is_string())
        destination = args["destination"].get<std::string>();

    if (source.empty() || destination.empty())
    {
        return ToolCallResult::Validation("rename_file requires 'source'/'path' and 'destination'");
    }

    source = NormalizePath(source);
    destination = NormalizePath(destination);
    if (!IsPathAllowed(source) || !IsPathAllowed(destination))
    {
        return ToolCallResult::Sandbox("Source or destination is not in workspace allowlist");
    }
    if (!fs::exists(source))
    {
        return ToolCallResult::Error("Source path not found: " + source);
    }

    std::error_code ec;
    fs::rename(source, destination, ec);
    if (ec)
    {
        return ToolCallResult::Error("rename_file failed: " + ec.message());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["source"] = source;
    meta["destination"] = destination;
    meta["moved"] = true;
    return ToolCallResult::Ok("File moved", meta);
}

// ============================================================================
// copy_file — Copy file to destination
// ============================================================================
ToolCallResult AgentToolHandlers::CopyFile(const json& args)
{
    std::string source;
    std::string destination;
    if (args.contains("source") && args["source"].is_string())
        source = args["source"].get<std::string>();
    if (args.contains("path") && args["path"].is_string() && source.empty())
        source = args["path"].get<std::string>();
    if (args.contains("destination") && args["destination"].is_string())
        destination = args["destination"].get<std::string>();
    const bool overwrite = args.value("overwrite", true);

    if (source.empty() || destination.empty())
    {
        return ToolCallResult::Validation("copy_file requires 'source'/'path' and 'destination'");
    }

    source = NormalizePath(source);
    destination = NormalizePath(destination);
    if (!IsPathAllowed(source) || !IsPathAllowed(destination))
    {
        return ToolCallResult::Sandbox("Source or destination is not in workspace allowlist");
    }
    if (!fs::exists(source) || !fs::is_regular_file(source))
    {
        return ToolCallResult::Error("Source file not found: " + source);
    }

    std::error_code ec;
    fs::copy_file(source, destination, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none, ec);
    if (ec)
    {
        return ToolCallResult::Error("copy_file failed: " + ec.message());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["source"] = source;
    meta["destination"] = destination;
    meta["overwrite"] = overwrite;
    return ToolCallResult::Ok("File copied", meta);
}

// ============================================================================
// mkdir / create_directory — Create directory tree
// ============================================================================
ToolCallResult AgentToolHandlers::MakeDirectory(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("mkdir requires 'path' (string)");
    }
    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    std::error_code ec;
    const bool created = fs::create_directories(path, ec);
    if (ec)
    {
        return ToolCallResult::Error("mkdir failed: " + ec.message());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["path"] = path;
    meta["created"] = created;
    meta["already_exists"] = fs::exists(path);
    return ToolCallResult::Ok(created ? "Directory created" : "Directory already exists", meta);
}

// ============================================================================
// stat_file / get_file_info — Return basic metadata
// ============================================================================
ToolCallResult AgentToolHandlers::StatFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("stat_file requires 'path' (string)");
    }
    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    std::error_code ec;
    const fs::file_status st = fs::status(path, ec);
    if (ec || !fs::exists(st))
    {
        return ToolCallResult::Error("Path not found: " + path);
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["path"] = path;
    meta["exists"] = true;
    meta["is_file"] = fs::is_regular_file(st);
    meta["is_directory"] = fs::is_directory(st);
    if (fs::is_regular_file(st))
    {
        meta["size_bytes"] = fs::file_size(path, ec);
    }
    return ToolCallResult::Ok(meta.dump(2), meta);
}

// ============================================================================
// git_status — Run git status in workspace root
// ============================================================================
ToolCallResult AgentToolHandlers::GitStatus(const json& args)
{
    std::string root;
    if (args.contains("root") && args["root"].is_string())
    {
        root = NormalizePath(args["root"].get<std::string>());
    }
    else if (!s_guardrails.allowedRoots.empty())
    {
        root = NormalizePath(s_guardrails.allowedRoots.front());
    }
    else
    {
        return ToolCallResult::Error("No workspace root configured for git_status");
    }

    if (!IsPathAllowed(root))
    {
        return ToolCallResult::Sandbox("Root not in workspace allowlist: " + root);
    }

    nlohmann::json cmdArgs = nlohmann::json::object();
    cmdArgs["command"] = "cd /d \"" + root + "\" && git status --short --branch";
    if (args.contains("timeout"))
        cmdArgs["timeout"] = args["timeout"];
    return ExecuteCommand(cmdArgs);
}

// ============================================================================
// execute_command — Run terminal command (sandboxed)
// ============================================================================

bool AgentToolHandlers::RunProcess(const std::wstring& cmdLine, uint32_t timeoutMs, std::string& output,
                                   uint32_t& exitCode)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
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
    BOOL created =
        CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!created)
    {
        CloseHandle(hRead);
        output = "Failed to create process";
        return false;
    }

    std::string buffer;
    buffer.reserve(4096);
    DWORD startTick = GetTickCount();

    while (true)
    {
        DWORD available = 0;
        if (PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr) && available > 0)
        {
            char temp[4096];
            DWORD bytesRead = 0;
            if (::ReadFile(hRead, temp, sizeof(temp) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                temp[bytesRead] = '\0';
                buffer.append(temp, bytesRead);
                if (buffer.size() > s_guardrails.maxOutputCaptureBytes)
                {
                    buffer.resize(s_guardrails.maxOutputCaptureBytes);
                    buffer += "\n[OUTPUT TRUNCATED]";
                    break;
                }
            }
        }

        DWORD waitResult = WaitForSingleObject(pi.hProcess, 100);
        if (waitResult == WAIT_OBJECT_0)
        {
            // Read remaining output
            while (true)
            {
                DWORD avail2 = 0;
                if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &avail2, nullptr) || avail2 == 0)
                    break;
                char temp[4096];
                DWORD bytesRead = 0;
                if (::ReadFile(hRead, temp, sizeof(temp) - 1, &bytesRead, nullptr) && bytesRead > 0)
                {
                    temp[bytesRead] = '\0';
                    buffer.append(temp, bytesRead);
                }
                else
                    break;
            }
            break;
        }

        if (GetTickCount() - startTick > timeoutMs)
        {
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

ToolCallResult AgentToolHandlers::ExecuteCommand(const json& args)
{
    if (!args.contains("command") || !args["command"].is_string())
    {
        return ToolCallResult::Validation("execute_command requires 'command' (string)");
    }

    std::string command = args["command"].get<std::string>();
    uint32_t timeout = s_guardrails.commandTimeoutMs;
    if (args.contains("timeout") && args["timeout"].is_number())
    {
        timeout = static_cast<uint32_t>(args["timeout"].get<int>());
        // Cap timeout at 5 minutes
        if (timeout > 300000)
            timeout = 300000;
    }

    // Build command line via cmd.exe
    std::wstring cmdLine = L"cmd.exe /C " + ToWide(command);

    std::string output;
    uint32_t exitCode = 0;

    auto startTime = std::chrono::steady_clock::now();
    bool success = RunProcess(cmdLine, timeout, output, exitCode);
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();

    if (!success && exitCode == WAIT_TIMEOUT)
    {
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
    if (output.size() > kMaxOutput)
    {
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

    ToolCallResult result =
        (exitCode == 0) ? ToolCallResult::Ok(output, res_metadata)
                        : ToolCallResult::Error("Command exited with code " + std::to_string(exitCode) + "\n" + output,
                                                ToolOutcome::ExecutionError);
    if (exitCode != 0)
    {
        result.metadata = res_metadata;
        result.output = output;  // Include output even on error
    }
    return result;
}

// ============================================================================
// search_code — Recursive file search
// ============================================================================

ToolCallResult AgentToolHandlers::SearchCode(const json& args)
{
    if (!args.contains("query") || !args["query"].is_string())
    {
        return ToolCallResult::Validation("search_code requires 'query' (string)");
    }

    std::string query = args["query"].get<std::string>();
    if (query.empty())
    {
        return ToolCallResult::Validation("search_code query cannot be empty");
    }
    std::string filePattern = args.value("file_pattern", "*.*");
    bool caseSensitive = args.value("case_sensitive", true);
    bool useRegex = args.value("regex", false);
    int maxResults = s_guardrails.maxSearchResults;
    if (args.contains("max_results") && args["max_results"].is_number())
    {
        maxResults = std::clamp(args["max_results"].get<int>(), 1, 1000);
    }
    size_t contextBytes = static_cast<size_t>(std::max<int>(args.value("context_bytes", 200), 40));

    if (s_guardrails.allowedRoots.empty())
    {
        return ToolCallResult::Error("No workspace root configured for search");
    }

    std::string searchRoot = s_guardrails.allowedRoots[0];  // Primary workspace
    if (args.contains("root") && args["root"].is_string())
    {
        std::string candidate = NormalizePath(args["root"].get<std::string>());
        if (!IsPathAllowed(candidate))
        {
            return ToolCallResult::Sandbox("Search root not allowed: " + candidate);
        }
        searchRoot = candidate;
    }
    size_t maxFileSize = s_guardrails.maxFileSizeBytes;
    int hitCount = 0;
    int scannedFiles = 0;
    int skippedBinary = 0;

    std::ostringstream results;

    try
    {
        for (auto it = fs::recursive_directory_iterator(searchRoot, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it)
        {

            if (hitCount >= maxResults)
                break;
            if (!it->is_regular_file())
                continue;
            if (!IsPathAllowed(it->path().string()))
                continue;

            std::string filename = it->path().filename().string();

            // Simple pattern matching for file_pattern
            if (filePattern != "*.*" && filePattern != "*")
            {
                // Check extension
                std::string ext = it->path().extension().string();
                if (filePattern[0] == '*' && filePattern.size() > 1)
                {
                    std::string reqExt = filePattern.substr(1);
                    if (ext != reqExt)
                        continue;
                }
            }

            // Skip binary/large files
            auto fsize = it->file_size();
            if (fsize == 0 || fsize > maxFileSize)
                continue;
            scannedFiles++;

            // Read and search
            std::ifstream file(it->path(), std::ios::binary);
            if (!file.is_open())
                continue;

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            if (IsLikelyBinary(content))
            {
                skippedBinary++;
                continue;
            }

            if (useRegex)
            {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (!caseSensitive)
                    flags |= std::regex::icase;
                std::regex re;
                try
                {
                    re = std::regex(query, flags);
                }
                catch (const std::exception& ex)
                {
                    return ToolCallResult::Validation(std::string("Invalid regex: ") + ex.what());
                }

                int lineNum = 1;
                size_t lineStart = 0;
                for (auto itMatch = std::sregex_iterator(content.begin(), content.end(), re);
                     itMatch != std::sregex_iterator() && hitCount < maxResults; ++itMatch)
                {
                    size_t found = static_cast<size_t>(itMatch->position());
                    // Count lines to found position
                    for (size_t i = lineStart; i < found; ++i)
                    {
                        if (content[i] == '\n')
                            ++lineNum;
                    }
                    lineStart = found;
                    size_t contextStart = content.rfind('\n', found);
                    contextStart = (contextStart == std::string::npos) ? 0 : contextStart + 1;
                    size_t contextEnd = content.find('\n', found);
                    if (contextEnd == std::string::npos)
                        contextEnd = content.size();

                    std::string lineText =
                        content.substr(contextStart, std::min(contextEnd - contextStart, contextBytes));

                    std::string relPath = fs::relative(it->path(), searchRoot).string();
                    results << relPath << ":" << lineNum << ": " << lineText << "\n";

                    ++hitCount;
                }
                continue;
            }

            std::string lowerContent;
            std::string lowerQuery;
            if (!caseSensitive)
            {
                lowerContent = content;
                lowerQuery = query;
                std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            }

            size_t pos = 0;
            int lineNum = 1;
            size_t lineStart = 0;

            while (pos < content.size() && hitCount < maxResults)
            {
                // Track line numbers
                while (lineStart < pos)
                {
                    if (content[lineStart] == '\n')
                        ++lineNum;
                    ++lineStart;
                }

                size_t found;
                found = caseSensitive ? content.find(query, pos) : lowerContent.find(lowerQuery, pos);
                if (found == std::string::npos)
                    break;

                // Count lines to found position
                for (size_t i = lineStart; i < found; ++i)
                {
                    if (content[i] == '\n')
                        ++lineNum;
                }
                lineStart = found;

                // Extract line context
                size_t contextStart = content.rfind('\n', found);
                contextStart = (contextStart == std::string::npos) ? 0 : contextStart + 1;
                size_t contextEnd = content.find('\n', found);
                if (contextEnd == std::string::npos)
                    contextEnd = content.size();

                std::string lineText = content.substr(contextStart, std::min(contextEnd - contextStart, contextBytes));

                // Relative path from search root
                std::string relPath = fs::relative(it->path(), searchRoot).string();
                results << relPath << ":" << lineNum << ": " << lineText << "\n";

                ++hitCount;
                pos = found + query.size();
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (hitCount == 0)
        {
            return ToolCallResult::Error(std::string("Search failed: ") + ex.what());
        }
        // Partial results are still useful
    }

    if (hitCount == 0)
    {
        nlohmann::json zero_matches = nlohmann::json::object();
        zero_matches["matches"] = 0;
        return ToolCallResult::Ok("No matches found for: " + query, zero_matches);
    }

    std::string truncMsg;
    if (hitCount >= maxResults)
    {
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

ToolCallResult AgentToolHandlers::GetDiagnostics(const json& args)
{
    if (!args.contains("file") || !args["file"].is_string())
    {
        return ToolCallResult::Validation("get_diagnostics requires 'file' (string)");
    }

    std::string file = NormalizePath(args["file"].get<std::string>());

    // Run cl.exe /Zs (syntax check only) for C++ files
    std::string ext = fs::path(file).extension().string();
    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
    {
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
ToolCallResult AgentToolHandlers::RunShell(const json& args)
{
    if (!args.contains("command") || !args["command"].is_string())
    {
        return ToolCallResult::Validation("run_shell requires 'command' (string)");
    }
    std::string command = args["command"].get<std::string>();

    if (s_guardrails.sandboxTier == ToolGuardrails::CommandSandboxTier::Blocked)
    {
        ToolCallResult res = ToolCallResult::Sandbox("run_shell is blocked by sandbox tier policy");
        res.metadata = {{"command", command}, {"sandbox_tier", "blocked"}};
        return res;
    }

    if (s_guardrails.sandboxTier == ToolGuardrails::CommandSandboxTier::Safe)
    {
        std::istringstream iss(command);
        std::string first;
        iss >> first;
        bool allowed = false;
        for (const auto& ac : s_guardrails.allowedCommands)
        {
            if (first == ac)
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
        {
            nlohmann::json meta = nlohmann::json::object();
            meta["command"] = command;
            meta["first_token"] = first;
            meta["policy"] = "allowedCommands";
            ToolCallResult res = ToolCallResult::Sandbox("Command not allowed by policy: " + first);
            res.metadata = meta;
            return res;
        }
    }

    // Elevated tier allows all commands within allowedRoots/dynamic checks
    return ExecuteCommand(args);
}

// ============================================================================
// semantic_search — Lightweight TF cosine search across workspace files
// ============================================================================
ToolCallResult AgentToolHandlers::SemanticSearch(const json& args)
{
    if (!args.contains("query") || !args["query"].is_string())
    {
        return ToolCallResult::Validation("semantic_search requires 'query' (string)");
    }
    if (s_guardrails.allowedRoots.empty())
    {
        return ToolCallResult::Error("No workspace root configured for semantic search");
    }
    std::string query = args["query"].get<std::string>();
    std::string root = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string())
    {
        root = NormalizePath(args["root"].get<std::string>());
        if (!IsPathAllowed(root))
        {
            return ToolCallResult::Sandbox("Root not in allowlist: " + root);
        }
    }
    int topK = args.value("top_k", 5);
    if (topK <= 0)
        topK = 5;
    if (topK > 25)
        topK = 25;
    int maxFiles = s_guardrails.maxIndexFiles;

    bool include_non_code = false;
    if (args.contains("include_non_code"))
    {
        const auto& inc = args["include_non_code"];
        if (inc.is_boolean())
        {
            include_non_code = inc.get<bool>();
        }
        else if (inc.is_number_integer())
        {
            include_non_code = inc.get<int>() != 0;
        }
    }

    std::string cacheKey = root + "|" + query + "|" + std::to_string(topK) + "|" + (include_non_code ? "1" : "0");
    {
        std::lock_guard<std::mutex> lock(s_semanticSearchCacheMutex);
        auto it = s_semanticSearchCache.find(cacheKey);
        if (it != s_semanticSearchCache.end())
        {
            auto age = std::chrono::steady_clock::now() - it->second.timestamp;
            if (age < std::chrono::seconds(120))
            {
                ToolCallResult cached(ToolCallResult::Ok(it->second.body, it->second.metadata));
                return cached;
            }
            s_semanticSearchCache.erase(it);
        }
    }
    static const std::unordered_set<std::string> kCodeExt = {
        ".cpp",   ".c",   ".cc",   ".cxx",  ".h",   ".hpp",  ".hh",  ".hxx", ".cs",    ".java", ".kt",
        ".rs",    ".go",  ".ts",   ".tsx",  ".js",  ".jsx",  ".py",  ".rb",  ".swift", ".m",    ".mm",
        ".scala", ".sql", ".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".cmake", ".sh",   ".ps1"};

    auto qTokens = Tokenize(query);
    if (qTokens.empty())
    {
        return ToolCallResult::Validation("semantic_search query produced no tokens");
    }
    std::unordered_map<std::string, double> qtf;
    for (const auto& t : qTokens)
        qtf[t] += 1.0;
    double qnorm = 0.0;
    for (auto& kv : qtf)
    {
        kv.second = kv.second / static_cast<double>(qTokens.size());
        qnorm += kv.second * kv.second;
    }
    qnorm = std::sqrt(qnorm);

    std::vector<IndexedFile> indexed;
    int scanned = 0;
    int skippedBinary = 0;
    try
    {
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it)
        {
            if (scanned >= maxFiles)
                break;
            if (!it->is_regular_file())
                continue;
            auto fsize = it->file_size();
            if (fsize == 0 || fsize > s_guardrails.maxFileSizeBytes)
                continue;
            std::string p = it->path().string();
            std::string ext = ToLowerCopy(it->path().extension().string());
            if (!include_non_code && !ext.empty() && kCodeExt.find(ext) == kCodeExt.end())
                continue;
            if (!IsPathAllowed(p))
                continue;
            // Quick binary sniff to avoid heavy tokenization
            bool binaryFlag = false;
            try
            {
                std::ifstream sniff(p, std::ios::binary);
                if (sniff.is_open())
                {
                    std::string head(512, '\0');
                    sniff.read(head.data(), static_cast<std::streamsize>(head.size()));
                    head.resize(static_cast<size_t>(sniff.gcount()));
                    binaryFlag = IsLikelyBinary(head);
                }
            }
            catch (...)
            { /* ignore */
            }
            if (binaryFlag)
            {
                skippedBinary++;
                continue;
            }
            auto idx = BuildIndexForFile(p, s_guardrails.maxFileSizeBytes);
            if (!idx.tf.empty())
            {
                indexed.push_back(std::move(idx));
                ++scanned;
            }
        }
    }
    catch (...)
    {
        // tolerate partial scan
    }

    if (indexed.empty())
    {
        return ToolCallResult::Error("No indexable files found under " + root);
    }

    struct Scored
    {
        std::string path;
        double score;
    };
    std::vector<Scored> scores;
    for (const auto& f : indexed)
    {
        double s = CosineScore(f, qtf, qnorm);
        if (s > 0.0)
            scores.push_back({f.path, s});
    }
    std::sort(scores.begin(), scores.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
    if ((int)scores.size() > topK)
        scores.resize(topK);

    nlohmann::json res = nlohmann::json::array();
    double scoreSum = 0.0;
    for (const auto& s : scores)
    {
        nlohmann::json row;
        row["path"] = fs::relative(s.path, root).string();
        row["score"] = s.score;
        scoreSum += s.score;
        // Optional snippet preview (first 240 chars)
        std::string snippet;
        try
        {
            std::ifstream preview(s.path, std::ios::binary);
            if (preview.is_open())
            {
                std::string buf(240, '\0');
                preview.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                buf.resize(static_cast<size_t>(preview.gcount()));
                snippet = buf;
            }
        }
        catch (...)
        { /* ignore */
        }
        if (!snippet.empty())
        {
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
    if (!scores.empty())
    {
        meta["avg_score"] = scoreSum / static_cast<double>(scores.size());
    }
    return ToolCallResult::Ok(res.dump(2), meta);
}

// ============================================================================
// mention_lookup — Symbol-aware alias over semantic_search
// ============================================================================
ToolCallResult AgentToolHandlers::MentionLookup(const json& args)
{
    json copy = args;
    if (!copy.contains("query") && copy.contains("symbol"))
    {
        copy["query"] = copy["symbol"];
    }
    if (!copy.contains("top_k"))
    {
        copy["top_k"] = 3;
    }
    // Clamp to avoid huge responses
    if (copy.contains("top_k") && copy["top_k"].is_number())
    {
        int tk = std::clamp(copy["top_k"].get<int>(), 1, 10);
        copy["top_k"] = tk;
    }
    // Favor current workspace root when not provided
    if (!copy.contains("root") && !s_guardrails.allowedRoots.empty())
    {
        copy["root"] = s_guardrails.allowedRoots[0];
    }
    if (args.contains("include_non_code"))
    {
        copy["include_non_code"] = args["include_non_code"];
    }
    return SemanticSearch(copy);
}

// ============================================================================
// next_edit_hint — Heuristic “next edit” suggestion from context
// ============================================================================
ToolCallResult AgentToolHandlers::NextEditHint(const json& args)
{
    if (!args.contains("context") || !args["context"].is_string())
    {
        return ToolCallResult::Validation("next_edit_hint requires 'context' (string)");
    }
    std::string ctx = args["context"].get<std::string>();
    std::vector<std::string> hints;
    std::unordered_set<std::string> seen;
    auto addHint = [&](const std::string& h)
    {
        if (seen.insert(h).second && hints.size() < 3)
            hints.push_back(h);
    };
    if (ctx.find("TODO") != std::string::npos)
    {
        addHint("Address the TODO with a small, testable change and add a unit test.");
    }
    if (ctx.find("function") != std::string::npos || ctx.find("def ") != std::string::npos)
    {
        addHint("Add docstring/comments and edge-case handling (empty input, null pointers).");
    }
    if (ctx.find("class") != std::string::npos || ctx.find("struct") != std::string::npos)
    {
        addHint("Ensure constructors initialize all fields and add default move/clone semantics if needed.");
    }
    if (hints.empty())
    {
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
ToolCallResult AgentToolHandlers::ProposeMultiFileEdits(const json& args)
{
    if (!args.contains("files") || !args["files"].is_array())
    {
        return ToolCallResult::Validation("propose_multifile_edits requires 'files' (array)");
    }
    std::string instruction = args.value("instruction", "Apply requested change across files.");
    nlohmann::json plans = nlohmann::json::array();
    size_t capped = 0;
    size_t skipped = 0;
    for (const auto& f : args["files"])
    {
        if (!f.is_string())
        {
            skipped++;
            continue;
        }
        std::string path = NormalizePath(f.get<std::string>());
        if (!IsPathAllowed(path) || !fs::exists(path))
        {
            skipped++;
            continue;
        }
        if (plans.size() >= 20)
        {
            capped++;
            continue;
        }
        nlohmann::json step = nlohmann::json::object();
        step["file"] = path;
        step["plan"] = instruction + " (review, edit, validate)";
        plans.push_back(step);
    }
    if (plans.empty())
    {
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
ToolCallResult AgentToolHandlers::LoadRules(const json& args)
{
    std::string path;
    bool hasInline = args.contains("content") && args["content"].is_string();
    if (args.contains("path") && args["path"].is_string())
    {
        path = NormalizePath(args["path"].get<std::string>());
    }
    else if (!s_guardrails.allowedRoots.empty())
    {
        path = NormalizePath(s_guardrails.allowedRoots[0] + "/.rawrrules");
    }
    else
    {
        return ToolCallResult::Error("No workspace root configured to locate .rawrrules");
    }
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Rules file not in allowlist: " + path);
    }
    if (!hasInline && !fs::exists(path))
    {
        return ToolCallResult::Error("Rules file not found: " + path);
    }
    if (!path.empty())
    {
        std::string ext = ToLowerCopy(fs::path(path).extension().string());
        if (!ext.empty() && ext != ".rawrrules" && ext != ".rules" && ext != ".txt")
        {
            return ToolCallResult::Validation("Rules path must be .rawrrules/.rules/.txt");
        }
    }

    nlohmann::json rules = nlohmann::json::object();
    auto parseLine = [&](const std::string& line)
    {
        if (line.empty() || line[0] == '#')
            return;
        auto pos = line.find(':');
        if (pos == std::string::npos)
            return;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (!key.empty())
            rules[key] = val;
    };

    size_t lineCount = 0;
    std::string concat;
    if (hasInline)
    {
        std::istringstream ss(args["content"].get<std::string>());
        std::string line;
        while (std::getline(ss, line))
        {
            parseLine(line);
            concat += line;
            ++lineCount;
        }
    }
    else
    {
        std::ifstream in(path);
        if (!in.is_open())
        {
            return ToolCallResult::Error("Failed to open rules file: " + path);
        }
        std::string line;
        while (std::getline(in, line))
        {
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
    if (rules.empty())
    {
        ToolCallResult res = ToolCallResult::Validation("Rules parsed but no entries found");
        res.metadata = meta;
        return res;
    }
    return ToolCallResult::Ok(rules.dump(2), meta);
}

// ============================================================================
// plan_tasks — Lightweight deterministic plan generator
// ============================================================================
ToolCallResult AgentToolHandlers::PlanTasks(const json& args)
{
    if (!args.contains("goal") || !args["goal"].is_string())
    {
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
    if (plan.size() < static_cast<size_t>(maxSteps))
    {
        plan.push_back("Validate with unit/smoke tests and capture artifacts.");
    }
    if (plan.size() < static_cast<size_t>(maxSteps))
    {
        plan.push_back("Summarize changes, risks, and follow-ups.");
    }
    if (plan.is_array() && static_cast<int>(plan.size()) > maxSteps)
    {
        json trimmed = json::array();
        for (int i = 0; i < maxSteps && i < static_cast<int>(plan.size()); ++i)
        {
            trimmed.push_back(plan[i]);
        }
        plan = std::move(trimmed);
    }
    nlohmann::json meta = nlohmann::json::object();
    meta["steps"] = plan.size();
    meta["goal_length"] = goal.size();
    meta["max_steps"] = maxSteps;
    if (!deadline.empty())
        meta["deadline"] = deadline;
    if (!owner.empty())
        meta["owner"] = owner;
    return ToolCallResult::Ok(plan.dump(2), meta);
}

// ============================================================================
// set/get/reset_iteration_status — Long-running model/agent progress state
// ============================================================================
ToolCallResult AgentToolHandlers::SetIterationStatus(const json& args)
{
    std::lock_guard<std::mutex> lock(s_iterationStatusMutex);

    if (args.contains("busy") && args["busy"].is_boolean())
    {
        s_iterationBusy = args["busy"].get<bool>();
    }
    if (args.contains("current") && args["current"].is_number_integer())
    {
        s_iterationCurrent = std::max(0, args["current"].get<int>());
    }
    if (args.contains("total") && args["total"].is_number_integer())
    {
        s_iterationTotal = std::max(0, args["total"].get<int>());
    }
    if (args.contains("phase") && args["phase"].is_string())
    {
        s_iterationPhase = args["phase"].get<std::string>();
    }
    if (args.contains("message") && args["message"].is_string())
    {
        s_iterationMessage = args["message"].get<std::string>();
    }

    if (!args.contains("busy") && s_iterationTotal > 0)
    {
        s_iterationBusy = s_iterationCurrent < s_iterationTotal;
    }

    json out = json::object();
    out["busy"] = s_iterationBusy;
    out["current"] = s_iterationCurrent;
    out["total"] = s_iterationTotal;
    out["phase"] = s_iterationPhase;
    out["message"] = s_iterationMessage;
    return ToolCallResult::Ok(out.dump(2));
}

ToolCallResult AgentToolHandlers::GetIterationStatus(const json& /*args*/)
{
    std::lock_guard<std::mutex> lock(s_iterationStatusMutex);
    json out = json::object();
    out["busy"] = s_iterationBusy;
    out["current"] = s_iterationCurrent;
    out["total"] = s_iterationTotal;
    out["phase"] = s_iterationPhase;
    out["message"] = s_iterationMessage;
    return ToolCallResult::Ok(out.dump(2));
}

ToolCallResult AgentToolHandlers::ResetIterationStatus(const json& /*args*/)
{
    std::lock_guard<std::mutex> lock(s_iterationStatusMutex);
    s_iterationBusy = false;
    s_iterationCurrent = 0;
    s_iterationTotal = 0;
    s_iterationPhase = "idle";
    s_iterationMessage.clear();

    json out = json::object();
    out["busy"] = s_iterationBusy;
    out["current"] = s_iterationCurrent;
    out["total"] = s_iterationTotal;
    out["phase"] = s_iterationPhase;
    out["message"] = s_iterationMessage;
    return ToolCallResult::Ok(out.dump(2));
}

// ============================================================================
// compact_conversation — Compact agent history to last N events
// ============================================================================
ToolCallResult AgentToolHandlers::CompactConversation(const json& args)
{
    if (!s_historyRecorder)
    {
        // Recorder not wired yet — return a structured no-op result instead of an error
        // so callers can distinguish "no history to compact" from a real failure.
        size_t keepLast = static_cast<size_t>(std::max(0, args.value("keep_last", 200)));
        nlohmann::json meta = nlohmann::json::object();
        meta["keep_last"] = keepLast;
        meta["retention_days"] = args.value("retention_days", -1);
        meta["removed"] = 0;
        meta["flushed"] = false;
        meta["recorder_available"] = false;
        return ToolCallResult::Ok("No history recorder configured; nothing to compact", meta);
    }
    size_t keepLast = static_cast<size_t>(std::max(0, args.value("keep_last", 200)));
    int retentionDays = args.value("retention_days", -1);
    bool flush = args.value("flush", true);

    if (retentionDays > 0)
    {
        s_historyRecorder->setRetentionDays(retentionDays);
        s_historyRecorder->purgeExpired();
    }

    size_t removed = s_historyRecorder->compact(keepLast);
    if (flush)
    {
        s_historyRecorder->flush();
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["keep_last"] = keepLast;
    meta["retention_days"] = retentionDays;
    meta["removed"] = removed;
    meta["flushed"] = flush;
    return ToolCallResult::Ok("Conversation history compacted", meta);
}

// ============================================================================
// optimize_tool_selection — Recommend tool set for a task
// ============================================================================
ToolCallResult AgentToolHandlers::OptimizeToolSelection(const json& args)
{
    if (!args.contains("task") || !args["task"].is_string())
    {
        return ToolCallResult::Validation("optimize_tool_selection requires 'task' (string)");
    }
    std::string task = ToLowerCopy(args["task"].get<std::string>());
    std::vector<std::string> tools = {"read_file",      "search_code", "search_files",          "semantic_search",
                                      "resolve_symbol", "plan_tasks",  "plan_code_exploration", "read_lines",
                                      "get_diagnostics"};
    std::unordered_map<std::string, int> scores;
    for (const auto& t : tools)
        scores[t] = 0;

    auto bump = [&](const std::vector<std::string>& keys, const std::vector<std::string>& adds)
    {
        for (const auto& k : keys)
        {
            if (task.find(k) != std::string::npos)
            {
                for (const auto& a : adds)
                    scores[a] += 3;
            }
        }
    };

    bump({"search", "find", "locate"}, {"search_code", "search_files", "semantic_search"});
    bump({"read", "inspect", "snippet"}, {"read_file", "read_lines"});
    bump({"plan", "explore", "audit"}, {"plan_tasks", "plan_code_exploration"});
    bump({"symbol", "resolve", "definition"}, {"resolve_symbol"});
    bump({"error", "diagnostic", "build"}, {"get_diagnostics"});

    std::vector<std::pair<std::string, int>> ranked(scores.begin(), scores.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    int topN = std::clamp(args.value("max_tools", 6), 1, 10);
    nlohmann::json out = nlohmann::json::array();
    for (int i = 0; i < topN && i < static_cast<int>(ranked.size()); ++i)
    {
        nlohmann::json row;
        row["tool"] = ranked[i].first;
        row["score"] = ranked[i].second;
        out.push_back(row);
    }
    nlohmann::json meta = nlohmann::json::object();
    meta["task"] = args["task"];
    meta["recommended"] = out.size();
    return ToolCallResult::Ok(out.dump(2), meta);
}

// ============================================================================
// resolve_symbol — Search codebase for symbol occurrences
// ============================================================================
ToolCallResult AgentToolHandlers::ResolveSymbol(const json& args)
{
    if (!args.contains("symbol") || !args["symbol"].is_string())
    {
        return ToolCallResult::Validation("resolve_symbol requires 'symbol' (string)");
    }
    std::string symbol = args["symbol"].get<std::string>();
    if (symbol.empty())
    {
        return ToolCallResult::Validation("resolve_symbol requires non-empty 'symbol'");
    }
    json searchArgs = json::object();
    searchArgs["query"] = "\\b" + EscapeRegex(symbol) + "\\b";
    searchArgs["regex"] = true;
    searchArgs["case_sensitive"] = true;
    searchArgs["file_pattern"] = args.value("file_pattern", "*.*");
    if (args.contains("root"))
        searchArgs["root"] = args["root"];
    if (args.contains("max_results"))
        searchArgs["max_results"] = args["max_results"];
    return SearchCode(searchArgs);
}

// ============================================================================
// read_lines — Read a specific line range from a file
// ============================================================================
ToolCallResult AgentToolHandlers::ReadLines(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("read_lines requires 'path' (string)");
    }
    std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path))
    {
        return ToolCallResult::Error("File not found: " + path);
    }

    int startLine = args.value("start_line", 1);
    int endLine = args.value("end_line", startLine);
    if (args.contains("range") && args["range"].is_string())
    {
        std::string range = args["range"].get<std::string>();
        auto dash = range.find('-');
        if (dash != std::string::npos)
        {
            try
            {
                startLine = std::stoi(range.substr(0, dash));
                endLine = std::stoi(range.substr(dash + 1));
            }
            catch (...)
            {
                return ToolCallResult::Validation("read_lines invalid range format; use 'start-end'");
            }
        }
    }
    if (startLine < 1 || endLine < startLine)
    {
        return ToolCallResult::Validation("read_lines requires start_line <= end_line and both >= 1");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return ToolCallResult::Error("Cannot open file: " + path);
    }

    std::ostringstream out;
    std::string line;
    int current = 0;
    int returned = 0;
    while (std::getline(file, line))
    {
        current++;
        if (current < startLine)
            continue;
        if (current > endLine)
            break;
        out << line << "\n";
        returned++;
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["start_line"] = startLine;
    meta["end_line"] = endLine;
    meta["returned"] = returned;
    meta["file"] = path;
    ToolCallResult result = ToolCallResult::Ok(out.str(), meta);
    result.filePath = path;
    return result;
}

// ============================================================================
// plan_code_exploration — Analyze workspace and emit exploration plan
// ============================================================================
ToolCallResult AgentToolHandlers::PlanCodeExploration(const json& args)
{
    if (!args.contains("goal") || !args["goal"].is_string())
    {
        return ToolCallResult::Validation("plan_code_exploration requires 'goal' (string)");
    }
    if (s_guardrails.allowedRoots.empty())
    {
        return ToolCallResult::Error("No workspace root configured for planning");
    }
    std::string root = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string())
    {
        root = NormalizePath(args["root"].get<std::string>());
    }
    if (!IsPathAllowed(root))
    {
        return ToolCallResult::Sandbox("Root not in allowlist: " + root);
    }

    ::Agentic::WorkspaceAnalyzer analyzer(root);
    bool ok = analyzer.analyze();

    nlohmann::json result = nlohmann::json::object();
    result["goal"] = args["goal"].get<std::string>();
    result["workspace"] = root;
    if (ok)
    {
        result["analysis"] = analyzer.getAnalysis().toJson();
    }
    else
    {
        result["analysis_error"] = "Workspace analysis failed";
    }

    json planArgs = json::object();
    planArgs["goal"] = args["goal"].get<std::string>();
    planArgs["max_steps"] = args.value("max_steps", 6);
    ToolCallResult plan = PlanTasks(planArgs);
    result["plan"] = plan.output;

    nlohmann::json meta = nlohmann::json::object();
    meta["analyzed"] = ok;
    return ToolCallResult::Ok(result.dump(2), meta);
}

// ============================================================================
// search_files — Find files matching glob or regex patterns
// ============================================================================
ToolCallResult AgentToolHandlers::SearchFiles(const json& args)
{
    if (!args.contains("pattern") || !args["pattern"].is_string())
    {
        return ToolCallResult::Validation("search_files requires 'pattern' (string)");
    }
    if (s_guardrails.allowedRoots.empty())
    {
        return ToolCallResult::Error("No workspace root configured for search_files");
    }
    std::string pattern = args["pattern"].get<std::string>();
    bool useRegex = args.value("regex", false);
    bool caseSensitive = args.value("case_sensitive", true);
    int maxResults = std::clamp(args.value("max_results", 200), 1, 2000);

    std::string root = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string())
    {
        root = NormalizePath(args["root"].get<std::string>());
    }
    if (!IsPathAllowed(root))
    {
        return ToolCallResult::Sandbox("Root not in allowlist: " + root);
    }

    std::regex re;
    if (useRegex)
    {
        try
        {
            std::regex::flag_type flags = std::regex::ECMAScript;
            if (!caseSensitive)
                flags |= std::regex::icase;
            re = std::regex(pattern, flags);
        }
        catch (const std::exception& ex)
        {
            return ToolCallResult::Validation(std::string("Invalid regex: ") + ex.what());
        }
    }
    else
    {
        std::string err;
        re = GlobToRegex(pattern, caseSensitive, err);
        if (!err.empty())
        {
            return ToolCallResult::Validation(std::string("Invalid glob: ") + err);
        }
    }

    nlohmann::json matches = nlohmann::json::array();
    int found = 0;
    try
    {
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it)
        {
            if (found >= maxResults)
                break;
            if (!it->is_regular_file())
                continue;
            std::string full = it->path().string();
            if (!IsPathAllowed(full))
                continue;
            std::string rel = fs::relative(it->path(), root).string();
            if (std::regex_match(rel, re))
            {
                matches.push_back(rel);
                found++;
            }
        }
    }
    catch (const std::exception& ex)
    {
        return ToolCallResult::Error(std::string("search_files failed: ") + ex.what());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["matches"] = found;
    meta["root"] = root;
    meta["pattern"] = pattern;
    meta["truncated"] = (found >= maxResults);
    return ToolCallResult::Ok(matches.dump(2), meta);
}

// ============================================================================
// restore_checkpoint — Resume workflow and reload history snapshot
// ============================================================================
ToolCallResult AgentToolHandlers::RestoreCheckpoint(const json& args)
{
    if (!args.contains("checkpoint_path") || !args["checkpoint_path"].is_string())
    {
        return ToolCallResult::Validation("restore_checkpoint requires 'checkpoint_path' (string)");
    }
    std::string checkpoint = args["checkpoint_path"].get<std::string>();
    if (checkpoint.empty())
    {
        return ToolCallResult::Validation("restore_checkpoint requires non-empty checkpoint_path");
    }

    int ok = AgentWorkflow_Resume(checkpoint.c_str());
    if (ok != 0)
    {
        return ToolCallResult::Error("Failed to resume workflow from checkpoint: " + checkpoint +
                                     " (exit code: " + std::to_string(ok) + ")");
    }

    bool historyLoaded = false;
    if (s_historyRecorder && args.contains("history_log_path") && args["history_log_path"].is_string())
    {
        historyLoaded = s_historyRecorder->loadFromLogFile(args["history_log_path"].get<std::string>());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["checkpoint_path"] = checkpoint;
    meta["workflow_resumed"] = (ok == 1);
    meta["history_loaded"] = historyLoaded;
    if (ok != 1)
    {
        return ToolCallResult::Error("Failed to resume workflow from checkpoint", ToolOutcome::ExecutionError);
    }
    return ToolCallResult::Ok("Checkpoint restored", meta);
}

// ============================================================================
// evaluate_integration_audit_feasibility — Workspace readiness for audit
// ============================================================================
ToolCallResult AgentToolHandlers::EvaluateIntegrationAuditFeasibility(const json& args)
{
    if (s_guardrails.allowedRoots.empty())
    {
        return ToolCallResult::Error("No workspace root configured for feasibility evaluation");
    }
    std::string root = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string())
    {
        root = NormalizePath(args["root"].get<std::string>());
    }
    if (!IsPathAllowed(root))
    {
        return ToolCallResult::Sandbox("Root not in allowlist: " + root);
    }

    ::Agentic::WorkspaceAnalyzer analyzer(root);
    bool ok = analyzer.analyze();
    nlohmann::json meta = nlohmann::json::object();
    meta["workspace"] = root;
    if (!ok)
    {
        return ToolCallResult::Error("Workspace analysis failed", ToolOutcome::ExecutionError);
    }
    auto analysis = analyzer.getAnalysis();
    bool feasible = analysis.total_source_files <= 5000 && analysis.total_lines_of_code <= 5000000;
    meta["total_source_files"] = analysis.total_source_files;
    meta["total_lines"] = analysis.total_lines_of_code;
    meta["feasible"] = feasible;
    return ToolCallResult::Ok(analysis.toJson().dump(2), meta);
}

// ============================================================================
// GetCoverage — BBCov / DiffCov coverage retrieval
// ============================================================================
ToolCallResult AgentToolHandlers::GetCoverage(const json& args)
{
    std::string file = args.value("file", "");
    std::string func = args.value("function_name", "");
    std::string mode = args.value("mode", "diffcov");

    auto& registry = SubsystemRegistry::instance();

    SubsystemId targetMode = (mode == "bbcov")
        ? SubsystemId::BBCov
        : SubsystemId::DiffCov;

    if (!registry.isAvailable(targetMode))
    {
        return ToolCallResult::Error(mode + " subsystem not available");
    }

    SubsystemParams params{};
    params.id = targetMode;
    SubsystemResult result = registry.invoke(params);

    if (!result.success)
    {
        return ToolCallResult::Error(
            std::string(mode) + " failed: " +
            (result.detail ? result.detail : "unknown error"));
    }

    std::ostringstream oss;
    oss << mode << " analysis complete";
    if (result.artifactPath) oss << " — artifact: " << result.artifactPath;
    oss << " (" << result.latencyMs << "ms)";
    if (!file.empty()) oss << " [filter: " << file << "]";
    if (!func.empty()) oss << " [function: " << func << "]";
    return ToolCallResult::Ok(oss.str());
}

// ============================================================================
// RunBuild — CMake build trigger
// ============================================================================
ToolCallResult AgentToolHandlers::RunBuild(const json& args)
{
    std::string target = args.value("target", "all");
    std::string config = args.value("config", "Release");

    std::string cmd = "cmake --build . --config " + config;
    if (target != "all") cmd += " --target " + target;

    json execArgs;
    execArgs["command"] = cmd;
    execArgs["timeout"] = 120000;
    return ExecuteCommand(execArgs);
}

// ============================================================================
// ApplyHotpatch — Runtime hotpatch via unified manager
// ============================================================================
ToolCallResult AgentToolHandlers::ApplyHotpatch(const json& args)
{
    std::string layer = args.value("layer", "");
    std::string target = args.value("target", "");
    std::string data = args.value("data", "");
    if (layer.empty() || target.empty())
        return ToolCallResult::Validation("Missing required parameters: layer, target");

    auto& manager = UnifiedHotpatchManager::instance();
    UnifiedResult ur;

    if (layer == "memory")
    {
        uintptr_t addr = 0;
        sscanf(target.c_str(), "%llx", &addr);
        if (addr == 0)
            return ToolCallResult::Error("Invalid memory address: " + target);

        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 1 < data.size(); i += 2)
        {
            uint8_t byte = 0;
            sscanf(data.c_str() + i, "%2hhx", &byte);
            bytes.push_back(byte);
        }
        if (bytes.empty())
            return ToolCallResult::Error("No patch data provided");

        ur = manager.apply_memory_patch(reinterpret_cast<void*>(addr),
                                         bytes.size(), bytes.data());
    }
    else if (layer == "byte")
    {
        auto colonPos = data.find(':');
        if (colonPos == std::string::npos)
            return ToolCallResult::Validation("byte layer data must be PATTERN_HEX:REPLACEMENT_HEX");

        std::string patternHex = data.substr(0, colonPos);
        std::string replaceHex = data.substr(colonPos + 1);
        std::vector<uint8_t> pattern, replacement;

        for (size_t i = 0; i + 1 < patternHex.size(); i += 2)
        {
            uint8_t b; sscanf(patternHex.c_str() + i, "%2hhx", &b);
            pattern.push_back(b);
        }
        for (size_t i = 0; i + 1 < replaceHex.size(); i += 2)
        {
            uint8_t b; sscanf(replaceHex.c_str() + i, "%2hhx", &b);
            replacement.push_back(b);
        }

        ur = manager.apply_byte_search_patch(target.c_str(), pattern, replacement);
    }
    else if (layer == "server")
    {
        if (data == "remove")
        {
            ur = manager.remove_server_patch(target.c_str());
        }
        else
        {
            return ToolCallResult::Error(
                "Server patches must be registered programmatically. "
                "Use target=name, data=remove to remove.");
        }
    }
    else
    {
        return ToolCallResult::Error("Unknown layer: " + layer +
                                     ". Valid: memory, byte, server");
    }

    if (!ur.result.success)
    {
        const char* detail = ur.result.detail.empty() ? "unknown error" : ur.result.detail.c_str();
        return ToolCallResult::Error(
            std::string(layer) + " layer failed: " + detail);
    }

    std::ostringstream oss;
    oss << layer << " layer applied successfully"
        << " | target=" << target
        << " | seq=" << ur.sequenceId;
    return ToolCallResult::Ok(oss.str());
}

// ============================================================================
// DiskRecovery — WD My Book USB bridge recovery agent
// ============================================================================
ToolCallResult AgentToolHandlers::DiskRecovery(const json& args)
{
    std::string action = args.value("action", "");
    if (action.empty())
        return ToolCallResult::Validation("Missing required parameter: action");

    static RawrXD::Recovery::DiskRecoveryAsmAgent agent;

    if (action == "scan" || action == "find")
    {
        int driveNum = agent.FindDrive();
        if (driveNum < 0)
            return ToolCallResult::Error("No dying WD device found on PhysicalDrive0-15");
        return ToolCallResult::Ok("Found candidate: PhysicalDrive" + std::to_string(driveNum));
    }
    else if (action == "init" || action == "initialize")
    {
        int driveNum = args.value("drive", -1);
        if (driveNum < 0)
            return ToolCallResult::Error("Missing parameter: drive (0-15)");
        auto r = agent.Initialize(driveNum);
        if (!r.success) return ToolCallResult::Error(r.detail);
        return ToolCallResult::Ok(r.detail);
    }
    else if (action == "extract_key" || action == "key")
    {
        auto r = agent.ExtractEncryptionKey();
        if (!r.success) return ToolCallResult::Error(r.detail);
        return ToolCallResult::Ok(r.detail);
    }
    else if (action == "run" || action == "recover")
    {
        auto r = agent.RunRecovery();
        if (!r.success) return ToolCallResult::Error(r.detail);
        return ToolCallResult::Ok(r.detail);
    }
    else if (action == "abort" || action == "stop")
    {
        agent.Abort();
        return ToolCallResult::Ok("Abort signal sent to recovery worker");
    }
    else if (action == "stats" || action == "status")
    {
        auto stats = agent.GetStats();
        json j;
        j["good_sectors"] = stats.goodSectors;
        j["bad_sectors"] = stats.badSectors;
        j["current_lba"] = stats.currentLBA;
        j["total_sectors"] = stats.totalSectors;
        j["progress_percent"] = stats.ProgressPercent();
        return ToolCallResult::Ok(j.dump(2));
    }
    else if (action == "reset")
    {
        agent = RawrXD::Recovery::DiskRecoveryAsmAgent();
        return ToolCallResult::Ok("Disk recovery agent reset");
    }
    else
    {
        return ToolCallResult::Error(
            "Unknown action: " + action +
            ". Valid: scan, init, extract_key, run, abort, stats, reset");
    }
}

// ============================================================================
// Schema generation — OpenAI function-calling format
// ============================================================================

json AgentToolHandlers::GetAllSchemas()
{
    json tools = json::array();

    // Helper: build a JSON array of strings (avoids json::array() initializer issues)
    auto jstrArr = [](std::initializer_list<const char*> items) -> json
    {
        json arr = json::array();
        for (auto s : items)
            arr.push_back(s);
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
    rif_f["description"] =
        "Search and replace a block of text in a file. Include 3+ lines of context in old_string for uniqueness.";
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

    // set_iteration_status
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "set_iteration_status";
        f["description"] = "Set long-running model/agent iteration progress status.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pbusy = json::object(); pbusy["type"] = "boolean"; pbusy["description"] = "Whether the model/agent is currently busy";
        json pcur = json::object(); pcur["type"] = "integer"; pcur["description"] = "Current iteration index";
        json ptotal = json::object(); ptotal["type"] = "integer"; ptotal["description"] = "Total iterations expected";
        json pphase = json::object(); pphase["type"] = "string"; pphase["description"] = "Current phase label";
        json pmsg = json::object(); pmsg["type"] = "string"; pmsg["description"] = "Optional human-readable status";
        pr["busy"] = pbusy; pr["current"] = pcur; pr["total"] = ptotal; pr["phase"] = pphase; pr["message"] = pmsg;
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // get_iteration_status
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "get_iteration_status";
        f["description"] = "Get current long-running model/agent iteration progress status.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // reset_iteration_status
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "reset_iteration_status";
        f["description"] = "Reset long-running model/agent iteration progress status to idle.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // compact_conversation
    json cc = json::object();
    cc["type"] = "function";
    json cc_f = json::object();
    cc_f["name"] = "compact_conversation";
    cc_f["description"] = "Compact agent conversation/history to the last N events.";
    json cc_p = json::object();
    cc_p["type"] = "object";
    json cc_prop = json::object();
    json cc_keep = json::object();
    cc_keep["type"] = "integer";
    cc_keep["description"] = "Number of most recent events to keep";
    json cc_ret = json::object();
    cc_ret["type"] = "integer";
    cc_ret["description"] = "Retention days for history purge (optional)";
    json cc_flush = json::object();
    cc_flush["type"] = "boolean";
    cc_flush["description"] = "Flush to disk after compaction";
    cc_prop["keep_last"] = cc_keep;
    cc_prop["retention_days"] = cc_ret;
    cc_prop["flush"] = cc_flush;
    cc_p["properties"] = cc_prop;
    cc_f["parameters"] = cc_p;
    cc["function"] = cc_f;
    tools.push_back(cc);

    // optimize_tool_selection
    json ots = json::object();
    ots["type"] = "function";
    json ots_f = json::object();
    ots_f["name"] = "optimize_tool_selection";
    ots_f["description"] = "Recommend a tool shortlist based on task intent.";
    json ots_p = json::object();
    ots_p["type"] = "object";
    json ots_prop = json::object();
    json ots_task = json::object();
    ots_task["type"] = "string";
    ots_task["description"] = "Task description to optimize tool selection";
    json ots_max = json::object();
    ots_max["type"] = "integer";
    ots_max["description"] = "Max tools to return";
    ots_prop["task"] = ots_task;
    ots_prop["max_tools"] = ots_max;
    ots_p["properties"] = ots_prop;
    ots_p["required"] = jstrArr({"task"});
    ots_f["parameters"] = ots_p;
    ots["function"] = ots_f;
    tools.push_back(ots);

    // resolve_symbol
    json rsym = json::object();
    rsym["type"] = "function";
    json rsym_f = json::object();
    rsym_f["name"] = "resolve_symbol";
    rsym_f["description"] = "Find symbol occurrences/definitions in the codebase.";
    json rsym_p = json::object();
    rsym_p["type"] = "object";
    json rsym_prop = json::object();
    json rsym_sym = json::object();
    rsym_sym["type"] = "string";
    rsym_sym["description"] = "Symbol name to resolve";
    json rsym_root = json::object();
    rsym_root["type"] = "string";
    rsym_root["description"] = "Optional root path";
    rsym_prop["symbol"] = rsym_sym;
    rsym_prop["root"] = rsym_root;
    rsym_p["properties"] = rsym_prop;
    rsym_p["required"] = jstrArr({"symbol"});
    rsym_f["parameters"] = rsym_p;
    rsym["function"] = rsym_f;
    tools.push_back(rsym);

    // read_lines
    json rl = json::object();
    rl["type"] = "function";
    json rl_f = json::object();
    rl_f["name"] = "read_lines";
    rl_f["description"] = "Read a specific line range from a file.";
    json rl_p = json::object();
    rl_p["type"] = "object";
    json rl_prop = json::object();
    json rl_path = json::object();
    rl_path["type"] = "string";
    rl_path["description"] = "Absolute path to the file";
    json rl_start = json::object();
    rl_start["type"] = "integer";
    rl_start["description"] = "Start line (1-based)";
    json rl_end = json::object();
    rl_end["type"] = "integer";
    rl_end["description"] = "End line (1-based)";
    json rl_range = json::object();
    rl_range["type"] = "string";
    rl_range["description"] = "Line range string 'start-end'";
    rl_prop["path"] = rl_path;
    rl_prop["start_line"] = rl_start;
    rl_prop["end_line"] = rl_end;
    rl_prop["range"] = rl_range;
    rl_p["properties"] = rl_prop;
    rl_p["required"] = jstrArr({"path"});
    rl_f["parameters"] = rl_p;
    rl["function"] = rl_f;
    tools.push_back(rl);

    // plan_code_exploration
    json pce = json::object();
    pce["type"] = "function";
    json pce_f = json::object();
    pce_f["name"] = "plan_code_exploration";
    pce_f["description"] = "Analyze workspace and generate a targeted exploration plan.";
    json pce_p = json::object();
    pce_p["type"] = "object";
    json pce_prop = json::object();
    json pce_goal = json::object();
    pce_goal["type"] = "string";
    pce_goal["description"] = "Exploration goal";
    json pce_root = json::object();
    pce_root["type"] = "string";
    pce_root["description"] = "Optional workspace root";
    pce_prop["goal"] = pce_goal;
    pce_prop["root"] = pce_root;
    pce_p["properties"] = pce_prop;
    pce_p["required"] = jstrArr({"goal"});
    pce_f["parameters"] = pce_p;
    pce["function"] = pce_f;
    tools.push_back(pce);

    // search_files
    json sf = json::object();
    sf["type"] = "function";
    json sf_f = json::object();
    sf_f["name"] = "search_files";
    sf_f["description"] = "Search for files by glob or regex pattern.";
    json sf_p = json::object();
    sf_p["type"] = "object";
    json sf_prop = json::object();
    json sf_pat = json::object();
    sf_pat["type"] = "string";
    sf_pat["description"] = "Glob or regex pattern";
    json sf_root = json::object();
    sf_root["type"] = "string";
    sf_root["description"] = "Optional search root";
    json sf_rx = json::object();
    sf_rx["type"] = "boolean";
    sf_rx["description"] = "Treat pattern as regex";
    json sf_max = json::object();
    sf_max["type"] = "integer";
    sf_max["description"] = "Maximum results";
    sf_prop["pattern"] = sf_pat;
    sf_prop["root"] = sf_root;
    sf_prop["regex"] = sf_rx;
    sf_prop["max_results"] = sf_max;
    sf_p["properties"] = sf_prop;
    sf_p["required"] = jstrArr({"pattern"});
    sf_f["parameters"] = sf_p;
    sf["function"] = sf_f;
    tools.push_back(sf);

    // restore_checkpoint
    json rc = json::object();
    rc["type"] = "function";
    json rc_f = json::object();
    rc_f["name"] = "restore_checkpoint";
    rc_f["description"] = "Restore workflow and optionally reload history log.";
    json rc_p = json::object();
    rc_p["type"] = "object";
    json rc_prop = json::object();
    json rc_path = json::object();
    rc_path["type"] = "string";
    rc_path["description"] = "Workflow checkpoint path";
    json rc_hist = json::object();
    rc_hist["type"] = "string";
    rc_hist["description"] = "Optional history log path";
    rc_prop["checkpoint_path"] = rc_path;
    rc_prop["history_log_path"] = rc_hist;
    rc_p["properties"] = rc_prop;
    rc_p["required"] = jstrArr({"checkpoint_path"});
    rc_f["parameters"] = rc_p;
    rc["function"] = rc_f;
    tools.push_back(rc);

    // evaluate_integration_audit_feasibility
    json eia = json::object();
    eia["type"] = "function";
    json eia_f = json::object();
    eia_f["name"] = "evaluate_integration_audit_feasibility";
    eia_f["description"] = "Assess workspace size/complexity for integration audit feasibility.";
    json eia_p = json::object();
    eia_p["type"] = "object";
    json eia_prop = json::object();
    json eia_root = json::object();
    eia_root["type"] = "string";
    eia_root["description"] = "Optional workspace root";
    eia_prop["root"] = eia_root;
    eia_p["properties"] = eia_prop;
    eia_f["parameters"] = eia_p;
    eia["function"] = eia_f;
    tools.push_back(eia);

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

    // list_directory
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "list_directory";
        f["description"] = "List the contents of a directory.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pp = json::object(); pp["type"] = "string"; pp["description"] = "Absolute path to directory";
        pr["path"] = pp;
        p["properties"] = pr;
        p["required"] = jstrArr({"path"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // delete_file
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "delete_file";
        f["description"] = "Delete a file at the given path.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pp = json::object(); pp["type"] = "string"; pp["description"] = "Absolute path to the file to delete";
        pr["path"] = pp;
        p["properties"] = pr;
        p["required"] = jstrArr({"path"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // rename_file
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "rename_file";
        f["description"] = "Move or rename a file.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json ps = json::object(); ps["type"] = "string"; ps["description"] = "Source file path";
        json pd = json::object(); pd["type"] = "string"; pd["description"] = "Destination file path";
        pr["source"] = ps; pr["destination"] = pd;
        p["properties"] = pr;
        p["required"] = jstrArr({"source", "destination"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // copy_file
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "copy_file";
        f["description"] = "Copy a file to a new location.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json ps = json::object(); ps["type"] = "string"; ps["description"] = "Source file path";
        json pd = json::object(); pd["type"] = "string"; pd["description"] = "Destination file path";
        json po = json::object(); po["type"] = "boolean"; po["description"] = "Overwrite if exists (default true)";
        pr["source"] = ps; pr["destination"] = pd; pr["overwrite"] = po;
        p["properties"] = pr;
        p["required"] = jstrArr({"source", "destination"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // make_directory
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "make_directory";
        f["description"] = "Create a directory (and parent directories).";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pp = json::object(); pp["type"] = "string"; pp["description"] = "Absolute path to create";
        pr["path"] = pp;
        p["properties"] = pr;
        p["required"] = jstrArr({"path"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // stat_file
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "stat_file";
        f["description"] = "Get file metadata (size, type, existence).";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pp = json::object(); pp["type"] = "string"; pp["description"] = "Absolute path to file or directory";
        pr["path"] = pp;
        p["properties"] = pr;
        p["required"] = jstrArr({"path"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // git_status
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "git_status";
        f["description"] = "Run git status in the workspace root.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pp = json::object(); pp["type"] = "string"; pp["description"] = "Optional workspace root path";
        pr["root"] = pp;
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // get_coverage
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "get_coverage";
        f["description"] = "Run code coverage analysis (BBCov or DiffCov).";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pf = json::object(); pf["type"] = "string"; pf["description"] = "Optional file filter";
        json pfn = json::object(); pfn["type"] = "string"; pfn["description"] = "Optional function name filter";
        json pm = json::object(); pm["type"] = "string"; pm["description"] = "Coverage mode: bbcov or diffcov (default diffcov)";
        pr["file"] = pf; pr["function_name"] = pfn; pr["mode"] = pm;
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // run_build
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "run_build";
        f["description"] = "Trigger a CMake build with optional target and configuration.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pt = json::object(); pt["type"] = "string"; pt["description"] = "Build target (default 'all')";
        json pc = json::object(); pc["type"] = "string"; pc["description"] = "Build config (default 'Release')";
        pr["target"] = pt; pr["config"] = pc;
        p["properties"] = pr;
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // apply_hotpatch
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "apply_hotpatch";
        f["description"] = "Apply a runtime hotpatch via memory, byte-search, or server layer.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pl = json::object(); pl["type"] = "string"; pl["description"] = "Patch layer: memory, byte, or server";
        json ptgt = json::object(); ptgt["type"] = "string"; ptgt["description"] = "Target address (memory), module name (byte), or patch name (server)";
        json pd = json::object(); pd["type"] = "string"; pd["description"] = "Hex data (memory), PATTERN:REPLACEMENT hex (byte), or 'remove' (server)";
        pr["layer"] = pl; pr["target"] = ptgt; pr["data"] = pd;
        p["properties"] = pr;
        p["required"] = jstrArr({"layer", "target"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    // disk_recovery
    {
        json t = json::object(); t["type"] = "function";
        json f = json::object();
        f["name"] = "disk_recovery";
        f["description"] = "WD My Book USB bridge recovery agent. Actions: scan, init, extract_key, run, abort, stats, reset.";
        json p = json::object(); p["type"] = "object";
        json pr = json::object();
        json pa = json::object(); pa["type"] = "string"; pa["description"] = "Action: scan, init, extract_key, run, abort, stats, reset";
        json pd = json::object(); pd["type"] = "integer"; pd["description"] = "Drive number 0-15 (required for init)";
        pr["action"] = pa; pr["drive"] = pd;
        p["properties"] = pr;
        p["required"] = jstrArr({"action"});
        f["parameters"] = p; t["function"] = f; tools.push_back(t);
    }

    return tools;
}

std::string AgentToolHandlers::GetSystemPrompt(const std::string& cwd, const std::vector<std::string>& openFiles)
{
    std::string filesStr;
    for (const auto& f : openFiles)
    {
        filesStr += "- " + f + "\n";
    }

    std::ostringstream ss;
    ss << "You are RawrXD Agent, a local high-performance autonomous coding assistant.\n"
       << "You have direct access to the filesystem and terminal.\n\n"
       << "Current Directory: " << cwd << "\n"
       << "Open Files:\n"
       << (filesStr.empty() ? "  (none)\n" : filesStr) << "\n"
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

namespace
{

struct DaeTraceState
{
    std::mutex mu;
    std::ofstream stream;
    bool initialized = false;
    bool enabled = false;
    uint64_t nextSeq = 1;
    RawrXD::DAE::ContentHash lastEventHash = RawrXD::DAE::ContentHash::zero();
    std::chrono::steady_clock::time_point sessionStart = std::chrono::steady_clock::now();
};

DaeTraceState& GetDaeTraceState()
{
    static DaeTraceState s;
    return s;
}

std::string BuildDaeTracePath()
{
    std::error_code ec;
    fs::path tempRoot = fs::temp_directory_path(ec);
    if (ec || tempRoot.empty())
    {
        return "rawrxd_dae_tool_loop.trace";
    }

    fs::path fileName =
        fs::path("rawrxd_dae_tool_loop_" + std::to_string(::GetCurrentProcessId()) + ".trace");
    return (tempRoot / fileName).string();
}

std::string HashToHex(const RawrXD::DAE::ContentHash& h)
{
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (int i = 0; i < 32; ++i)
    {
        out[2 * i] = kHex[(h.bytes[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[h.bytes[i] & 0xF];
    }
    return out;
}

RawrXD::DAE::ContentHash EventHash(const RawrXD::DAE::TraceEvent& e)
{
    std::ostringstream os;
    os << e.seq << '|' << e.timestampUs << '|' << e.opTypeName << '|' << e.targetPath << '|'
       << HashToHex(e.stateBeforeHash) << '|' << HashToHex(e.actionDigest) << '|'
       << HashToHex(e.stateAfterHash) << '|' << HashToHex(e.prevEventHash) << '|'
       << (e.succeeded ? 1 : 0);
    return RawrXD::DAE::ShadowFilesystem::ComputeHash(os.str());
}

RawrXD::Agent::ToolOutcome OutcomeFromString(const std::string& outcome)
{
    const std::string lower = ToLowerCopy(outcome);
    if (lower == "success")
        return RawrXD::Agent::ToolOutcome::Success;
    if (lower == "partial_success")
        return RawrXD::Agent::ToolOutcome::PartialSuccess;
    if (lower == "validation_failed")
        return RawrXD::Agent::ToolOutcome::ValidationFailed;
    if (lower == "sandbox_blocked")
        return RawrXD::Agent::ToolOutcome::SandboxBlocked;
    if (lower == "timeout")
        return RawrXD::Agent::ToolOutcome::Timeout;
    if (lower == "cancelled")
        return RawrXD::Agent::ToolOutcome::Cancelled;
    if (lower == "not_found")
        return RawrXD::Agent::ToolOutcome::NotFound;
    if (lower == "rate_limited")
        return RawrXD::Agent::ToolOutcome::RateLimited;
    return RawrXD::Agent::ToolOutcome::ExecutionError;
}

RawrXD::Agent::ToolOutcome OutcomeFromDaeError(RawrXD::DAE::ToolError err)
{
    using RawrXD::Agent::ToolOutcome;
    switch (err)
    {
    case RawrXD::DAE::ToolError::None:
        return ToolOutcome::Success;
    case RawrXD::DAE::ToolError::NotFound:
        return ToolOutcome::NotFound;
    case RawrXD::DAE::ToolError::InvalidArgs:
        return ToolOutcome::ValidationFailed;
    case RawrXD::DAE::ToolError::Timeout:
        return ToolOutcome::Timeout;
    case RawrXD::DAE::ToolError::SideEffectDenied:
    case RawrXD::DAE::ToolError::PolicyViolation:
        return ToolOutcome::SandboxBlocked;
    case RawrXD::DAE::ToolError::IdempotencyViolation:
    case RawrXD::DAE::ToolError::ExecutionFailed:
    default:
        return ToolOutcome::ExecutionError;
    }
}

RawrXD::DAE::ToolError DaeErrorFromOutcome(RawrXD::Agent::ToolOutcome outcome)
{
    switch (outcome)
    {
    case RawrXD::Agent::ToolOutcome::Success:
    case RawrXD::Agent::ToolOutcome::PartialSuccess:
        return RawrXD::DAE::ToolError::None;
    case RawrXD::Agent::ToolOutcome::ValidationFailed:
        return RawrXD::DAE::ToolError::InvalidArgs;
    case RawrXD::Agent::ToolOutcome::SandboxBlocked:
        return RawrXD::DAE::ToolError::SideEffectDenied;
    case RawrXD::Agent::ToolOutcome::Timeout:
        return RawrXD::DAE::ToolError::Timeout;
    case RawrXD::Agent::ToolOutcome::NotFound:
        return RawrXD::DAE::ToolError::NotFound;
    case RawrXD::Agent::ToolOutcome::Cancelled:
    case RawrXD::Agent::ToolOutcome::RateLimited:
        return RawrXD::DAE::ToolError::PolicyViolation;
    case RawrXD::Agent::ToolOutcome::ExecutionError:
    default:
        return RawrXD::DAE::ToolError::ExecutionFailed;
    }
}

bool ShouldUseDaeAbi(const std::string& name)
{
    return name == "read_file" || name == "search_code" || name == "replace_in_file";
}

std::string DeriveTraceTarget(const std::string& toolName, const json& args)
{
    static const char* const keys[] = {"path", "file", "filePath", "directory", "dir", "target", "query"};
    for (const char* key : keys)
    {
        if (args.contains(key) && args[key].is_string())
            return args[key].get<std::string>();
    }
    return toolName;
}

std::string BuildDaeIdempotencyKey(const std::string& name, const json& args)
{
    static std::atomic<uint64_t> s_seq{0};
    const uint64_t localSeq = ++s_seq;
    const auto hash = RawrXD::DAE::ShadowFilesystem::ComputeHash(args.dump());
    return name + ":" + std::to_string(localSeq) + ":" + std::to_string(hash.bytes[0]);
}

void EnsureDaeTraceReadyLocked(DaeTraceState& state)
{
    if (state.initialized)
        return;

    state.initialized = true;
    state.sessionStart = std::chrono::steady_clock::now();
    state.stream.open(BuildDaeTracePath(), std::ios::out | std::ios::trunc);
    state.enabled = state.stream.is_open();
}

void AppendDaeTraceEvent(const std::string& toolName,
                         const json& args,
                         const RawrXD::Agent::ToolCallResult& result)
{
    DaeTraceState& state = GetDaeTraceState();
    std::lock_guard<std::mutex> lock(state.mu);
    EnsureDaeTraceReadyLocked(state);
    if (!state.enabled)
        return;

    RawrXD::DAE::TraceEvent ev;
    ev.seq = state.nextSeq++;
    ev.timestampUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - state.sessionStart)
            .count());
    ev.opTypeName = toolName;
    ev.targetPath = DeriveTraceTarget(toolName, args);
    ev.prevEventHash = state.lastEventHash;
    ev.stateBeforeHash = RawrXD::DAE::ShadowFilesystem::ComputeHash(toolName + "|" + args.dump());
    ev.actionDigest =
        RawrXD::DAE::ShadowFilesystem::ComputeHash(toolName + "|" + args.dump() + "|" + result.outcomeString());
    ev.stateAfterHash = RawrXD::DAE::ShadowFilesystem::ComputeHash(result.toJson().dump());
    ev.succeeded = result.isSuccess();

    state.stream << ev.seq << '\t' << ev.timestampUs << '\t' << ev.opTypeName << '\t' << ev.targetPath << '\t'
                 << HashToHex(ev.stateBeforeHash) << '\t' << HashToHex(ev.actionDigest) << '\t'
                 << HashToHex(ev.stateAfterHash) << '\t' << HashToHex(ev.prevEventHash) << '\t'
                 << (ev.succeeded ? 1 : 0) << '\n';

    if (!state.stream.good())
    {
        state.enabled = false;
        return;
    }

    state.lastEventHash = EventHash(ev);
}

ToolCallResult DispatchToolDirect(const std::string& name, const json& args)
{
    if (name == "read_file")
        return AgentToolHandlers::ToolReadFile(args);
    if (name == "write_file")
        return AgentToolHandlers::WriteFile(args);
    if (name == "replace_in_file")
        return AgentToolHandlers::ReplaceInFile(args);
    if (name == "list_dir" || name == "list_directory")
        return AgentToolHandlers::ListDir(args);
    if (name == "delete_file")
        return AgentToolHandlers::DeleteFile(args);
    if (name == "rename_file" || name == "move_file")
        return AgentToolHandlers::RenameFile(args);
    if (name == "copy_file")
        return AgentToolHandlers::CopyFile(args);
    if (name == "mkdir" || name == "create_directory" || name == "make_directory")
        return AgentToolHandlers::MakeDirectory(args);
    if (name == "stat_file" || name == "get_file_info")
        return AgentToolHandlers::StatFile(args);
    if (name == "git_status")
        return AgentToolHandlers::GitStatus(args);
    if (name == "execute_command")
        return AgentToolHandlers::ExecuteCommand(args);
    if (name == "run_shell")
        return AgentToolHandlers::RunShell(args);
    if (name == "search_code")
        return AgentToolHandlers::SearchCode(args);
    if (name == "semantic_search")
        return AgentToolHandlers::SemanticSearch(args);
    if (name == "mention_lookup")
        return AgentToolHandlers::MentionLookup(args);
    if (name == "next_edit_hint")
        return AgentToolHandlers::NextEditHint(args);
    if (name == "propose_multifile_edits")
        return AgentToolHandlers::ProposeMultiFileEdits(args);
    if (name == "load_rules")
        return AgentToolHandlers::LoadRules(args);
    if (name == "plan_tasks")
        return AgentToolHandlers::PlanTasks(args);
    if (name == "set_iteration_status")
        return AgentToolHandlers::SetIterationStatus(args);
    if (name == "get_iteration_status")
        return AgentToolHandlers::GetIterationStatus(args);
    if (name == "reset_iteration_status")
        return AgentToolHandlers::ResetIterationStatus(args);
    if (name == "get_diagnostics")
        return AgentToolHandlers::GetDiagnostics(args);
    if (name == "compact_conversation" || name == "compacted_conversation")
        return AgentToolHandlers::CompactConversation(args);
    if (name == "optimize_tool_selection")
        return AgentToolHandlers::OptimizeToolSelection(args);
    if (name == "resolve_symbol")
        return AgentToolHandlers::ResolveSymbol(args);
    if (name == "read_lines")
        return AgentToolHandlers::ReadLines(args);
    if (name == "plan_code_exploration")
        return AgentToolHandlers::PlanCodeExploration(args);
    if (name == "search_files")
        return AgentToolHandlers::SearchFiles(args);
    if (name == "restore_checkpoint")
        return AgentToolHandlers::RestoreCheckpoint(args);
    if (name == "evaluate_integration_audit_feasibility")
        return AgentToolHandlers::EvaluateIntegrationAuditFeasibility(args);
    if (name == "get_coverage")
        return AgentToolHandlers::GetCoverage(args);
    if (name == "run_build")
        return AgentToolHandlers::RunBuild(args);
    if (name == "apply_hotpatch")
        return AgentToolHandlers::ApplyHotpatch(args);
    if (name == "disk_recovery")
        return AgentToolHandlers::DiskRecovery(args);
    return ToolCallResult::NotFound(name);
}

void EnsureDaeBridgeToolsRegistered()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto& registry = RawrXD::DAE::ToolRegistry::Instance();
        const struct
        {
            const char* name;
            RawrXD::DAE::SideEffectClass sideEffects;
        } defs[] = {{"read_file", RawrXD::DAE::SideEffectClass::ReadOnly},
                    {"search_code", RawrXD::DAE::SideEffectClass::ReadOnly},
                    {"replace_in_file", RawrXD::DAE::SideEffectClass::WriteReal}};

        for (const auto& def : defs)
        {
            if (registry.Find(def.name) != nullptr)
                continue;

            registry.Register(RawrXD::DAE::ToolDescriptor{
                def.name,
                std::string("Bridge dispatch for ") + def.name,
                def.sideEffects,
                [toolName = std::string(def.name)](const RawrXD::DAE::ToolInvocation& inv)
                    -> RawrXD::DAE::ToolResult_v {
                    json parsed = json::parse(inv.argsJson, nullptr, false);
                    if (parsed.is_discarded() || !parsed.is_object())
                    {
                        return std::unexpected(RawrXD::DAE::ToolError::InvalidArgs);
                    }

                    ToolCallResult inner = DispatchToolDirect(toolName, parsed);
                    RawrXD::DAE::ToolResponse response;
                    response.status = DaeErrorFromOutcome(inner.outcome);
                    response.resultJson = inner.toJson().dump();
                    response.diagnostics = inner.error;
                    return response;
                }});
        }
    });
}

ToolCallResult DeserializeDaeResponse(const std::string& toolName,
                                      const json& args,
                                      const RawrXD::DAE::ToolResponse& response)
{
    ToolCallResult result;
    result.toolName = toolName;
    result.argsUsed = args;
    result.durationMs = static_cast<int64_t>(response.elapsed.count() / 1000);

    json payload = json::parse(response.resultJson, nullptr, false);
    if (!payload.is_discarded() && payload.is_object())
    {
        if (payload.contains("outcome") && payload["outcome"].is_string())
            result.outcome = OutcomeFromString(payload["outcome"].get<std::string>());
        else
            result.outcome = OutcomeFromDaeError(response.status);

        if (payload.contains("output") && payload["output"].is_string())
            result.output = payload["output"].get<std::string>();
        if (payload.contains("error") && payload["error"].is_string())
            result.error = payload["error"].get<std::string>();
        if (payload.contains("metadata") && payload["metadata"].is_object())
            result.metadata = payload["metadata"];
        if (payload.contains("file") && payload["file"].is_string())
            result.filePath = payload["file"].get<std::string>();
        if (payload.contains("bytes_read") && payload["bytes_read"].is_number())
            result.bytesRead = payload["bytes_read"].get<size_t>();
        if (payload.contains("bytes_written") && payload["bytes_written"].is_number())
            result.bytesWritten = payload["bytes_written"].get<size_t>();
        if (payload.contains("lines_affected") && payload["lines_affected"].is_number_integer())
            result.linesAffected = payload["lines_affected"].get<int>();
    }
    else
    {
        result.outcome = OutcomeFromDaeError(response.status);
        if (response.status == RawrXD::DAE::ToolError::None)
            result.output = response.resultJson;
        else
            result.error = response.diagnostics.empty() ? "DAE invocation failed" : response.diagnostics;
    }

    if (!response.diagnostics.empty() && result.error.empty() && !result.isSuccess())
    {
        result.error = response.diagnostics;
    }

    return result;
}

ToolCallResult BuildDaeInvokeError(const std::string& toolName,
                                   const json& args,
                                   RawrXD::DAE::ToolError error)
{
    ToolCallResult failed = ToolCallResult::Error("DAE invoke failed", OutcomeFromDaeError(error));
    failed.toolName = toolName;
    failed.argsUsed = args;
    return failed;
}

} // namespace

// ============================================================================
// Generic dispatch — Instance / HasTool / Execute
// Used by DeterministicReplayEngine for transcript replay.
// ============================================================================
AgentToolHandlers& AgentToolHandlers::Instance()
{
    static AgentToolHandlers instance;
    return instance;
}

bool AgentToolHandlers::HasTool(const std::string& name) const
{
    static const char* const tools[] = {"read_file",
                                        "write_file",
                                        "replace_in_file",
                                        "list_dir",
                                        "list_directory",
                                        "delete_file",
                                        "rename_file",
                                        "move_file",
                                        "copy_file",
                                        "mkdir",
                                        "create_directory",
                                        "stat_file",
                                        "get_file_info",
                                        "git_status",
                                        "execute_command",
                                        "run_shell",
                                        "search_code",
                                        "semantic_search",
                                        "mention_lookup",
                                        "next_edit_hint",
                                        "propose_multifile_edits",
                                        "load_rules",
                                        "plan_tasks",
                                        "set_iteration_status",
                                        "get_iteration_status",
                                        "reset_iteration_status",
                                        "get_diagnostics",
                                        "compact_conversation",
                                        "compacted_conversation",
                                        "optimize_tool_selection",
                                        "resolve_symbol",
                                        "read_lines",
                                        "plan_code_exploration",
                                        "search_files",
                                        "restore_checkpoint",
                                        "evaluate_integration_audit_feasibility",
                                        "get_coverage",
                                        "run_build",
                                        "apply_hotpatch",
                                        "disk_recovery",
                                        "make_directory"};
    for (const auto* t : tools)
    {
        if (name == t)
            return true;
    }
    return false;
}

ToolCallResult AgentToolHandlers::Execute(const std::string& name, const nlohmann::json& args)
{
    // Strict schema enforcement for function calls
    if (name.empty())
    {
        return ToolCallResult::Validation("Tool name required");
    }
    if (!args.is_object())
    {
        return ToolCallResult::Validation("Tool args must be a JSON object");
    }
    if (!HasTool(name))
    {
        return ToolCallResult::NotFound(name);
    }

    if (ShouldUseDaeAbi(name))
    {
        EnsureDaeBridgeToolsRegistered();

        RawrXD::DAE::ToolInvocation inv;
        inv.toolName = name;
        inv.idempotencyKey = BuildDaeIdempotencyKey(name, args);
        inv.argsJson = args.dump();
        inv.dryRun = false;

        RawrXD::DAE::ToolPolicy policy;
        policy.timeoutMs = s_guardrails.commandTimeoutMs;
        policy.allowNetwork = true;
        policy.allowProcess = true;
        policy.allowRealWrite = true;

        auto daeResult = RawrXD::DAE::ToolRegistry::Instance().Invoke(inv, policy);
        if (!daeResult.has_value())
        {
            ToolCallResult failed = BuildDaeInvokeError(name, args, daeResult.error());
            AppendDaeTraceEvent(name, args, failed);
            return failed;
        }

        ToolCallResult routed = DeserializeDaeResponse(name, args, *daeResult);
        AppendDaeTraceEvent(name, args, routed);
        return routed;
    }

    return DispatchToolDirect(name, args);
}
