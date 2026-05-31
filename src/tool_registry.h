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
    static bool has_tool(const std::string& name);
    static bool execute_tool(const std::string& name, const std::string& params_json, std::string& out_result);
    static void ensure_core_tools();
};

extern "C" __declspec(dllexport) void* __stdcall InitializeToolRegistry();
extern "C" __declspec(dllexport) int __stdcall ExecuteToolByName(
    const char* tool_name,
    const char* json_params,
    char* result_buffer,
    unsigned long long buffer_size);
