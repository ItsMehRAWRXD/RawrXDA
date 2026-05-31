#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct NUMANode {
    uint32_t nodeId = 0;
    std::vector<uint32_t> cores;
    size_t memoryBytes = 0;
};

struct LayerPlacement {
    uint32_t layerIdx = 0;
    uint32_t numaNode = 0;
    size_t bytes = 0;
};

class NUMATopologyAwarePlacement {
public:
    NUMATopologyAwarePlacement();

    bool detectTopology();
    uint32_t assignLayer(uint32_t layerIdx, size_t bytes, const std::vector<uint32_t>& dependentLayers);
    bool migrateLayer(uint32_t layerIdx, uint32_t targetNode);

    const NUMANode* getNode(uint32_t nodeId) const;
    const LayerPlacement* getLayerPlacement(uint32_t layerIdx) const;
    size_t nodeCount() const;
    size_t layerCount() const;

private:
    mutable std::mutex m_mutex;
    std::vector<NUMANode> m_nodes;
    std::unordered_map<uint32_t, LayerPlacement> m_layers;

    uint32_t selectBestNode(size_t bytes, const std::vector<uint32_t>& deps) const;
};

} // namespace RawrXD::Memory
