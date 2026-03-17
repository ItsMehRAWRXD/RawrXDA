// ============================================================================
// AgentToolHandlers.h — Agentic Tool Handler Implementations
// ============================================================================
// Concrete tool handlers for the RawrXD Agent:
//   read_file       — Read file contents (bounded by max size)
//   write_file      — Write/create files (sandboxed to workspace)
//   replace_in_file — Search+replace block in file (with backup)
//   list_dir        — List directory contents
//   execute_command  — Run terminal command (sandboxed, timeout-enforced)
//   search_code     — Fast codebase search (recursive grep)
//   get_diagnostics — Retrieve compiler/LSP errors for a file
//
// All handlers return ToolCallResult (structured, never bool).
// All file paths are validated against workspace root allowlist.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ToolCallResult.h"

namespace RawrXD {
namespace Agent {

// ============================================================================
// Execution guardrails — applied before any tool runs
// ============================================================================
struct ToolGuardrails {
    std::vector<std::string> allowedRoots;       // Workspace roots (absolute paths)
    std::vector<std::string> denyPatterns;        // Glob patterns to block (e.g. "*.exe")
    size_t maxFileSizeBytes       = 10 * 1024 * 1024;  // 10 MB max file read/write
    size_t maxOutputCaptureBytes  = 4 * 1024 * 1024;   // 4 MB max command output
    uint32_t commandTimeoutMs     = 30000;              // 30 second default timeout
    int    maxSearchResults       = 200;                // Cap search hits
    bool   requireBackupOnWrite   = true;               // Always backup before overwrite
};

// ============================================================================
// AgentToolHandlers — Static tool implementations
// ============================================================================
class AgentToolHandlers {
public:
    // ---- Initialization ----
    static void SetGuardrails(const ToolGuardrails& guards);
    static const ToolGuardrails& GetGuardrails();

    // ---- Tool implementations ----
    static ToolCallResult ToolReadFile(const nlohmann::json& args);
    static ToolCallResult WriteFile(const nlohmann::json& args);
    static ToolCallResult ReplaceInFile(const nlohmann::json& args);
    static ToolCallResult ListDir(const nlohmann::json& args);
    static ToolCallResult ExecuteCommand(const nlohmann::json& args);
    static ToolCallResult SearchCode(const nlohmann::json& args);
    static ToolCallResult GetDiagnostics(const nlohmann::json& args);

    // ---- Schema generation (OpenAI function-calling format) ----
    static nlohmann::json GetAllSchemas();
    static std::string GetSystemPrompt(const std::string& cwd,
                                        const std::vector<std::string>& openFiles);

private:
    // ---- Path validation ----
    static bool IsPathAllowed(const std::string& path);
    static std::string NormalizePath(const std::string& path);
    static bool MatchesDenyPattern(const std::string& path);

    // ---- Internal helpers ----
    static std::string CreateBackup(const std::string& path);
    static bool RunProcess(const std::wstring& cmdLine, uint32_t timeoutMs,
                           std::string& output, uint32_t& exitCode);

    static ToolGuardrails s_guardrails;
};

} // namespace Agent
} // namespace RawrXD
