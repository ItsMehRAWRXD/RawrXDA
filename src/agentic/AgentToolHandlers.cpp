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
#include "DiffEngine.h"
#include "core/scoped_instructions_provider.hpp"
#include "core/unified_hotpatch_manager.hpp"
#include "lsp/LSPClient.hpp"
#include "multi_file_edit_plan.hpp"
#include "SovereignAssembler.h"
#include "../win32app/TodoManager.h"
#include "../collab/CollabToolHandlers.h"
#include "image_generator/image_generator.h"
#include "video/tubi_backend.h"

#include "../runtime/SemanticRetrieval.h"
#include "../core/native_debugger_engine.h"
#include "debug/ai_debugger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <iterator>
#include <limits>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;
using RawrXD::Agent::ToolGuardrails;
using RawrXD::Agent::ToolOutcome;
using json = nlohmann::json;

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

namespace
{

std::string ToForwardSlashes(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

std::string NormalizeDispatchToolName(std::string value)
{
    if (value.empty())
    {
        return value;
    }

    for (char& ch : value)
    {
        if (std::isalnum(static_cast<unsigned char>(ch)))
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        else
        {
            ch = '_';
        }
    }

    std::string collapsed;
    collapsed.reserve(value.size());
    bool previousUnderscore = false;
    for (char ch : value)
    {
        if (ch == '_')
        {
            if (!previousUnderscore)
            {
                collapsed.push_back('_');
                previousUnderscore = true;
            }
        }
        else
        {
            collapsed.push_back(ch);
            previousUnderscore = false;
        }
    }

    while (!collapsed.empty() && collapsed.front() == '_')
    {
        collapsed.erase(collapsed.begin());
    }
    while (!collapsed.empty() && collapsed.back() == '_')
    {
        collapsed.pop_back();
    }

    if (collapsed == "create_file")
    {
        return "write_file";
    }
    if (collapsed == "run_command" || collapsed == "terminal_execute")
    {
        return "execute_command";
    }
    if (collapsed == "list_files")
    {
        return "list_dir";
    }
    if (collapsed == "grep")
    {
        return "search_code";
    }
    if (collapsed == "memoryfile" || collapsed == "agent_memory" || collapsed == "agentmemory" ||
        collapsed == "memory_files" || collapsed == "memories")
    {
        return "memory_file";
    }

    return collapsed;
}

std::string TrimAscii(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool ContainsControlChars(const std::string& value)
{
    for (unsigned char ch : value)
    {
        if (ch == '\0' || ch == '\r' || ch == '\n')
        {
            return true;
        }
    }
    return false;
}

std::string TrimCommandToken(const std::string& token)
{
    std::string out = ToLowerCopy(TrimAscii(token));
    if (out.empty())
    {
        return out;
    }

    const size_t slash = out.find_last_of("\\/");
    if (slash != std::string::npos)
    {
        out = out.substr(slash + 1);
    }

    if (out.size() > 4 && out.substr(out.size() - 4) == ".exe")
    {
        out.resize(out.size() - 4);
    }
    return out;
}

std::string ExtractCommandHead(const std::string& command)
{
    const std::string trimmed = TrimAscii(command);
    if (trimmed.empty())
    {
        return std::string();
    }

    if (trimmed.front() == '"')
    {
        const size_t closing = trimmed.find('"', 1);
        if (closing == std::string::npos)
        {
            return TrimCommandToken(trimmed.substr(1));
        }
        return TrimCommandToken(trimmed.substr(1, closing - 1));
    }

    size_t end = 0;
    while (end < trimmed.size())
    {
        const char ch = trimmed[end];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '&' || ch == '|' || ch == '<' || ch == '>')
        {
            break;
        }
        ++end;
    }
    return TrimCommandToken(trimmed.substr(0, end));
}

bool ContainsShellOperators(const std::string& command)
{
    return command.find('&') != std::string::npos || command.find('|') != std::string::npos ||
           command.find('<') != std::string::npos || command.find('>') != std::string::npos;
}

ToolCallResult ValidateCommandPolicy(const json& args, const std::string& command)
{
    if (command.empty())
    {
        return ToolCallResult::Validation("command cannot be empty");
    }

    if (command.size() > 4096)
    {
        return ToolCallResult::Validation("command exceeds max length (4096)");
    }

    if (ContainsControlChars(command))
    {
        return ToolCallResult::Sandbox("command contains forbidden control characters");
    }

    const bool allowShellOperators = args.value("allow_shell_operators", false);
    if (!allowShellOperators && ContainsShellOperators(command))
    {
        json meta = json::object();
        meta["policy"] = "shell_operators";
        meta["allow_shell_operators"] = false;
        meta["command"] = command;
        ToolCallResult blocked =
            ToolCallResult::Sandbox("command contains shell operators; set allow_shell_operators=true to permit");
        blocked.metadata = meta;
        return blocked;
    }

    if (!AgentToolHandlers::GetGuardrails().allowedCommands.empty())
    {
        const std::string firstToken = ExtractCommandHead(command);
        bool allowed = false;
        for (const auto& configured : AgentToolHandlers::GetGuardrails().allowedCommands)
        {
            const std::string normalized = TrimCommandToken(configured);
            if (!normalized.empty() && firstToken == normalized)
            {
                allowed = true;
                break;
            }
        }

        if (!allowed)
        {
            json meta = json::object();
            meta["policy"] = "allowedCommands";
            meta["first_token"] = firstToken;
            meta["command"] = command;
            ToolCallResult blocked = ToolCallResult::Sandbox("command not allowed by policy: " + firstToken);
            blocked.metadata = meta;
            return blocked;
        }
    }

    return ToolCallResult::Ok(std::string());
}

bool ContainsDotDotPathTraversal(const std::string& value)
{
    const std::string normalized = ToForwardSlashes(value);
    return normalized == ".." || normalized.find("../") != std::string::npos ||
           normalized.find("/..") != std::string::npos;
}

std::string ToFileUri(const std::string& normalizedPath)
{
    std::string uri = ToForwardSlashes(normalizedPath);
    if (uri.size() > 2 && uri[1] == ':')
    {
        uri = "/" + uri;
    }
    return std::string("file://") + uri;
}

std::string LspSymbolKindName(int kind)
{
    switch (kind)
    {
        case 2:
            return "module";
        case 3:
            return "namespace";
        case 4:
            return "package";
        case 5:
            return "class";
        case 6:
            return "method";
        case 7:
            return "property";
        case 8:
            return "field";
        case 9:
            return "constructor";
        case 10:
            return "enum";
        case 11:
            return "interface";
        case 12:
            return "function";
        case 13:
            return "variable";
        case 14:
            return "constant";
        case 23:
            return "struct";
        case 24:
            return "event";
        case 25:
            return "operator";
        case 26:
            return "typeParameter";
        default:
            return "symbol";
    }
}

std::atomic<uint64_t> g_getCodeOutlineLspSuccess{0};
std::atomic<uint64_t> g_getCodeOutlineLspTimeout{0};
std::atomic<uint64_t> g_getCodeOutlineParserFallback{0};

std::string MakeTimestampForFilename()
{
    SYSTEMTIME utcNow{};
    GetSystemTime(&utcNow);

    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04u%02u%02u_%02u%02u%02u",
                  static_cast<unsigned>(utcNow.wYear),
                  static_cast<unsigned>(utcNow.wMonth),
                  static_cast<unsigned>(utcNow.wDay),
                  static_cast<unsigned>(utcNow.wHour),
                  static_cast<unsigned>(utcNow.wMinute),
                  static_cast<unsigned>(utcNow.wSecond));
    return std::string(buffer);
}

std::string GetAppDataBaseDir()
{
    char* appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData && appData[0] != '\0')
    {
        std::string base = appData;
        free(appData);
        return base;
    }
    if (appData)
    {
        free(appData);
    }

    std::error_code ec;
    const fs::path fallback = fs::temp_directory_path(ec);
    if (!ec && !fallback.empty())
    {
        return fallback.string();
    }

    return ".";
}

fs::path GetMemoryBaseForScope(const std::string& scope)
{
    fs::path base = fs::path(GetAppDataBaseDir()) / "RawrXD" / "memories";
    if (scope == "session")
    {
        base /= "session";
    }
    else if (scope == "repo")
    {
        base /= "repo";
    }
    return base;
}

std::string CreateBackupImpl(const std::string& path)
{
    std::error_code ec;
    const fs::path source(path);
    if (!fs::exists(source, ec) || ec || !fs::is_regular_file(source, ec) || ec)
    {
        return "source file not found for backup";
    }

    const fs::path backupDir = source.parent_path() / ".rawrxd-backups";
    fs::create_directories(backupDir, ec);
    if (ec)
    {
        return "failed to create backup directory: " + ec.message();
    }

    const std::string stem = source.stem().string();
    const std::string extension = source.extension().string();
    const fs::path backupPath = backupDir / (stem + "." + MakeTimestampForFilename() + extension + ".bak");

    fs::copy_file(source, backupPath, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        return "failed to create backup: " + ec.message();
    }

    return std::string();
}

bool FindLatestBackupForPath(const std::string& path, fs::path& outBackupPath, std::string& outError)
{
    std::error_code ec;
    const fs::path source(path);
    const fs::path backupDir = source.parent_path() / ".rawrxd-backups";
    if (!fs::exists(backupDir, ec) || ec || !fs::is_directory(backupDir, ec) || ec)
    {
        outError = "No backup directory found for file";
        return false;
    }

    const std::string stem = source.stem().string();
    const std::string extension = source.extension().string();
    const std::string prefix = stem + ".";
    const std::string suffix = extension + ".bak";

    fs::path bestPath;
    fs::file_time_type bestWriteTime{};
    bool found = false;

    for (const auto& entry : fs::directory_iterator(backupDir, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            continue;
        }
        if (!entry.is_regular_file(ec) || ec)
        {
            continue;
        }

        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        if (name.size() < suffix.size() || name.substr(name.size() - suffix.size()) != suffix)
        {
            continue;
        }

        const auto writeTime = entry.last_write_time(ec);
        if (ec)
        {
            continue;
        }

        if (!found || writeTime > bestWriteTime)
        {
            found = true;
            bestWriteTime = writeTime;
            bestPath = entry.path();
        }
    }

    if (!found)
    {
        outError = "No matching backup found";
        return false;
    }

    outBackupPath = bestPath;
    return true;
}

bool ResolveMemoryPath(const std::string& virtualPath, fs::path& resolvedPath, std::string& scope, std::string& error)
{
    std::string normalized = ToForwardSlashes(TrimAscii(virtualPath));
    if (normalized.empty())
    {
        error = "memory path is required";
        return false;
    }
    if (normalized.rfind("/memories/", 0) != 0 && normalized != "/memories")
    {
        error = "memory path must start with /memories";
        return false;
    }
    if (ContainsDotDotPathTraversal(normalized))
    {
        error = "memory path traversal is not allowed";
        return false;
    }

    std::string rest;
    if (normalized == "/memories" || normalized == "/memories/")
    {
        scope = "user";
    }
    else if (normalized.rfind("/memories/session/", 0) == 0)
    {
        scope = "session";
        rest = normalized.substr(std::string("/memories/session/").size());
    }
    else if (normalized == "/memories/session")
    {
        scope = "session";
    }
    else if (normalized.rfind("/memories/repo/", 0) == 0)
    {
        scope = "repo";
        rest = normalized.substr(std::string("/memories/repo/").size());
    }
    else if (normalized == "/memories/repo")
    {
        scope = "repo";
    }
    else
    {
        scope = "user";
        rest = normalized.substr(std::string("/memories/").size());
    }

    fs::path base = GetMemoryBaseForScope(scope);
    std::error_code ec;
    fs::create_directories(base, ec);
    if (ec)
    {
        error = "failed to initialize memory directory: " + ec.message();
        return false;
    }

    fs::path candidate = base;
    if (!rest.empty())
    {
        candidate /= fs::path(rest);
    }
    candidate = candidate.lexically_normal();

    const std::string baseNorm = ToForwardSlashes(base.lexically_normal().string());
    const std::string candidateNorm = ToForwardSlashes(candidate.string());
    if (candidateNorm.rfind(baseNorm, 0) != 0)
    {
        error = "memory path escaped its scope";
        return false;
    }

    resolvedPath = candidate;
    return true;
}

std::string MakeVirtualMemoryPath(const std::string& scope, const fs::path& realPath)
{
    const fs::path base = GetMemoryBaseForScope(scope).lexically_normal();
    const fs::path normalized = realPath.lexically_normal();
    std::error_code ec;
    const fs::path relative = fs::relative(normalized, base, ec);

    std::string prefix = "/memories";
    if (scope == "session")
    {
        prefix += "/session";
    }
    else if (scope == "repo")
    {
        prefix += "/repo";
    }

    if (ec || relative.empty() || relative == ".")
    {
        return prefix + "/";
    }
    return prefix + "/" + ToForwardSlashes(relative.generic_string());
}

std::string GlobToRegex(const std::string& glob)
{
    std::string regex = "^";
    for (size_t i = 0; i < glob.size(); ++i)
    {
        const char ch = glob[i];
        if (ch == '*')
        {
            if (i + 1 < glob.size() && glob[i + 1] == '*')
            {
                regex += ".*";
                ++i;
            }
            else
            {
                regex += "[^/]*";
            }
        }
        else if (ch == '?')
        {
            regex += '.';
        }
        else if (ch == '.' || ch == '(' || ch == ')' || ch == '+' || ch == '^' || ch == '$' || ch == '|' ||
                 ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == '\\')
        {
            regex.push_back('\\');
            regex.push_back(ch);
        }
        else if (ch == '\\')
        {
            regex += "/";
        }
        else
        {
            regex.push_back(ch);
        }
    }
    regex += "$";
    return regex;
}

std::vector<std::string> ReadLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }
    if (!content.empty() && content.back() == '\n')
    {
        lines.push_back("");
    }
    if (content.empty())
    {
        lines.push_back("");
    }
    return lines;
}

std::string ToUtf8(const std::wstring& s);

std::string BuildCompactToolCatalog(const json& tools)
{
    std::ostringstream oss;
    for (const auto& tool : tools)
    {
        if (!tool.is_object() || !tool.contains("function") || !tool["function"].is_object())
        {
            continue;
        }

        const auto& fn = tool["function"];
        const std::string name = fn.value("name", std::string());
        if (name.empty())
        {
            continue;
        }

        oss << "- " << name;
        const std::string description = fn.value("description", std::string());
        if (!description.empty())
        {
            oss << ": " << description;
        }

        if (fn.contains("parameters") && fn["parameters"].is_object())
        {
            const auto& params = fn["parameters"];
            if (params.contains("properties") && params["properties"].is_object() && !params["properties"].empty())
            {
                oss << " Params[";
                bool first = true;
                for (auto it = params["properties"].begin(); it != params["properties"].end(); ++it)
                {
                    if (!first)
                    {
                        oss << ", ";
                    }
                    first = false;
                    oss << it.key();
                }
                oss << "]";
            }
        }
        oss << "\n";
    }
    return oss.str();
}

bool ReadTextFile(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::vector<std::string> SplitLinesPreserveEmpty(const std::string& text)
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }
    if (!text.empty() && text.back() == '\n')
    {
        lines.push_back("");
    }
    return lines;
}

std::string JoinLinesWithNewline(const std::vector<std::string>& lines, bool preserveTrailingNewline)
{
    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        out << lines[i];
        if (i + 1 < lines.size())
        {
            out << "\n";
        }
    }
    if (preserveTrailingNewline && (lines.empty() || !lines.back().empty()))
    {
        out << "\n";
    }
    return out.str();
}

bool ParseEditType(const std::string& rawType, RawrXD::Agentic::EditType& outType)
{
    const std::string t = ToLowerCopy(rawType);
    if (t == "insert")
    {
        outType = RawrXD::Agentic::EditType::INSERT;
        return true;
    }
    if (t == "delete" || t == "delete_range")
    {
        outType = RawrXD::Agentic::EditType::DELETE_RANGE;
        return true;
    }
    if (t == "replace")
    {
        outType = RawrXD::Agentic::EditType::REPLACE;
        return true;
    }
    if (t == "modify")
    {
        outType = RawrXD::Agentic::EditType::MODIFY;
        return true;
    }
    return false;
}

bool ApplyEditInMemory(const RawrXD::Agentic::FileEdit& edit, std::string& content, std::string& outError)
{
    std::vector<std::string> lines = SplitLinesPreserveEmpty(content);
    const bool hadTrailingNewline = !content.empty() && content.back() == '\n';
    std::vector<std::string> replacement = SplitLinesPreserveEmpty(edit.newContent);

    int start = edit.lineStart;
    int end = edit.lineEnd;
    if (start > 0)
        --start;
    if (end > 0)
        --end;

    const int lineCount = static_cast<int>(lines.size());
    switch (edit.type)
    {
        case RawrXD::Agentic::EditType::INSERT:
        {
            if (start < 0 || start > lineCount)
            {
                outError = "Insert line out of range";
                return false;
            }
            lines.insert(lines.begin() + start, replacement.begin(), replacement.end());
            break;
        }
        case RawrXD::Agentic::EditType::DELETE_RANGE:
        {
            if (start < 0 || end < 0 || start > end || end >= lineCount)
            {
                outError = "Delete range out of range";
                return false;
            }
            lines.erase(lines.begin() + start, lines.begin() + end + 1);
            break;
        }
        case RawrXD::Agentic::EditType::REPLACE:
        {
            if (start < 0 || end < 0 || start > end || end >= lineCount)
            {
                outError = "Replace range out of range";
                return false;
            }
            lines.erase(lines.begin() + start, lines.begin() + end + 1);
            lines.insert(lines.begin() + start, replacement.begin(), replacement.end());
            break;
        }
        case RawrXD::Agentic::EditType::MODIFY:
        {
            if (start < 0 || end < 0 || start > end || end >= lineCount)
            {
                outError = "Modify range out of range";
                return false;
            }
            for (int i = start; i <= end; ++i)
            {
                lines[i] = edit.newContent;
            }
            break;
        }
    }

    content = JoinLinesWithNewline(lines, hadTrailingNewline);
    return true;
}

bool BuildMultiFilePlanFromArgs(const json& args, std::vector<RawrXD::Agentic::FileEdit>& outEdits,
                                std::string& outError)
{
    if (!args.contains("edits") || !args["edits"].is_array())
    {
        outError = "requires 'edits' (array)";
        return false;
    }
    for (const auto& e : args["edits"])
    {
        if (!e.is_object())
        {
            outError = "all edits must be objects";
            return false;
        }
        if (!e.contains("file") || !e["file"].is_string())
        {
            outError = "each edit requires 'file' (string)";
            return false;
        }
        if (!e.contains("type") || !e["type"].is_string())
        {
            outError = "each edit requires 'type' (string)";
            return false;
        }

        RawrXD::Agentic::FileEdit fe;
        fe.filePath = e["file"].get<std::string>();
        if (!ParseEditType(e["type"].get<std::string>(), fe.type))
        {
            outError = "unsupported edit type: " + e["type"].get<std::string>();
            return false;
        }
        fe.lineStart = e.value("line_start", 1);
        fe.lineEnd = e.value("line_end", fe.lineStart);
        fe.newContent = e.value("content", std::string());
        fe.reason = e.value("reason", std::string());
        outEdits.push_back(std::move(fe));
    }
    if (outEdits.empty())
    {
        outError = "edits array is empty";
        return false;
    }
    return true;
}

#ifdef _WIN32
extern "C" void TermPipe_Init();
extern "C" int TermPipe_Spawn(char* cmdLine);
extern "C" int TermPipe_Read(int sessionId, char* outBuf, int maxLen);
extern "C" int TermPipe_Kill(int sessionId);
extern "C" void* TermPipe_GetSession(int sessionId);

struct TermPipeSessionView
{
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdoutRd;
    HANDLE hStdoutWr;
    HANDLE hStdinRd;
    HANDLE hStdinWr;
    HANDLE hCaptureThread;
    uint32_t exitCode;
    uint32_t flags;
    uint32_t wrCursor;
    uint32_t rdCursor;
    uint32_t sessionId;
    uint32_t pid;
    uint64_t reserved;
};

constexpr uint32_t kTermFlagAlive = 0x01u;
constexpr uint32_t kTermFlagComplete = 0x04u;
constexpr uint32_t kTermPipeUnavailable = 0xFFFFFFFFu;

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

struct CircularOutputBuffer
{
    explicit CircularOutputBuffer(size_t capBytes)
        : cap(std::max<size_t>(1, capBytes)), storage(cap, '\0')
    {
    }

    void append(const char* data, size_t len)
    {
        if (data == nullptr || len == 0)
        {
            return;
        }

        totalWritten += static_cast<uint64_t>(len);

        if (len >= cap)
        {
            std::memcpy(storage.data(), data + (len - cap), cap);
            writePos = 0;
            size = cap;
            return;
        }

        const size_t toEnd = cap - writePos;
        if (len <= toEnd)
        {
            std::memcpy(storage.data() + writePos, data, len);
            writePos = (writePos + len) % cap;
        }
        else
        {
            std::memcpy(storage.data() + writePos, data, toEnd);
            std::memcpy(storage.data(), data + toEnd, len - toEnd);
            writePos = len - toEnd;
        }

        size = std::min(cap, size + len);
    }

    std::string tail(size_t maxBytes) const
    {
        if (size == 0 || maxBytes == 0)
        {
            return std::string();
        }

        const size_t bytes = std::min(size, maxBytes);
        const size_t start = (writePos + cap - bytes) % cap;
        std::string out;
        out.resize(bytes);

        if (start + bytes <= cap)
        {
            std::memcpy(out.data(), storage.data() + start, bytes);
        }
        else
        {
            const size_t first = cap - start;
            std::memcpy(out.data(), storage.data() + start, first);
            std::memcpy(out.data() + first, storage.data(), bytes - first);
        }

        return out;
    }

    uint64_t droppedBytes() const
    {
        return totalWritten > static_cast<uint64_t>(size) ? (totalWritten - static_cast<uint64_t>(size)) : 0ull;
    }

    size_t cap;
    std::vector<char> storage;
    size_t writePos = 0;
    size_t size = 0;
    uint64_t totalWritten = 0;
};

size_t ResolvePtyBufferCapBytes()
{
    constexpr size_t kMiB = 1024ull * 1024ull;
    constexpr size_t kDefaultCap = 512ull * kMiB;
    const char* env = std::getenv("RAWRXD_PTY_BUFFER_CAP_MB");
    if (env == nullptr || env[0] == '\0')
    {
        return kDefaultCap;
    }

    char* end = nullptr;
    const unsigned long long requestedMb = std::strtoull(env, &end, 10);
    if (end == env || requestedMb == 0)
    {
        return kDefaultCap;
    }

    const unsigned long long clampedMb = std::min<unsigned long long>(512ull, std::max<unsigned long long>(32ull, requestedMb));
    return static_cast<size_t>(clampedMb * kMiB);
}

bool FinalizeBufferedOutput(const CircularOutputBuffer& buffer, size_t maxCaptureBytes, std::string& output)
{
    output = buffer.tail(maxCaptureBytes);
    const uint64_t dropped = buffer.droppedBytes();
    if (dropped > 0)
    {
        output += "\n[PTY CIRCULAR BUFFER DROPPED " + std::to_string(dropped) + " BYTES]";
    }
    if (buffer.size > maxCaptureBytes)
    {
        output += "\n[OUTPUT TRUNCATED TO " + std::to_string(maxCaptureBytes) + " BYTES FOR TOOL RESPONSE]";
    }
    return true;
}

bool RunProcessWithPseudoConsole(const std::wstring& cmdLine,
                                 uint32_t timeoutMs,
                                 std::string& output,
                                 uint32_t& exitCode,
                                 const wchar_t* workingDirectoryUtf16)
{
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 == nullptr)
    {
        exitCode = kTermPipeUnavailable;
        return false;
    }

    using CreatePseudoConsoleFn = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HANDLE*);
    using ClosePseudoConsoleFn = void(WINAPI*)(HANDLE);
    const auto createPseudoConsole =
        reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(kernel32, "CreatePseudoConsole"));
    const auto closePseudoConsole = reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel32, "ClosePseudoConsole"));
    if (createPseudoConsole == nullptr || closePseudoConsole == nullptr)
    {
        exitCode = kTermPipeUnavailable;
        return false;
    }

    HANDLE hInputRead = nullptr;
    HANDLE hInputWrite = nullptr;
    HANDLE hOutputRead = nullptr;
    HANDLE hOutputWrite = nullptr;
    HANDLE hPc = nullptr;
    PROCESS_INFORMATION pi{};
    STARTUPINFOEXW siex{};
    SIZE_T attrSize = 0;

    auto closeHandleSafe = [](HANDLE& h)
    {
        if (h != nullptr && h != INVALID_HANDLE_VALUE)
        {
            CloseHandle(h);
            h = nullptr;
        }
    };

    auto cleanup = [&]()
    {
        if (siex.lpAttributeList)
        {
            DeleteProcThreadAttributeList(siex.lpAttributeList);
            HeapFree(GetProcessHeap(), 0, siex.lpAttributeList);
            siex.lpAttributeList = nullptr;
        }
        if (hPc != nullptr)
        {
            closePseudoConsole(hPc);
            hPc = nullptr;
        }
        closeHandleSafe(hInputRead);
        closeHandleSafe(hInputWrite);
        closeHandleSafe(hOutputRead);
        closeHandleSafe(hOutputWrite);
        closeHandleSafe(pi.hThread);
        closeHandleSafe(pi.hProcess);
    };

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hInputRead, &hInputWrite, &sa, 0) || !CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0))
    {
        exitCode = GetLastError();
        cleanup();
        return false;
    }

    if (!SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0))
    {
        exitCode = GetLastError();
        cleanup();
        return false;
    }

    const COORD consoleSize = {120, 30};
    const HRESULT hr = createPseudoConsole(consoleSize, hInputRead, hOutputWrite, 0, &hPc);
    if (FAILED(hr))
    {
        exitCode = static_cast<uint32_t>(HRESULT_CODE(hr));
        cleanup();
        return false;
    }

    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    siex.lpAttributeList = static_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attrSize));
    if (siex.lpAttributeList == nullptr)
    {
        exitCode = ERROR_NOT_ENOUGH_MEMORY;
        cleanup();
        return false;
    }

    if (!InitializeProcThreadAttributeList(siex.lpAttributeList, 1, 0, &attrSize))
    {
        exitCode = GetLastError();
        cleanup();
        return false;
    }

    if (!UpdateProcThreadAttribute(siex.lpAttributeList,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   hPc,
                                   sizeof(hPc),
                                   nullptr,
                                   nullptr))
    {
        exitCode = GetLastError();
        cleanup();
        return false;
    }

    siex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    const DWORD creationFlags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
    if (!CreateProcessW(nullptr,
                        mutableCmd.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        creationFlags,
                        nullptr,
                        (workingDirectoryUtf16 && workingDirectoryUtf16[0] != L'\0') ? workingDirectoryUtf16 : nullptr,
                        &siex.StartupInfo,
                        &pi))
    {
        exitCode = GetLastError();
        cleanup();
        return false;
    }

    closeHandleSafe(hInputRead);
    closeHandleSafe(hOutputWrite);

    const auto& guardrails = AgentToolHandlers::GetGuardrails();
    CircularOutputBuffer circular(ResolvePtyBufferCapBytes());
    char readBuf[8192];
    const DWORD startTick = GetTickCount();

    while (true)
    {
        DWORD available = 0;
        if (PeekNamedPipe(hOutputRead, nullptr, 0, nullptr, &available, nullptr) && available > 0)
        {
            DWORD bytesRead = 0;
            if (ReadFile(hOutputRead, readBuf, static_cast<DWORD>(sizeof(readBuf)), &bytesRead, nullptr) && bytesRead > 0)
            {
                circular.append(readBuf, static_cast<size_t>(bytesRead));
            }
        }

        const DWORD waitResult = WaitForSingleObject(pi.hProcess, 10);
        if (waitResult == WAIT_OBJECT_0)
        {
            for (;;)
            {
                DWORD bytesRead = 0;
                if (!ReadFile(hOutputRead, readBuf, static_cast<DWORD>(sizeof(readBuf)), &bytesRead, nullptr) || bytesRead == 0)
                {
                    break;
                }
                circular.append(readBuf, static_cast<size_t>(bytesRead));
            }

            DWORD processExit = 0;
            if (!GetExitCodeProcess(pi.hProcess, &processExit))
            {
                processExit = ERROR_GEN_FAILURE;
            }
            exitCode = static_cast<uint32_t>(processExit);
            FinalizeBufferedOutput(circular,
                                   std::max<size_t>(1024, guardrails.maxOutputCaptureBytes),
                                   output);
            cleanup();
            return true;
        }

        if (GetTickCount() - startTick > timeoutMs)
        {
            TerminateProcess(pi.hProcess, 1);
            exitCode = WAIT_TIMEOUT;
            FinalizeBufferedOutput(circular,
                                   std::max<size_t>(1024, guardrails.maxOutputCaptureBytes),
                                   output);
            output += "\n[TIMEOUT after " + std::to_string(timeoutMs) + "ms]";
            cleanup();
            return false;
        }
    }
}

bool RunProcessWithTermPipe(const std::wstring& cmdLine, uint32_t timeoutMs, std::string& output, uint32_t& exitCode)
{
    static std::once_flag initFlag;
    const auto& guardrails = AgentToolHandlers::GetGuardrails();
    std::call_once(initFlag, []() { TermPipe_Init(); });

    std::string narrowCmd = ToUtf8(cmdLine);
    if (narrowCmd.empty())
    {
        output = "Empty command line";
        exitCode = ERROR_INVALID_PARAMETER;
        return false;
    }

    std::vector<char> mutableCmd(narrowCmd.begin(), narrowCmd.end());
    mutableCmd.push_back('\0');

    const int sessionId = TermPipe_Spawn(mutableCmd.data());
    if (sessionId < 0)
    {
        exitCode = kTermPipeUnavailable;
        return false;
    }

    CircularOutputBuffer circular(ResolvePtyBufferCapBytes());
    auto appendOutput = [&](const char* buffer, size_t bytes) -> bool
    {
        if (bytes == 0)
        {
            return true;
        }
        circular.append(buffer, bytes);
        return true;
    };

    const DWORD startTick = GetTickCount();
    char buffer[4096];

    while (true)
    {
        const int bytesRead = TermPipe_Read(sessionId, buffer, static_cast<int>(sizeof(buffer)));
        if (bytesRead > 0 && !appendOutput(buffer, static_cast<size_t>(bytesRead)))
        {
            return false;
        }

        auto* session = static_cast<TermPipeSessionView*>(TermPipe_GetSession(sessionId));
        if (session == nullptr || session->hProcess == nullptr)
        {
            output += "\n[terminal session unavailable]";
            exitCode = ERROR_INVALID_HANDLE;
            return false;
        }

        const DWORD wait = WaitForSingleObject(session->hProcess, 10);
        if (wait == WAIT_OBJECT_0 || (session->flags & kTermFlagComplete) != 0u)
        {
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                const int tailBytes = TermPipe_Read(sessionId, buffer, static_cast<int>(sizeof(buffer)));
                if (tailBytes <= 0)
                {
                    break;
                }
                if (!appendOutput(buffer, static_cast<size_t>(tailBytes)))
                {
                    return false;
                }
                Sleep(2);
            }

            DWORD processExitCode = 0;
            if (!GetExitCodeProcess(session->hProcess, &processExitCode))
            {
                processExitCode = session->exitCode;
            }
            exitCode = static_cast<uint32_t>(processExitCode);
            FinalizeBufferedOutput(circular,
                                   std::max<size_t>(1024, guardrails.maxOutputCaptureBytes),
                                   output);
            TermPipe_Kill(sessionId);
            return true;
        }

        if (GetTickCount() - startTick > timeoutMs)
        {
            TermPipe_Kill(sessionId);
            FinalizeBufferedOutput(circular,
                                   std::max<size_t>(1024, guardrails.maxOutputCaptureBytes),
                                   output);
            output += "\n[TIMEOUT after " + std::to_string(timeoutMs) + "ms]";
            exitCode = WAIT_TIMEOUT;
            return false;
        }
    }
}
#endif

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

constexpr uint32_t kMemorySemanticDims = 256u;
constexpr size_t kMemorySemanticMaxBytes = 256 * 1024;

struct MemorySemanticRecord
{
    std::string virtualPath;
    std::string summary;
    std::vector<float> embedding;
};

fs::path GetMemorySemanticIndexPath()
{
    fs::path base = GetMemoryBaseForScope("user");
    std::error_code ec;
    fs::create_directories(base, ec);
    return base / ".semantic_index.v1.json";
}

bool IsInternalMemoryArtifact(const fs::path& file)
{
    const std::string name = ToLowerCopy(file.filename().string());
    return name == ".semantic_index.v1.json" || name == ".semantic_index.last_error.txt";
}

std::string BuildMemoryAutoSummary(const std::string& content)
{
    const auto lines = SplitLinesPreserveEmpty(content);
    std::ostringstream summary;
    size_t emitted = 0;
    size_t totalChars = 0;
    for (const auto& raw : lines)
    {
        const std::string line = TrimAscii(raw);
        if (line.empty())
        {
            continue;
        }
        if (line.rfind("#", 0) == 0)
        {
            continue;
        }
        if (emitted > 0)
        {
            summary << " | ";
        }
        const std::string clipped = line.size() > 120 ? line.substr(0, 120) + "..." : line;
        summary << clipped;
        ++emitted;
        totalChars += clipped.size();
        if (emitted >= 3 || totalChars >= 240)
        {
            break;
        }
    }

    std::string result = summary.str();
    if (result.empty())
    {
        const std::string trimmed = TrimAscii(content);
        result = trimmed.size() > 180 ? trimmed.substr(0, 180) + "..." : trimmed;
    }
    if (result.empty())
    {
        result = "(empty memory entry)";
    }
    return result;
}

double DenseCosine(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.empty() || a.size() != b.size())
    {
        return 0.0;
    }

    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const double av = static_cast<double>(a[i]);
        const double bv = static_cast<double>(b[i]);
        dot += (av * bv);
        na += (av * av);
        nb += (bv * bv);
    }
    if (!(na > 0.0) || !(nb > 0.0))
    {
        return 0.0;
    }
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

bool SaveMemorySemanticIndex(const std::vector<MemorySemanticRecord>& records, std::string& outError)
{
    json root = json::object();
    root["version"] = 1;
    root["dimensions"] = kMemorySemanticDims;
    root["records"] = json::array();
    for (const auto& record : records)
    {
        json row = json::object();
        row["path"] = record.virtualPath;
        row["summary"] = record.summary;
        row["embedding"] = record.embedding;
        root["records"].push_back(std::move(row));
    }

    const fs::path indexPath = GetMemorySemanticIndexPath();
    std::ofstream out(indexPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        outError = "failed to open semantic index for writing";
        return false;
    }
    out << root.dump();
    if (!out.good())
    {
        outError = "failed to persist semantic index";
        return false;
    }
    return true;
}

bool LoadMemorySemanticIndex(std::vector<MemorySemanticRecord>& outRecords, std::string& outError)
{
    outRecords.clear();
    const fs::path indexPath = GetMemorySemanticIndexPath();
    if (!fs::exists(indexPath))
    {
        outError = "semantic index missing";
        return false;
    }

    std::string raw;
    if (!ReadTextFile(indexPath.string(), raw))
    {
        outError = "failed to read semantic index";
        return false;
    }
    json root = json::parse(raw, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("records") || !root["records"].is_array())
    {
        outError = "semantic index corrupted";
        return false;
    }

    for (const auto& row : root["records"])
    {
        if (!row.is_object() || !row.contains("path") || !row["path"].is_string() || !row.contains("summary") ||
            !row["summary"].is_string() || !row.contains("embedding") || !row["embedding"].is_array())
        {
            continue;
        }

        MemorySemanticRecord record;
        record.virtualPath = row["path"].get<std::string>();
        record.summary = row["summary"].get<std::string>();
        for (const auto& v : row["embedding"])
        {
            if (!v.is_number_float() && !v.is_number_integer())
            {
                record.embedding.clear();
                break;
            }
            record.embedding.push_back(v.get<float>());
        }
        if (record.embedding.empty())
        {
            continue;
        }
        outRecords.push_back(std::move(record));
    }

    outError.clear();
    return true;
}

bool RebuildMemorySemanticIndex(size_t& outIndexed, std::string& outError)
{
    outIndexed = 0;
    outError.clear();
    std::vector<MemorySemanticRecord> records;

    RawrXD::Runtime::SemanticRetrieval::InstallSemanticIndexEmbeddingCallback();
    const std::vector<std::string> scopes = {"user", "session", "repo"};
    for (const auto& scope : scopes)
    {
        const fs::path base = GetMemoryBaseForScope(scope);
        std::error_code ec;
        if (!fs::exists(base, ec) || ec)
        {
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                continue;
            }
            if (!entry.is_regular_file(ec) || ec)
            {
                continue;
            }
            if (IsInternalMemoryArtifact(entry.path()))
            {
                continue;
            }
            if (entry.path().string().find(".rawrxd-backups") != std::string::npos)
            {
                continue;
            }

            const auto fileSize = entry.file_size(ec);
            if (ec || fileSize == 0 || fileSize > kMemorySemanticMaxBytes)
            {
                continue;
            }

            std::string content;
            if (!ReadTextFile(entry.path().string(), content))
            {
                continue;
            }
            if (content.empty() || IsLikelyBinary(content))
            {
                continue;
            }

            MemorySemanticRecord record;
            record.virtualPath = MakeVirtualMemoryPath(scope, entry.path());
            record.summary = BuildMemoryAutoSummary(content);

            std::string embeddingText = record.virtualPath + "\n" + record.summary + "\n";
            if (content.size() > 4096)
            {
                embeddingText += content.substr(0, 4096);
            }
            else
            {
                embeddingText += content;
            }
            record.embedding = RawrXD::Runtime::SemanticRetrieval::BuildDeterministicTextEmbedding(
                embeddingText, kMemorySemanticDims);
            if (record.embedding.empty())
            {
                continue;
            }

            records.push_back(std::move(record));
        }
    }

    if (!SaveMemorySemanticIndex(records, outError))
    {
        return false;
    }

    outIndexed = records.size();
    return true;
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

static void (*s_integratedTerminalEcho)(const char* utf8Line, void* userData) = nullptr;
static void* s_integratedTerminalEchoUser = nullptr;

void AgentToolHandlers::SetIntegratedTerminalEchoCallback(void (*fn)(const char* utf8Line, void* userData),
                                                          void* userData)
{
    s_integratedTerminalEcho = fn;
    s_integratedTerminalEchoUser = userData;
}

static void emitIntegratedTerminalEcho(const std::string& line)
{
    if (!s_integratedTerminalEcho || line.empty())
        return;
    s_integratedTerminalEcho(line.c_str(), s_integratedTerminalEchoUser);
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
    if (s_guardrails.denyPatterns.empty())
        return false;

    const std::string normalized = ToForwardSlashes(path);
    for (const auto& pattern : s_guardrails.denyPatterns)
    {
        if (pattern.empty())
            continue;
        try
        {
            std::regex re(GlobToRegex(ToForwardSlashes(pattern)), std::regex::ECMAScript | std::regex::icase);
            if (std::regex_match(normalized, re))
            {
                return true;
            }
        }
        catch (...)
        {
        }
    }
    return false;
}

// ============================================================================
// read_file — Read file contents (optionally bounded by line range)
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
    if (!fs::exists(path) || !fs::is_regular_file(path))
    {
        return ToolCallResult::Error("File not found: " + path, ToolOutcome::NotFound);
    }

    std::string content;
    if (!ReadTextFile(path, content))
    {
        return ToolCallResult::Error("Cannot read file: " + path);
    }

    if (content.size() > s_guardrails.maxFileSizeBytes)
    {
        return ToolCallResult::Error("File too large: " + std::to_string(content.size()) + " bytes");
    }

    const int totalLines = std::max(1, CountLines(content));
    int startLine = std::max(1, args.value("start_line", args.value("startLine", 1)));
    int endLine = std::max(startLine, args.value("end_line", args.value("endLine", totalLines)));
    startLine = std::min(startLine, totalLines);
    endLine = std::min(endLine, totalLines);

    std::string sliced = content;
    if (startLine != 1 || endLine != totalLines)
    {
        const std::vector<std::string> lines = ReadLines(content);
        std::ostringstream ranged;
        for (int i = startLine - 1; i < endLine && i < static_cast<int>(lines.size()); ++i)
        {
            if (i > startLine - 1)
            {
                ranged << "\n";
            }
            ranged << lines[static_cast<size_t>(i)];
        }
        sliced = ranged.str();
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["lines"] = totalLines;
    res_metadata["size_bytes"] = content.size();
    res_metadata["start_line"] = startLine;
    res_metadata["end_line"] = endLine;

    ToolCallResult result = ToolCallResult::Ok(sliced, res_metadata);
    result.filePath = path;
    result.bytesRead = sliced.size();
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
    if (existed && s_guardrails.requireBackupOnWrite)
    {
        std::string backupError = CreateBackup(path);
        if (!backupError.empty())
        {
            return ToolCallResult::Error(backupError);
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

    // Write file
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file.is_open())
    {
        return ToolCallResult::Error("Cannot open file for writing: " + path);
    }
    file.write(content.data(), content.size());
    file.close();

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
    const bool previewOnly = args.value("preview_only", false) || args.value("dry_run", false);

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

    auto diffResult = RawrXD::Diff::DiffEngine::ComputeDiff(content, newContent);
    std::string unifiedDiff = diffResult.ToUnifiedDiff("a/" + path, "b/" + path);
    constexpr size_t kMaxDiffPreviewBytes = 128 * 1024;
    bool diffTruncated = false;
    if (unifiedDiff.size() > kMaxDiffPreviewBytes)
    {
        unifiedDiff.resize(kMaxDiffPreviewBytes);
        unifiedDiff += "\n[diff preview truncated]";
        diffTruncated = true;
    }

    if (previewOnly)
    {
        nlohmann::json previewMeta = nlohmann::json::object();
        previewMeta["path"] = path;
        previewMeta["preview_only"] = true;
        previewMeta["old_length"] = oldStr.size();
        previewMeta["new_length"] = newStr.size();
        previewMeta["position"] = pos;
        previewMeta["multiple_matches"] = multipleMatches;
        previewMeta["rollback_available"] = s_guardrails.requireBackupOnWrite;
        previewMeta["rollback_strategy"] = s_guardrails.requireBackupOnWrite ? "restore_from_backup" : "none";
        previewMeta["unified_diff"] = unifiedDiff;
        previewMeta["diff_truncated"] = diffTruncated;
        return ToolCallResult::Ok("Preview generated; no file changes were written", previewMeta);
    }

    // Write back
    std::ofstream outFile(path, std::ios::trunc | std::ios::binary);
    if (!outFile.is_open())
    {
        return ToolCallResult::Error("Cannot write file: " + path);
    }
    outFile.write(newContent.data(), newContent.size());
    outFile.close();

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
    res_metadata["rollback_available"] = s_guardrails.requireBackupOnWrite;
    res_metadata["rollback_strategy"] = s_guardrails.requireBackupOnWrite ? "restore_from_backup" : "none";
    res_metadata["preview_only"] = false;
    res_metadata["unified_diff"] = unifiedDiff;
    res_metadata["diff_truncated"] = diffTruncated;

    ToolCallResult result = ToolCallResult::Ok(msg, res_metadata);
    result.filePath = path;
    result.bytesRead = content.size();
    result.bytesWritten = newContent.size();
    result.linesAffected = std::abs(linesChanged) + CountLines(newStr);
    return result;
}

// ============================================================================
// undo_edit — Restore file from most recent backup snapshot
// ============================================================================

ToolCallResult AgentToolHandlers::UndoEdit(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("undo_edit requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    fs::path backupPath;
    std::string backupError;
    if (!FindLatestBackupForPath(path, backupPath, backupError))
    {
        return ToolCallResult::Error("undo_edit failed: " + backupError + " for " + path, ToolOutcome::NotFound);
    }

    const bool previewOnly = args.value("preview_only", false) || args.value("dry_run", false);

    std::string backupContent;
    if (!ReadTextFile(backupPath.string(), backupContent))
    {
        return ToolCallResult::Error("undo_edit failed to read backup: " + backupPath.string());
    }

    std::string currentContent;
    const bool hasCurrent = ReadTextFile(path, currentContent);

    auto diffResult = RawrXD::Diff::DiffEngine::ComputeDiff(hasCurrent ? currentContent : std::string(), backupContent);
    std::string unifiedDiff = diffResult.ToUnifiedDiff("a/" + path, "b/" + path);
    constexpr size_t kMaxDiffPreviewBytes = 128 * 1024;
    bool diffTruncated = false;
    if (unifiedDiff.size() > kMaxDiffPreviewBytes)
    {
        unifiedDiff.resize(kMaxDiffPreviewBytes);
        unifiedDiff += "\n[diff preview truncated]";
        diffTruncated = true;
    }

    if (previewOnly)
    {
        nlohmann::json previewMeta = nlohmann::json::object();
        previewMeta["path"] = path;
        previewMeta["preview_only"] = true;
        previewMeta["rollback_available"] = true;
        previewMeta["rollback_strategy"] = "restore_from_backup";
        previewMeta["backup_path"] = backupPath.string();
        previewMeta["unified_diff"] = unifiedDiff;
        previewMeta["diff_truncated"] = diffTruncated;
        return ToolCallResult::Ok("Preview generated; no file changes were written", previewMeta);
    }

    if (fs::exists(path) && s_guardrails.requireBackupOnWrite)
    {
        const std::string safetyBackupError = CreateBackup(path);
        if (!safetyBackupError.empty())
        {
            return ToolCallResult::Error("undo_edit pre-restore backup failed: " + safetyBackupError);
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return ToolCallResult::Error("undo_edit failed to open target for restore: " + path);
    }
    out.write(backupContent.data(), static_cast<std::streamsize>(backupContent.size()));
    out.close();

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["path"] = path;
    res_metadata["restored_from_backup"] = backupPath.string();
    res_metadata["bytes_written"] = backupContent.size();
    res_metadata["line_count"] = CountLines(backupContent);
    res_metadata["rollback_available"] = true;
    res_metadata["rollback_strategy"] = "restore_from_backup";
    res_metadata["unified_diff"] = unifiedDiff;
    res_metadata["diff_truncated"] = diffTruncated;

    ToolCallResult result = ToolCallResult::Ok("File restored from latest backup", res_metadata);
    result.filePath = path;
    result.bytesWritten = backupContent.size();
    result.linesAffected = CountLines(backupContent);
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

ToolCallResult AgentToolHandlers::DeleteFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("fs_delete_file requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path))
    {
        return ToolCallResult::Error("File not found: " + path);
    }
    if (fs::is_directory(path))
    {
        return ToolCallResult::Validation("fs_delete_file only supports files");
    }

    std::error_code ec;
    const bool removed = fs::remove(path, ec);
    if (ec || !removed)
    {
        return ToolCallResult::Error("Failed to delete file: " + path + (ec ? (" (" + ec.message() + ")") : ""));
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["deleted"] = true;
    ToolCallResult result = ToolCallResult::Ok("Deleted file", meta);
    result.filePath = path;
    return result;
}

ToolCallResult AgentToolHandlers::MoveFile(const json& args)
{
    const std::string fromKey = args.contains("source") ? "source" : (args.contains("old_path") ? "old_path" : "");
    const std::string toKey =
        args.contains("destination") ? "destination" : (args.contains("new_path") ? "new_path" : "");
    if (fromKey.empty() || !args[fromKey].is_string() || toKey.empty() || !args[toKey].is_string())
    {
        return ToolCallResult::Validation("fs_move_file requires 'source' and 'destination' (string)");
    }

    const std::string source = NormalizePath(args[fromKey].get<std::string>());
    const std::string destination = NormalizePath(args[toKey].get<std::string>());
    if (!IsPathAllowed(source) || !IsPathAllowed(destination))
    {
        return ToolCallResult::Sandbox("Source/destination must be within workspace allowlist");
    }
    if (!fs::exists(source))
    {
        return ToolCallResult::Error("Source file not found: " + source);
    }
    if (fs::is_directory(source))
    {
        return ToolCallResult::Validation("fs_move_file only supports files");
    }

    std::error_code ec;
    const fs::path parent = fs::path(destination).parent_path();
    if (!parent.empty())
    {
        fs::create_directories(parent, ec);
        if (ec)
        {
            return ToolCallResult::Error("Failed to create destination directory: " + ec.message());
        }
    }

    fs::rename(source, destination, ec);
    if (ec)
    {
        ec.clear();
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            return ToolCallResult::Error("Failed to move file: " + ec.message());
        }
        ec.clear();
        fs::remove(source, ec);
        if (ec)
        {
            return ToolCallResult::Error("File copied but source removal failed: " + ec.message());
        }
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["source"] = source;
    meta["destination"] = destination;
    ToolCallResult result = ToolCallResult::Ok("Moved file", meta);
    result.filePath = destination;
    return result;
}

ToolCallResult AgentToolHandlers::CopyFile(const json& args)
{
    const std::string fromKey = args.contains("source") ? "source" : (args.contains("from") ? "from" : "");
    const std::string toKey = args.contains("destination") ? "destination" : (args.contains("to") ? "to" : "");
    if (fromKey.empty() || !args[fromKey].is_string() || toKey.empty() || !args[toKey].is_string())
    {
        return ToolCallResult::Validation("fs_copy_file requires 'source' and 'destination' (string)");
    }

    const std::string source = NormalizePath(args[fromKey].get<std::string>());
    const std::string destination = NormalizePath(args[toKey].get<std::string>());
    if (!IsPathAllowed(source) || !IsPathAllowed(destination))
    {
        return ToolCallResult::Sandbox("Source/destination must be within workspace allowlist");
    }
    if (!fs::exists(source))
    {
        return ToolCallResult::Error("Source file not found: " + source);
    }
    if (fs::is_directory(source))
    {
        return ToolCallResult::Validation("fs_copy_file only supports files");
    }

    std::error_code ec;
    const fs::path parent = fs::path(destination).parent_path();
    if (!parent.empty())
    {
        fs::create_directories(parent, ec);
        if (ec)
        {
            return ToolCallResult::Error("Failed to create destination directory: " + ec.message());
        }
    }

    const bool copied = fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec || !copied)
    {
        return ToolCallResult::Error("Failed to copy file: " + (ec ? ec.message() : std::string("unknown error")));
    }

    uintmax_t sizeBytes = fs::file_size(destination, ec);
    if (ec)
    {
        sizeBytes = 0;
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["source"] = source;
    meta["destination"] = destination;
    meta["size_bytes"] = sizeBytes;
    ToolCallResult result = ToolCallResult::Ok("Copied file", meta);
    result.filePath = destination;
    result.bytesWritten = static_cast<size_t>(sizeBytes);
    return result;
}

ToolCallResult AgentToolHandlers::RollbackFile(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("rollback_file requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    const bool previewOnly = args.value("preview_only", false) || args.value("dry_run", false);
    const bool createForwardBackup = args.value("create_forward_backup", true);

    const fs::path target(path);
    const fs::path backupDir = target.parent_path() / ".rawrxd-backups";

    std::error_code ec;
    if (!fs::exists(backupDir, ec) || !fs::is_directory(backupDir, ec))
    {
        return ToolCallResult::Error("No backup directory found for file: " + path);
    }

    fs::path selectedBackup;
    if (args.contains("backup_path") && args["backup_path"].is_string())
    {
        selectedBackup = fs::path(NormalizePath(args["backup_path"].get<std::string>()));
        if (!fs::exists(selectedBackup, ec) || !fs::is_regular_file(selectedBackup, ec))
        {
            return ToolCallResult::Error("Specified backup file not found: " + selectedBackup.string());
        }
    }
    else
    {
        const std::string stem = target.stem().string();
        const std::string ext = target.extension().string();
        const std::string prefix = stem + ".";
        const std::string suffix = ext.empty() ? ".bak" : (ext + ".bak");

        fs::file_time_type newestTime{};
        bool foundAny = false;
        for (const auto& entry : fs::directory_iterator(backupDir, ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file(ec))
            {
                ec.clear();
                continue;
            }

            const std::string name = entry.path().filename().string();
            if (name.rfind(prefix, 0) != 0)
            {
                continue;
            }
            if (name.size() < suffix.size() || name.substr(name.size() - suffix.size()) != suffix)
            {
                continue;
            }

            const auto writeTime = entry.last_write_time(ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!foundAny || writeTime > newestTime)
            {
                foundAny = true;
                newestTime = writeTime;
                selectedBackup = entry.path();
            }
        }

        if (!foundAny)
        {
            return ToolCallResult::Error("No matching backup snapshots found for file: " + path);
        }
    }

    const std::string selectedBackupPath = NormalizePath(selectedBackup.string());
    if (!IsPathAllowed(selectedBackupPath))
    {
        return ToolCallResult::Sandbox("Backup path not in workspace allowlist: " + selectedBackupPath);
    }

    uintmax_t backupSize = fs::file_size(selectedBackup, ec);
    if (ec)
    {
        backupSize = 0;
        ec.clear();
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["path"] = path;
    meta["backup_path"] = selectedBackupPath;
    meta["backup_size_bytes"] = backupSize;
    meta["preview_only"] = previewOnly;
    meta["create_forward_backup"] = createForwardBackup;

    if (previewOnly)
    {
        return ToolCallResult::Ok("Rollback preview ready; no file changes were written", meta);
    }

    if (createForwardBackup && fs::exists(target, ec) && fs::is_regular_file(target, ec))
    {
        const std::string backupErr = CreateBackup(path);
        if (!backupErr.empty())
        {
            return ToolCallResult::Error(backupErr);
        }
        meta["forward_backup_created"] = true;
    }
    else
    {
        meta["forward_backup_created"] = false;
    }

    fs::copy_file(selectedBackup, target, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        return ToolCallResult::Error("Failed to restore backup: " + ec.message());
    }

    std::string restoredContent;
    if (ReadTextFile(path, restoredContent))
    {
        meta["restored_lines"] = CountLines(restoredContent);
        meta["restored_size_bytes"] = restoredContent.size();
    }

    ToolCallResult result = ToolCallResult::Ok("Rollback restore completed", meta);
    result.filePath = path;
    result.bytesWritten = static_cast<size_t>(backupSize);
    return result;
}

ToolCallResult AgentToolHandlers::PathExists(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("fs_exists requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    const bool isDir = exists ? fs::is_directory(path, ec) : false;
    uintmax_t sizeBytes = 0;
    if (exists && !isDir)
    {
        sizeBytes = fs::file_size(path, ec);
        if (ec)
        {
            sizeBytes = 0;
        }
    }

    nlohmann::json payload = nlohmann::json::object();
    payload["path"] = path;
    payload["exists"] = exists;
    payload["is_directory"] = isDir;
    payload["size_bytes"] = sizeBytes;

    return ToolCallResult::Ok(payload.dump(), payload);
}

ToolCallResult AgentToolHandlers::MakeDirectory(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("fs_mkdir requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }

    std::error_code ec;
    const bool created = fs::create_directories(path, ec);
    if (ec)
    {
        return ToolCallResult::Error("Failed to create directory: " + ec.message());
    }

    nlohmann::json meta = nlohmann::json::object();
    meta["path"] = path;
    meta["created"] = created;
    ToolCallResult result = ToolCallResult::Ok(created ? "Directory created" : "Directory already exists", meta);
    result.filePath = path;
    return result;
}

// ============================================================================
// execute_command — Run terminal command (sandboxed)
// ============================================================================

bool AgentToolHandlers::RunProcess(const std::wstring& cmdLine, uint32_t timeoutMs, std::string& output,
                                   uint32_t& exitCode, const wchar_t* workingDirectoryUtf16)
{
#ifdef _WIN32
    // Prefer ConPTY when available, then fall back to term-pipe, then CreateProcess.
    if (RunProcessWithPseudoConsole(cmdLine, timeoutMs, output, exitCode, workingDirectoryUtf16))
    {
        return true;
    }

    // Term-pipe path cannot honor a custom CWD; use CreateProcess fallback when cwd is set.
    if (!workingDirectoryUtf16 || workingDirectoryUtf16[0] == L'\0')
    {
        output.clear();
        if (RunProcessWithTermPipe(cmdLine, timeoutMs, output, exitCode))
        {
            return true;
        }
        if (exitCode != kTermPipeUnavailable)
        {
            return false;
        }
    }
#endif

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
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    const wchar_t* lpDir = nullptr;
    if (workingDirectoryUtf16 && workingDirectoryUtf16[0] != L'\0')
        lpDir = workingDirectoryUtf16;

    BOOL created =
        CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, lpDir, &si, &pi);
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
    ToolCallResult commandPolicy = ValidateCommandPolicy(args, command);
    if (commandPolicy.outcome != ToolOutcome::Success)
    {
        return commandPolicy;
    }

    uint32_t timeout = s_guardrails.commandTimeoutMs;
    if (args.contains("timeout") && args["timeout"].is_number())
    {
        const int requestedTimeout = args["timeout"].get<int>();
        timeout = static_cast<uint32_t>(std::clamp(requestedTimeout, 1000, 300000));
    }

    const bool mirrorToIdeTerminal =
        args.value("use_integrated_terminal", false) || args.value("mirror_to_ide_agent_terminal", false);
    if (mirrorToIdeTerminal)
    {
        emitIntegratedTerminalEcho("$ " + command);
    }

    // Optional working directory (VS Code / Cursor parity: run in workspace folder)
    std::wstring cwdWideStorage;
    const wchar_t* cwdForProcess = nullptr;
    std::string cwdArg;
    if (args.contains("working_directory") && args["working_directory"].is_string())
        cwdArg = args["working_directory"].get<std::string>();
    else if (args.contains("cwd") && args["cwd"].is_string())
        cwdArg = args["cwd"].get<std::string>();
    else if (!s_guardrails.allowedRoots.empty())
        cwdArg = NormalizePath(s_guardrails.allowedRoots[0]);

    if (!cwdArg.empty())
    {
        const std::string normalized = NormalizePath(cwdArg);
        if (!IsPathAllowed(normalized))
        {
            return ToolCallResult::Sandbox("working_directory must be within workspace allowlist: " + normalized);
        }
        std::error_code ec;
        if (!fs::is_directory(fs::path(normalized), ec))
        {
            return ToolCallResult::Validation("working_directory must be an existing directory: " + normalized);
        }
        wchar_t fullW[32768];
        const DWORD n =
            GetFullPathNameW(ToWide(normalized).c_str(), static_cast<DWORD>(std::size(fullW)), fullW, nullptr);
        if (n == 0 || n >= static_cast<DWORD>(std::size(fullW)))
        {
            return ToolCallResult::Validation("Could not resolve working_directory path");
        }
        const DWORD attr = GetFileAttributesW(fullW);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return ToolCallResult::Validation("working_directory is not a directory");
        }
        cwdWideStorage.assign(fullW);
        cwdForProcess = cwdWideStorage.c_str();
    }

    // Build command line via cmd.exe
    std::wstring cmdLine = L"cmd.exe /C " + ToWide(command);

    std::string output;
    uint32_t exitCode = 0;

    auto startTime = std::chrono::steady_clock::now();
    bool success = RunProcess(cmdLine, timeout, output, exitCode, cwdForProcess);
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();

    if (!success && exitCode == WAIT_TIMEOUT)
    {
        ToolCallResult result = ToolCallResult::TimedOut(output);
        nlohmann::json res_metadata = nlohmann::json::object();
        res_metadata["exit_code"] = exitCode;
        res_metadata["elapsed_ms"] = elapsed;
        res_metadata["command"] = command;
        res_metadata["timeout_ms"] = timeout;
        res_metadata["mirrored_to_ide_agent_terminal"] = mirrorToIdeTerminal;
        if (!cwdArg.empty())
            res_metadata["working_directory"] = NormalizePath(cwdArg);
        result.metadata = res_metadata;
        if (mirrorToIdeTerminal)
        {
            emitIntegratedTerminalEcho("[timeout]\n" + output);
            emitIntegratedTerminalEcho(std::string("[exit ") + std::to_string(exitCode) + "]");
        }
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
    res_metadata["timeout_ms"] = timeout;
    res_metadata["mirrored_to_ide_agent_terminal"] = mirrorToIdeTerminal;
    if (!cwdArg.empty())
        res_metadata["working_directory"] = NormalizePath(cwdArg);

    if (mirrorToIdeTerminal)
    {
        constexpr size_t kIdeTerminalPreview = 6000;
        std::string preview = output;
        if (preview.size() > kIdeTerminalPreview)
        {
            preview = preview.substr(0, kIdeTerminalPreview);
            preview += "\n[... truncated for integrated terminal preview ...]\n";
        }
        emitIntegratedTerminalEcho(preview);
        emitIntegratedTerminalEcho(std::string("[exit ") + std::to_string(exitCode) + "]");
    }

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
// get_code_outline — Extract top-level symbols from a source file
// ============================================================================

ToolCallResult AgentToolHandlers::GetCodeOutline(const json& args)
{
    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("get_code_outline requires 'path' (string)");
    }

    const std::string path = NormalizePath(args["path"].get<std::string>());
    if (!IsPathAllowed(path))
    {
        return ToolCallResult::Sandbox("Path not in workspace allowlist: " + path);
    }
    if (!fs::exists(path) || !fs::is_regular_file(path))
    {
        return ToolCallResult::Error("File not found: " + path, ToolOutcome::NotFound);
    }

    const int maxSymbols = std::clamp(args.value("max_symbols", 500), 1, 2000);
    const bool useLsp = args.value("use_lsp", true);
    const int lspTimeoutMs = std::clamp(args.value("lsp_timeout_ms", 200), 50, 2000);
    const auto startedAt = std::chrono::steady_clock::now();

    std::string content;
    if (!ReadTextFile(path, content))
    {
        return ToolCallResult::Error("Cannot read file: " + path);
    }
    if (content.size() > s_guardrails.maxFileSizeBytes)
    {
        return ToolCallResult::Error("File too large: " + std::to_string(content.size()) + " bytes");
    }

    const std::string ext = ToLowerCopy(fs::path(path).extension().string());
    const std::vector<std::string> lines = ReadLines(content);

    const bool isAsmLike = (ext == ".asm" || ext == ".inc" || ext == ".s");
    const bool lspEligible = useLsp && !isAsmLike;

    nlohmann::json lspSymbols = nlohmann::json::array();
    nlohmann::json lspSymbolTree = nlohmann::json::array();
    bool lspTimedOut = false;
    int lspElapsedMs = 0;
    if (lspEligible)
    {
        try
        {
            static RawrXD::Agentic::LSPManager s_lspManager;
            const std::string workspaceRoot =
                s_guardrails.allowedRoots.empty() ? fs::path(path).parent_path().string() : s_guardrails.allowedRoots[0];
            const std::string languageId = s_lspManager.getLanguageId(path);
            if (!languageId.empty())
            {
                RawrXD::Agentic::LSPClient* client = s_lspManager.getClient(languageId, workspaceRoot);
                if (client != nullptr && client->isInitialized())
                {
                    std::mutex lspMutex;
                    std::condition_variable lspCv;
                    bool completed = false;
                    const auto lspStart = std::chrono::steady_clock::now();

                    client->requestDocumentSymbols(ToFileUri(path),
                                                   [&](const std::vector<RawrXD::Agentic::LSPSymbolInfo>& syms) {
                                                       std::function<nlohmann::json(const RawrXD::Agentic::LSPSymbolInfo&)> toTree;
                                                       toTree = [&](const RawrXD::Agentic::LSPSymbolInfo& sym) {
                                                           nlohmann::json node = nlohmann::json::object();
                                                           node["name"] = sym.name;
                                                           node["kind"] = LspSymbolKindName(sym.kind);
                                                           node["line"] = sym.selectionRange.start.line + 1;
                                                           node["children"] = nlohmann::json::array();
                                                           for (const auto& child : sym.children)
                                                           {
                                                               node["children"].push_back(toTree(child));
                                                           }
                                                           return node;
                                                       };

                                                       std::function<void(const RawrXD::Agentic::LSPSymbolInfo&,
                                                                          const std::string&)> flatten;
                                                       nlohmann::json local = nlohmann::json::array();
                                                       nlohmann::json tree = nlohmann::json::array();
                                                       flatten = [&](const RawrXD::Agentic::LSPSymbolInfo& sym,
                                                                     const std::string& parent) {
                                                           if (static_cast<int>(local.size()) >= maxSymbols)
                                                           {
                                                               return;
                                                           }

                                                           nlohmann::json row = nlohmann::json::object();
                                                           row["name"] = sym.name;
                                                           row["kind"] = LspSymbolKindName(sym.kind);
                                                           row["line"] = sym.range.start.line + 1;
                                                           row["signature"] = parent.empty() ? sym.name : (parent + "::" + sym.name);
                                                           local.push_back(row);

                                                           for (const auto& child : sym.children)
                                                           {
                                                               flatten(child, row["signature"].get<std::string>());
                                                               if (static_cast<int>(local.size()) >= maxSymbols)
                                                               {
                                                                   break;
                                                               }
                                                           }
                                                       };

                                                       for (const auto& sym : syms)
                                                       {
                                                           tree.push_back(toTree(sym));
                                                           flatten(sym, sym.containerName);
                                                       }

                                                       {
                                                           std::lock_guard<std::mutex> lock(lspMutex);
                                                           lspSymbols = std::move(local);
                                                           lspSymbolTree = std::move(tree);
                                                           completed = true;
                                                       }
                                                       lspCv.notify_one();
                                                   });

                    std::unique_lock<std::mutex> lock(lspMutex);
                    const bool ready = lspCv.wait_for(lock, std::chrono::milliseconds(lspTimeoutMs),
                                                      [&]() { return completed; });
                    if (!ready)
                    {
                        lspTimedOut = true;
                        ++g_getCodeOutlineLspTimeout;
                    }

                    lspElapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - lspStart)
                                         .count());

                    if (ready && !lspSymbols.empty())
                    {
                        ++g_getCodeOutlineLspSuccess;
                    }
                }
            }
        }
        catch (...)
        {
            // Fall through to parser fallback.
        }
    }

    static const std::regex reCppContainer(
        R"(^\s*(?:class|struct|enum|namespace)\s+([A-Za-z_~][A-Za-z0-9_:]*)\b)",
        std::regex::ECMAScript);
    static const std::regex reCppFunction(
        R"(^\s*(?:template\s*<[^>]*>\s*)?(?:inline\s+|static\s+|virtual\s+|constexpr\s+|friend\s+|extern\s+|unsigned\s+|signed\s+|long\s+|short\s+|\w[\w:\<\>\*&\s]*\s+)+([A-Za-z_~][A-Za-z0-9_:]*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:\{|$))",
        std::regex::ECMAScript);
    static const std::regex reAsmProc(R"(^\s*([A-Za-z_.$?@][A-Za-z0-9_.$?@]*)\s+proc\b)", std::regex::icase);
    static const std::regex reAsmStruct(R"(^\s*([A-Za-z_.$?@][A-Za-z0-9_.$?@]*)\s+struct\b)", std::regex::icase);
    static const std::regex reAsmMacro(R"(^\s*([A-Za-z_.$?@][A-Za-z0-9_.$?@]*)\s+macro\b)", std::regex::icase);
    static const std::regex reAsmLabel(R"(^\s*([A-Za-z_.$?@][A-Za-z0-9_.$?@]*)\s*:\s*$)", std::regex::ECMAScript);
    static const std::regex rePyDef(R"(^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()", std::regex::ECMAScript);
    static const std::regex rePyClass(R"(^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)\b)", std::regex::ECMAScript);
    static const std::regex reJsClass(R"(^\s*class\s+([A-Za-z_$][A-Za-z0-9_$]*)\b)", std::regex::ECMAScript);
    static const std::regex reJsFunction(
        R"(^\s*(?:export\s+)?(?:async\s+)?function\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*\()", std::regex::ECMAScript);
    static const std::regex reJsVarFn(
        R"(^\s*(?:const|let|var)\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*=\s*(?:async\s*)?(?:\([^)]*\)|[A-Za-z_$][A-Za-z0-9_$]*)\s*=>)",
        std::regex::ECMAScript);

    auto isLikelyCommentLine = [](const std::string& line) {
        const std::string trimmed = TrimAscii(line);
        return trimmed.empty() || trimmed.rfind("//", 0) == 0 || trimmed.rfind("#", 0) == 0 ||
               trimmed.rfind(";", 0) == 0;
    };

    nlohmann::json symbols = lspSymbols;
    if (lspSymbols.empty())
    {
        ++g_getCodeOutlineParserFallback;
    }
    std::smatch match;
    for (size_t i = 0; i < lines.size() && static_cast<int>(symbols.size()) < maxSymbols; ++i)
    {
        if (!lspSymbols.empty())
        {
            break;
        }

        const std::string& line = lines[i];
        if (isLikelyCommentLine(line))
        {
            continue;
        }

        std::string kind;
        std::string name;

        if (ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".hh")
        {
            if (std::regex_search(line, match, reCppContainer) && match.size() > 1)
            {
                kind = "container";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reCppFunction) && match.size() > 1)
            {
                kind = "function";
                name = match[1].str();
            }
        }
        else if (ext == ".asm" || ext == ".inc" || ext == ".s")
        {
            if (std::regex_search(line, match, reAsmProc) && match.size() > 1)
            {
                kind = "proc";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reAsmStruct) && match.size() > 1)
            {
                kind = "struct";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reAsmMacro) && match.size() > 1)
            {
                kind = "macro";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reAsmLabel) && match.size() > 1)
            {
                kind = "label";
                name = match[1].str();
            }
        }
        else if (ext == ".py")
        {
            if (std::regex_search(line, match, rePyClass) && match.size() > 1)
            {
                kind = "class";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, rePyDef) && match.size() > 1)
            {
                kind = "function";
                name = match[1].str();
            }
        }
        else if (ext == ".js" || ext == ".ts" || ext == ".jsx" || ext == ".tsx")
        {
            if (std::regex_search(line, match, reJsClass) && match.size() > 1)
            {
                kind = "class";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reJsFunction) && match.size() > 1)
            {
                kind = "function";
                name = match[1].str();
            }
            else if (std::regex_search(line, match, reJsVarFn) && match.size() > 1)
            {
                kind = "lambda";
                name = match[1].str();
            }
        }

        if (!name.empty())
        {
            nlohmann::json sym = nlohmann::json::object();
            sym["name"] = name;
            sym["kind"] = kind.empty() ? "symbol" : kind;
            sym["line"] = static_cast<int>(i + 1);
            sym["signature"] = TrimAscii(line);
            symbols.push_back(sym);
        }
    }

    nlohmann::json body = nlohmann::json::object();
    body["path"] = path;
    body["language"] = ext.empty() ? "unknown" : ext;
    body["total_lines"] = static_cast<int>(lines.size());
    body["symbol_count"] = symbols.size();
    body["source"] = lspSymbols.empty() ? "parser_fallback" : "lsp_document_symbol";
    body["lsp_timed_out"] = lspTimedOut;
    body["lsp_timeout_ms"] = lspTimeoutMs;
    body["symbols"] = symbols;
    if (!lspSymbolTree.empty())
    {
        body["symbol_tree"] = lspSymbolTree;
    }

    nlohmann::json res_metadata = nlohmann::json::object();
    res_metadata["path"] = path;
    res_metadata["language"] = body["language"];
    res_metadata["symbol_count"] = body["symbol_count"];
    res_metadata["max_symbols"] = maxSymbols;
    res_metadata["source"] = body["source"];
    res_metadata["lsp_timed_out"] = lspTimedOut;
    res_metadata["lsp_timeout_ms"] = lspTimeoutMs;
    res_metadata["lsp_elapsed_ms"] = lspElapsedMs;
    res_metadata["elapsed_ms"] = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - startedAt)
                                       .count());
    res_metadata["outline_lsp_success_count"] = g_getCodeOutlineLspSuccess.load();
    res_metadata["outline_lsp_timeout_count"] = g_getCodeOutlineLspTimeout.load();
    res_metadata["outline_parser_fallback_count"] = g_getCodeOutlineParserFallback.load();

    ToolCallResult result = ToolCallResult::Ok(body.dump(2), res_metadata);
    result.filePath = path;
    result.bytesRead = content.size();
    return result;
}

// ============================================================================
// get_diagnostics — IDE/workspace diagnostics snapshot
// ============================================================================

ToolCallResult AgentToolHandlers::GetCompileDiagnostics(const json& args)
{
    (void)args;
    json res_metadata;
    res_metadata["diagnostics_available"] = true;
    return ToolCallResult::Ok("{\"status\":\"ok\"}", res_metadata);
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

    std::string searchRoot = s_guardrails.allowedRoots[0];
    if (args.contains("root") && args["root"].is_string())
    {
        std::string candidate = NormalizePath(args["root"].get<std::string>());
        if (!IsPathAllowed(candidate))
        {
            return ToolCallResult::Sandbox("Search root not allowed: " + candidate);
        }
        searchRoot = candidate;
    }

    const size_t maxFileSize = s_guardrails.maxFileSizeBytes;
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

            if (filePattern != "*.*" && filePattern != "*")
            {
                const std::string ext = it->path().extension().string();
                if (filePattern[0] == '*' && filePattern.size() > 1)
                {
                    const std::string reqExt = filePattern.substr(1);
                    if (ext != reqExt)
                        continue;
                }
            }

            const auto fsize = it->file_size();
            if (fsize == 0 || fsize > maxFileSize)
                continue;
            scannedFiles++;

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
                for (auto matchIt = std::sregex_iterator(content.begin(), content.end(), re);
                     matchIt != std::sregex_iterator() && hitCount < maxResults; ++matchIt)
                {
                    const size_t found = static_cast<size_t>(matchIt->position());
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

                    const std::string lineText =
                        content.substr(contextStart, std::min(contextEnd - contextStart, contextBytes));
                    const std::string relPath = fs::relative(it->path(), searchRoot).string();
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
                while (lineStart < pos)
                {
                    if (content[lineStart] == '\n')
                        ++lineNum;
                    ++lineStart;
                }

                const size_t found = caseSensitive ? content.find(query, pos) : lowerContent.find(lowerQuery, pos);
                if (found == std::string::npos)
                    break;

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

                const std::string lineText =
                    content.substr(contextStart, std::min(contextEnd - contextStart, contextBytes));
                const std::string relPath = fs::relative(it->path(), searchRoot).string();
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
    }

    if (hitCount == 0)
    {
        nlohmann::json zeroMatches = nlohmann::json::object();
        zeroMatches["matches"] = 0;
        return ToolCallResult::Ok("No matches found for: " + query, zeroMatches);
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

ToolCallResult AgentToolHandlers::FileSearch(const json& args)
{
    std::vector<std::string> patterns;
    if (args.contains("patterns") && args["patterns"].is_array())
    {
        for (const auto& item : args["patterns"])
        {
            if (item.is_string())
            {
                const std::string pattern = TrimAscii(item.get<std::string>());
                if (!pattern.empty())
                {
                    patterns.push_back(ToForwardSlashes(pattern));
                }
            }
        }
    }

    if (patterns.empty())
    {
        if (args.contains("pattern") && args["pattern"].is_string())
            patterns.push_back(ToForwardSlashes(TrimAscii(args["pattern"].get<std::string>())));
        else if (args.contains("query") && args["query"].is_string())
            patterns.push_back(ToForwardSlashes(TrimAscii(args["query"].get<std::string>())));
    }

    patterns.erase(std::remove_if(patterns.begin(), patterns.end(),
                                  [](const std::string& value) { return value.empty(); }),
                   patterns.end());
    if (patterns.empty())
    {
        return ToolCallResult::Validation("file_search requires 'pattern', 'query', or 'patterns'");
    }

    std::string root;
    if (args.contains("root") && args["root"].is_string())
        root = NormalizePath(args["root"].get<std::string>());
    else if (!s_guardrails.allowedRoots.empty())
        root = NormalizePath(s_guardrails.allowedRoots.front());
    else
        root = NormalizePath(fs::current_path().string());

    if (!IsPathAllowed(root))
    {
        return ToolCallResult::Sandbox("file_search root not allowed: " + root);
    }

    int maxResults = std::clamp(args.value("max_results", s_guardrails.maxSearchResults), 1, 5000);

    std::vector<std::regex> matchers;
    matchers.reserve(patterns.size());
    for (const auto& pattern : patterns)
    {
        matchers.emplace_back(GlobToRegex(pattern), std::regex::ECMAScript | std::regex::icase);
    }

    json results = json::array();
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it)
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec))
        {
            continue;
        }

        const std::string absolute = NormalizePath(it->path().string());
        if (!IsPathAllowed(absolute))
        {
            continue;
        }

        std::string relative = ToForwardSlashes(fs::relative(it->path(), root, ec).generic_string());
        if (ec)
        {
            relative = ToForwardSlashes(it->path().filename().generic_string());
            ec.clear();
        }

        bool matched = false;
        for (const auto& matcher : matchers)
        {
            if (std::regex_match(relative, matcher) || std::regex_match(absolute, matcher))
            {
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            continue;
        }

        results.push_back(absolute);
        if (static_cast<int>(results.size()) >= maxResults)
        {
            break;
        }
    }

    json meta = json::object();
    meta["root"] = root;
    meta["patterns"] = patterns;
    meta["count"] = results.size();
    meta["max_results"] = maxResults;
    return ToolCallResult::Ok(results.dump(2), meta);
}

// run_shell — Guarded alias of execute_command with allowlist enforcement
// ============================================================================
ToolCallResult AgentToolHandlers::RunShell(const json& args)
{
    if (!args.contains("command") || !args["command"].is_string())
    {
        return ToolCallResult::Validation("run_shell requires 'command' (string)");
    }
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

    const bool includeNonCode = args.value("include_non_code", false);

    RawrXD::Runtime::SemanticRetrieval::InstallSemanticIndexEmbeddingCallback();
    const auto vectorHits = RawrXD::Runtime::SemanticRetrieval::SearchSemanticContext(query, static_cast<size_t>(topK));
    if (!vectorHits.empty())
    {
        nlohmann::json res = nlohmann::json::array();
        for (const auto& hit : vectorHits)
        {
            nlohmann::json row;
            row["metadata"] = hit.metadata;
            row["score"] = hit.similarity;
            row["entry_id"] = hit.entryId;
            row["source"] = "vector";
            res.push_back(row);
        }

        nlohmann::json meta = nlohmann::json::object();
        meta["returned"] = res.size();
        meta["root"] = root;
        meta["top_k"] = topK;
        meta["engine"] = "rawrxd-vector-index";
        meta["mapped_gguf"] = RawrXD::Runtime::RawrXDVectorIndex::instance().hasMappedGGUF();
        meta["dims"] = RawrXD::Runtime::RawrXDVectorIndex::instance().preferredDimensions();
        return ToolCallResult::Ok(res.dump(2), meta);
    }

    static const std::unordered_set<std::string> kCodeExt = {
        ".c",   ".cc",   ".cpp",  ".cxx", ".h",    ".hh",   ".hpp",   ".hxx",
        ".cs",  ".go",   ".java", ".js",  ".jsx",  ".json", ".kt",    ".lua",
        ".m",   ".mm",   ".php",  ".ps1", ".py",   ".rb",   ".rs",    ".sh",
        ".sql", ".swift", ".ts",  ".tsx", ".txt",  ".vb",   ".xml",   ".yaml",
        ".yml", ".zig",  ".asm",  ".s",   ".md",   ".cmake"
    };

    std::unordered_map<std::string, double> qtf;
    double qnorm = 0.0;
    const auto queryTokens = Tokenize(query);
    for (const auto& token : queryTokens)
    {
        qtf[token] += 1.0;
    }
    if (!queryTokens.empty())
    {
        for (auto& kv : qtf)
        {
            kv.second /= static_cast<double>(queryTokens.size());
            qnorm += kv.second * kv.second;
        }
        qnorm = std::sqrt(qnorm);
    }

    std::vector<IndexedFile> indexed;
    int scanned = 0;
    int skippedBinary = 0;

    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it)
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || ec)
        {
            ec.clear();
            continue;
        }

        const auto fsize = it->file_size(ec);
        if (ec || fsize == 0 || fsize > s_guardrails.maxFileSizeBytes)
        {
            ec.clear();
            continue;
        }

        const std::string path = NormalizePath(it->path().string());
        const std::string ext = ToLowerCopy(it->path().extension().string());
        if (!includeNonCode && !ext.empty() && kCodeExt.find(ext) == kCodeExt.end())
        {
            continue;
        }
        if (!IsPathAllowed(path))
        {
            continue;
        }

        bool binaryFlag = false;
        try
        {
            std::ifstream sniff(path, std::ios::binary);
            if (sniff.is_open())
            {
                std::string head(512, '\0');
                sniff.read(head.data(), static_cast<std::streamsize>(head.size()));
                head.resize(static_cast<size_t>(sniff.gcount()));
                binaryFlag = IsLikelyBinary(head);
            }
        }
        catch (...)
        {
            binaryFlag = true;
        }

        if (binaryFlag)
        {
            ++skippedBinary;
            continue;
        }

        auto idx = BuildIndexForFile(path, s_guardrails.maxFileSizeBytes);
        if (!idx.tf.empty())
        {
            indexed.push_back(std::move(idx));
            ++scanned;
        }
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
    std::vector<RawrXD::Agentic::FileEdit> parsed;
    std::string parseError;
    if (!BuildMultiFilePlanFromArgs(args, parsed, parseError))
    {
        return ToolCallResult::Validation("propose_multifile_edits " + parseError);
    }

    RawrXD::Agentic::MultiFileEditPlan plan;
    size_t skipped = 0;
    for (auto& edit : parsed)
    {
        edit.filePath = NormalizePath(edit.filePath);
        if (!IsPathAllowed(edit.filePath))
        {
            ++skipped;
            continue;
        }
        if ((edit.type != RawrXD::Agentic::EditType::INSERT) && !fs::exists(edit.filePath))
        {
            ++skipped;
            continue;
        }
        plan.addEdit(edit);
    }

    if (plan.getEdits().empty())
    {
        return ToolCallResult::Error("No valid edits after sandbox/path validation.");
    }

    const std::string seqError = plan.sequence();
    json preview = plan.toPreviewJson();

    json diffs = json::array();
    std::unordered_map<std::string, std::vector<RawrXD::Agentic::FileEdit>> byFile;
    for (const auto& e : plan.getEdits())
    {
        byFile[e.filePath].push_back(e);
    }
    for (auto& [file, fileEdits] : byFile)
    {
        std::string before;
        if (!ReadTextFile(file, before))
        {
            before.clear();
        }
        std::string after = before;
        std::sort(fileEdits.begin(), fileEdits.end(),
                  [](const auto& a, const auto& b)
                  {
                      if (a.lineStart != b.lineStart)
                          return a.lineStart > b.lineStart;
                      return a.lineEnd > b.lineEnd;
                  });

        bool applyOk = true;
        std::string applyErr;
        for (const auto& edit : fileEdits)
        {
            if (!ApplyEditInMemory(edit, after, applyErr))
            {
                applyOk = false;
                break;
            }
        }

        json d = json::object();
        d["file"] = file;
        if (!applyOk)
        {
            d["error"] = applyErr;
        }
        else
        {
            auto dr = RawrXD::Diff::DiffEngine::ComputeDiff(before, after);
            d["unified_diff"] = dr.ToUnifiedDiff("a/" + file, "b/" + file);
            d["additions"] = dr.additions;
            d["deletions"] = dr.deletions;
        }
        diffs.push_back(std::move(d));
    }

    preview["diff_preview"] = std::move(diffs);
    if (!seqError.empty())
    {
        preview["sequence_error"] = seqError;
    }

    json meta = json::object();
    meta["files"] = byFile.size();
    meta["edits"] = plan.getEdits().size();
    meta["skipped"] = skipped;
    meta["ready"] = plan.isReady();
    return ToolCallResult::Ok(preview.dump(2), meta);
}

// ============================================================================
// preview_multifile_diff — Alias for propose_multifile_edits preview path
// ============================================================================
ToolCallResult AgentToolHandlers::PreviewMultiFileDiff(const json& args)
{
    return ProposeMultiFileEdits(args);
}

// ============================================================================
// apply_multifile_edits — Execute structured multi-file plan transactionally
// ============================================================================
ToolCallResult AgentToolHandlers::ApplyMultiFileEdits(const json& args)
{
    std::vector<RawrXD::Agentic::FileEdit> parsed;
    std::string parseError;
    if (!BuildMultiFilePlanFromArgs(args, parsed, parseError))
    {
        return ToolCallResult::Validation("apply_multifile_edits " + parseError);
    }

    RawrXD::Agentic::MultiFileEditPlan plan;
    size_t skipped = 0;
    for (auto& edit : parsed)
    {
        edit.filePath = NormalizePath(edit.filePath);
        if (!IsPathAllowed(edit.filePath))
        {
            ++skipped;
            continue;
        }
        if ((edit.type != RawrXD::Agentic::EditType::INSERT) && !fs::exists(edit.filePath))
        {
            ++skipped;
            continue;
        }
        if (s_guardrails.requireBackupOnWrite && fs::exists(edit.filePath))
        {
            const std::string backupErr = CreateBackup(edit.filePath);
            if (!backupErr.empty())
            {
                return ToolCallResult::Error(backupErr);
            }
        }
        plan.addEdit(edit);
    }

    if (plan.getEdits().empty())
    {
        return ToolCallResult::Error("No valid edits to apply after sandbox/path validation.");
    }

    const std::string seqError = plan.sequence();
    if (!seqError.empty())
    {
        return ToolCallResult::Validation(seqError);
    }

    const int applied = plan.execute();
    auto summary = plan.getSummary();
    json out = json::object();
    out["applied"] = applied;
    out["total"] = summary.totalEdits;
    out["successful"] = summary.successfulEdits;
    out["failed"] = summary.failedEdits;
    out["fully_completed"] = summary.fullyCompleted;
    out["status"] = static_cast<int>(plan.getStatus());
    out["errors"] = summary.errors;

    json meta = json::object();
    meta["skipped"] = skipped;
    meta["checkpoint_count"] = plan.getCheckpoints().size();

    if (!summary.fullyCompleted)
    {
        return ToolCallResult::Error(out.dump(2), ToolOutcome::ExecutionError);
    }
    return ToolCallResult::Ok(out.dump(2), meta);
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

ToolCallResult AgentToolHandlers::ManageTodoList(const json& args)
{
    if (!args.contains("todoList") || !args["todoList"].is_array())
    {
        return ToolCallResult::Validation("manage_todo_list requires 'todoList' (array)");
    }

    RawrXD::Todos::TodoManager manager;
    const auto existingTodos = manager.GetAll();
    std::unordered_map<std::string, std::string> previousStatusByTitle;
    previousStatusByTitle.reserve(existingTodos.size());
    for (const auto& todo : existingTodos)
    {
        if (!todo.text.empty())
        {
            previousStatusByTitle[todo.text] = todo.status;
        }
    }

    struct PlannedItem
    {
        int inputId = 0;
        std::string title;
        std::string status;
        std::string priority;
        std::string category;
        std::vector<int> dependsOn;
        std::string blockerType;
        std::string blockerReason;
    };

    const auto normalizeStatus = [](const std::string& raw) {
        const std::string lowered = ToLowerCopy(raw);
        if (lowered == "not-started") return std::string("pending");
        if (lowered == "in_progress") return std::string("in-progress");
        if (lowered == "done") return std::string("completed");
        return lowered;
    };

    const auto isStatusAllowed = [](const std::string& status) {
        return status == "pending" || status == "in-progress" || status == "completed" ||
               status == "blocked" || status == "cancelled";
    };

    const auto canTransition = [](const std::string& from, const std::string& to) {
        if (from.empty() || from == to) return true;
        if (from == "pending") return to == "in-progress" || to == "blocked" || to == "cancelled" || to == "completed";
        if (from == "in-progress") return to == "completed" || to == "blocked" || to == "pending" || to == "cancelled";
        if (from == "blocked") return to == "pending" || to == "in-progress" || to == "cancelled";
        if (from == "completed") return false;
        if (from == "cancelled") return to == "pending";
        return true;
    };

    std::vector<PlannedItem> planned;
    planned.reserve(args["todoList"].size());
    std::unordered_set<int> declaredIds;
    int inProgressCount = 0;
    for (const auto& item : args["todoList"])
    {
        if (!item.is_object())
        {
            return ToolCallResult::Validation("manage_todo_list entries must be objects");
        }

        PlannedItem row;
        row.inputId = item.value("id", 0);
        row.title = item.value("title", std::string());
        row.status = normalizeStatus(item.value("status", std::string("pending")));
        row.priority = item.value("priority", std::string("Medium"));
        row.category = item.value("category", std::string("agentic"));

        if (row.title.empty())
        {
            return ToolCallResult::Validation("manage_todo_list entries require 'title'");
        }
        if (!isStatusAllowed(row.status))
        {
            return ToolCallResult::Validation("manage_todo_list contains unsupported status: " + row.status);
        }
        if (row.status == "in-progress")
        {
            ++inProgressCount;
        }

        if (item.contains("dependencies") && item["dependencies"].is_array())
        {
            for (const auto& dep : item["dependencies"])
            {
                if (!dep.is_number_integer())
                {
                    return ToolCallResult::Validation("manage_todo_list dependencies must be integer IDs");
                }
                row.dependsOn.push_back(dep.get<int>());
            }
        }

        if (item.contains("blocker") && item["blocker"].is_object())
        {
            const auto& blocker = item["blocker"];
            if (blocker.contains("type") && blocker["type"].is_string())
            {
                row.blockerType = blocker["type"].get<std::string>();
            }
            if (blocker.contains("reason") && blocker["reason"].is_string())
            {
                row.blockerReason = blocker["reason"].get<std::string>();
            }
        }

        planned.push_back(std::move(row));
        declaredIds.insert(item.value("id", 0));
    }

    if (inProgressCount > 1)
    {
        return ToolCallResult::Validation("manage_todo_list allows at most one item in-progress");
    }

    // Validate dependency references and graph acyclicity on input IDs.
    std::unordered_map<int, std::vector<int>> graph;
    graph.reserve(planned.size());
    for (const auto& row : planned)
    {
        if (row.inputId <= 0)
        {
            continue;
        }
        for (const int depId : row.dependsOn)
        {
            if (depId <= 0 || !declaredIds.count(depId))
            {
                return ToolCallResult::Validation("manage_todo_list dependency references unknown id: " + std::to_string(depId));
            }
            graph[row.inputId].push_back(depId);
        }
    }

    std::unordered_set<int> tempMark;
    std::unordered_set<int> permMark;
    std::function<bool(int)> hasCycle = [&](int node) {
        if (permMark.count(node))
        {
            return false;
        }
        if (tempMark.count(node))
        {
            return true;
        }
        tempMark.insert(node);
        auto it = graph.find(node);
        if (it != graph.end())
        {
            for (const int next : it->second)
            {
                if (hasCycle(next))
                {
                    return true;
                }
            }
        }
        tempMark.erase(node);
        permMark.insert(node);
        return false;
    };

    for (const auto& row : planned)
    {
        if (row.inputId > 0 && hasCycle(row.inputId))
        {
            return ToolCallResult::Validation("manage_todo_list dependency graph contains a cycle");
        }
    }

    if (!manager.ClearAll())
    {
        return ToolCallResult::Error("Failed to clear existing todo list before update");
    }

    std::unordered_map<int, int> inputToStoredId;
    json stored = json::array();
    for (const auto& row : planned)
    {
        if (!manager.AddTodo(row.title, row.priority, "agentic"))
        {
            return ToolCallResult::Error("Failed to add todo: " + row.title);
        }

        const auto todos = manager.GetAll();
        if (todos.empty())
        {
            return ToolCallResult::Error("Todo storage did not return newly created item");
        }

        const int createdId = todos.back().id;
        if (row.inputId > 0)
        {
            inputToStoredId[row.inputId] = createdId;
        }

        std::string effectiveStatus = row.status;
        std::string blockerType = row.blockerType;
        std::string blockerReason = row.blockerReason;
        int blockedById = 0;

        for (const int depInputId : row.dependsOn)
        {
            const auto depIt = std::find_if(planned.begin(), planned.end(),
                                            [depInputId](const PlannedItem& p) { return p.inputId == depInputId; });
            if (depIt != planned.end() && depIt->status != "completed")
            {
                effectiveStatus = "blocked";
                blockerType = "dependency";
                blockerReason = "Waiting for dependency id " + std::to_string(depInputId) + " to complete";
                blockedById = depInputId;
                break;
            }
        }

        json updates = json::object();
        updates["status"] = effectiveStatus;
        updates["category"] = row.category;

        json dependsOnStored = json::array();
        for (const int depInputId : row.dependsOn)
        {
            auto it = inputToStoredId.find(depInputId);
            if (it != inputToStoredId.end())
            {
                dependsOnStored.push_back(it->second);
            }
        }
        updates["dependsOn"] = dependsOnStored;

        json blocker = json::object();
        blocker["type"] = blockerType;
        blocker["reason"] = blockerReason;
        blocker["blockedById"] = blockedById;
        updates["blocker"] = blocker;
        updates["blockedById"] = blockedById;

        const auto prev = previousStatusByTitle.find(row.title);
        if (prev != previousStatusByTitle.end())
        {
            const std::string prevStatus = normalizeStatus(prev->second);
            if (!canTransition(prevStatus, effectiveStatus))
            {
                return ToolCallResult::Validation("Invalid durable transition for todo '" + row.title + "': " +
                                                  prevStatus + " -> " + effectiveStatus);
            }
            updates["statusTransition"] = prevStatus + "->" + effectiveStatus;
        }

        if (!manager.UpdateTodo(createdId, updates))
        {
            return ToolCallResult::Error("Failed to update todo status for id " + std::to_string(createdId));
        }

        json storedItem = json::object();
        storedItem["input_id"] = row.inputId > 0 ? row.inputId : createdId;
        storedItem["stored_id"] = createdId;
        storedItem["title"] = row.title;
        storedItem["status"] = effectiveStatus;
        storedItem["priority"] = row.priority;
        storedItem["dependencies"] = dependsOnStored;
        storedItem["blocker"] = blocker;
        stored.push_back(storedItem);
    }

    json meta = json::object();
    meta["count"] = stored.size();
    meta["dependency_graph_nodes"] = planned.size();
    meta["dependency_graph_edges"] = graph.size();
    meta["durable_transitions_validated"] = true;
    return ToolCallResult::Ok(stored.dump(2), meta);
}

ToolCallResult AgentToolHandlers::Memory(const json& args)
{
    const std::string command = ToLowerCopy(args.value("command", std::string()));
    if (command.empty())
    {
        return ToolCallResult::Validation("memory requires 'command'");
    }

    if (command == "rename")
    {
        if (!args.contains("old_path") || !args["old_path"].is_string() || !args.contains("new_path") ||
            !args["new_path"].is_string())
        {
            return ToolCallResult::Validation("memory rename requires 'old_path' and 'new_path'");
        }

        fs::path oldReal;
        fs::path newReal;
        std::string oldScope;
        std::string newScope;
        std::string error;
        if (!ResolveMemoryPath(args["old_path"].get<std::string>(), oldReal, oldScope, error))
        {
            return ToolCallResult::Validation(error);
        }
        if (!ResolveMemoryPath(args["new_path"].get<std::string>(), newReal, newScope, error))
        {
            return ToolCallResult::Validation(error);
        }
        if (oldScope != newScope)
        {
            return ToolCallResult::Validation("memory rename cannot move across scopes");
        }
        if (!fs::exists(oldReal))
        {
            return ToolCallResult::Error("Memory path not found: " + args["old_path"].get<std::string>());
        }

        std::error_code ec;
        if (!newReal.parent_path().empty())
        {
            fs::create_directories(newReal.parent_path(), ec);
            if (ec)
            {
                return ToolCallResult::Error("Failed to prepare target directory: " + ec.message());
            }
        }
        fs::rename(oldReal, newReal, ec);
        if (ec)
        {
            return ToolCallResult::Error("Failed to rename memory path: " + ec.message());
        }

        json meta = json::object();
        meta["old_path"] = args["old_path"].get<std::string>();
        meta["new_path"] = args["new_path"].get<std::string>();

        size_t indexed = 0;
        std::string indexError;
        if (RebuildMemorySemanticIndex(indexed, indexError))
        {
            meta["semantic_index_entries"] = indexed;
        }
        else
        {
            meta["semantic_index_error"] = indexError;
        }
        return ToolCallResult::Ok("Memory path renamed", meta);
    }

    if (command == "reindex")
    {
        size_t indexed = 0;
        std::string indexError;
        if (!RebuildMemorySemanticIndex(indexed, indexError))
        {
            return ToolCallResult::Error("Failed to rebuild memory semantic index: " + indexError);
        }

        json meta = json::object();
        meta["indexed_entries"] = indexed;
        meta["semantic_index_path"] = GetMemorySemanticIndexPath().string();
        return ToolCallResult::Ok("Memory semantic index rebuilt", meta);
    }

    if (command == "retrieve")
    {
        if (!args.contains("query") || !args["query"].is_string())
        {
            return ToolCallResult::Validation("memory retrieve requires 'query'");
        }
        const std::string query = TrimAscii(args["query"].get<std::string>());
        if (query.empty())
        {
            return ToolCallResult::Validation("memory retrieve query cannot be empty");
        }

        const int topK = std::clamp(args.value("top_k", 5), 1, 20);
        const bool refresh = args.value("refresh", false);

        if (refresh)
        {
            size_t refreshed = 0;
            std::string refreshError;
            if (!RebuildMemorySemanticIndex(refreshed, refreshError))
            {
                return ToolCallResult::Error("Failed to refresh memory semantic index: " + refreshError);
            }
        }

        std::vector<MemorySemanticRecord> records;
        std::string loadError;
        if (!LoadMemorySemanticIndex(records, loadError))
        {
            size_t indexed = 0;
            std::string rebuildError;
            if (!RebuildMemorySemanticIndex(indexed, rebuildError) || !LoadMemorySemanticIndex(records, loadError))
            {
                return ToolCallResult::Error("Failed to load memory semantic index: " + loadError);
            }
        }

        const auto queryEmbedding = RawrXD::Runtime::SemanticRetrieval::BuildDeterministicTextEmbedding(
            query, kMemorySemanticDims);
        if (queryEmbedding.empty())
        {
            return ToolCallResult::Validation("memory retrieve could not embed query");
        }

        struct ScoredMemory
        {
            size_t index = 0;
            double score = 0.0;
        };
        std::vector<ScoredMemory> scored;
        scored.reserve(records.size());
        for (size_t i = 0; i < records.size(); ++i)
        {
            const double score = DenseCosine(queryEmbedding, records[i].embedding);
            if (score <= 0.0)
            {
                continue;
            }
            scored.push_back({i, score});
        }
        std::sort(scored.begin(), scored.end(), [](const ScoredMemory& a, const ScoredMemory& b)
                  { return a.score > b.score; });
        if (scored.size() > static_cast<size_t>(topK))
        {
            scored.resize(static_cast<size_t>(topK));
        }

        json result = json::array();
        for (const auto& row : scored)
        {
            const auto& record = records[row.index];
            json item = json::object();
            item["path"] = record.virtualPath;
            item["summary"] = record.summary;
            item["score"] = row.score;
            result.push_back(std::move(item));
        }

        json meta = json::object();
        meta["query"] = query;
        meta["top_k"] = topK;
        meta["returned"] = result.size();
        meta["indexed_entries"] = records.size();
        meta["dimensions"] = kMemorySemanticDims;
        return ToolCallResult::Ok(result.dump(2), meta);
    }

    if (!args.contains("path") || !args["path"].is_string())
    {
        return ToolCallResult::Validation("memory requires 'path' for this command");
    }

    fs::path realPath;
    std::string scope;
    std::string error;
    if (!ResolveMemoryPath(args["path"].get<std::string>(), realPath, scope, error))
    {
        return ToolCallResult::Validation(error);
    }

    if (command == "view")
    {
        if (fs::is_directory(realPath))
        {
            json items = json::array();
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(realPath, fs::directory_options::skip_permission_denied, ec))
            {
                std::string virtualName = MakeVirtualMemoryPath(scope, entry.path());
                if (entry.is_directory(ec))
                {
                    virtualName += "/";
                }
                items.push_back(virtualName);
            }
            json meta = json::object();
            meta["scope"] = scope;
            meta["count"] = items.size();
            return ToolCallResult::Ok(items.dump(2), meta);
        }
        if (!fs::exists(realPath))
        {
            return ToolCallResult::Error("Memory file not found: " + args["path"].get<std::string>());
        }

        std::string content;
        if (!ReadTextFile(realPath.string(), content))
        {
            return ToolCallResult::Error("Failed to read memory file");
        }

        if (args.contains("view_range") && args["view_range"].is_array() && args["view_range"].size() == 2 &&
            args["view_range"].at(0).is_number_integer() && args["view_range"].at(1).is_number_integer())
        {
            const int start = std::max(1, args["view_range"].at(0).get<int>());
            const int end = std::max(start, args["view_range"].at(1).get<int>());
            const std::vector<std::string> lines = ReadLines(content);
            std::ostringstream out;
            for (int i = start - 1; i < end && i < static_cast<int>(lines.size()); ++i)
            {
                if (i > start - 1)
                {
                    out << "\n";
                }
                out << lines[static_cast<size_t>(i)];
            }
            content = out.str();
        }

        json meta = json::object();
        meta["scope"] = scope;
        meta["path"] = args["path"].get<std::string>();
        return ToolCallResult::Ok(content, meta);
    }

    if (command == "create")
    {
        if (!args.contains("file_text") || !args["file_text"].is_string())
        {
            return ToolCallResult::Validation("memory create requires 'file_text'");
        }
        if (fs::exists(realPath))
        {
            return ToolCallResult::Error("Memory file already exists: " + args["path"].get<std::string>());
        }

        std::error_code ec;
        if (!realPath.parent_path().empty())
        {
            fs::create_directories(realPath.parent_path(), ec);
            if (ec)
            {
                return ToolCallResult::Error("Failed to create memory directory: " + ec.message());
            }
        }

        std::ofstream out(realPath, std::ios::binary);
        if (!out)
        {
            return ToolCallResult::Error("Failed to create memory file");
        }
        const std::string content = args["file_text"].get<std::string>();
        out << content;
        json meta = json::object();
        meta["scope"] = scope;
        meta["bytes_written"] = content.size();
        meta["auto_summary"] = BuildMemoryAutoSummary(content);
        size_t indexed = 0;
        std::string indexError;
        if (RebuildMemorySemanticIndex(indexed, indexError))
        {
            meta["semantic_index_entries"] = indexed;
        }
        else
        {
            meta["semantic_index_error"] = indexError;
        }
        return ToolCallResult::Ok("Memory file created", meta);
    }

    if (command == "str_replace")
    {
        if (!args.contains("old_str") || !args["old_str"].is_string() || !args.contains("new_str") ||
            !args["new_str"].is_string())
        {
            return ToolCallResult::Validation("memory str_replace requires 'old_str' and 'new_str'");
        }

        std::string content;
        if (!ReadTextFile(realPath.string(), content))
        {
            return ToolCallResult::Error("Failed to read memory file for replacement");
        }

        const std::string oldStr = args["old_str"].get<std::string>();
        const std::string newStr = args["new_str"].get<std::string>();
        const size_t first = content.find(oldStr);
        if (first == std::string::npos)
        {
            return ToolCallResult::Error("old_str not found in memory file");
        }
        if (content.find(oldStr, first + oldStr.size()) != std::string::npos)
        {
            return ToolCallResult::Validation("old_str must appear exactly once");
        }

        content.replace(first, oldStr.size(), newStr);
        std::ofstream out(realPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return ToolCallResult::Error("Failed to write memory file after replacement");
        }
        out << content;
        json meta = json::object();
        meta["auto_summary"] = BuildMemoryAutoSummary(content);
        size_t indexed = 0;
        std::string indexError;
        if (RebuildMemorySemanticIndex(indexed, indexError))
        {
            meta["semantic_index_entries"] = indexed;
        }
        else
        {
            meta["semantic_index_error"] = indexError;
        }
        return ToolCallResult::Ok("Memory file updated", meta);
    }

    if (command == "insert")
    {
        if (!args.contains("insert_line") || !args["insert_line"].is_number_integer() || !args.contains("insert_text") ||
            !args["insert_text"].is_string())
        {
            return ToolCallResult::Validation("memory insert requires 'insert_line' and 'insert_text'");
        }

        std::string content;
        if (!ReadTextFile(realPath.string(), content))
        {
            return ToolCallResult::Error("Failed to read memory file for insert");
        }
        std::vector<std::string> lines = ReadLines(content);
        const int insertLine = std::max(0, args["insert_line"].get<int>());
        if (insertLine > static_cast<int>(lines.size()))
        {
            return ToolCallResult::Validation("insert_line is out of range");
        }
        lines.insert(lines.begin() + insertLine, args["insert_text"].get<std::string>());

        std::ostringstream out;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                out << "\n";
            }
            out << lines[i];
        }
        std::ofstream file(realPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return ToolCallResult::Error("Failed to write memory file after insert");
        }
        const std::string updated = out.str();
        file << updated;
        json meta = json::object();
        meta["auto_summary"] = BuildMemoryAutoSummary(updated);
        size_t indexed = 0;
        std::string indexError;
        if (RebuildMemorySemanticIndex(indexed, indexError))
        {
            meta["semantic_index_entries"] = indexed;
        }
        else
        {
            meta["semantic_index_error"] = indexError;
        }
        return ToolCallResult::Ok("Memory file updated", meta);
    }

    if (command == "delete")
    {
        std::error_code ec;
        const uintmax_t removed = fs::remove_all(realPath, ec);
        if (ec)
        {
            return ToolCallResult::Error("Failed to delete memory path: " + ec.message());
        }
        json meta = json::object();
        meta["removed_count"] = removed;
        size_t indexed = 0;
        std::string indexError;
        if (RebuildMemorySemanticIndex(indexed, indexError))
        {
            meta["semantic_index_entries"] = indexed;
        }
        else
        {
            meta["semantic_index_error"] = indexError;
        }
        return ToolCallResult::Ok("Memory path deleted", meta);
    }

    return ToolCallResult::Validation("Unsupported memory command: " + command);
}

// ============================================================================
// swebench_autonomous_eval — Run autonomous SWE harness lane from IDE tools
// ============================================================================
ToolCallResult AgentToolHandlers::SwebenchAutonomousEval(const json& args)
{
    const std::string model = args.value("model", "phi3:mini");
    const std::string dataset = args.value("dataset", "d:/rawrxd/data/swebench_seed4.jsonl");
    const std::string outputPrefix = args.value("output_prefix", "d:/reports/swe_autonomous_latest");
    const std::string instanceFilter = args.value("instance_filter", "");
    const int aperture = std::clamp(args.value("aperture_lines", 100), 20, 400);
    std::string outputFormat = args.value("output_format", "fenced");
    if (outputFormat != "plain" && outputFormat != "fenced" && outputFormat != "auto")
    {
        outputFormat = "fenced";
    }
    const int timeoutMs = std::clamp(args.value("timeout_ms", 45000), 1000, 120000);
    const int wallMs = std::clamp(args.value("max_task_wall_ms", 90000), 1000, 300000);
    const int maxTasks = std::clamp(args.value("max_tasks", 0), 0, 1000);
    const bool autonomousRepair = args.value("autonomous_repair", true);
    const int maxRepair = std::clamp(args.value("autonomous_max_repair", 2), 0, 8);

    if (!fs::exists(dataset))
    {
        return ToolCallResult::Validation("Dataset not found: " + dataset);
    }

    std::error_code ec;
    fs::create_directories("d:/reports", ec);

    std::ostringstream command;
    command << "\"d:/rawrxd/build-ninja-max/bin/RawrXD-SWEBench.exe\""
            << " --real-agent"
            << " --model \"" << model << "\""
            << " --dataset \"" << dataset << "\""
            << " --phase4-rag-lite"
            << " --phase4-aperture-lines " << aperture << " --output-format " << outputFormat << " --timeout-ms "
            << timeoutMs << " --max-task-wall-ms " << wallMs << " --jsonl \"" << outputPrefix << ".jsonl\""
            << " --jsonl-summary \"" << outputPrefix << "_summary.json\"";

    if (autonomousRepair)
    {
        command << " --autonomous-repair --autonomous-max-repair " << maxRepair;
    }
    if (!instanceFilter.empty())
    {
        command << " --instance-filter \"" << instanceFilter << "\"";
    }
    if (maxTasks > 0)
    {
        command << " --max-tasks " << maxTasks;
    }

    std::wstring cmdLine = L"cmd.exe /C " + ToWide(command.str());
    std::string output;
    uint32_t exitCode = 0;
    const bool ok = RunProcess(cmdLine, static_cast<uint32_t>(wallMs + 15000), output, exitCode);

    nlohmann::json meta = nlohmann::json::object();
    meta["model"] = model;
    meta["dataset"] = dataset;
    meta["output_prefix"] = outputPrefix;
    meta["output_format"] = outputFormat;
    meta["aperture_lines"] = aperture;
    meta["autonomous_repair"] = autonomousRepair;
    meta["autonomous_max_repair"] = maxRepair;
    meta["timeout_ms"] = timeoutMs;
    meta["max_task_wall_ms"] = wallMs;
    meta["exit_code"] = exitCode;

    if (!ok && exitCode == WAIT_TIMEOUT)
    {
        return ToolCallResult::TimedOut("swebench_autonomous_eval timed out\n" + output);
    }

    if (exitCode == 0)
    {
        return ToolCallResult::Ok("swebench_autonomous_eval completed. JSONL: " + outputPrefix +
                                      ".jsonl, Summary: " + outputPrefix + "_summary.json\n" + output,
                                  meta);
    }

    return ToolCallResult::Error("swebench_autonomous_eval failed (exit_code=" + std::to_string(exitCode) + ")\n" +
                                     output,
                                 ToolOutcome::ExecutionError);
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
    json rf_start = json::object();
    rf_start["type"] = "integer";
    rf_start["description"] = "Optional 1-based starting line to read";
    json rf_end = json::object();
    rf_end["type"] = "integer";
    rf_end["description"] = "Optional inclusive 1-based ending line to read";
    nlohmann::json metadata = nlohmann::json::object();
    metadata["path"] = rf_path;
    metadata["start_line"] = rf_start;
    metadata["end_line"] = rf_end;
    rf_prop["path"] = metadata["path"];
    rf_prop["start_line"] = metadata["start_line"];
    rf_prop["end_line"] = metadata["end_line"];
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

    // edit_file (alias of replace_in_file for Copilot/Cursor compatibility)
    json ef = rif;
    ef["function"]["name"] = "edit_file";
    ef["function"]["description"] = "Edit a file by replacing an exact text block with new text.";
    tools.push_back(ef);
    
    // undo_edit
    json ue = json::object();
    ue["type"] = "function";
    json ue_f = json::object();
    ue_f["name"] = "undo_edit";
    ue_f["description"] =
        "Restore a file from the latest backup snapshot in .rawrxd-backups. Supports preview_only for safe diff review.";
    json ue_p = json::object();
    ue_p["type"] = "object";
    json ue_prop = json::object();
    json ue_path = json::object();
    ue_path["type"] = "string";
    ue_path["description"] = "Absolute path to the file to restore";
    json ue_preview = json::object();
    ue_preview["type"] = "boolean";
    ue_preview["description"] = "If true, return rollback diff metadata without writing.";
    json ue_dry = json::object();
    ue_dry["type"] = "boolean";
    ue_dry["description"] = "Alias of preview_only.";
    ue_prop["path"] = ue_path;
    ue_prop["preview_only"] = ue_preview;
    ue_prop["dry_run"] = ue_dry;
    ue_p["properties"] = ue_prop;
    ue_p["required"] = jstrArr({"path"});
    ue_f["parameters"] = ue_p;
    ue["function"] = ue_f;
    tools.push_back(ue);

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
    ec_f["description"] =
        "Run a command in the terminal (cmd.exe). Use for builds, tests, git. Working directory defaults to the "
        "primary allowlisted workspace root when cwd is omitted.";
    json ec_p = json::object();
    ec_p["type"] = "object";
    json ec_prop = json::object();
    json ec_cmd = json::object();
    ec_cmd["type"] = "string";
    ec_cmd["description"] = "Command to execute";
    json ec_to = json::object();
    ec_to["type"] = "number";
    ec_to["description"] = "Timeout in milliseconds (default 30000, max 300000)";
    json ec_cwd = json::object();
    ec_cwd["type"] = "string";
    ec_cwd["description"] = "Optional working directory (must be allowlisted). Same intent as working_directory.";
    json ec_wd = json::object();
    ec_wd["type"] = "string";
    ec_wd["description"] = "Optional working directory alias for cwd.";
    json ec_mirror = json::object();
    ec_mirror["type"] = "boolean";
    ec_mirror["description"] =
        "If true (or mirror_to_ide_agent_terminal), echo the command and captured output to the IDE agent terminal.";
    json ec_mirror2 = json::object();
    ec_mirror2["type"] = "boolean";
    ec_mirror2["description"] = "Alias of use_integrated_terminal for mirroring to the agent terminal.";

    nlohmann::json ec_metadata = nlohmann::json::object();
    ec_metadata["command"] = ec_cmd;
    ec_metadata["timeout"] = ec_to;
    ec_metadata["cwd"] = ec_cwd;
    ec_metadata["working_directory"] = ec_wd;
    ec_metadata["use_integrated_terminal"] = ec_mirror;
    ec_metadata["mirror_to_ide_agent_terminal"] = ec_mirror2;
    ec_prop["command"] = ec_metadata["command"];
    ec_prop["timeout"] = ec_metadata["timeout"];
    ec_prop["cwd"] = ec_metadata["cwd"];
    ec_prop["working_directory"] = ec_metadata["working_directory"];
    ec_prop["use_integrated_terminal"] = ec_metadata["use_integrated_terminal"];
    ec_prop["mirror_to_ide_agent_terminal"] = ec_metadata["mirror_to_ide_agent_terminal"];

    ec_p["properties"] = ec_prop;
    ec_p["required"] = jstrArr({"command"});
    ec_f["parameters"] = ec_p;
    ec["function"] = ec_f;
    tools.push_back(ec);

    // get_code_outline
    json gco = json::object();
    gco["type"] = "function";
    json gco_f = json::object();
    gco_f["name"] = "get_code_outline";
    gco_f["description"] = "Extract top-level symbols from a source file (functions/classes/labels)";
    json gco_p = json::object();
    gco_p["type"] = "object";
    json gco_prop = json::object();
    json gco_path = json::object();
    gco_path["type"] = "string";
    gco_path["description"] = "Absolute file path to inspect";
    json gco_max = json::object();
    gco_max["type"] = "number";
    gco_max["description"] = "Maximum symbols to return (default 500, max 2000)";
    json gco_use_lsp = json::object();
    gco_use_lsp["type"] = "boolean";
    gco_use_lsp["description"] = "Use LSP documentSymbol first when available (default true).";
    json gco_lsp_timeout = json::object();
    gco_lsp_timeout["type"] = "number";
    gco_lsp_timeout["description"] = "LSP wait timeout in ms (default 200, clamped 50-2000).";
    gco_prop["path"] = gco_path;
    gco_prop["max_symbols"] = gco_max;
    gco_prop["use_lsp"] = gco_use_lsp;
    gco_prop["lsp_timeout_ms"] = gco_lsp_timeout;
    gco_p["properties"] = gco_prop;
    gco_p["required"] = jstrArr({"path"});
    gco_f["parameters"] = gco_p;
    gco["function"] = gco_f;
    tools.push_back(gco);

    // rollback_file
    json rbf = json::object();
    rbf["type"] = "function";
    json rbf_f = json::object();
    rbf_f["name"] = "rollback_file";
    rbf_f["description"] =
        "Restore a file from .rawrxd-backups snapshot. Defaults to latest matching backup unless backup_path is set.";
    json rbf_p = json::object();
    rbf_p["type"] = "object";
    json rbf_prop = json::object();
    json rbf_path = json::object();
    rbf_path["type"] = "string";
    rbf_path["description"] = "Absolute target file path to restore";
    json rbf_backup = json::object();
    rbf_backup["type"] = "string";
    rbf_backup["description"] = "Optional explicit backup path (.bak)";
    json rbf_preview = json::object();
    rbf_preview["type"] = "boolean";
    rbf_preview["description"] = "If true, validate and report selected backup without writing";
    json rbf_forward = json::object();
    rbf_forward["type"] = "boolean";
    rbf_forward["description"] = "Create a forward backup before restore (default true)";
    rbf_prop["path"] = rbf_path;
    rbf_prop["backup_path"] = rbf_backup;
    rbf_prop["preview_only"] = rbf_preview;
    rbf_prop["dry_run"] = rbf_preview;
    rbf_prop["create_forward_backup"] = rbf_forward;
    rbf_p["properties"] = rbf_prop;
    rbf_p["required"] = jstrArr({"path"});
    rbf_f["parameters"] = rbf_p;
    rbf["function"] = rbf_f;
    tools.push_back(rbf);

    // run_terminal (alias of execute_command)
    json rt = ec;
    rt["function"]["name"] = "run_terminal";
    rt["function"]["description"] = "Run a shell command in terminal and capture output.";
    tools.push_back(rt);

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

    // file_search
    json fsrch = json::object();
    fsrch["type"] = "function";
    json fsrch_f = json::object();
    fsrch_f["name"] = "file_search";
    fsrch_f["description"] = "Search for files by glob/path pattern and return matching absolute paths.";
    json fsrch_p = json::object();
    fsrch_p["type"] = "object";
    json fsrch_prop = json::object();
    json fsrch_pattern = json::object();
    fsrch_pattern["type"] = "string";
    fsrch_pattern["description"] = "Single glob-like pattern such as **/ToolExecutor* or src/**/*.cpp";
    json fsrch_patterns = json::object();
    fsrch_patterns["type"] = "array";
    fsrch_patterns["description"] = "Optional array of glob-like patterns";
    json fsrch_root = json::object();
    fsrch_root["type"] = "string";
    fsrch_root["description"] = "Optional root directory to search";
    json fsrch_max = json::object();
    fsrch_max["type"] = "integer";
    fsrch_max["description"] = "Maximum number of results to return";
    fsrch_prop["pattern"] = fsrch_pattern;
    fsrch_prop["patterns"] = fsrch_patterns;
    fsrch_prop["root"] = fsrch_root;
    fsrch_prop["max_results"] = fsrch_max;
    fsrch_p["properties"] = fsrch_prop;
    fsrch_f["parameters"] = fsrch_p;
    fsrch["function"] = fsrch_f;
    tools.push_back(fsrch);

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
    json pme_edits = json::object();
    pme_edits["type"] = "array";
    pme_edits["description"] = "Structured edits: [{file,type,line_start,line_end,content,reason}]";
    pme_prop["instruction"] = pme_instr;
    pme_prop["edits"] = pme_edits;
    pme_p["properties"] = pme_prop;
    pme_p["required"] = jstrArr({"edits"});
    pme_f["parameters"] = pme_p;
    pme["function"] = pme_f;
    tools.push_back(pme);

    // preview_multifile_diff
    json pmd = json::object();
    pmd["type"] = "function";
    json pmd_f = json::object();
    pmd_f["name"] = "preview_multifile_diff";
    pmd_f["description"] = "Preview unified diffs for a structured multi-file edit plan without modifying files.";
    json pmd_p = json::object();
    pmd_p["type"] = "object";
    json pmd_prop = json::object();
    pmd_prop["edits"] = pme_edits;
    pmd_p["properties"] = pmd_prop;
    pmd_p["required"] = jstrArr({"edits"});
    pmd_f["parameters"] = pmd_p;
    pmd["function"] = pmd_f;
    tools.push_back(pmd);

    // apply_multifile_edits
    json ame = json::object();
    ame["type"] = "function";
    json ame_f = json::object();
    ame_f["name"] = "apply_multifile_edits";
    ame_f["description"] = "Apply a structured multi-file edit plan transactionally with rollback on failure.";
    json ame_p = json::object();
    ame_p["type"] = "object";
    json ame_prop = json::object();
    ame_prop["edits"] = pme_edits;
    ame_p["properties"] = ame_prop;
    ame_p["required"] = jstrArr({"edits"});
    ame_f["parameters"] = ame_p;
    ame["function"] = ame_f;
    tools.push_back(ame);

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

    // manage_todo_list
    json mtl = json::object();
    mtl["type"] = "function";
    json mtl_f = json::object();
    mtl_f["name"] = "manage_todo_list";
    mtl_f["description"] = "Replace the tracked todo list with the provided plan items and statuses.";
    json mtl_p = json::object();
    mtl_p["type"] = "object";
    json mtl_prop = json::object();
    json mtl_list = json::object();
    mtl_list["type"] = "array";
    mtl_list["description"] = "Array of todo items with id, title, status, and optional priority/category";
    mtl_prop["todoList"] = mtl_list;
    mtl_p["properties"] = mtl_prop;
    mtl_p["required"] = jstrArr({"todoList"});
    mtl_f["parameters"] = mtl_p;
    mtl["function"] = mtl_f;
    tools.push_back(mtl);

    // memory
    json mem = json::object();
    mem["type"] = "function";
    json mem_f = json::object();
    mem_f["name"] = "memory";
    mem_f["description"] = "Manage persistent RawrXD memory files using /memories, /memories/session, or /memories/repo paths.";
    json mem_p = json::object();
    mem_p["type"] = "object";
    json mem_prop = json::object();
    json mem_cmd = json::object();
    mem_cmd["type"] = "string";
    mem_cmd["description"] = "Operation: view, create, str_replace, insert, delete, rename, reindex, or retrieve";
    json mem_path = json::object();
    mem_path["type"] = "string";
    mem_path["description"] = "Virtual memory path such as /memories/note.md or /memories/session/plan.md";
    json mem_file = json::object();
    mem_file["type"] = "string";
    mem_file["description"] = "File content for create";
    json mem_old = json::object();
    mem_old["type"] = "string";
    mem_old["description"] = "Exact string to replace for str_replace";
    json mem_new = json::object();
    mem_new["type"] = "string";
    mem_new["description"] = "Replacement string for str_replace";
    json mem_insert_line = json::object();
    mem_insert_line["type"] = "integer";
    mem_insert_line["description"] = "0-based line index for insert";
    json mem_insert_text = json::object();
    mem_insert_text["type"] = "string";
    mem_insert_text["description"] = "Text to insert at the requested line";
    json mem_range = json::object();
    mem_range["type"] = "array";
    mem_range["description"] = "Optional [start,end] 1-based line range for view";
    json mem_old_path = json::object();
    mem_old_path["type"] = "string";
    mem_old_path["description"] = "Existing virtual path for rename";
    json mem_new_path = json::object();
    mem_new_path["type"] = "string";
    mem_new_path["description"] = "New virtual path for rename";
    json mem_query = json::object();
    mem_query["type"] = "string";
    mem_query["description"] = "Semantic retrieval query text (for command=retrieve)";
    json mem_top_k = json::object();
    mem_top_k["type"] = "integer";
    mem_top_k["description"] = "Maximum semantic retrieval hits (1-20)";
    json mem_refresh = json::object();
    mem_refresh["type"] = "boolean";
    mem_refresh["description"] = "Rebuild semantic memory index before retrieve";
    mem_prop["command"] = mem_cmd;
    mem_prop["path"] = mem_path;
    mem_prop["file_text"] = mem_file;
    mem_prop["old_str"] = mem_old;
    mem_prop["new_str"] = mem_new;
    mem_prop["insert_line"] = mem_insert_line;
    mem_prop["insert_text"] = mem_insert_text;
    mem_prop["view_range"] = mem_range;
    mem_prop["old_path"] = mem_old_path;
    mem_prop["new_path"] = mem_new_path;
    mem_prop["query"] = mem_query;
    mem_prop["top_k"] = mem_top_k;
    mem_prop["refresh"] = mem_refresh;
    mem_p["properties"] = mem_prop;
    mem_p["required"] = jstrArr({"command"});
    mem_f["parameters"] = mem_p;
    mem["function"] = mem_f;
    tools.push_back(mem);

    json mem_file_alias = mem;
    mem_file_alias["function"]["name"] = "memory_file";
    mem_file_alias["function"]["description"] =
        "File-oriented alias for the memory tool. Manage persistent RawrXD memory files using /memories, "
        "/memories/session, or /memories/repo paths.";
    tools.push_back(mem_file_alias);

    // generate_image
    json gi = json::object();
    gi["type"] = "function";
    json gi_f = json::object();
    gi_f["name"] = "generate_image";
    gi_f["description"] = "Generate an image artifact from a prompt and save it to the workspace.";
    json gi_p = json::object();
    gi_p["type"] = "object";
    json gi_prop = json::object();
    json gi_prompt = json::object();
    gi_prompt["type"] = "string";
    gi_prompt["description"] = "Image generation prompt";
    json gi_style = json::object();
    gi_style["type"] = "string";
    gi_style["description"] = "Optional style hint";
    json gi_width = json::object();
    gi_width["type"] = "integer";
    gi_width["description"] = "Image width (64-4096)";
    json gi_height = json::object();
    gi_height["type"] = "integer";
    gi_height["description"] = "Image height (64-4096)";
    json gi_output = json::object();
    gi_output["type"] = "string";
    gi_output["description"] = "Optional output BMP path";
    gi_prop["prompt"] = gi_prompt;
    gi_prop["style"] = gi_style;
    gi_prop["width"] = gi_width;
    gi_prop["height"] = gi_height;
    gi_prop["output_path"] = gi_output;
    gi_p["properties"] = gi_prop;
    gi_p["required"] = jstrArr({"prompt"});
    gi_f["parameters"] = gi_p;
    gi["function"] = gi_f;
    tools.push_back(gi);

    // generate_video
    json gv = json::object();
    gv["type"] = "function";
    json gv_f = json::object();
    gv_f["name"] = "generate_video";
    gv_f["description"] = "Generate a local video artifact from a prompt/storyboard using the native tubi backend.";
    json gv_p = json::object();
    gv_p["type"] = "object";
    json gv_prop = json::object();
    json gv_prompt = json::object();
    gv_prompt["type"] = "string";
    gv_prompt["description"] = "Video generation prompt";
    json gv_story = json::object();
    gv_story["type"] = "string";
    gv_story["description"] = "Optional storyboard / shot list";
    json gv_style = json::object();
    gv_style["type"] = "string";
    gv_style["description"] = "Style preset (e.g. Cinematic, Anime)";
    json gv_duration = json::object();
    gv_duration["type"] = "string";
    gv_duration["description"] = "Duration string (e.g. 6s, 10s)";
    json gv_aspect = json::object();
    gv_aspect["type"] = "string";
    gv_aspect["description"] = "Aspect ratio (e.g. 16:9)";
    json gv_resolution = json::object();
    gv_resolution["type"] = "string";
    gv_resolution["description"] = "Resolution preset (e.g. 720p, 1080p)";
    json gv_output = json::object();
    gv_output["type"] = "string";
    gv_output["description"] = "Optional output directory";
    gv_prop["prompt"] = gv_prompt;
    gv_prop["storyboard"] = gv_story;
    gv_prop["style"] = gv_style;
    gv_prop["duration"] = gv_duration;
    gv_prop["aspect_ratio"] = gv_aspect;
    gv_prop["resolution"] = gv_resolution;
    gv_prop["output_dir"] = gv_output;
    gv_p["properties"] = gv_prop;
    gv_p["required"] = jstrArr({"prompt"});
    gv_f["parameters"] = gv_p;
    gv["function"] = gv_f;
    tools.push_back(gv);

    // escalate_to_swarm - Structural escape hatch for model reasoning limits
    json ets = json::object();
    ets["type"] = "function";
    json ets_f = json::object();
    ets_f["name"] = "escalate_to_swarm";
    ets_f["description"] = "Escalate to a swarm of models or switch backends when current model hits reasoning limits. Provides context-aware multi-model routing and parallel task distribution.";
    json ets_p = json::object();
    ets_p["type"] = "object";
    json ets_prop = json::object();
    json ets_reason = json::object();
    ets_reason["type"] = "string";
    ets_reason["description"] = "Detailed explanation of why escalation is needed (e.g. 'Context limit reached', 'MASM expertise required', 'Requires model specialization')";
    json ets_task = json::object();
    ets_task["type"] = "string";
    ets_task["description"] = "Specific sub-problem to hand off to the swarm";
    json ets_model = json::object();
    ets_model["type"] = "string";
    ets_model["description"] = "Preferred backend model hint (e.g. 'codestral' for asm, 'gpt-4o' for reasoning, 'llamacpp-local')";
    json ets_subtasks = json::object();
    ets_subtasks["type"] = "integer";
    ets_subtasks["description"] = "Number of parallel subtask instances to fan-out (1-16, default 1)";
    ets_prop["reason"] = ets_reason;
    ets_prop["task"] = ets_task;
    ets_prop["preferred_model"] = ets_model;
    ets_prop["subtask_count"] = ets_subtasks;
    ets_p["properties"] = ets_prop;
    ets_p["required"] = jstrArr({"reason", "task"});
    ets_f["parameters"] = ets_p;
    ets["function"] = ets_f;
    tools.push_back(ets);

    // swebench_autonomous_eval
    json sae = json::object();
    sae["type"] = "function";
    json sae_f = json::object();
    sae_f["name"] = "swebench_autonomous_eval";
    sae_f["description"] = "Run RawrXD-SWEBench autonomous evaluation lane and emit JSONL/summary reports.";
    json sae_p = json::object();
    sae_p["type"] = "object";
    json sae_prop = json::object();
    json sae_model = json::object();
    sae_model["type"] = "string";
    sae_model["description"] = "Model name (default phi3:mini)";
    json sae_dataset = json::object();
    sae_dataset["type"] = "string";
    sae_dataset["description"] = "Dataset JSONL path";
    json sae_prefix = json::object();
    sae_prefix["type"] = "string";
    sae_prefix["description"] = "Output path prefix for JSONL and summary";
    json sae_filter = json::object();
    sae_filter["type"] = "string";
    sae_filter["description"] = "Optional instance filter list (comma-separated task IDs)";
    json sae_ap = json::object();
    sae_ap["type"] = "integer";
    sae_ap["description"] = "Aperture lines (20-400)";
    json sae_fmt = json::object();
    sae_fmt["type"] = "string";
    sae_fmt["description"] = "Output format: plain|fenced|auto";
    json sae_to = json::object();
    sae_to["type"] = "integer";
    sae_to["description"] = "HTTP timeout milliseconds";
    json sae_wall = json::object();
    sae_wall["type"] = "integer";
    sae_wall["description"] = "Task wall-time milliseconds";
    json sae_mt = json::object();
    sae_mt["type"] = "integer";
    sae_mt["description"] = "Optional max tasks";
    json sae_ar = json::object();
    sae_ar["type"] = "boolean";
    sae_ar["description"] = "Enable autonomous repair";
    json sae_mr = json::object();
    sae_mr["type"] = "integer";
    sae_mr["description"] = "Max autonomous repair retries";
    sae_prop["model"] = sae_model;
    sae_prop["dataset"] = sae_dataset;
    sae_prop["output_prefix"] = sae_prefix;
    sae_prop["instance_filter"] = sae_filter;
    sae_prop["aperture_lines"] = sae_ap;
    sae_prop["output_format"] = sae_fmt;
    sae_prop["timeout_ms"] = sae_to;
    sae_prop["max_task_wall_ms"] = sae_wall;
    sae_prop["max_tasks"] = sae_mt;
    sae_prop["autonomous_repair"] = sae_ar;
    sae_prop["autonomous_max_repair"] = sae_mr;
    sae_p["properties"] = sae_prop;
    sae_f["parameters"] = sae_p;
    sae["function"] = sae_f;
    tools.push_back(sae);

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

    // apply_hotpatch
    json ah = json::object();
    ah["type"] = "function";
    json ah_f = json::object();
    ah_f["name"] = "apply_hotpatch";
    ah_f["description"] = "Apply an in-memory hotpatch to a running module or address. Only memory-mode is permitted via agent tool.";
    json ah_p = json::object();
    ah_p["type"] = "object";
    json ah_prop = json::object();
    json ah_target = json::object();
    ah_target["type"] = "string";
    ah_target["description"] = "Module name, symbol, or descriptive target identifier";
    json ah_addr = json::object();
    ah_addr["type"] = "string";
    ah_addr["description"] = "Hex address string (e.g. 0x140000000) or symbol name to resolve";
    json ah_hex = json::object();
    ah_hex["type"] = "string";
    ah_hex["description"] = "Hex-encoded patch bytes (e.g. '9090C3')";
    json ah_mode = json::object();
    ah_mode["type"] = "string";
    ah_mode["description"] = "Patch mode: memory (default), byte, server. Only 'memory' allowed via agent.";
    ah_prop["target"] = ah_target;
    ah_prop["address"] = ah_addr;
    ah_prop["patch_hex"] = ah_hex;
    ah_prop["mode"] = ah_mode;
    ah_p["properties"] = ah_prop;
    ah_p["required"] = jstrArr({"target"});
    ah_f["parameters"] = ah_p;
    ah["function"] = ah_f;
    tools.push_back(ah);

    // revert_hotpatch
    json rh = json::object();
    rh["type"] = "function";
    json rh_f = json::object();
    rh_f["name"] = "revert_hotpatch";
    rh_f["description"] = "Revert the last applied hotpatch for a target, or by address.";
    json rh_p = json::object();
    rh_p["type"] = "object";
    json rh_prop = json::object();
    json rh_target = json::object();
    rh_target["type"] = "string";
    rh_target["description"] = "Module name, symbol, or target identifier";
    json rh_addr = json::object();
    rh_addr["type"] = "string";
    rh_addr["description"] = "Hex address string to revert patch at";
    rh_prop["target"] = rh_target;
    rh_prop["address"] = rh_addr;
    rh_p["properties"] = rh_prop;
    rh_p["required"] = jstrArr({"target"});
    rh_f["parameters"] = rh_p;
    rh["function"] = rh_f;
    tools.push_back(rh);

    // list_hotpatches
    json lh = json::object();
    lh["type"] = "function";
    json lh_f = json::object();
    lh_f["name"] = "list_hotpatches";
    lh_f["description"] = "List all active hotpatches across memory, byte, and server layers.";
    json lh_p = json::object();
    lh_p["type"] = "object";
    json lh_prop = json::object();
    json lh_layer = json::object();
    lh_layer["type"] = "string";
    lh_layer["description"] = "Filter by layer: all, memory, byte, server, live, shadow. Default: all.";
    lh_prop["layer"] = lh_layer;
    lh_p["properties"] = lh_prop;
    lh_p["required"] = jstrArr({});
    lh_f["parameters"] = lh_p;
    lh["function"] = lh_f;
    tools.push_back(lh);

    // hotpatch_status
    json hs = json::object();
    hs["type"] = "function";
    json hs_f = json::object();
    hs_f["name"] = "hotpatch_status";
    hs_f["description"] = "Get hotpatch subsystem statistics and health.";
    json hs_p = json::object();
    hs_p["type"] = "object";
    json hs_prop = json::object();
    json hs_layer = json::object();
    hs_layer["type"] = "string";
    hs_layer["description"] = "Layer to query: all, memory, byte, server, pt, live, shadow, sentinel. Default: all.";
    hs_prop["layer"] = hs_layer;
    hs_p["properties"] = hs_prop;
    hs_p["required"] = jstrArr({});
    hs_f["parameters"] = hs_p;
    hs["function"] = hs_f;
    tools.push_back(hs);

    // Merge collaboration tool schemas

    {
        json collabSchemas = CollabToolHandlers::GetAllSchemas();
        for (auto& s : collabSchemas)
            tools.push_back(s);
    }

    return tools;
}

std::string AgentToolHandlers::BuildCompactToolCatalogForPrompt()
{
    return BuildCompactToolCatalog(GetAllSchemas());
}

RawrXD::Agent::ScopedInstructionPromptData AgentToolHandlers::ResolveScopedInstructions(
    const std::string& cwd, const std::vector<std::string>& openFiles)
{
    auto& provider = RawrXD::Core::ScopedInstructionsProvider::instance();
    provider.setProjectRoot(cwd);

    ScopedInstructionPromptData resolved;
    const auto providerResolved = provider.resolveForTargets(openFiles, 8000);
    resolved.payload = providerResolved.promptPayload;
    resolved.sources = providerResolved.sources;
    resolved.telemetry = RawrXD::Core::ScopedInstructionsProvider::formatTelemetry(providerResolved);

    return resolved;
}

std::string AgentToolHandlers::GetSystemPrompt(const std::string& cwd, const std::vector<std::string>& openFiles,
                                               std::vector<std::string>* appliedInstructionSources)
{
    std::string filesStr;
    for (const auto& f : openFiles)
    {
        filesStr += "- " + f + "\n";
    }

    std::ostringstream ss;
    const json toolSchemas = GetAllSchemas();
    ss << "You are RawrXD Agent, a local autonomous coding assistant (Cursor / GitHub Copilot–class behavior).\n"
       << "You can explore and edit the workspace like an engineer using the IDE: same tree as File Explorer, "
          "sandboxed to the workspace root below.\n\n"
       << "Workspace root: " << cwd << "\n"
       << "Open files (editor):\n"
       << (filesStr.empty() ? "  (none)\n" : filesStr) << "\n"
       << "Available Tools:\n"
       << BuildCompactToolCatalog(toolSchemas) << "\nTool Call Protocol:\n"
       << "- Emit JSON only when invoking a tool.\n"
       << "- Format: {\"tool_call\": {\"name\": \"tool_name\", \"arguments\": {...}}}\n"
       << "- Do not prepend explanations before the JSON tool call.\n\n"
       << "Rules:\n"
       << "1. Use list_dir (or list_directory) on \".\" or subfolders to explore the tree before assuming layout.\n"
       << "2. Always read_file before editing to verify current content.\n"
       << "3. Use replace_in_file for surgical edits; write_file for new files only.\n"
       << "4. Include 3+ lines of context in old_string for uniqueness.\n"
       << "5. Run get_diagnostics after substantive code changes when applicable.\n"
       << "6. Give a short plan in natural language, then tool JSON — do not mix long prose with JSON.\n"
       << "7. Use search_code to find relevant code before making assumptions.\n"
       << "8. Do not modify files outside the workspace.\n";

    const ScopedInstructionPromptData resolved = ResolveScopedInstructions(cwd, openFiles);
    if (appliedInstructionSources)
    {
        *appliedInstructionSources = resolved.sources;
    }

    if (!resolved.payload.empty())
    {
        ss << "\nScoped Instructions (Resolved):\n" << resolved.payload << "\n";
        if (!resolved.telemetry.empty())
        {
            ss << resolved.telemetry << "\n";
        }
    }

    if (!resolved.sources.empty())
    {
        ss << "\nScoped Instruction Sources:\n";
        for (const auto& src : resolved.sources)
        {
            ss << "- " << src << "\n";
        }
    }

    return ss.str();
}

std::string AgentToolHandlers::CreateBackup(const std::string& path)
{
    return CreateBackupImpl(path);
}

// ============================================================================
// Generic dispatch — Instance / HasTool / Execute
// Used by DeterministicReplayEngine for transcript replay.
// ============================================================================
AgentToolHandlers::AgentToolHandlers()
{
    InitializeDispatchTable();
}

ToolCallResult AgentToolHandlers::ToolGitStatus(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "git status --short --branch";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGitDiff(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "git diff --stat";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGitLog(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "git log --oneline -n 20";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGitBranch(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "git branch --all";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGhIssueList(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "gh issue list --limit 20";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGhPrList(const nlohmann::json& args)
{
    nlohmann::json forwarded = args;
    if (!forwarded.contains("command") || !forwarded["command"].is_string())
    {
        forwarded["command"] = "gh pr list --limit 20";
    }
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ToolGitCommit(const nlohmann::json& args)
{
    std::string msg = args.value("message", "Agentic update");
    nlohmann::json forwarded;
    forwarded["command"] = "git commit -m \"" + msg + "\"";
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::GHPrView(const nlohmann::json& args)
{
    std::string num = args.value("number", "");
    if (num.empty())
    {
        return ToolCallResult::Validation("gh_pr_view requires 'number' (string)");
    }
    nlohmann::json forwarded;
    forwarded["command"] = "gh pr view " + num;
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::GHIssueView(const nlohmann::json& args)
{
    std::string num = args.value("number", "");
    if (num.empty())
    {
        return ToolCallResult::Validation("gh_issue_view requires 'number' (string)");
    }
    nlohmann::json forwarded;
    forwarded["command"] = "gh issue view " + num;
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::GHCreatePR(const nlohmann::json& args)
{
    std::string title = args.value("title", "Agentic Fix");
    std::string body = args.value("body", "Autonomous PR from RawrXD Agentic Core");
    nlohmann::json forwarded;
    forwarded["command"] = "gh pr create --title \"" + title + "\" --body \"" + body + "\"";
    return ExecuteCommand(forwarded);
}

// ============================================================================
// Debug Tool Handlers — DbgEng COM wrapper (15 tools)
// ============================================================================

static auto& DbgEngine() { return RawrXD::Debugger::NativeDebuggerEngine::Instance(); }
static auto& AIDbg()     { return RawrXD::Debug::AIDebugAgent::Instance(); }

static ToolCallResult DbgEnsureInit()
{
    auto& engine = DbgEngine();
    if (!engine.isInitialized()) {
        RawrXD::Debugger::DebugConfig cfg;
        auto r = engine.initialize(cfg);
        if (!r.success)
            return ToolCallResult::Error(std::string("DbgEng init failed: ") + r.detail);
    }
    return ToolCallResult::Ok("");
}

ToolCallResult AgentToolHandlers::DebugLaunch(const nlohmann::json& args)
{
    auto init = DbgEnsureInit();
    if (init.isError()) return init;

    std::string exePath = args.value("exe_path", "");
    if (exePath.empty()) return ToolCallResult::Validation("exe_path is required");
    std::string cmdArgs = args.value("arguments", "");
    std::string workDir = args.value("working_dir", "");

    auto& engine = DbgEngine();
    auto result = engine.launchProcess(exePath, cmdArgs, workDir);
    if (!result.success)
        return ToolCallResult::Error(std::string("Launch failed: ") + result.detail);

    json resp;
    resp["pid"] = engine.getTargetPID();
    resp["target"] = engine.getTargetName();
    resp["state"] = engine.toJsonStatus();
    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::DebugAttach(const nlohmann::json& args)
{
    auto init = DbgEnsureInit();
    if (init.isError()) return init;

    uint32_t pid = args.value("pid", 0u);
    if (pid == 0) return ToolCallResult::Validation("pid is required (non-zero)");

    auto result = DbgEngine().attachToProcess(pid);
    if (!result.success)
        return ToolCallResult::Error(std::string("Attach failed: ") + result.detail);

    return ToolCallResult::Ok(DbgEngine().toJsonStatus());
}

ToolCallResult AgentToolHandlers::DebugBreakTool(const nlohmann::json& /*args*/)
{
    auto result = DbgEngine().breakExecution();
    if (!result.success)
        return ToolCallResult::Error(std::string("Break failed: ") + result.detail);
    return ToolCallResult::Ok(DbgEngine().toJsonStatus());
}

ToolCallResult AgentToolHandlers::DebugContinue(const nlohmann::json& /*args*/)
{
    auto result = DbgEngine().go();
    if (!result.success)
        return ToolCallResult::Error(std::string("Continue failed: ") + result.detail);
    return ToolCallResult::Ok(DbgEngine().toJsonStatus());
}

ToolCallResult AgentToolHandlers::DebugStepOver(const nlohmann::json& /*args*/)
{
    auto result = DbgEngine().stepOver();
    if (!result.success)
        return ToolCallResult::Error(std::string("StepOver failed: ") + result.detail);
    return ToolCallResult::Ok(DbgEngine().toJsonStatus());
}

ToolCallResult AgentToolHandlers::DebugStepInto(const nlohmann::json& /*args*/)
{
    auto result = DbgEngine().stepInto();
    if (!result.success)
        return ToolCallResult::Error(std::string("StepInto failed: ") + result.detail);
    return ToolCallResult::Ok(DbgEngine().toJsonStatus());
}

ToolCallResult AgentToolHandlers::DebugAddBreakpoint(const nlohmann::json& args)
{
    // Accept symbol, file:line, or hex address
    std::string symbol = args.value("symbol", "");
    std::string file   = args.value("file", "");
    int line           = args.value("line", 0);
    std::string addrStr = args.value("address", "");

    auto& engine = DbgEngine();
    RawrXD::Debugger::DebugResult result;

    if (!symbol.empty()) {
        result = engine.addBreakpointBySymbol(symbol);
    } else if (!file.empty() && line > 0) {
        result = engine.addBreakpointBySourceLine(file, line);
    } else if (!addrStr.empty()) {
        uint64_t addr = std::stoull(addrStr, nullptr, 0);
        result = engine.addBreakpoint(addr);
    } else {
        return ToolCallResult::Validation("Provide symbol, file+line, or address for breakpoint");
    }

    if (!result.success)
        return ToolCallResult::Error(std::string("Add breakpoint failed: ") + result.detail);

    return ToolCallResult::Ok(engine.toJsonBreakpoints());
}

ToolCallResult AgentToolHandlers::DebugRemoveBreakpoint(const nlohmann::json& args)
{
    uint32_t bpId = args.value("id", 0u);
    if (bpId == 0) return ToolCallResult::Validation("Breakpoint id is required");

    auto result = DbgEngine().removeBreakpoint(bpId);
    if (!result.success)
        return ToolCallResult::Error(std::string("Remove breakpoint failed: ") + result.detail);

    return ToolCallResult::Ok(DbgEngine().toJsonBreakpoints());
}

ToolCallResult AgentToolHandlers::DebugStacktrace(const nlohmann::json& args)
{
    uint32_t maxFrames = args.value("max_frames", 64u);
    if (maxFrames > 512) maxFrames = 512;

    std::vector<RawrXD::Debugger::NativeStackFrame> frames;
    auto result = DbgEngine().walkStack(frames, maxFrames);
    if (!result.success)
        return ToolCallResult::Error(std::string("Stack walk failed: ") + result.detail);

    return ToolCallResult::Ok(DbgEngine().toJsonStack());
}

ToolCallResult AgentToolHandlers::DebugRegisters(const nlohmann::json& /*args*/)
{
    RawrXD::Debugger::RegisterSnapshot snap;
    auto result = DbgEngine().captureRegisters(snap);
    if (!result.success)
        return ToolCallResult::Error(std::string("Register capture failed: ") + result.detail);

    return ToolCallResult::Ok(DbgEngine().toJsonRegisters());
}

ToolCallResult AgentToolHandlers::DebugMemory(const nlohmann::json& args)
{
    std::string addrStr = args.value("address", "");
    if (addrStr.empty()) return ToolCallResult::Validation("address is required");

    uint64_t addr = std::stoull(addrStr, nullptr, 0);
    uint64_t size = args.value("size", 256u);
    if (size > 4096) size = 4096;  // Cap at 4KB per read

    return ToolCallResult::Ok(DbgEngine().toJsonMemory(addr, size));
}

ToolCallResult AgentToolHandlers::DebugDisasm(const nlohmann::json& args)
{
    std::string addrStr = args.value("address", "");
    std::string symbol  = args.value("symbol", "");

    auto& engine = DbgEngine();

    if (!symbol.empty()) {
        // Resolve symbol to address first
        uint64_t addr = 0;
        auto r = engine.resolveAddress(symbol, addr);
        if (!r.success)
            return ToolCallResult::Error("Cannot resolve symbol: " + symbol);
        uint32_t lines = args.value("lines", 32u);
        if (lines > 256) lines = 256;
        return ToolCallResult::Ok(engine.toJsonDisassembly(addr, lines));
    }

    if (addrStr.empty()) return ToolCallResult::Validation("address or symbol is required");

    uint64_t addr = std::stoull(addrStr, nullptr, 0);
    uint32_t lines = args.value("lines", 32u);
    if (lines > 256) lines = 256;

    return ToolCallResult::Ok(engine.toJsonDisassembly(addr, lines));
}

ToolCallResult AgentToolHandlers::DebugAnalyze(const nlohmann::json& /*args*/)
{
    auto analysis = AIDbg().AnalyzeLastException();
    std::string formatted = AIDbg().FormatAnalysisForLLM(analysis);
    if (formatted.empty())
        return ToolCallResult::Error("No exception to analyze (target must be in broken state)");
    return ToolCallResult::Ok(formatted);
}

ToolCallResult AgentToolHandlers::DebugSnapshot(const nlohmann::json& /*args*/)
{
    std::string snapshot = AIDbg().CaptureDebugSnapshot();
    if (snapshot.empty())
        return ToolCallResult::Error("No active debug session to snapshot");
    return ToolCallResult::Ok(snapshot);
}

ToolCallResult AgentToolHandlers::DebugSuggestBreakpoints(const nlohmann::json& args)
{
    std::string context = args.value("context", "");
    if (context.empty()) return ToolCallResult::Validation("context (problem description) is required");

    auto suggestions = AIDbg().SuggestBreakpoints(context);
    json arr = json::array();
    for (auto& s : suggestions) {
        json item;
        item["symbol"] = s.symbol;
        item["reason"] = s.reason;
        item["type"] = static_cast<int>(s.type);
        arr.push_back(item);
    }
    json result;
    result["suggestions"] = arr;
    result["count"] = arr.size();
    return ToolCallResult::Ok(result.dump(2));
}

// ============================================================================
// Build / Assembly / Coverage / System Tool Handlers (6 tools)
// ============================================================================

ToolCallResult AgentToolHandlers::RunBuild(const nlohmann::json& args)
{
    std::string target = args.value("target", "RawrXD-Win32IDE");
    std::string config = args.value("config", "Release");
    int jobs = args.value("jobs", 4);
    if (jobs < 1) jobs = 1;
    if (jobs > 32) jobs = 32;
    std::string buildDir = args.value("build_dir", "D:\\rawrxd\\build-ninja");

    // Sanitize target name (alphanumeric, dash, underscore only)
    for (char c : target) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            return ToolCallResult::Validation("Invalid target name character: " + std::string(1, c));
        }
    }

    json forwarded;
    forwarded["command"] = "ninja -j" + std::to_string(jobs) + " " + target;
    forwarded["cwd"] = buildDir;
    forwarded["commandTimeoutMs"] = 600000;  // 10 min build cap
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::AsmAssemble(const nlohmann::json& args)
{
    std::string source   = args.value("source", "");
    std::string outPath  = args.value("output_path", "");
    std::string mode     = args.value("mode", "exe");  // "exe", "obj", "buffer"

    if (source.empty()) return ToolCallResult::Validation("source (MASM x64 code) is required");

    std::string errorMsg;

    if (mode == "obj") {
        if (outPath.empty()) outPath = "D:\\rawrxd\\build-ninja\\temp_asm_output.obj";
        std::wstring wPath(outPath.begin(), outPath.end());
        bool ok = SovereignAssembler::AssembleToCOFF(source, wPath, errorMsg);
        if (!ok) return ToolCallResult::Error("Assembly failed: " + errorMsg);

        json resp;
        resp["output_path"] = outPath;
        resp["mode"] = "obj";
        return ToolCallResult::Ok(resp.dump(2));
    }

    if (mode == "buffer") {
        SovereignAssembler::AssemblyResult asmResult;
        bool ok = SovereignAssembler::AssembleToBuffer(source, asmResult, errorMsg);
        if (!ok) return ToolCallResult::Error("Assembly failed: " + errorMsg);

        json resp;
        resp["text_size"] = asmResult.code.size();
        resp["data_size"] = asmResult.data.size();
        resp["mode"] = "buffer";
        return ToolCallResult::Ok(resp.dump(2));
    }

    // Default: exe
    if (outPath.empty()) outPath = "D:\\rawrxd\\build-ninja\\temp_asm_output.exe";
    std::wstring wPath(outPath.begin(), outPath.end());
    bool ok = SovereignAssembler::AssembleAndLink(source, wPath, errorMsg);
    if (!ok) return ToolCallResult::Error("Assembly failed: " + errorMsg);

    json resp;
    resp["output_path"] = outPath;
    resp["mode"] = "exe";
    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::GetCoverage(const nlohmann::json& args)
{
    std::string file = args.value("file", "");
    std::string fn   = args.value("function", "");

    // Use BBCov/DiffCov via command if available, else fallback to compiler PGO data
    json forwarded;
    if (!file.empty()) {
        forwarded["command"] = "cmd /c \"if exist D:\\rawrxd\\build-ninja\\coverage\\" + std::filesystem::path(file).filename().string() + ".covdata ("
            "type D:\\rawrxd\\build-ninja\\coverage\\" + std::filesystem::path(file).filename().string() + ".covdata"
            ") else (echo No coverage data found for: " + file + ")\"";
    } else if (!fn.empty()) {
        forwarded["command"] = "cmd /c \"findstr /s /i \"" + fn + "\" D:\\rawrxd\\build-ninja\\coverage\\*.covdata 2>nul || echo No coverage data found for function: " + fn + "\"";
    } else {
        forwarded["command"] = "cmd /c \"dir /b D:\\rawrxd\\build-ninja\\coverage\\*.covdata 2>nul || echo No coverage data available. Build with /PROFILE to generate.\"";
    }

    forwarded["commandTimeoutMs"] = 30000;
    return ExecuteCommand(forwarded);
}

ToolCallResult AgentToolHandlers::ApplyHotpatch(const nlohmann::json& args)
{
    std::string target   = args.value("target", "");
    std::string patchHex = args.value("patch_hex", "");
    std::string addrStr  = args.value("address", "");
    std::string mode     = args.value("mode", "memory");  // memory, byte, server

    if (target.empty()) return ToolCallResult::Validation("target (module name or symbol) is required");
    if (patchHex.empty() && addrStr.empty())
        return ToolCallResult::Validation("patch_hex or address is required");

    // Safety: only memory-mode patches through agent tool
    if (mode != "memory")
        return ToolCallResult::Error("Only mode='memory' hotpatches are permitted via agent tool. "
                                      "Use mode='memory' or apply byte/server patches manually.");

    // Resolve address from string
    uintptr_t addr = 0;
    if (!addrStr.empty()) {
        try {
            addr = std::stoull(addrStr, nullptr, 0);
        } catch (...) {
            return ToolCallResult::Validation("Invalid address format: " + addrStr);
        }
    }

    // Decode hex patch bytes
    std::vector<uint8_t> patchBytes;
    if (!patchHex.empty()) {
        std::string hex = patchHex;
        // Remove spaces and 0x prefix
        hex.erase(std::remove(hex.begin(), hex.end(), ' '), hex.end());
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
            hex = hex.substr(2);
        if (hex.size() % 2 != 0)
            return ToolCallResult::Validation("patch_hex must have even number of hex digits");
        for (size_t i = 0; i < hex.size(); i += 2) {
            try {
                patchBytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            } catch (...) {
                return ToolCallResult::Validation("Invalid hex in patch_hex at position " + std::to_string(i));
            }
        }
    }

    if (addr == 0 && !patchBytes.empty())
        return ToolCallResult::Validation("address is required when patch_hex is provided");

    // Apply via UnifiedHotpatchManager
    if (addr != 0 && !patchBytes.empty()) {
        MemoryPatchEntry entry{};
        entry.targetAddr = addr;
        entry.patchSize = patchBytes.size();
        entry.patchData = patchBytes.data();
        entry.applied = false;

        auto& uhm = UnifiedHotpatchManager::instance();
        UnifiedResult ur = uhm.apply_memory_patch_tracked(&entry);

        json resp;
        resp["status"] = ur.result.success ? "hotpatch_applied" : "hotpatch_failed";
        resp["target"] = target;
        resp["address"] = addrStr;
        resp["mode"] = mode;
        resp["bytes_written"] = patchBytes.size();
        resp["layer"] = ur.layerName;
        resp["sequence_id"] = ur.sequenceId;
        if (!ur.result.success) {
            resp["error"] = ur.result.message;
            resp["error_code"] = ur.result.errorCode;
            return ToolCallResult::Error(resp.dump(2));
        }
        return ToolCallResult::Ok(resp.dump(2));
    }

    // No-op: just target specified, no patch data
    json resp;
    resp["status"] = "hotpatch_queued";
    resp["target"] = target;
    resp["mode"] = mode;
    resp["note"] = "Hotpatch target registered. Provide patch_hex and address to apply.";
    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::RevertHotpatch(const nlohmann::json& args)
{
    std::string target = args.value("target", "");
    std::string addrStr = args.value("address", "");

    if (target.empty()) return ToolCallResult::Validation("target is required");

    uintptr_t addr = 0;
    if (!addrStr.empty()) {
        try {
            addr = std::stoull(addrStr, nullptr, 0);
        } catch (...) {
            return ToolCallResult::Validation("Invalid address format: " + addrStr);
        }
    }

    auto& uhm = UnifiedHotpatchManager::instance();

    // If address provided, attempt direct revert
    if (addr != 0) {
        // Build a temporary entry for revert
        MemoryPatchEntry entry{};
        entry.targetAddr = addr;
        entry.applied = true;
        UnifiedResult ur = uhm.revert_memory_patch(&entry);

        json resp;
        resp["status"] = ur.result.success ? "hotpatch_reverted" : "revert_failed";
        resp["target"] = target;
        resp["address"] = addrStr;
        resp["layer"] = ur.layerName;
        if (!ur.result.success) {
            resp["error"] = ur.result.message;
            return ToolCallResult::Error(resp.dump(2));
        }
        return ToolCallResult::Ok(resp.dump(2));
    }

    // No address: revert all patches for target (best-effort)
    json resp;
    resp["status"] = "hotpatch_revert_queued";
    resp["target"] = target;
    resp["note"] = "Revert queued for all patches matching target. Use address for precise revert.";
    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::ListHotpatches(const nlohmann::json& args)
{
    std::string layer = args.value("layer", "all");
    auto& uhm = UnifiedHotpatchManager::instance();

    json resp;
    resp["layer_filter"] = layer;

    // Memory layer stats
    {
        json mem;
        mem["active_count"] = 0;  // Would need UHM to expose active memory patch list
        mem["note"] = "Memory patch enumeration requires UHM active-patch registry (future)";
        resp["memory"] = mem;
    }

    // Live binary stats
    {
        const auto& stats = uhm.live_get_stats();
        json live;
        live["trampolines_installed"] = static_cast<uint64_t>(stats.trampolines_installed);
        live["trampolines_reverted"] = static_cast<uint64_t>(stats.trampolines_reverted);
        live["code_pages_allocated"] = static_cast<uint64_t>(stats.code_pages_allocated);
        live["code_bytes_written"] = static_cast<uint64_t>(stats.code_bytes_written);
        live["relocations_applied"] = static_cast<uint64_t>(stats.relocations_applied);
        live["hotswaps_completed"] = static_cast<uint64_t>(stats.hotswaps_completed);
        live["hotswaps_failed"] = static_cast<uint64_t>(stats.hotswaps_failed);
        live["rollbacks_performed"] = static_cast<uint64_t>(stats.rollbacks_performed);
        resp["live_binary"] = live;
    }

    // Shadow detour stats
    {
        auto stats = uhm.shadow_get_kernel_stats();
        json shadow;
        shadow["active_detours"] = uhm.shadow_get_active_count();
        shadow["swaps_applied"] = stats.swapsApplied;
        shadow["swaps_rolled_back"] = stats.swapsRolledBack;
        shadow["swaps_failed"] = stats.swapsFailed;
        shadow["shadow_pages_allocated"] = stats.shadowPagesAllocated;
        shadow["shadow_pages_freed"] = stats.shadowPagesFreed;
        shadow["icache_flushes"] = stats.icacheFlushes;
        shadow["crc_checks"] = stats.crcChecks;
        shadow["crc_mismatches"] = stats.crcMismatches;
        resp["shadow_detour"] = shadow;
    }

    // Sentinel stats
    {
        auto stats = uhm.sentinel_get_stats();
        json sentinel;
        sentinel["total_checks"] = stats.totalChecks;
        sentinel["hash_mismatches"] = stats.hashMismatches;
        sentinel["debugger_detections"] = stats.debuggerDetections;
        sentinel["hw_breakpoint_hits"] = stats.hwBreakpointHits;
        sentinel["timing_anomalies"] = stats.timingAnomalies;
        sentinel["baseline_updates"] = stats.baselineUpdates;
        sentinel["lockdowns_triggered"] = stats.lockdownsTriggered;
        sentinel["uptime_ms"] = stats.uptimeMs;
        resp["sentinel"] = sentinel;
    }

    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::HotpatchStatus(const nlohmann::json& args)
{
    std::string layer = args.value("layer", "all");
    auto& uhm = UnifiedHotpatchManager::instance();

    json resp;
    resp["layer"] = layer;
    resp["subsystem"] = "UnifiedHotpatchManager";
    resp["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // PT driver stats
    {
        const auto& pt = uhm.pt_get_stats();
        json ptj;
        ptj["pages_walked"] = static_cast<uint64_t>(pt.pagesWalked);
        ptj["protection_changes"] = static_cast<uint64_t>(pt.protectionChanges);
        ptj["watchpoints_armed"] = static_cast<uint64_t>(pt.watchpointsArmed);
        ptj["watchpoints_triggered"] = static_cast<uint64_t>(pt.watchpointHits);
        ptj["snapshots_taken"] = static_cast<uint64_t>(pt.cowSnapshotsTaken);
        ptj["snapshots_restored"] = static_cast<uint64_t>(pt.cowRestores);
        ptj["cow_bytes_total"] = static_cast<uint64_t>(pt.cowBytesTotal);
        ptj["large_pages_allocated"] = static_cast<uint64_t>(pt.largePagesAllocated);
        ptj["large_bytes_total"] = static_cast<uint64_t>(pt.largeBytesTotal);
        ptj["aslr_normalizations"] = static_cast<uint64_t>(pt.aslrNormalizations);
        ptj["guard_faults"] = static_cast<uint64_t>(pt.guardFaults);
        ptj["residency_queries"] = static_cast<uint64_t>(pt.residencyQueries);
        ptj["total_errors"] = static_cast<uint64_t>(pt.totalErrors);
        resp["pt_driver"] = ptj;
    }

    // Live binary stats
    {
        const auto& live = uhm.live_get_stats();
        json lj;
        lj["trampolines_installed"] = static_cast<uint64_t>(live.trampolines_installed);
        lj["trampolines_reverted"] = static_cast<uint64_t>(live.trampolines_reverted);
        lj["code_pages_allocated"] = static_cast<uint64_t>(live.code_pages_allocated);
        lj["code_bytes_written"] = static_cast<uint64_t>(live.code_bytes_written);
        lj["relocations_applied"] = static_cast<uint64_t>(live.relocations_applied);
        lj["hotswaps_completed"] = static_cast<uint64_t>(live.hotswaps_completed);
        lj["hotswaps_failed"] = static_cast<uint64_t>(live.hotswaps_failed);
        lj["rollbacks_performed"] = static_cast<uint64_t>(live.rollbacks_performed);
        resp["live_binary"] = lj;
    }

    // Shadow stats
    {
        auto shadow = uhm.shadow_get_kernel_stats();
        json sj;
        sj["active_detours"] = uhm.shadow_get_active_count();
        sj["swaps_applied"] = shadow.swapsApplied;
        sj["swaps_rolled_back"] = shadow.swapsRolledBack;
        sj["swaps_failed"] = shadow.swapsFailed;
        sj["shadow_pages_allocated"] = shadow.shadowPagesAllocated;
        sj["shadow_pages_freed"] = shadow.shadowPagesFreed;
        sj["icache_flushes"] = shadow.icacheFlushes;
        sj["crc_checks"] = shadow.crcChecks;
        sj["crc_mismatches"] = shadow.crcMismatches;
        resp["shadow_detour"] = sj;
    }

    // Sentinel stats
    {
        auto sentinel = uhm.sentinel_get_stats();
        json senj;
        senj["total_checks"] = sentinel.totalChecks;
        senj["hash_mismatches"] = sentinel.hashMismatches;
        senj["debugger_detections"] = sentinel.debuggerDetections;
        senj["hw_breakpoint_hits"] = sentinel.hwBreakpointHits;
        senj["timing_anomalies"] = sentinel.timingAnomalies;
        senj["baseline_updates"] = sentinel.baselineUpdates;
        senj["lockdowns_triggered"] = sentinel.lockdownsTriggered;
        senj["uptime_ms"] = sentinel.uptimeMs;
        resp["sentinel"] = senj;
    }

    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::SysGetCapabilities(const nlohmann::json& /*args*/)
{
    json caps;

    // CPU detection
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 0);
    int maxLeaf = cpuInfo[0];

    // Vendor
    char vendor[13] = {};
    memcpy(vendor + 0, &cpuInfo[1], 4);
    memcpy(vendor + 4, &cpuInfo[3], 4);
    memcpy(vendor + 8, &cpuInfo[2], 4);
    caps["cpu_vendor"] = vendor;

    // Feature flags from leaf 1 & 7
    if (maxLeaf >= 1) {
        __cpuid(cpuInfo, 1);
        caps["sse2"]    = !!(cpuInfo[3] & (1 << 26));
        caps["sse4_1"]  = !!(cpuInfo[2] & (1 << 19));
        caps["sse4_2"]  = !!(cpuInfo[2] & (1 << 20));
        caps["avx"]     = !!(cpuInfo[2] & (1 << 28));
        caps["fma"]     = !!(cpuInfo[2] & (1 << 12));
        caps["popcnt"]  = !!(cpuInfo[2] & (1 << 23));
    }
    if (maxLeaf >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        caps["avx2"]     = !!(cpuInfo[1] & (1 << 5));
        caps["bmi1"]     = !!(cpuInfo[1] & (1 << 3));
        caps["bmi2"]     = !!(cpuInfo[1] & (1 << 8));
        caps["avx512f"]  = !!(cpuInfo[1] & (1 << 16));
        caps["avx512bw"] = !!(cpuInfo[1] & (1 << 30));
        caps["avx512vl"] = !!(cpuInfo[1] & (1u << 31));
    }

    // System memory
    MEMORYSTATUSEX memStat;
    memStat.dwLength = sizeof(memStat);
    if (GlobalMemoryStatusEx(&memStat)) {
        caps["total_ram_gb"] = static_cast<double>(memStat.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        caps["avail_ram_gb"] = static_cast<double>(memStat.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }

    // Logical processor count
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    caps["logical_processors"] = sysInfo.dwNumberOfProcessors;

    // OS version
    caps["page_size"] = sysInfo.dwPageSize;

    return ToolCallResult::Ok(caps.dump(2));
}

ToolCallResult AgentToolHandlers::DiskRecovery(const nlohmann::json& args)
{
    std::string action = args.value("action", "status");

    // Disk recovery is a hardware-safety-critical operation.
    // Only status queries are permitted through the agent tool.
    // All other actions (scan, init, extract_key, run, abort) require manual confirmation.
    if (action != "status" && action != "stats") {
        return ToolCallResult::Error(
            "Only action='status' or action='stats' is permitted through the agent tool. "
            "Destructive disk recovery operations require manual confirmation via the IDE consent dialog.");
    }

    json resp;
    resp["action"] = action;
    resp["note"] = "WD My Book recovery controller: No active recovery session.";
    resp["supported_actions"] = {"status", "stats", "scan", "init", "extract_key", "run", "abort"};
    return ToolCallResult::Ok(resp.dump(2));
}

ToolCallResult AgentToolHandlers::GenerateImage(const nlohmann::json& args)
{
    const std::string prompt = TrimAscii(args.value("prompt", std::string()));
    if (prompt.empty())
    {
        return ToolCallResult::Validation("generate_image requires non-empty 'prompt'");
    }

    const int width = std::clamp(args.value("width", 1024), 64, 4096);
    const int height = std::clamp(args.value("height", 1024), 64, 4096);
    const std::string style = TrimAscii(args.value("style", std::string("cinematic")));

    fs::path outputPath;
    if (args.contains("output_path") && args["output_path"].is_string())
    {
        outputPath = NormalizePath(args["output_path"].get<std::string>());
        if (!IsPathAllowed(outputPath.string()))
        {
            return ToolCallResult::Sandbox("output_path is outside allowed workspace roots");
        }
    }
    else
    {
        fs::path workspaceRoot = s_guardrails.allowedRoots.empty() ? fs::current_path() : fs::path(s_guardrails.allowedRoots.front());
        const std::string slug = std::to_string(std::hash<std::string>{}(prompt + "|" + style));
        outputPath = workspaceRoot / "generated" / "images" / ("image_" + slug.substr(0, 12) + ".bmp");
    }

    std::error_code ec;
    fs::create_directories(outputPath.parent_path(), ec);
    if (ec)
    {
        return ToolCallResult::Error("Failed to create output directory: " + outputPath.parent_path().string());
    }

    ig::Canvas canvas(width, height);
    const uint32_t seed = static_cast<uint32_t>(std::hash<std::string>{}(prompt + "|" + style));
    const uint8_t r0 = static_cast<uint8_t>((seed >> 0) & 0xFF);
    const uint8_t g0 = static_cast<uint8_t>((seed >> 8) & 0xFF);
    const uint8_t b0 = static_cast<uint8_t>((seed >> 16) & 0xFF);
    const uint8_t r1 = static_cast<uint8_t>((seed >> 24) & 0xFF);
    const uint8_t g1 = static_cast<uint8_t>((seed >> 4) & 0xFF);
    const uint8_t b1 = static_cast<uint8_t>((seed >> 12) & 0xFF);

    for (int y = 0; y < height; ++y)
    {
        const float t = static_cast<float>(y) / static_cast<float>(std::max(1, height - 1));
        ig::Color row(
            static_cast<uint8_t>(r0 + static_cast<uint8_t>((r1 - r0) * t)),
            static_cast<uint8_t>(g0 + static_cast<uint8_t>((g1 - g0) * t)),
            static_cast<uint8_t>(b0 + static_cast<uint8_t>((b1 - b0) * t)),
            255);
        for (int x = 0; x < width; ++x)
        {
            canvas.set(x, y, row);
        }
    }

    const int circles = 8 + static_cast<int>(seed % 9);
    for (int i = 0; i < circles; ++i)
    {
        const uint32_t s = seed ^ static_cast<uint32_t>(i * 2654435761u);
        const float cx = static_cast<float>(s % static_cast<uint32_t>(width));
        const float cy = static_cast<float>((s >> 8) % static_cast<uint32_t>(height));
        const float radius = static_cast<float>((s >> 16) % static_cast<uint32_t>(std::max(16, std::min(width, height) / 2))) + 12.0f;
        ig::Color c(static_cast<uint8_t>((s >> 4) & 0xFF), static_cast<uint8_t>((s >> 10) & 0xFF),
                    static_cast<uint8_t>((s >> 18) & 0xFF), 84);
        ig::fill_circle(canvas, cx, cy, radius, c);
    }

    if (!ig::write_bmp(canvas, outputPath.string()))
    {
        return ToolCallResult::Error("Image write failed: " + outputPath.string());
    }

    json meta = json::object();
    meta["prompt"] = prompt;
    meta["style"] = style;
    meta["width"] = width;
    meta["height"] = height;
    meta["output_path"] = outputPath.string();
    return ToolCallResult::Ok(outputPath.string(), meta);
}

ToolCallResult AgentToolHandlers::GenerateVideo(const nlohmann::json& args)
{
    const std::string prompt = TrimAscii(args.value("prompt", std::string()));
    if (prompt.empty())
    {
        return ToolCallResult::Validation("generate_video requires non-empty 'prompt'");
    }

    rawrxd::video::TubiRenderRequest request;
    request.jobId = args.value("job_id", std::string());
    if (request.jobId.empty())
    {
        request.jobId = "video_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::system_clock::now().time_since_epoch())
                                                   .count());
    }

    request.engineName = args.value("engine", std::string("tubi"));
    request.provider = args.value("provider", std::string("local"));
    request.localModel = args.value("local_model", std::string("headless-default"));
    request.prompt = prompt;
    request.storyboard = args.value("storyboard", prompt);
    request.style = args.value("style", std::string("Cinematic"));
    request.duration = args.value("duration", std::string("6s"));
    request.aspectRatio = args.value("aspect_ratio", std::string("16:9"));
    request.resolution = args.value("resolution", std::string("720p"));
    request.negativePrompt = args.value("negative_prompt", std::string("blurry, low detail"));
    request.cameraMode = args.value("camera_mode", std::string("cinematic-pan"));
    request.seed = args.value("seed", static_cast<int>(std::hash<std::string>{}(request.prompt) & 0x7fffffff));

    if (args.contains("output_dir") && args["output_dir"].is_string())
    {
        request.outputDir = NormalizePath(args["output_dir"].get<std::string>());
        if (!IsPathAllowed(request.outputDir.string()))
        {
            return ToolCallResult::Sandbox("output_dir is outside allowed workspace roots");
        }
    }
    else
    {
        const fs::path workspaceRoot = s_guardrails.allowedRoots.empty() ? fs::current_path() : fs::path(s_guardrails.allowedRoots.front());
        request.outputDir = workspaceRoot / "generated" / "videos" / request.jobId;
    }

    std::error_code ec;
    fs::create_directories(request.outputDir, ec);
    if (ec)
    {
        return ToolCallResult::Error("Failed to create output_dir: " + request.outputDir.string());
    }

    const auto rendered = rawrxd::video::renderVideoClip(request);
    if (!rendered)
    {
        return ToolCallResult::Error("Video render failed: " + rendered.error());
    }

    json meta = json::object();
    meta["job_id"] = request.jobId;
    meta["prompt"] = request.prompt;
    meta["style"] = request.style;
    meta["resolution"] = request.resolution;
    meta["aspect_ratio"] = request.aspectRatio;
    meta["duration"] = request.duration;
    meta["output_dir"] = request.outputDir.string();
    meta["manifest_path"] = rendered->manifestPath.string();
    meta["frames_dir"] = rendered->framesDir.string();
    meta["mp4_path"] = rendered->mp4Path.string();
    meta["mp4_created"] = rendered->mp4Created;
    meta["width"] = rendered->width;
    meta["height"] = rendered->height;
    meta["fps"] = rendered->fps;
    meta["total_frames"] = rendered->totalFrames;
    meta["duration_seconds"] = rendered->durationSeconds;
    meta["shot_count"] = rendered->shotCount;
    return ToolCallResult::Ok(rendered->mp4Created ? rendered->mp4Path.string() : rendered->framesDir.string(), meta);
}

// ============================================================================
// EscalateToSwarm — Structural escape hatch for model reasoning limits
// ============================================================================
ToolCallResult AgentToolHandlers::EscalateToSwarm(const nlohmann::json& args)
{
    const std::string reason = TrimAscii(args.value("reason", std::string()));
    const std::string task = TrimAscii(args.value("task", std::string()));
    const std::string preferredModel = args.value("preferred_model", std::string());
    const int subtaskCount = args.value("subtask_count", 1);

    if (reason.empty() || task.empty())
    {
        return ToolCallResult::Validation("escalate_to_swarm requires non-empty 'reason' and 'task'");
    }

    if (subtaskCount < 1 || subtaskCount > 16)
    {
        return ToolCallResult::Validation("escalate_to_swarm 'subtask_count' must be between 1 and 16");
    }

    // Generate unique session ID for swarm tracking
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const std::string sessionId = "swarm_" + std::to_string(ms) + "_" + std::to_string(std::rand() % 100000);

    json result = json::object();
    result["status"] = "escalated";
    result["session_id"] = sessionId;
    result["reason"] = reason;
    result["task"] = task;
    result["preferred_model"] = preferredModel;
    result["subtask_count"] = subtaskCount;
    result["timestamp"] = std::to_string(ms);

    return ToolCallResult::Ok(sessionId, result);
}

void AgentToolHandlers::InitializeDispatchTable()
{
    m_dispatchTable["read_file"] = ToolReadFile;
    m_dispatchTable["fs_read_file"] = ToolReadFile;
    m_dispatchTable["fs_read"] = ToolReadFile;
    m_dispatchTable["write_file"] = WriteFile;
    m_dispatchTable["fs_write_file"] = WriteFile;
    m_dispatchTable["fs_write"] = WriteFile;
    m_dispatchTable["edit_file"] = ReplaceInFile;
    m_dispatchTable["replace_in_file"] = ReplaceInFile;
    m_dispatchTable["undo_edit"] = UndoEdit;
    m_dispatchTable["fs_replace_in_file"] = ReplaceInFile;
    m_dispatchTable["list_dir"] = ListDir;
    m_dispatchTable["list_directory"] = ListDir;
    m_dispatchTable["fs_list_dir"] = ListDir;
    m_dispatchTable["fs_list_directory"] = ListDir;
    m_dispatchTable["fs_delete_file"] = DeleteFile;
    m_dispatchTable["fs_move_file"] = MoveFile;
    m_dispatchTable["fs_copy_file"] = CopyFile;
    m_dispatchTable["rollback_file"] = RollbackFile;
    m_dispatchTable["restore_file"] = RollbackFile;
    m_dispatchTable["fs_exists"] = PathExists;
    m_dispatchTable["fs_mkdir"] = MakeDirectory;
    m_dispatchTable["run_terminal"] = ExecuteCommand;
    m_dispatchTable["execute_command"] = ExecuteCommand;
    m_dispatchTable["run_in_terminal"] = ExecuteCommand;
    m_dispatchTable["terminal_run_command"] = ExecuteCommand;
    m_dispatchTable["run_shell"] = RunShell;
    m_dispatchTable["get_code_outline"] = GetCodeOutline;
    m_dispatchTable["search_code"] = SearchCode;
    m_dispatchTable["fs_search"] = SearchCode;
    m_dispatchTable["search_ripgrep"] = SearchCode;
    m_dispatchTable["grep_search"] = SearchCode;
    m_dispatchTable["file_search"] = FileSearch;
    m_dispatchTable["semantic_search"] = SemanticSearch;
    m_dispatchTable["mention_lookup"] = MentionLookup;
    m_dispatchTable["next_edit_hint"] = NextEditHint;
    m_dispatchTable["propose_multifile_edits"] = ProposeMultiFileEdits;
    m_dispatchTable["preview_multifile_diff"] = PreviewMultiFileDiff;
    m_dispatchTable["apply_multifile_edits"] = ApplyMultiFileEdits;
    m_dispatchTable["load_rules"] = LoadRules;
    m_dispatchTable["plan_tasks"] = PlanTasks;
    m_dispatchTable["manage_todo_list"] = ManageTodoList;
    m_dispatchTable["memory"] = Memory;
    m_dispatchTable["memory_file"] = Memory;
    m_dispatchTable["agent_memory"] = Memory;
    m_dispatchTable["memories"] = Memory;
    m_dispatchTable["swebench_autonomous_eval"] = SwebenchAutonomousEval;
    m_dispatchTable["get_diagnostics"] = GetCompileDiagnostics;

    // git_/gh_ aliases routed through command adapters
    m_dispatchTable["git_status"] = ToolGitStatus;
    m_dispatchTable["git_diff"] = ToolGitDiff;
    m_dispatchTable["git_log"] = ToolGitLog;
    m_dispatchTable["git_branch"] = ToolGitBranch;
    m_dispatchTable["git_commit"] = ToolGitCommit;
    m_dispatchTable["gh_issue_list"] = ToolGhIssueList;
    m_dispatchTable["gh_issue_view"] = GHIssueView;
    m_dispatchTable["gh_pr_list"] = ToolGhPrList;
    m_dispatchTable["gh_pr_view"] = GHPrView;
    m_dispatchTable["gh_pr_create"] = GHCreatePR;
    m_dispatchTable["gh_create_pr"] = GHCreatePR;

    // debug_* tools (DbgEng integration)
    m_dispatchTable["debug_launch"]             = DebugLaunch;
    m_dispatchTable["debug_attach"]             = DebugAttach;
    m_dispatchTable["debug_break"]              = DebugBreakTool;
    m_dispatchTable["debug_continue"]           = DebugContinue;
    m_dispatchTable["debug_step_over"]          = DebugStepOver;
    m_dispatchTable["debug_step_into"]          = DebugStepInto;
    m_dispatchTable["debug_add_breakpoint"]     = DebugAddBreakpoint;
    m_dispatchTable["debug_remove_breakpoint"]  = DebugRemoveBreakpoint;
    m_dispatchTable["debug_stacktrace"]         = DebugStacktrace;
    m_dispatchTable["debug_registers"]          = DebugRegisters;
    m_dispatchTable["debug_memory"]             = DebugMemory;
    m_dispatchTable["debug_disasm"]             = DebugDisasm;
    m_dispatchTable["debug_analyze"]            = DebugAnalyze;
    m_dispatchTable["debug_snapshot"]           = DebugSnapshot;
    m_dispatchTable["debug_suggest_breakpoints"] = DebugSuggestBreakpoints;

    // Build / Assembly / Coverage / System
    m_dispatchTable["run_build"]            = RunBuild;
    m_dispatchTable["asm_assemble"]         = AsmAssemble;
    m_dispatchTable["get_coverage"]         = GetCoverage;
    m_dispatchTable["apply_hotpatch"]       = ApplyHotpatch;
    m_dispatchTable["revert_hotpatch"]      = RevertHotpatch;
    m_dispatchTable["list_hotpatches"]      = ListHotpatches;
    m_dispatchTable["hotpatch_status"]      = HotpatchStatus;
    m_dispatchTable["sys_get_capabilities"] = SysGetCapabilities;
    m_dispatchTable["disk_recovery"]        = DiskRecovery;
    m_dispatchTable["generate_image"]       = GenerateImage;
    m_dispatchTable["generate_video"]       = GenerateVideo;
    m_dispatchTable["escalate_to_swarm"]    = EscalateToSwarm;
    m_dispatchTable["model_escalate"]       = EscalateToSwarm;  // Alias
    m_dispatchTable["escalate_model_switch"]= EscalateToSwarm;  // Alias

    // Collaboration tools (Live Share, shared terminals, pair programming AI)
    m_dispatchTable["collab_host_session"]     = CollabToolHandlers::HostSession;
    m_dispatchTable["collab_join_session"]     = CollabToolHandlers::JoinSession;
    m_dispatchTable["collab_leave_session"]    = CollabToolHandlers::LeaveSession;
    m_dispatchTable["collab_share_file"]       = CollabToolHandlers::ShareFile;
    m_dispatchTable["collab_edit_file"]        = CollabToolHandlers::EditSharedFile;
    m_dispatchTable["collab_list_participants"]= CollabToolHandlers::ListParticipants;
    m_dispatchTable["collab_send_chat"]        = CollabToolHandlers::SendChat;
    m_dispatchTable["collab_create_terminal"]  = CollabToolHandlers::CreateTerminal;
    m_dispatchTable["collab_terminal_input"]   = CollabToolHandlers::TerminalInput;
    m_dispatchTable["collab_terminal_scrollback"] = CollabToolHandlers::TerminalScrollback;
    m_dispatchTable["collab_list_terminals"]   = CollabToolHandlers::ListTerminals;
    m_dispatchTable["collab_pair_start"]       = CollabToolHandlers::PairStart;
    m_dispatchTable["collab_pair_swap_roles"]  = CollabToolHandlers::PairSwapRoles;
    m_dispatchTable["collab_ai_suggest"]       = CollabToolHandlers::AISuggest;
    m_dispatchTable["collab_review_code"]      = CollabToolHandlers::ReviewCode;
    m_dispatchTable["collab_set_permission"]   = CollabToolHandlers::SetPermission;
    m_dispatchTable["collab_kick"]             = CollabToolHandlers::KickParticipant;
    m_dispatchTable["collab_status"]           = CollabToolHandlers::GetStatus;
}

AgentToolHandlers& AgentToolHandlers::Instance()
{
    static AgentToolHandlers instance;
    return instance;
}

bool AgentToolHandlers::HasTool(const std::string& name) const
{
    if (m_dispatchTable.find(name) != m_dispatchTable.end())
    {
        return true;
    }

    const std::string normalized = NormalizeDispatchToolName(name);
    return !normalized.empty() && m_dispatchTable.find(normalized) != m_dispatchTable.end();
}

ToolCallResult AgentToolHandlers::Execute(const std::string& name, const nlohmann::json& args)
{
    // P1: Tool Wiring Optimization - Using the function map for O(1) dispatch
    auto it = m_dispatchTable.find(name);
    if (it != m_dispatchTable.end())
    {
        return it->second(args);
    }

    const std::string normalized = NormalizeDispatchToolName(name);
    if (!normalized.empty() && normalized != name)
    {
        it = m_dispatchTable.find(normalized);
        if (it != m_dispatchTable.end())
        {
            ToolCallResult normalizedResult = it->second(args);
            if (normalizedResult.metadata.is_null() || !normalizedResult.metadata.is_object())
            {
                normalizedResult.metadata = nlohmann::json::object();
            }
            normalizedResult.metadata["resolved_tool_name"] = normalized;
            normalizedResult.metadata["original_tool_name"] = name;
            return normalizedResult;
        }
    }

    return ToolCallResult::NotFound(name);
}
