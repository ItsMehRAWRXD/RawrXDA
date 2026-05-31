#include "cross_session_memory_dedup.h"

namespace RawrXD::Memory {

uint64_t CrossSessionMemoryDedup::combineHashes(uint64_t a, uint64_t b) {
    return (a * 11400714819323198485ull) ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
}

uint64_t CrossSessionMemoryDedup::bindOrInsert(const std::string& tenant,
                                               uint64_t semanticHash,
                                               uint64_t exactHash,
                                               uint64_t candidateKvId,
                                               size_t bytes,
                                               CSMDScope scope) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const uint64_t key = combineHashes(semanticHash, exactHash);
    auto idxIt = m_indexByCombined.find(key);
    if (idxIt != m_indexByCombined.end()) {
        for (uint64_t existingKv : idxIt->second) {
            auto entryIt = m_entriesByKv.find(existingKv);
            if (entryIt == m_entriesByKv.end()) {
                continue;
            }
            auto& e = entryIt->second;
            const bool sameTenant = (e.ownerTenant == tenant);
            const bool globalOptIn = (e.scope == CSMDScope::GlobalOptIn && scope == CSMDScope::GlobalOptIn);
            if (sameTenant || globalOptIn) {
                ++e.refCount;
                return e.kvId;
            }
        }
    }

    CSMDEntry e;
    e.kvId = candidateKvId;
    e.semanticHash = semanticHash;
    e.exactHash = exactHash;
    e.ownerTenant = tenant;
    e.scope = scope;
    e.bytes = bytes;
    e.refCount = 1;

    m_entriesByKv[e.kvId] = e;
    m_indexByCombined[key].insert(e.kvId);
    return e.kvId;
}

std::optional<CSMDEntry> CrossSessionMemoryDedup::lookup(const std::string& tenant,
                                                         uint64_t semanticHash,
                                                         uint64_t exactHash,
                                                         bool allowSemanticOnly) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Primary: Exact match (Semantic + Exact Hash)
    const uint64_t exactKey = combineHashes(semanticHash, exactHash);
    auto idxIt = m_indexByCombined.find(exactKey);
    if (idxIt != m_indexByCombined.end()) {
        for (uint64_t id : idxIt->second) {
            auto eIt = m_entriesByKv.find(id);
            if (eIt == m_entriesByKv.end()) {
                continue;
            }
            const auto& e = eIt->second;
            if (e.ownerTenant == tenant || e.scope == CSMDScope::GlobalOptIn) {
                return e;
            }
        }
    }

    if (!allowSemanticOnly) {
        return std::nullopt;
    }

    // Secondary: Semantic match for cases where exact hashes drift (e.g. slight precision or metadata change)
    for (const auto& [_, e] : m_entriesByKv) {
        if (e.semanticHash == semanticHash && (e.ownerTenant == tenant || e.scope == CSMDScope::GlobalOptIn)) {
            return e;
        }
    }

    return std::nullopt;
}

bool CrossSessionMemoryDedup::releaseRef(uint64_t kvId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entriesByKv.find(kvId);
    if (it == m_entriesByKv.end()) {
        return false;
    }

    if (it->second.refCount > 0) {
        --it->second.refCount;
    }

    if (it->second.refCount == 0) {
        const uint64_t key = combineHashes(it->second.semanticHash, it->second.exactHash);
        auto idxIt = m_indexByCombined.find(key);
        if (idxIt != m_indexByCombined.end()) {
            idxIt->second.erase(kvId);
            if (idxIt->second.empty()) {
                m_indexByCombined.erase(idxIt);
            }
        }
        m_entriesByKv.erase(it);
    }

    return true;
}

} // namespace RawrXD::Memory
