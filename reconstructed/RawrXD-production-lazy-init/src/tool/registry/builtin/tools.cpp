#include "tool_registry_builtin_tools.hpp"

#include "tool_registry.hpp"

#include "backend/agentic_tools.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace RawrXD {

using json = nlohmann::json;

static json tryParseJsonOrWrap(const std::string& maybeJson) {
    try {
        return json::parse(maybeJson);
    } catch (...) {
        return json{{"output", maybeJson}};
    }
}

void registerBackendAgenticTools(ToolRegistry& registry, const std::string& workspaceRoot) {
    auto exec = std::make_shared<RawrXD::Backend::AgenticToolExecutor>(workspaceRoot);

    const auto schemas = exec->getToolSchemas();
    for (const auto& schema : schemas) {
        ToolDefinition tool;
        tool.name = schema.name;
        tool.description = schema.description;

        // Basic categorization
        if (schema.name.rfind("file_", 0) == 0 || schema.name.rfind("dir_", 0) == 0) {
            tool.category = ToolCategory::FileSystem;
            tool.tags = {"file", "fs"};
        } else if (schema.name.rfind("git_", 0) == 0) {
            tool.category = ToolCategory::VersionControl;
            tool.tags = {"git", "vcs"};
        } else {
            tool.category = ToolCategory::Custom;
        }

        // ToolRegistry will treat exceptions as tool failure.
        tool.handler = [exec, toolName = schema.name](const json& params) -> json {
            auto r = exec->executeTool(toolName, params.dump());
            if (!r.success) {
                throw std::runtime_error(r.error_message.empty() ? "Tool execution failed" : r.error_message);
            }
            return tryParseJsonOrWrap(r.result_data);
        };

        registry.registerTool(tool);
    }
}

} // namespace RawrXD
