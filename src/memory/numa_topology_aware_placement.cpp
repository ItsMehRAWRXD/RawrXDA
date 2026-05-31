#include "numa_topology_aware_placement.h"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <numa.h>
#include <sys/sysinfo.h>
#endif

namespace RawrXD::Memory {

NUMATopologyAwarePlacement::NUMATopologyAwarePlacement() {
    detectTopology();
}

bool NUMATopologyAwarePlacement::detectTopology() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nodes.clear();

#ifdef _WIN32
    ULONG highestNode = 0;
    if (!GetNumaHighestNodeNumber(&highestNode)) {
        return false;
    }

    for (ULONG node = 0; node <= highestNode; ++node) {
        ULONGLONG mask = 0;
        if (!GetNumaNodeProcessorMask(node, &mask)) continue;

        NUMANode n;
        n.nodeId = static_cast<uint32_t>(node);
        for (ULONG i = 0; i < sizeof(mask) * 8; ++i) {
            if (mask & (1ULL << i)) {
                n.cores.push_back(static_cast<uint32_t>(i));
            }
        }

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(memInfo);
        GlobalMemoryStatusEx(&memInfo);
        n.memoryBytes = memInfo.ullTotalPhys / (highestNode + 1);

        m_nodes.push_back(std::move(n));
    }
#else
    if (numa_available() < 0) {
        NUMANode n;
        n.nodeId = 0;
        n.memoryBytes = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
        for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); ++i) {
            n.cores.push_back(static_cast<uint32_t>(i));
        }
        m_nodes.push_back(std::move(n));
    } else {
        int maxNode = numa_max_node() + 1;
        for (int node = 0; node < maxNode; ++node) {
            NUMANode n;
            n.nodeId = static_cast<uint32_t>(node);
            n.memoryBytes = numa_node_size(node, nullptr);
            bitmask* mask = numa_allocate_cpumask();
            numa_node_to_cpus(node, mask);
            for (int i = 0; i < numa_num_configured_cpus(); ++i) {
                if (numa_bitmask_isbitset(mask, i)) {
                    n.cores.push_back(static_cast<uint32_t>(i));
                }
            }
            numa_free_cpumask(mask);
            m_nodes.push_back(std::move(n));
        }
    }
#endif

    return !m_nodes.empty();
}

uint32_t NUMATopologyAwarePlacement::assignLayer(uint32_t layerIdx, size_t bytes, const std::vector<uint32_t>& dependentLayers) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t node = selectBestNode(bytes, dependentLayers);

    LayerPlacement p;
    p.layerIdx = layerIdx;
    p.numaNode = node;
    p.bytes = bytes;
    m_layers[layerIdx] = std::move(p);

    return node;
}

bool NUMATopologyAwarePlacement::migrateLayer(uint32_t layerIdx, uint32_t targetNode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_layers.find(layerIdx);
    if (it == m_layers.end()) return false;

    it->second.numaNode = targetNode;
    return true;
}

const NUMANode* NUMATopologyAwarePlacement::getNode(uint32_t nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& n : m_nodes) {
        if (n.nodeId == nodeId) return &n;
    }
    return nullptr;
}

const LayerPlacement* NUMATopologyAwarePlacement::getLayerPlacement(uint32_t layerIdx) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_layers.find(layerIdx);
    return it == m_layers.end() ? nullptr : &it->second;
}

size_t NUMATopologyAwarePlacement::nodeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes.size();
}

size_t NUMATopologyAwarePlacement::layerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_layers.size();
}

uint32_t NUMATopologyAwarePlacement::selectBestNode(size_t bytes, const std::vector<uint32_t>& deps) const {
    // Count layers per node
    std::unordered_map<uint32_t, size_t> nodeLoad;
    for (const auto& [id, p] : m_layers) {
        nodeLoad[p.numaNode] += p.bytes;
    }

    // Prefer nodes with dependent layers
    std::unordered_map<uint32_t, size_t> depNodes;
    for (uint32_t dep : deps) {
        auto it = m_layers.find(dep);
        if (it != m_layers.end()) {
            depNodes[it->second.numaNode]++;
        }
    }

    uint32_t bestNode = 0;
    size_t bestScore = SIZE_MAX;
    for (const auto& n : m_nodes) {
        size_t load = nodeLoad[n.nodeId];
        size_t depScore = depNodes.count(n.nodeId) ? depNodes.at(n.nodeId) : 0;
        size_t score = load - depScore * 1000; // Prefer nodes with dependencies
        if (score < bestScore) {
            bestScore = score;
            bestNode = n.nodeId;
        }
    }

    return bestNode;
}

} // namespace RawrXD::Memory
