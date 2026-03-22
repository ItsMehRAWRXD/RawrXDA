#pragma once

#include "RuntimeTypes.hpp"

#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>

namespace RawrXD::Runtime {

enum class LayerLoadState : std::uint8_t {
    Offloaded = 0,
    Resident = 1,
    Generating = 2,
};

/// Per-transformer-layer load balance: **0** active resident targets until a lane requests generation.
/// Modular backends register by name; layers keyed by stable id (e.g. `blk12.ffn`).
class ModularModelLoader {
public:
    static ModularModelLoader& instance();

    using RegisterResult = std::expected<void, std::string>;

    [[nodiscard]] RegisterResult registerBackendModule(const std::string& name, void* opaqueHandle);
    void unregisterBackendModule(const std::string& name);

    [[nodiscard]] std::expected<LayerLoadState, std::string> layerState(const std::string& layerId) const;
    [[nodiscard]] RegisterResult setLayerState(const std::string& layerId, LayerLoadState s);

    /// Target count of **resident** layers for load balancer (0 = all offloaded when idle).
    void setResidentLayerBudget(int n);
    [[nodiscard]] int residentLayerBudget() const { return m_residentBudget; }

private:
    ModularModelLoader() = default;

    struct LayerEntry {
        LayerLoadState state = LayerLoadState::Offloaded;
    };

    mutable std::mutex m_mutex{};
    std::unordered_map<std::string, void*> m_backends{};
    std::unordered_map<std::string, LayerEntry> m_layers{};
    int m_residentBudget = 0;
};

}  // namespace RawrXD::Runtime
