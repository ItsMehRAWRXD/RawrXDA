#pragma once
#include <vector>
#include <string>
#include <memory>
#include "logging/logger.h"
#include "plugins/MemoryPlugin.hpp"

// Standard RAM-based memory context (Default)
class StandardMemoryPlugin : public RawrXD::IMemoryPlugin {
public:
    std::string GetName() const override { return "Standard RAM Context"; }
    size_t GetMaxContext() const override { return 32 * 1024; } // 32k default limit
    bool Configure(size_t limit) override {
        return limit <= GetMaxContext();
    }
    bool Optimize() override { return true; }
};

// Large Context Plugin (simulated paging/mmap)
class LargeContextPlugin : public RawrXD::IMemoryPlugin {
    size_t m_currentLimit = 0;
    Logger m_logger{"MemoryPlugin"};
public:
    std::string GetName() const override { return "Extended Memory Extension (1M+)"; }
    size_t GetMaxContext() const override { return 1024 * 1024; } // 1 Million
    bool Configure(size_t limit) override {
        m_currentLimit = limit;
        m_logger.info("Configured Extended Context to {} tokens.", limit);
        return true;
    }
    bool Optimize() override { 
        m_logger.info("Optimizing large context KV cache...");
        return true;
    }
};
