#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace RawrXD::Memory {

enum class CSMDScope {
    TenantLocal,
    GlobalOptIn
};

struct CSMDEntry {
    uint64_t kvId = 0;
    uint64_t semanticHash = 0;
    uint64_t exactHash = 0;
    std::string ownerTenant;
    CSMDScope scope = CSMDScope::TenantLocal;
    size_t bytes = 0;
    size_t refCount = 0;
};

class CrossSessionMemoryDedup {
public:
    // Returns kvId to use (existing canonical or provided one if inserted).
    uint64_t bindOrInsert(const std::string& tenant,
                          uint64_t semanticHash,
                          uint64_t exactHash,
                          uint64_t candidateKvId,
                          size_t bytes,
                          CSMDScope scope = CSMDScope::TenantLocal);

    std::optional<CSMDEntry> lookup(const std::string& tenant,
                                    uint64_t semanticHash,
                                    uint64_t exactHash,
                                    bool allowSemanticOnly = true) const;

    bool releaseRef(uint64_t kvId);

private:
    static uint64_t combineHashes(uint64_t a, uint64_t b);

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, CSMDEntry> m_entriesByKv;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> m_indexByCombined;
};

} // namespace RawrXD::Memory
