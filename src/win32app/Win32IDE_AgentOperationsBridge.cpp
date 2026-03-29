// ============================================================================
// Win32IDE_AgentOperationsBridge.cpp
// Implementation of bridge to production agent operations
// ============================================================================

#include "Win32IDE_AgentOperationsBridge.h"
#include "../agentic/agent_operations.h"
#include <sstream>

using RawrXD::Agentic::AgentOperations;

namespace RawrXD::Win32IDE {

bool AgentOperationsBridge::m_initialized = false;

void AgentOperationsBridge::Initialize()
{
    if (!m_initialized) {
        AgentOperations::initializeOperations();
        m_initialized = true;
    }
}

std::string AgentOperationsBridge::ExecuteCompactConversation(const std::string& text, size_t target_tokens)
{
    json params;
    params["text"] = text;
    params["target_tokens"] = target_tokens;
    params["preserve_semantics"] = true;
    
    auto result = AgentOperations::executeOperation("compact_conversation", params.dump());
    return FormatOperationResult("Conversation Compaction", result.output);
}

std::string AgentOperationsBridge::ExecuteOptimizeToolSelection(
    const std::string& intent,
    const std::vector<std::string>& tools)
{
    json params;
    params["current_intent"] = intent;
    params["available_tools"] = tools;
    
    auto result = AgentOperations::executeOperation("optimize_tool_selection", params.dump());
    return FormatOperationResult("Tool Selection Optimization", result.output);
}

std::string AgentOperationsBridge::ExecuteResolveSymbol(
    const std::string& symbol_name,
    const std::vector<std::string>& search_paths)
{
    json params;
    params["symbol_name"] = symbol_name;
    params["search_paths"] = search_paths;
    
    auto result = AgentOperations::executeOperation("resolve_symbol", params.dump());
    return FormatOperationResult("Symbol Resolution", result.output);
}

std::string AgentOperationsBridge::ExecuteReadFileLines(
    const std::string& filepath,
    size_t start_line,
    size_t end_line)
{
    json params;
    params["filepath"] = filepath;
    params["start_line"] = start_line;
    params["end_line"] = end_line;
    params["include_line_numbers"] = true;
    
    auto result = AgentOperations::executeOperation("read_file_lines", params.dump());
    return FormatOperationResult("Targeted File Reading", result.output);
}

std::string AgentOperationsBridge::ExecutePlanCodeExploration(
    const std::string& root_path,
    const std::string& query)
{
    json params;
    params["root_path"] = root_path;
    params["initial_query"] = query;
    
    auto result = AgentOperations::executeOperation("plan_code_exploration", params.dump());
    return FormatOperationResult("Code Exploration Planning", result.output);
}

std::string AgentOperationsBridge::ExecuteSearchFiles(
    const std::string& pattern,
    const std::vector<std::string>& search_paths,
    bool use_regex)
{
    json params;
    params["search_pattern"] = pattern;
    params["root_paths"] = search_paths;
    params["use_regex"] = use_regex;
    params["max_results"] = 1000;
    
    auto result = AgentOperations::executeOperation("search_files", params.dump());
    return FormatOperationResult("File Search", result.output);
}

std::string AgentOperationsBridge::ExecuteCheckpointManager(
    const std::string& action,
    const std::string& checkpoint_id,
    const std::string& root_path)
{
    json params;
    params["action"] = action;
    params["checkpoint_id"] = checkpoint_id;
    params["root_path"] = root_path;
    
    auto result = AgentOperations::executeOperation("checkpoint_manager", params.dump());
    return FormatOperationResult("Checkpoint Management", result.output);
}

std::string AgentOperationsBridge::FormatOperationResult(
    const std::string& operation_name,
    const std::string& json_result)
{
    std::ostringstream ss;
    ss << "[Agent Operation] " << operation_name << "\n";
    ss << "═════════════════════════════════════════════════════════\n\n";
    
    // Try to pretty-print JSON result
    try {
        auto parsed = json::parse(json_result);
        ss << parsed.dump(2);
    } catch (...) {
        ss << json_result;
    }
    
    ss << "\n\n═════════════════════════════════════════════════════════\n";
    return ss.str();
}

}  // namespace RawrXD::Win32IDE
