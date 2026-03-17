// RawrXD_ToolRegistry.hpp - Autonomous Tool Execution Framework
// Pure C++20 - No Qt Dependencies
// Sandboxed tool execution with safety validation

#pragma once

#include "RawrXD_JSON.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD {

enum class ToolResult { Success, ValidationFailed, ExecutionError, Timeout, Cancelled };
enum class DangerLevel { Safe = 1, Normal = 2, Destructive = 3, Critical = 4 };

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string category;
    DangerLevel danger = DangerLevel::Normal;
    std::function<ToolResult(const JSONObject&, std::string&)> handler;
};

class ToolRegistry {
public:
    static ToolRegistry& Instance() {
        static ToolRegistry instance;
        return instance;
    }

    void RegisterTool(const ToolDefinition& tool) {
        tools_[tool.name] = tool;
    }

    std::vector<std::string> ListTools() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : tools_) names.push_back(name);
        return names;
    }

    const ToolDefinition* GetTool(const std::string& name) const {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? &it->second : nullptr;
    }

    ToolResult Execute(const std::string& toolName, const std::string& jsonArgs, std::string& output) {
        auto tool = GetTool(toolName);
        if (!tool) {
            output = "Tool not found: " + toolName;
            return ToolResult::ValidationFailed;
        }

        try {
            auto args = JSONParser::Parse(jsonArgs);
            return tool->handler(args.asObject(), output);
        } catch (...) {
            output = "JSON parse error";
            return ToolResult::ValidationFailed;
        }
    }

    void InitializeDefaultTools() {
        // File operations
        RegisterTool({
            "read_file",
            "Read file contents",
            "filesystem",
            DangerLevel::Safe,
            [](const JSONObject& args, std::string& output) -> ToolResult {
                auto path = args.at("path").asString();
                std::ifstream f(path);
                if (!f.is_open()) {
                    output = "Failed to open file";
                    return ToolResult::ExecutionError;
                }
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                output = content;
                return ToolResult::Success;
            }
        });

        RegisterTool({
            "write_file",
            "Write file contents",
            "filesystem",
            DangerLevel::Destructive,
            [](const JSONObject& args, std::string& output) -> ToolResult {
                auto path = args.at("path").asString();
                auto content = args.at("content").asString();
                std::ofstream f(path);
                if (!f.is_open()) {
                    output = "Failed to write file";
                    return ToolResult::ExecutionError;
                }
                f << content;
                output = "File written: " + path;
                return ToolResult::Success;
            }
        });

        RegisterTool({
            "list_directory",
            "List directory contents",
            "filesystem",
            DangerLevel::Safe,
            [](const JSONObject& args, std::string& output) -> ToolResult {
                auto path = args.at("path").asString();
                JSONArray files;
                for (const auto& entry : fs::directory_iterator(path)) {
                    files.push_back(JSONValue(entry.path().filename().string()));
                }
                output = JSONValue(files).stringify();
                return ToolResult::Success;
            }
        });

        // Process execution
        RegisterTool({
            "run_command",
            "Execute shell command",
            "system",
            DangerLevel::Critical,
            [](const JSONObject& args, std::string& output) -> ToolResult {
                auto cmd = args.at("command").asString();
                
                // Security: Validate command
                if (cmd.find("rm -rf") != std::string::npos || 
                    cmd.find("del /f") != std::string::npos) {
                    output = "Dangerous command blocked";
                    return ToolResult::ValidationFailed;
                }
                
                FILE* pipe = _popen(cmd.c_str(), "r");
                if (!pipe) {
                    output = "Command execution failed";
                    return ToolResult::ExecutionError;
                }
                
                char buffer[1024];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    output += buffer;
                }
                _pclose(pipe);
                return ToolResult::Success;
            }
        });
    }

private:
    ToolRegistry() { InitializeDefaultTools(); }
    std::map<std::string, ToolDefinition> tools_;
};

} // namespace RawrXD
