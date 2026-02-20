#include "tool_registry.h"
#include <sstream>

static std::unordered_map<std::string, ToolFunc> tools;

void ToolRegistry::register_tool(const std::string& name, ToolFunc fn) {
    tools[name] = fn;
}

std::string ToolRegistry::list_tools() {
    std::stringstream ss;
    for (const auto& kv : tools) {
        ss << "- " << kv.first << "\n";
    }
    return ss.str();
}

void ToolRegistry::inject_tools(AgentRequest& req) {
    if (!tools.empty()) {
        req.prompt = "[TOOLS AVAILABLE]\n" + list_tools() + "\n" + req.prompt;
    }
}
