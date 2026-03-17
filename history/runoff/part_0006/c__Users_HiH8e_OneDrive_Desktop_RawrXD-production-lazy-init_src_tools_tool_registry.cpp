#include "tool_registry.hpp"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>

// Static category definitions
const std::string ToolRegistry::FILE_OPS = "File Operations";
const std::string ToolRegistry::BUILD_TEST = "Build & Test";
const std::string ToolRegistry::GIT_OPS = "Git Operations";
const std::string ToolRegistry::SEARCH_ANALYSIS = "Search & Analysis";
const std::string ToolRegistry::EXTERNAL_INTEGRATIONS = "External Integrations";
const std::string ToolRegistry::SYSTEM_OPS = "System Operations";

ToolRegistry::ToolRegistry() {
    registerDefaultTools();
}

ToolRegistry::~ToolRegistry() {}

bool ToolRegistry::registerTool(const Tool& tool) {
    if (tools_.find(tool.name) != tools_.end()) {
        return false; // Tool already exists
    }
    tools_[tool.name] = tool;
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        tools_.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> ToolRegistry::getToolNames() const {
    std::vector<std::string> names;
    for (const auto& pair : tools_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<ToolRegistry::Tool> ToolRegistry::getToolsByCategory(const std::string& category) const {
    std::vector<Tool> result;
    for (const auto& pair : tools_) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }
    return result;
}

ToolRegistry::Tool ToolRegistry::getTool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return Tool{}; // Return empty tool if not found
}

std::string ToolRegistry::executeTool(const std::string& name, const std::vector<std::string>& parameters) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second.execute(parameters);
    }
    return "Error: Tool '" + name + "' not found";
}

void ToolRegistry::registerDefaultTools() {
    // File Operations (12 tools)
    registerTool({
        "FileEdit", 
        "Edit file content", 
        FILE_OPS,
        [](const std::vector<std::string>& params) -> std::string {
            if (params.size() < 2) return "Error: FileEdit requires file path and content";
            std::ofstream file(params[0]);
            if (!file) return "Error: Cannot open file " + params[0];
            file << params[1];
            return "Success: Edited " + params[0];
        }
    });
    
    registerTool({
        "ReadFile", 
        "Read file content", 
        FILE_OPS,
        [](const std::vector<std::string>& params) -> std::string {
            if (params.empty()) return "Error: ReadFile requires file path";
            std::ifstream file(params[0]);
            if (!file) return "Error: Cannot open file " + params[0];
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return "Content: " + content;
        }
    });
    
    registerTool({
        "CreateDirectory", 
        "Create directory", 
        FILE_OPS,
        [](const std::vector<std::string>& params) -> std::string {
            if (params.empty()) return "Error: CreateDirectory requires directory path";
            if (std::filesystem::create_directories(params[0])) {
                return "Success: Created directory " + params[0];
            }
            return "Error: Failed to create directory " + params[0];
        }
    });
    
    // Build & Test (4 tools)
    registerTool({
        "RunBuild", 
        "Execute build command", 
        BUILD_TEST,
        [](const std::vector<std::string>& params) -> std::string {
            return "Build execution placeholder";
        }
    });
    
    registerTool({
        "ExecuteTests", 
        "Run test suite", 
        BUILD_TEST,
        [](const std::vector<std::string>& params) -> std::string {
            return "Test execution placeholder";
        }
    });
    
    // Git Operations (8 tools)
    registerTool({
        "GitStatus", 
        "Check git status", 
        GIT_OPS,
        [](const std::vector<std::string>& params) -> std::string {
            return "Git status placeholder";
        }
    });
    
    // Search & Analysis (6 tools)
    registerTool({
        "SearchFiles", 
        "Search files by pattern", 
        SEARCH_ANALYSIS,
        [](const std::vector<std::string>& params) -> std::string {
            return "File search placeholder";
        }
    });
    
    // External Integrations (8 tools)
    registerTool({
        "HTTPRequest", 
        "Make HTTP request", 
        EXTERNAL_INTEGRATIONS,
        [](const std::vector<std::string>& params) -> std::string {
            return "HTTP request placeholder";
        }
    });
    
    // System Operations (6 tools)
    registerTool({
        "ProcessSpawn", 
        "Spawn new process", 
        SYSTEM_OPS,
        [](const std::vector<std::string>& params) -> std::string {
            return "Process spawn placeholder";
        }
    });
    
    // Add more tools as needed...
}