#pragma once
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {

class MemoryPlugin {
public:
    virtual ~MemoryPlugin() = default;
    virtual std::string getName() const = 0;
    virtual int getContextSize() const = 0;
    virtual bool isCompatible(size_t availableRamMB) const = 0; // Simulation for compatibility check
};

class MemoryPlugin4K : public MemoryPlugin {
public:
    std::string getName() const override { return "4K (Standard)"; }
    int getContextSize() const override { return 4096; }
    bool isCompatible(size_t) const override { return true; }
};

class MemoryPlugin32K : public MemoryPlugin {
public:
    std::string getName() const override { return "32K (Large)"; }
    int getContextSize() const override { return 32768; }
    bool isCompatible(size_t ram) const override { return ram > 8192; }
};

class MemoryPlugin64K : public MemoryPlugin {
public:
    std::string getName() const override { return "64K (Extra Large)"; }
    int getContextSize() const override { return 65536; }
    bool isCompatible(size_t ram) const override { return ram > 16384; }
};

class MemoryPlugin128K : public MemoryPlugin {
public:
    std::string getName() const override { return "128K (Ultra)"; }
    int getContextSize() const override { return 131072; }
    bool isCompatible(size_t ram) const override { return ram > 24000; }
};

class MemoryPlugin256K : public MemoryPlugin {
public:
    std::string getName() const override { return "256K (Extreme)"; }
    int getContextSize() const override { return 262144; }
    bool isCompatible(size_t ram) const override { return ram > 48000; }
};

class MemoryPlugin512K : public MemoryPlugin {
public:
    std::string getName() const override { return "512K (Ludicrous)"; }
    int getContextSize() const override { return 524288; }
    bool isCompatible(size_t ram) const override { return ram > 96000; }
};

class MemoryPlugin1M : public MemoryPlugin {
public:
    std::string getName() const override { return "1M (Omniscient)"; }
    int getContextSize() const override { return 1048576; }
    bool isCompatible(size_t ram) const override { return ram > 192000; }
};

class MemoryPluginFactory {
public:
    static std::vector<std::shared_ptr<MemoryPlugin>> getAvailablePlugins() {
        return {
            std::make_shared<MemoryPlugin4K>(),
            std::make_shared<MemoryPlugin32K>(),
            std::make_shared<MemoryPlugin64K>(),
            std::make_shared<MemoryPlugin128K>(),
            std::make_shared<MemoryPlugin256K>(),
            std::make_shared<MemoryPlugin512K>(),
            std::make_shared<MemoryPlugin1M>()
        };
    }
};

}
