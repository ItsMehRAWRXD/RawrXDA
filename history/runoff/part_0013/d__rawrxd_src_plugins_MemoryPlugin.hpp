#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace RawrXD {

class IMemoryPlugin {
public:
    virtual ~IMemoryPlugin() = default;
    virtual std::string GetName() const = 0;
    virtual size_t GetMaxContext() const = 0;
    virtual bool Configure(size_t limit) = 0;
    virtual bool Optimize() = 0;
};

class StandardMemoryPlugin : public IMemoryPlugin {
public:
    std::string GetName() const override { return "Standard (4k)"; }
    size_t GetMaxContext() const override { return 4096; }
    bool Configure(size_t limit) override { return true; }
    bool Optimize() override { return true; }
};

class LargeContextPlugin : public IMemoryPlugin {
    size_t m_currentLimit = 32768;
public:
    std::string GetName() const override { return "Large Context (" + std::to_string(m_currentLimit/1024) + "k)"; }
    size_t GetMaxContext() const override { return 1024 * 1024; } // 1M Support
    bool Configure(size_t limit) override { 
        m_currentLimit = limit; 
        return true; 
    }
    bool Optimize() override { return true; }
};

class MemoryPluginFactory {
public:
    static std::vector<std::shared_ptr<IMemoryPlugin>> getAvailablePlugins() {
        std::vector<std::shared_ptr<IMemoryPlugin>> plugins;
        plugins.push_back(std::make_shared<StandardMemoryPlugin>());
        plugins.push_back(std::make_shared<LargeContextPlugin>());
        return plugins;
    }
};

}