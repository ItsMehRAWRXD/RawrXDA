#pragma once
#include <vector>
#include <string>
#include <memory>
#include <iostream>

namespace RawrXD {

class IMemoryPlugin {
public:
    virtual ~IMemoryPlugin() = default;
    virtual std::string GetName() const = 0;
    virtual size_t GetMaxContext() const = 0;
    virtual bool Configure(size_t tokenLimit) = 0;
    virtual void Optimize() = 0;
};

// Standard RAM-based memory context (Default)
class StandardMemoryPlugin : public IMemoryPlugin {
public:
    std::string GetName() const override { return "Standard RAM Context"; }
    size_t GetMaxContext() const override { return 32 * 1024; } // 32k default limit
    bool Configure(size_t limit) override {
        // In a real plugin, this would allocate/reserve memory
        return limit <= GetMaxContext();
    }
    bool Optimize() override { /* Standard optimization */ return true; }
};

// Large Context Plugin (simulated paging/mmap)
class LargeContextPlugin : public IMemoryPlugin {
    size_t m_currentLimit = 0;
public:
    std::string GetName() const override { return "Extended Memory Extension (1M+)"; }
    size_t GetMaxContext() const override { return 1024 * 1024; } // 1 Million
    bool Configure(size_t limit) override {
        m_currentLimit = limit;
        // In reality, this would setup specific KV cache offloading
        std::cout << "[MemoryPlugin] Configured Extended Context to " << limit << " tokens." << std::endl;
        return true;
    }
    bool Optimize() override { 
        std::cout << "[MemoryPlugin] Optimizing large context KV cache..." << std::endl;
        return true;
    }
};

}
