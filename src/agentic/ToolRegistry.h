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

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// Tool ID enum — explicit registration (no X-Macro)
// Each entry maps 1:1 to an OpenAI function-calling tool definition.
// Parameter schemas are defined programmatically in InitDescriptors().
// ---------------------------------------------------------------------------
enum class ToolId : uint32_t {
    // Core file/build tools
    read_file,
    write_file,
    replace_in_file,
    execute_command,
    search_code,
    get_diagnostics,
    list_directory,
    get_coverage,
    run_build,
    apply_hotpatch,
    disk_recovery,
    // Extended file/system tools (CLI/GUI parity)
    delete_file,
    rename_file,
    copy_file,
    make_directory,
    stat_file,
    git_status,
    run_shell,
    // AI-specific tools
    semantic_search,
    mention_lookup,
    next_edit_hint,
    propose_multifile_edits,
    load_rules,
    plan_tasks,
    set_iteration_status,
    get_iteration_status,
    reset_iteration_status,
    // Agent operation tools (outside-hotpatch accessible)
    compact_conversation,
    optimize_tool_selection,
    resolve_symbol,
    read_lines,
    plan_code_exploration,
    search_files,
    restore_checkpoint,
    evaluate_integration,
    _COUNT
};

// ---------------------------------------------------------------------------
// Tool execution result (no exceptions — follows PatchResult pattern)
// ---------------------------------------------------------------------------
struct ToolExecResult {
    bool success;
    std::string output;
    int exit_code;
    double elapsed_ms;

    static ToolExecResult ok(const std::string& out, double ms = 0.0) {
        return {true, out, 0, ms};
    }
    static ToolExecResult error(const std::string& msg, int code = -1) {
        return {false, msg, code, 0.0};
    }
};

// ---------------------------------------------------------------------------
// Tool handler function pointer (no std::function in hot path)
// ---------------------------------------------------------------------------
using ToolHandler = ToolExecResult(*)(const json& args);

// ---------------------------------------------------------------------------
// ToolDescriptor — runtime metadata for a single tool
// ---------------------------------------------------------------------------
struct ToolDescriptor {
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
class AgentToolRegistry {
public:
    static AgentToolRegistry& Instance();

    // Generate OpenAI-compatible tool schemas for the LLM
    json GetToolSchemas() const;

    // Generate the full system prompt with tool documentation
    std::string GetSystemPrompt(const std::string& cwd,
                                const std::vector<std::string>& openFiles) const;

    // Dispatch a tool call by name
    ToolExecResult Dispatch(const std::string& tool_name, const json& args);

    // Register a handler for a tool (called during init)
    void RegisterHandler(const std::string& tool_name, ToolHandler handler);

    // Get tool names
    std::vector<std::string> ListTools() const;

    // Get stats
    uint64_t GetTotalInvocations() const;
    uint64_t GetTotalErrors() const;

    // Validate args against schema
    bool ValidateArgs(const std::string& tool_name, const json& args,
                      std::string& error) const;

private:
    AgentToolRegistry();
    ~AgentToolRegistry() = default;

    void InitDescriptors();

    mutable std::mutex m_mutex;
    std::vector<ToolDescriptor> m_tools;
    std::unordered_map<std::string, size_t> m_nameIndex;
};

} // namespace Agent
} // namespace RawrXD
