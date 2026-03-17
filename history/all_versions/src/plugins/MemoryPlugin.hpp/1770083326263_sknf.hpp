#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace RawrXD {

class IMemoryPlugin {
public:
    virtual ~IMemoryPlugin() = default;
    virtual std::string getName() = 0;
    virtual int getMaxContext() = 0;
    virtual bool configure(int limit) = 0;
    virtual bool optimize() = 0;
};

class StandardMemoryPlugin : public IMemoryPlugin {
public:
    std::string getName() override { return "Standard (4k)"; }
    int getMaxContext() override { return 4096; }
    bool configure(int limit) override { return true; }
    bool optimize() override { return true; }
};

class LargeContextPlugin : public IMemoryPlugin {
    int m_currentLimit = 32768;
public:
    std::string getName() override { return "Large Context (" + std::to_string(m_currentLimit/1024) + "k)"; }
    int getMaxContext() override { return 1024 * 1024; } // 1M Support
    bool configure(int limit) override { 
        m_currentLimit = limit; 
        return true; 
    }
    bool optimize() override { return true; }
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