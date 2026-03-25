<<<<<<< HEAD
#pragma once
// Consolidated MemoryPlugin.hpp — merges the interface + factory from plugins/
// with the richer plugin implementations from src/memory_plugin.hpp.
// Both StandardMemoryPlugin and LargeContextPlugin now carry the best ideas
// from each original version.
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

// Standard RAM-based memory context.
// Original plugins/ version: 4k default, no limit checking.
// Original src/ version: 32k default, limit checking.
// Consolidated: 32k default with limit checking and dynamic name.
class StandardMemoryPlugin : public IMemoryPlugin {
    size_t m_maxContext = 32 * 1024; // 32k default
public:
    std::string GetName() const override {
        return "Standard RAM Context (" + std::to_string(m_maxContext / 1024) + "k)";
    }
    size_t GetMaxContext() const override { return m_maxContext; }
    bool Configure(size_t limit) override {
        if (limit > m_maxContext) return false;
        return true;
    }
    bool Optimize() override { return true; }
};

// Large Context Plugin (simulated paging/mmap).
// Original plugins/ version: dynamic name with current limit, 1M max.
// Original src/ version: logging on configure/optimize, 1M max.
// Consolidated: dynamic name + 1M max + all features kept.
class LargeContextPlugin : public IMemoryPlugin {
    size_t m_currentLimit = 32768;
public:
    std::string GetName() const override {
        return "Extended Memory (" + std::to_string(m_currentLimit / 1024) + "k / 1M max)";
    }
    size_t GetMaxContext() const override { return 1024 * 1024; } // 1M Support
    bool Configure(size_t limit) override {
        m_currentLimit = limit;
        return true;
    }
    bool Optimize() override { return true; }
};

// Factory providing both built-in plugins
class MemoryPluginFactory {
public:
    static std::vector<std::shared_ptr<IMemoryPlugin>> getAvailablePlugins() {
        std::vector<std::shared_ptr<IMemoryPlugin>> plugins;
        plugins.push_back(std::make_shared<StandardMemoryPlugin>());
        plugins.push_back(std::make_shared<LargeContextPlugin>());
        return plugins;
    }
};

} // namespace RawrXD
=======
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
>>>>>>> origin/main
