// =============================================================================
// ToolRegistry.h — X-Macro Tool Registry & LLM Schema Generator
// =============================================================================
// Defines the interface between LLM intent and engine execution.
// Adding a tool here automatically updates:
//   1. The JSON schema sent to the LLM (OpenAI function-calling format)
//   2. The dispatch logic in AgentOrchestrator
//   3. The system prompt injected into every conversation
//
// Uses the X-Macro pattern for single-source-of-truth tool definitions.
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace RawrXD
{
namespace Agent
{

// ---------------------------------------------------------------------------
// X-Macro: AGENT_TOOLS_X(M)
// M(internal_name, description)
//
// Parameter schemas are defined programmatically in InitDescriptors().
// Each tool maps 1:1 to an OpenAI function-calling tool definition.
// ---------------------------------------------------------------------------
#define AGENT_TOOLS_X(M)                                                                                               \
    M(read_file, "Read the content of a file at a specific path. Returns UTF-8 text.")                                 \
    M(write_file, "Create a new file or overwrite an existing one with the provided content.")                         \
    M(replace_in_file, "Search and replace a block of text within a file. Uses exact string matching.")                \
    M(execute_command, "Run a shell command in the terminal. Returns stdout, stderr, and exit code.")                  \
    M(screenshot, "Capture a screenshot to a PNG file (desktop or active window). Returns {path,width,height}.")       \
    M(tool_registry_self_check,                                                                                        \
      "Report missing tool handlers and required tool wiring gaps. Returns {ok,missing_handlers}.")                    \
    M(ingame_frame_check, "Capture and analyze an in-game frame for likely clipping/z-fighting artifacts, optionally " \
                          "with Unity/Unreal profiler context.")                                                       \
    M(search_code, "Fast regex/literal search across the codebase. Uses AVX-512 SIMD accelerator when available.")     \
    M(get_diagnostics, "Retrieve current compiler/LSP errors and warnings for a specific file or all files.")          \
    M(list_directory, "List files and subdirectories at a given path.")                                                \
    M(get_coverage, "Retrieve BBCov/DiffCov coverage data for a file or function to verify logic path changes.")       \
    M(run_build, "Trigger a CMake build with specified target and configuration.")                                     \
    M(apply_hotpatch,                                                                                                  \
      "Apply a runtime hotpatch through the unified hotpatch manager (memory, byte-level, or server layer).")          \
    M(disk_recovery, "Control the hardware disk recovery agent for dying WD My Book USB bridges (scan, init, extract " \
                     "key, run, abort, stats).")                                                                       \
    M(get_gpu_telemetry, "Query GPU backend telemetry (VRAM, utilization, etc.).")                                     \
    M(tune_vram_limit, "Tune VRAM limit for the inference backend.")                                                   \
    M(bench_kernel, "Benchmark internal kernels for performance characterization.")                                    \
    M(create_memory_silo, "Create an isolated memory silo with quotas and limits.")                                    \
    M(query_silo_stats, "Query current stats for a memory silo.")                                                      \
    M(set_silo_quota, "Set/update quota limits for a memory silo.")                                                    \
    M(map_model_aperture, "Map a window (aperture) of a model file into memory for inspection/patching.")              \
    M(query_virtual_memory, "Query virtual memory information for a given address and process.")                       \
    M(manage_local_embeddings, "Manage local embeddings index (index/query/clear/stats) for semantic retrieval.")      \
    M(memory_file, "Create, view, update, delete, rename, reindex, or retrieve persistent RawrXD agent memory files.") \
    M(purge_telemetry, "Purge telemetry/traces/cache/logs with optional dry-run and retention control.")               \
    M(git_status, "Get the current status of the git repository (branch, modified, staged files).")                    \
    M(git_diff, "Get the diff of changes in the repository against a target (default: HEAD).")                         \
    M(git_commit, "Commit staged changes with a descriptive message.")                                                 \
    M(gh_issue_list, "List GitHub issues for the current repository.")                                                 \
    M(gh_issue_view, "View details for a GitHub issue by number.")                                                     \
    M(gh_pr_list, "List GitHub pull requests for the current repository.")                                             \
    M(gh_pr_view, "View details for a GitHub pull request by number.")                                                 \
    M(gh_create_pr, "Create a new GitHub pull request for the current branch.")                                        \
    M(gh_pr_checks, "Show CI/status checks for a GitHub pull request.")                                                \
    M(gh_pr_diff, "Show unified diff for a GitHub pull request.")                                                      \
    M(gh_pr_review, "Submit a GitHub pull request review (approve, comment, request_changes).")                        \
    M(gh_pr_comment, "Add a comment to a GitHub pull request.")                                                        \
    M(gh_pr_merge, "Merge a GitHub pull request with merge/squash/rebase strategy.")                                   \
    M(propose_multifile_edits, "Plan structured multi-file edits and generate unified diff previews.")                 \
    M(preview_multifile_diff, "Preview unified diffs for a structured multi-file edit plan.")                          \
    M(apply_multifile_edits, "Apply a structured multi-file edit plan transactionally with rollback metadata.")        \
    M(refactor_rename_symbol,                                                                                          \
      "Rename an identifier across one file or a workspace subtree with word-boundary safety.")                        \
    M(asm_assemble, "Assemble MASM/x64 source code into a PE64 executable using the internal SovereignAssembler.")     \
    M(term_exec, "Execute a terminal command (agent terminal lane) with cwd/timeout control.")                         \
    M(sys_get_capabilities,                                                                                            \
      "Query host hardware capabilities (CPU/GPU) to determine optimal execution tier (VRAM, AVX-512, etc.).")         \
    M(debug_launch, "Launch an executable under the native Windows debugger (DbgEng). Returns PID and initial state.") \
    M(debug_attach, "Attach the native debugger to a running process by PID.")                                         \
    M(debug_break, "Break (pause) the currently debugged process.")                                                    \
    M(debug_continue, "Continue execution of the paused debugged process.")                                            \
    M(debug_step_over, "Step over the current instruction/line in the debugged process.")                              \
    M(debug_step_into, "Step into the current function call in the debugged process.")                                 \
    M(debug_add_breakpoint, "Add a breakpoint at a symbol name, source file:line, or hex address.")                    \
    M(debug_remove_breakpoint, "Remove a breakpoint by its ID number.")                                                \
    M(debug_stacktrace, "Capture the current call stack with symbol resolution.")                                      \
    M(debug_registers, "Capture all x64 register values (RAX-R15, RIP, RFLAGS).")                                      \
    M(debug_memory, "Read and display a hex dump of memory at a given address.")                                       \
    M(debug_disasm, "Disassemble instructions at a given address with annotations.")                                   \
    M(debug_analyze,                                                                                                   \
      "Analyze the last exception/crash with AI-generated root cause hypothesis and suggested actions.")               \
    M(debug_snapshot,                                                                                                  \
      "Capture a full debug session snapshot (state, exception, stack, modules) formatted for AI analysis.")           \
    M(debug_suggest_breakpoints,                                                                                       \
      "Get AI-suggested breakpoints based on a problem description (e.g. 'crash', 'memory leak', 'deadlock').")        \
    M(debug_status, "Get current debugger session status (state, PID, target metadata).")                              \
    M(debug_modules, "List currently loaded modules in the debuggee process.")                                         \
    M(debug_detach, "Detach debugger from the current target process.")                                                \
    M(debug_terminate, "Terminate the current debuggee process and end session.")

// ---------------------------------------------------------------------------
// Tool ID enum — auto-generated from X-Macro
// ---------------------------------------------------------------------------
enum class ToolId : uint32_t
{
#define ENUM_ENTRY(tool_name_, tool_desc_) tool_name_,
    AGENT_TOOLS_X(ENUM_ENTRY)
#undef ENUM_ENTRY
    _COUNT
};

// ---------------------------------------------------------------------------
// Tool execution result (no exceptions — follows PatchResult pattern)
// ---------------------------------------------------------------------------
struct ToolExecResult
{
    bool success;
    std::string output;
    int exit_code;
    double elapsed_ms;

    static ToolExecResult ok(const std::string& out, double ms = 0.0) { return {true, out, 0, ms}; }
    static ToolExecResult error(const std::string& msg, int code = -1) { return {false, msg, code, 0.0}; }
};

// ---------------------------------------------------------------------------
// Tool handler function pointer (no std::function in hot path)
// ---------------------------------------------------------------------------
using ToolHandler = ToolExecResult (*)(const json& args);

// ---------------------------------------------------------------------------
// ToolDescriptor — runtime metadata for a single tool
// ---------------------------------------------------------------------------
struct ToolDescriptor
{
    const char* name;
    const char* description;
    json params_schema;
    ToolHandler handler;
    uint64_t invocation_count = 0;  // Protected by registry mutex
    uint64_t error_count = 0;       // Protected by registry mutex
};

// ---------------------------------------------------------------------------
// AgentToolRegistry — singleton registry with LLM schema generation
// ---------------------------------------------------------------------------
class AgentToolRegistry
{
  public:
    static AgentToolRegistry& Instance();

    // Generate OpenAI-compatible tool schemas for the LLM
    json GetToolSchemas() const;

    // Generate the full system prompt with tool documentation
    std::string GetSystemPrompt(const std::string& cwd, const std::vector<std::string>& openFiles,
                                std::vector<std::string>* appliedInstructionSources = nullptr) const;

    // Dispatch a tool call by name
    ToolExecResult Dispatch(const std::string& tool_name, const json& args);

    // Register a handler for a tool (called during init)
    void RegisterHandler(const std::string& tool_name, ToolHandler handler);

    // Get tool names
    std::vector<std::string> ListTools() const;

    // Tool wiring validation (production hardening)
    // Returns tool names present in the schema but missing a wired handler.
    std::vector<std::string> GetToolsMissingHandlers() const;

    // Get stats
    uint64_t GetTotalInvocations() const;
    uint64_t GetTotalErrors() const;

    // Validate args against schema
    bool ValidateArgs(const std::string& tool_name, const json& args, std::string& error) const;

    // Ranked tool matches for a normalized query string.
    // similarity ∈ [0,1] using bigram Jaccard.
    struct IntentMatch
    {
        std::string toolName;
        float similarity;
    };

    // Returns the best-matching registered tool for an arbitrary intent string.
    // If similarity < threshold the first result's name may be empty.
    // Results are cached — subsequent calls with the same query are O(1).
    std::vector<IntentMatch> RankByIntent(const std::string& query, size_t topK = 3) const;

  private:
    struct CachedToolResult
    {
        ToolExecResult result;
        std::chrono::steady_clock::time_point expiresAt;
    };

    AgentToolRegistry();
    ~AgentToolRegistry() = default;

    void InitDescriptors();
    bool IsResultCacheEligibleTool(const std::string& normalizedTool) const;
    int ResultCacheTtlMs(const std::string& normalizedTool) const;
    std::string BuildResultCacheKey(const std::string& normalizedTool, const json& args) const;

    mutable std::mutex m_mutex;
    std::vector<ToolDescriptor> m_tools;
    std::unordered_map<std::string, size_t> m_nameIndex;

    // ---- Intent similarity cache ----
    // Maps normalized query → topK ranked tool names + scores.
    // Populated lazily on the first Dispatch/RankByIntent call for each
    // unique query.  Protected by m_mutex.
    mutable std::unordered_map<std::string, std::vector<IntentMatch>> m_intentCache;

    // ---- Tool result cache ----
    // Key: normalized tool + canonical args JSON dump.
    // Stores successful responses for read-only deterministic tools.
    mutable std::unordered_map<std::string, CachedToolResult> m_toolResultCache;
};

}  // namespace Agent
}  // namespace RawrXD
