// ============================================================================
// Win32IDE_AgentOperationsBridge.h
// Bridge between hotpatch UI and production agent operations
// Provides clean interface for executing real agent operations from hotpatch handlers
// ============================================================================

#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD::Win32IDE {

class AgentOperationsBridge {
public:
    // Initialize the bridge (called at IDE startup)
    static void Initialize();
    
    // Execute each of the 7 agent operations from hotpatch handlers
    static std::string ExecuteCompactConversation(const std::string& text, size_t target_tokens = 2048);
    static std::string ExecuteOptimizeToolSelection(const std::string& intent, const std::vector<std::string>& tools);
    static std::string ExecuteResolveSymbol(const std::string& symbol_name, const std::vector<std::string>& search_paths);
    static std::string ExecuteReadFileLines(const std::string& filepath, size_t start_line, size_t end_line);
    static std::string ExecutePlanCodeExploration(const std::string& root_path, const std::string& query);
    static std::string ExecuteSearchFiles(const std::string& pattern, const std::vector<std::string>& search_paths, bool use_regex = false);
    static std::string ExecuteCheckpointManager(const std::string& action, const std::string& checkpoint_id, const std::string& root_path);
    
    // Format operation result for UI display
    static std::string FormatOperationResult(const std::string& operation_name, const std::string& json_result);
    
private:
    static bool m_initialized;
};

}  // namespace RawrXD::Win32IDE
