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

#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ToolCallResult.h"

namespace RawrXD
{
namespace Agent
{

struct ScopedInstructionPromptData
{
    std::string payload;
    std::vector<std::string> sources;
    std::string telemetry;
};

// ============================================================================
// Execution guardrails — applied before any tool runs
// ============================================================================
struct ToolGuardrails
{
    std::vector<std::string> allowedRoots;           // Workspace roots (absolute paths)
    std::vector<std::string> denyPatterns;           // Glob patterns to block (e.g. "*.exe")
    std::vector<std::string> allowedCommands;        // Whitelisted shell commands (execute/run_shell)
    size_t maxFileSizeBytes = 10 * 1024 * 1024;      // 10 MB max file read/write
    size_t maxOutputCaptureBytes = 4 * 1024 * 1024;  // 4 MB max command output
    uint32_t commandTimeoutMs = 30000;               // 30 second default timeout
    int maxSearchResults = 200;                      // Cap search hits
    int maxIndexFiles = 200;                         // Cap semantic index scan
    bool requireBackupOnWrite = true;                // Always backup before overwrite
};

// ============================================================================
// AgentToolHandlers — Static tool implementations
// ============================================================================
class AgentToolHandlers
{
  public:
    // ---- Initialization ----
    static void SetGuardrails(const ToolGuardrails& guards);
    static const ToolGuardrails& GetGuardrails();

    /// When `execute_command` / `run_in_terminal` args include `use_integrated_terminal` (or
    /// `mirror_to_ide_agent_terminal`), mirror the command and captured stdout into the IDE agent
    /// terminal via this callback (Win32IDE registers it; headless builds leave it unset).
    static void SetIntegratedTerminalEchoCallback(void (*fn)(const char* utf8Line, void* userData), void* userData);

    // ---- Tool implementations ----
    static ToolCallResult ToolReadFile(const nlohmann::json& args);
    static ToolCallResult WriteFile(const nlohmann::json& args);
    static ToolCallResult ReplaceInFile(const nlohmann::json& args);
    static ToolCallResult UndoEdit(const nlohmann::json& args);
    static ToolCallResult ListDir(const nlohmann::json& args);
    static ToolCallResult DeleteFile(const nlohmann::json& args);
    static ToolCallResult MoveFile(const nlohmann::json& args);
    static ToolCallResult CopyFile(const nlohmann::json& args);
    static ToolCallResult RollbackFile(const nlohmann::json& args);
    static ToolCallResult PathExists(const nlohmann::json& args);
    static ToolCallResult MakeDirectory(const nlohmann::json& args);
    static ToolCallResult ExecuteCommand(const nlohmann::json& args);
    static ToolCallResult RunShell(const nlohmann::json& args);
    static ToolCallResult GetCodeOutline(const nlohmann::json& args);
    static ToolCallResult SearchCode(const nlohmann::json& args);
    static ToolCallResult FileSearch(const nlohmann::json& args);
    static ToolCallResult GetDiagnostics(const nlohmann::json& args);
    static ToolCallResult SemanticSearch(const nlohmann::json& args);
    static ToolCallResult MentionLookup(const nlohmann::json& args);
    static ToolCallResult NextEditHint(const nlohmann::json& args);
    static ToolCallResult ProposeMultiFileEdits(const nlohmann::json& args);
    static ToolCallResult PreviewMultiFileDiff(const nlohmann::json& args);
    static ToolCallResult ApplyMultiFileEdits(const nlohmann::json& args);
    static ToolCallResult LoadRules(const nlohmann::json& args);
    static ToolCallResult PlanTasks(const nlohmann::json& args);
    static ToolCallResult ManageTodoList(const nlohmann::json& args);
    static ToolCallResult Memory(const nlohmann::json& args);
    static ToolCallResult SwebenchAutonomousEval(const nlohmann::json& args);

    // ---- Git/GitHub Tools ----
    static ToolCallResult GitStatus(const nlohmann::json& args);
    static ToolCallResult GitDiff(const nlohmann::json& args);
    static ToolCallResult GitCommit(const nlohmann::json& args);
    static ToolCallResult GHCreatePR(const nlohmann::json& args);
    static ToolCallResult GHPrView(const nlohmann::json& args);
    static ToolCallResult GHIssueView(const nlohmann::json& args);

    // ---- Debug Tools (DbgEng integration) ----
    static ToolCallResult DebugLaunch(const nlohmann::json& args);
    static ToolCallResult DebugAttach(const nlohmann::json& args);
    static ToolCallResult DebugBreakTool(const nlohmann::json& args);
    static ToolCallResult DebugContinue(const nlohmann::json& args);
    static ToolCallResult DebugStepOver(const nlohmann::json& args);
    static ToolCallResult DebugStepInto(const nlohmann::json& args);
    static ToolCallResult DebugAddBreakpoint(const nlohmann::json& args);
    static ToolCallResult DebugRemoveBreakpoint(const nlohmann::json& args);
    static ToolCallResult DebugStacktrace(const nlohmann::json& args);
    static ToolCallResult DebugRegisters(const nlohmann::json& args);
    static ToolCallResult DebugMemory(const nlohmann::json& args);
    static ToolCallResult DebugDisasm(const nlohmann::json& args);
    static ToolCallResult DebugAnalyze(const nlohmann::json& args);
    static ToolCallResult DebugSnapshot(const nlohmann::json& args);
    static ToolCallResult DebugSuggestBreakpoints(const nlohmann::json& args);

    // ---- Build / Assembly / Coverage / System Tools ----
    static ToolCallResult RunBuild(const nlohmann::json& args);
    static ToolCallResult AsmAssemble(const nlohmann::json& args);
    static ToolCallResult GetCoverage(const nlohmann::json& args);
    static ToolCallResult ApplyHotpatch(const nlohmann::json& args);
    static ToolCallResult RevertHotpatch(const nlohmann::json& args);
    static ToolCallResult ListHotpatches(const nlohmann::json& args);
    static ToolCallResult HotpatchStatus(const nlohmann::json& args);
    static ToolCallResult SysGetCapabilities(const nlohmann::json& args);
    static ToolCallResult DiskRecovery(const nlohmann::json& args);
    static ToolCallResult GenerateImage(const nlohmann::json& args);
    static ToolCallResult GenerateVideo(const nlohmann::json& args);

    // ---- Schema generation (OpenAI function-calling format) ----
    static nlohmann::json GetAllSchemas();
    /// Human-readable tool list for small-model XML prompts (MinimalAgentController / Copilot parity).
    static std::string BuildCompactToolCatalogForPrompt();
    static ScopedInstructionPromptData ResolveScopedInstructions(const std::string& cwd,
                                                                 const std::vector<std::string>& openFiles);
    static std::string GetSystemPrompt(const std::string& cwd, const std::vector<std::string>& openFiles,
                                       std::vector<std::string>* appliedInstructionSources = nullptr);

    // ---- Generic dispatch (for DeterministicReplayEngine) ----
    static AgentToolHandlers& Instance();
    bool HasTool(const std::string& name) const;
    ToolCallResult Execute(const std::string& name, const nlohmann::json& args);

  private:
    AgentToolHandlers();

    // P1: Tool Wiring Optimization - Using a function map for O(1) dispatch
    typedef ToolCallResult (*ToolHandlerFunc)(const nlohmann::json&);
    std::unordered_map<std::string, ToolHandlerFunc> m_dispatchTable;
    void InitializeDispatchTable();

    // ---- Path validation ----
    static bool IsPathAllowed(const std::string& path);
    static std::string NormalizePath(const std::string& path);
    static bool MatchesDenyPattern(const std::string& path);

    // ---- Internal helpers ----
    static std::string CreateBackup(const std::string& path);
    /// Optional working directory (UTF-16) for CreateProcess; nullptr = process default. When set, term-pipe fast path
    /// is skipped.
    static bool RunProcess(const std::wstring& cmdLine, uint32_t timeoutMs, std::string& output, uint32_t& exitCode,
                           const wchar_t* workingDirectoryUtf16 = nullptr);

    // ---- Alias command adapters (P1 wiring) ----
    static ToolCallResult ToolGitStatus(const nlohmann::json& args);
    static ToolCallResult ToolGitDiff(const nlohmann::json& args);
    static ToolCallResult ToolGitLog(const nlohmann::json& args);
    static ToolCallResult ToolGitBranch(const nlohmann::json& args);
    static ToolCallResult ToolGitCommit(const nlohmann::json& args);
    static ToolCallResult ToolGhIssueList(const nlohmann::json& args);
    static ToolCallResult ToolGhPrList(const nlohmann::json& args);

    static ToolGuardrails s_guardrails;
};

}  // namespace Agent
}  // namespace RawrXD
