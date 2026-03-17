#pragma once
// DEPRECATED — Use UnifiedToolRegistry.h instead.
// This header forwards to the unified registry for backward compatibility.

#include "UnifiedToolRegistry.h"
#include "engine_iface.h"

typedef std::function<std::string(const std::string&)> ToolFunc;

class [[deprecated("Use UnifiedToolRegistry::Instance() instead")]] ToolRegistry {
public:
    static void register_tool(const std::string& name, ToolFunc fn) {
        UnifiedToolRegistry::Instance().Register(name,
            [fn](const AgentRequest& req) { fn(req.prompt); });
    }
    static void inject_tools(AgentRequest& /*req*/) { /* no-op */ }
    static std::string list_tools() {
        std::string result;
        for (const auto& t : UnifiedToolRegistry::Instance().ListTools())
            result += t + "\n";
        return result;
    }
};
