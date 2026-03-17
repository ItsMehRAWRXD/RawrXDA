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
// X-Macro: AGENT_TOOLS_X(M)
// M(internal_name, description, parameter_schema_json)
//
// Parameter schema uses nlohmann::json initializer-list syntax.
// Each tool maps 1:1 to an OpenAI function-calling tool definition.
// ---------------------------------------------------------------------------
#define AGENT_TOOLS_X(M) \
    M(read_file, \
      "Read the content of a file at a specific path. Returns UTF-8 text.", \
      {{"path", {{"type", "string"}, {"description", "Absolute or project-relative path to the file"}}}}) \
    \
    M(write_file, \
      "Create a new file or overwrite an existing one with the provided content.", \
      {{"path", {{"type", "string"}, {"description", "Target file path"}}}, \
       {"content", {{"type", "string"}, {"description", "Full file content to write"}}}}) \
    \
    M(replace_in_file, \
      "Search and replace a block of text within a file. Uses exact string matching.", \
      {{"path", {{"type", "string"}, {"description", "File to modify"}}}, \
       {"old_string", {{"type", "string"}, {"description", "Exact text to find (include context lines)"}}}, \
       {"new_string", {{"type", "string"}, {"description", "Replacement text"}}}}) \
    \
    M(execute_command, \
      "Run a shell command in the terminal. Returns stdout, stderr, and exit code.", \
      {{"command", {{"type", "string"}, {"description", "Shell command to execute"}}}, \
       {"timeout", {{"type", "number"}, {"description", "Timeout in milliseconds (default 30000)"}, {"default", 30000}}}}) \
    \
    M(search_code, \
      "Fast regex/literal search across the codebase. Uses AVX-512 SIMD accelerator when available.", \
      {{"query", {{"type", "string"}, {"description", "Search pattern (regex or literal)"}}}, \
       {"file_pattern", {{"type", "string"}, {"description", "Glob filter (default: *.*)"}, {"default", "*.*"}}}, \
       {"is_regex", {{"type", "boolean"}, {"description", "Treat query as regex"}, {"default", false}}}}) \
    \
    M(get_diagnostics, \
      "Retrieve current compiler/LSP errors and warnings for a specific file or all files.", \
      {{"file", {{"type", "string"}, {"description", "File path, or empty for all diagnostics"}}}}) \
    \
    M(list_directory, \
      "List files and subdirectories at a given path.", \
      {{"path", {{"type", "string"}, {"description", "Directory path to list"}}}, \
       {"recursive", {{"type", "boolean"}, {"description", "Recurse into subdirectories"}, {"default", false}}}}) \
    \
    M(get_coverage, \
      "Retrieve BBCov/DiffCov coverage data for a file or function to verify logic path changes.", \
      {{"file", {{"type", "string"}, {"description", "Source file to query coverage for"}}}, \
       {"function_name", {{"type", "string"}, {"description", "Optional: specific function to check coverage"}}}}) \
    \
    M(run_build, \
      "Trigger a CMake build with specified target and configuration.", \
      {{"target", {{"type", "string"}, {"description", "Build target (default: all)"}, {"default", "all"}}}, \
       {"config", {{"type", "string"}, {"description", "Build configuration (Release/Debug)"}, {"default", "Release"}}}}) \
    \
    M(apply_hotpatch, \
      "Apply a runtime hotpatch through the unified hotpatch manager (memory, byte-level, or server layer).", \
      {{"layer", {{"type", "string"}, {"description", "Patch layer: memory, byte, or server"}}}, \
       {"target", {{"type", "string"}, {"description", "Target address, file, or endpoint"}}}, \
       {"data", {{"type", "string"}, {"description", "Patch payload (hex-encoded for memory/byte)"}}}})

// ---------------------------------------------------------------------------
// Tool ID enum — auto-generated from X-Macro
// ---------------------------------------------------------------------------
enum class ToolId : uint32_t {
    #define ENUM_ENTRY(name, desc, params) name,
    AGENT_TOOLS_X(ENUM_ENTRY)
    #undef ENUM_ENTRY
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
    std::atomic<uint64_t> invocation_count{0};
    std::atomic<uint64_t> error_count{0};
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
