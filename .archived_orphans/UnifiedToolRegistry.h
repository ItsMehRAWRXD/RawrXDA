#pragma once
// UnifiedToolRegistry.h — Consolidated tool registry with SignalSlot dispatch
// Replaces 3 competing tool_registry variants with a single unified registry
// No Qt. No exceptions. C++20 only.

#include "RawrXD_SignalSlot.h"
#include "engine_iface.h"
#include <functional>
#include <unordered_map>
#include <string>

class UnifiedToolRegistry {
public:
    using ToolFunc = std::function<void(const AgentRequest&)>;

private:
    std::unordered_map<std::string, ToolFunc> m_tools;
    RawrXD::Signal<const std::string&, const AgentRequest&> m_toolInvoked;
    RawrXD::Signal<const std::string&, bool> m_toolCompleted;

public:
    static UnifiedToolRegistry& Instance() {
        static UnifiedToolRegistry instance;
        return instance;
    }

    void Register(const std::string& name, ToolFunc func) {
        m_tools[name] = std::move(func);
    }

    bool Invoke(const std::string& name, const AgentRequest& req) {
        auto it = m_tools.find(name);
        if (it == m_tools.end()) {
            m_toolCompleted.emit(name, false);
            return false;
        }
        m_toolInvoked.emit(name, req);
        it->second(req);
        m_toolCompleted.emit(name, true);
        return true;
    }

    bool HasTool(const std::string& name) const {
        return m_tools.count(name) > 0;
    }

    size_t ToolCount() const { return m_tools.size(); }

    std::vector<std::string> ListTools() const {
        std::vector<std::string> names;
        names.reserve(m_tools.size());
        for (const auto& [k, v] : m_tools) names.push_back(k);
        return names;
    }

    auto& OnToolInvoked() { return m_toolInvoked; }
    auto& OnToolCompleted() { return m_toolCompleted; }

private:
    UnifiedToolRegistry() = default;
    UnifiedToolRegistry(const UnifiedToolRegistry&) = delete;
    UnifiedToolRegistry& operator=(const UnifiedToolRegistry&) = delete;
};

#define REGISTER_TOOL(name, func) UnifiedToolRegistry::Instance().Register(name, func)
