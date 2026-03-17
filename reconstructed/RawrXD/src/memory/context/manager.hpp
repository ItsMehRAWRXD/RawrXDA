#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

namespace RawrXD {

class MemoryContextManager {
public:
    enum class ContextSize {
        k4K = 4096,
        k32K = 32768,
        k64K = 65536,
        k128K = 131072,
        k256K = 262144,
        k512K = 524288,
        k1M = 1048576
    };

    struct ContextPlugin {
        std::string name;
        ContextSize size;
        bool requiresHighMem;
        std::vector<float> reservedKvCache; // Simulation of KV Cache "module"
    };

    static MemoryContextManager& instance() {
        static MemoryContextManager mgr;
        return mgr;
    }

    void registerPlugin(const std::string& name, ContextSize size) {
        m_plugins[name] = {name, size, size > ContextSize::k32K};
    }

    bool loadPlugin(const std::string& name) {
        if (m_plugins.find(name) == m_plugins.end()) return false;
        m_currentContext = m_plugins[name].size;
        m_activePlugin = name;
        // In a real engine, we would resize the KV cache tensor here
        std::cout << "[MemoryManager] Switched to Context Plugin: " << name << " (" << (int)m_currentContext << " tokens)" << std::endl;
        return true;
    }

    int getCurrentContextLimit() const { return (int)m_currentContext; }
    std::string getActivePluginName() const { return m_activePlugin; }

    std::vector<std::string> getAvailablePlugins() {
        std::vector<std::string> names;
        for(auto& p : m_plugins) names.push_back(p.first);
        return names;
    }

private:
    MemoryContextManager() {
        // Default plugins
        registerPlugin("Standard-4k", ContextSize::k4K);
        registerPlugin("Pro-32k", ContextSize::k32K);
        registerPlugin("Ultra-64k", ContextSize::k64K);
        registerPlugin("Mega-128k", ContextSize::k128K);
        registerPlugin("Giga-256k", ContextSize::k256K);
        registerPlugin("Tera-512k", ContextSize::k512K);
        registerPlugin("Omni-1M", ContextSize::k1M);
        
        loadPlugin("Standard-4k");
    }

    std::map<std::string, ContextPlugin> m_plugins;
    ContextSize m_currentContext = ContextSize::k4K;
    std::string m_activePlugin;
};

}
