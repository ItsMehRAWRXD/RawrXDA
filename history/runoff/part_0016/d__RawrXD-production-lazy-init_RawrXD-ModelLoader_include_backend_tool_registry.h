#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "agentic_tools.h"

// Forward declare OllamaChatMessage
namespace RawrXD {
namespace Backend {
struct OllamaChatMessage;
}
}

namespace RawrXD {
namespace Backend {

// Tool registry for managing available tools
class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();
    
    // Initialize the registry with default tools
    bool Initialize();
    
    // Register a tool
    void RegisterTool(std::shared_ptr<AgenticTool> tool);
    
    // Get all registered tools
    std::vector<std::shared_ptr<AgenticTool>> GetAllTools() const;
    
    // Get tool by name
    std::shared_ptr<AgenticTool> GetTool(const std::string& name) const;
    
    // Check if tool exists
    bool HasTool(const std::string& name) const;
    
    // Execute a tool by name
    std::string ExecuteTool(const std::string& name, const std::vector<std::string>& params);
    
    // Execute a tool by name (returns structured result)
    ToolResult ExecuteToolStructured(const std::string& name, const std::vector<std::string>& params);
    
    // Get all tool schemas
    std::vector<std::string> GetAllToolSchemas() const;
    
    // Set workspace root for tools
    void SetWorkspace(const std::string& workspace);
    
    // Register all production tools
    void RegisterAllProductionTools();
    
    // Get tool statistics
    std::string GetStats() const;
    
    // Get all tool schemas (alias for GetAllToolSchemas)
    std::vector<std::string> getAllToolSchemas() const { return GetAllToolSchemas(); }
    
    // Execute tool (alias for ExecuteTool)
    std::string executeTool(const std::string& name, const std::vector<std::string>& params) {
        return ExecuteTool(name, params);
    }
    
    // Execute tool with single parameter (convenience)
    ToolResult executeTool(const std::string& name, const std::string& param) {
        return ExecuteToolStructured(name, std::vector<std::string>{param});
    }
    
    // Set workspace (alias for SetWorkspace)
    void setWorkspace(const std::string& workspace) { SetWorkspace(workspace); }
    
    // Register all production tools (alias for RegisterAllProductionTools)
    void registerAllProductionTools() { RegisterAllProductionTools(); }
    
    // Get stats (alias for GetStats)
    std::string getStats() const { return GetStats(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD