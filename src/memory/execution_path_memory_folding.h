#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct PathSignature {
    uint64_t hash = 0;
    uint32_t length = 0;
    uint32_t headMask = 0;

    bool operator==(const PathSignature& other) const {
        return hash == other.hash && length == other.length && headMask == other.headMask;
    }
};

struct FoldedPathNode {
    uint64_t nodeId = 0;
    PathSignature signature{};
    std::vector<uint64_t> kvPages;
    size_t refCount = 0;
};

class ExecutionPathMemoryFolding {
public:
    uint64_t foldOrCreate(const PathSignature& signature, const std::vector<uint64_t>& kvPages);
    bool retain(uint64_t nodeId);
    bool release(uint64_t nodeId);
    std::optional<FoldedPathNode> lookup(uint64_t nodeId) const;

    static PathSignature makeSignature(const std::vector<uint32_t>& topKIndices,
                                       const std::vector<float>& quantizedWeights,
                                       uint32_t headMask);

private:
    static uint64_t signatureKey(const PathSignature& sig);

    mutable std::mutex m_mutex;
    uint64_t m_nextNodeId{1};
    std::unordered_map<uint64_t, FoldedPathNode> m_nodes;
    std::unordered_map<uint64_t, uint64_t> m_indexBySignature;
};

} // namespace RawrXD::Memory
