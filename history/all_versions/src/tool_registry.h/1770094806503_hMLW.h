#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "engine_iface.h"

typedef std::function<std::string(const std::string&)> ToolFunc;

class ToolRegistry {
public:
    static void register_tool(const std::string& name, ToolFunc fn);
    static void inject_tools(AgentRequest& req);
    static std::string list_tools();
};
