#include "execution_path_memory_folding.h"

#include <algorithm>

namespace RawrXD::Memory {

uint64_t ExecutionPathMemoryFolding::foldOrCreate(const PathSignature& signature,
                                                  const std::vector<uint64_t>& kvPages) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const uint64_t sigKey = signatureKey(signature);
    auto idxIt = m_indexBySignature.find(sigKey);
    if (idxIt != m_indexBySignature.end()) {
        auto nodeIt = m_nodes.find(idxIt->second);
        if (nodeIt != m_nodes.end()) {
            ++nodeIt->second.refCount;
            return nodeIt->first;
        }
    }

    FoldedPathNode node{};
    node.nodeId = m_nextNodeId++;
    node.signature = signature;
    node.kvPages = kvPages;
    node.refCount = 1;

    m_nodes[node.nodeId] = node;
    m_indexBySignature[sigKey] = node.nodeId;
    return node.nodeId;
}

bool ExecutionPathMemoryFolding::retain(uint64_t nodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return false;
    }

    ++it->second.refCount;
    return true;
}

bool ExecutionPathMemoryFolding::release(uint64_t nodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return false;
    }

    if (it->second.refCount > 0) {
        --it->second.refCount;
    }

    if (it->second.refCount == 0) {
        m_indexBySignature.erase(signatureKey(it->second.signature));
        m_nodes.erase(it);
    }

    return true;
}

std::optional<FoldedPathNode> ExecutionPathMemoryFolding::lookup(uint64_t nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return std::nullopt;
    }

    return it->second;
}

PathSignature ExecutionPathMemoryFolding::makeSignature(const std::vector<uint32_t>& topKIndices,
                                                        const std::vector<float>& quantizedWeights,
                                                        uint32_t headMask) {
    uint64_t h = 1469598103934665603ull;
    const uint64_t prime = 1099511628211ull;

    for (uint32_t v : topKIndices) {
        h ^= static_cast<uint64_t>(v);
        h *= prime;
    }

    for (float w : quantizedWeights) {
        const uint32_t q = static_cast<uint32_t>(std::clamp(w, -1.0f, 1.0f) * 10000.0f + 10000.0f);
        h ^= static_cast<uint64_t>(q);
        h *= prime;
    }

    PathSignature sig{};
    sig.hash = h;
    sig.length = static_cast<uint32_t>(topKIndices.size());
    sig.headMask = headMask;
    return sig;
}

uint64_t ExecutionPathMemoryFolding::signatureKey(const PathSignature& sig) {
    uint64_t x = sig.hash;
    x ^= (static_cast<uint64_t>(sig.length) << 32);
    x ^= sig.headMask;
    x ^= (x >> 33);
    x *= 0xff51afd7ed558ccdull;
    x ^= (x >> 33);
    return x;
}

} // namespace RawrXD::Memory
