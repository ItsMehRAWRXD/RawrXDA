#include "context_topology_compression.h"

#include <algorithm>

namespace RawrXD::Memory {

uint64_t ContextTopologyCompression::addNode(const std::vector<uint32_t>& tokens) {
    std::lock_guard<std::mutex> lock(m_mutex);

    CTCNode n{};
    n.nodeId = m_nextNodeId++;
    n.tokens = tokens;
    n.hotness = 1;
    m_nodes[n.nodeId] = n;
    return n.nodeId;
}

void ContextTopologyCompression::link(uint64_t parent, uint64_t child) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto pIt = m_nodes.find(parent);
    auto cIt = m_nodes.find(child);
    if (pIt == m_nodes.end() || cIt == m_nodes.end()) {
        return;
    }

    auto& children = pIt->second.children;
    if (std::find(children.begin(), children.end(), child) == children.end()) {
        children.push_back(child);
    }
}

void ContextTopologyCompression::markHot(uint64_t nodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        ++it->second.hotness;
    }
}

std::vector<uint32_t> ContextTopologyCompression::materializeLinear(const std::vector<uint64_t>& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint32_t> out;
    for (uint64_t nodeId : path) {
        auto it = m_nodes.find(nodeId);
        if (it == m_nodes.end()) {
            continue;
        }
        out.insert(out.end(), it->second.tokens.begin(), it->second.tokens.end());
    }
    return out;
}

std::optional<CTCNode> ContextTopologyCompression::getNode(uint64_t nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return std::nullopt;
    }

    return it->second;
}

} // namespace RawrXD::Memory
