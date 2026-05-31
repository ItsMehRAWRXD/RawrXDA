#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct CTCNode {
    uint64_t nodeId = 0;
    std::vector<uint32_t> tokens;
    std::vector<uint64_t> children;
    uint32_t hotness = 0;
};

class ContextTopologyCompression {
public:
    uint64_t addNode(const std::vector<uint32_t>& tokens);
    void link(uint64_t parent, uint64_t child);
    void markHot(uint64_t nodeId);

    // Rehydrate graph topology to a contiguous linear token sequence.
    std::vector<uint32_t> materializeLinear(const std::vector<uint64_t>& path) const;

    std::optional<CTCNode> getNode(uint64_t nodeId) const;

private:
    mutable std::mutex m_mutex;
    uint64_t m_nextNodeId{1};
    std::unordered_map<uint64_t, CTCNode> m_nodes;
};

} // namespace RawrXD::Memory
