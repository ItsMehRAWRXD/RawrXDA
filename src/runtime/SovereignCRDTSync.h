#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Runtime {

struct CRDTNode {
    std::string value;
    uint64_t    LWW_Timestamp; ///< Monotonic µs since steady_clock epoch
    uint32_t    vectorCounter; ///< Local write counter (0 for remote merges)
};

class SovereignCRDTSync {
public:
    static SovereignCRDTSync& instance();

    // Local propose — appends serialized delta to outbox
    bool proposeUpdate(const std::string& key, const std::string& value);

    // Inbound remote merge (LWW)
    bool mergeRemote(const std::string& key, const std::string& value, uint64_t timestamp);

    // Delta codec (mesh broadcast outbox)
    std::vector<uint8_t> nextDelta();              ///< Pop next outbound delta
    bool applyDelta(const std::vector<uint8_t>&);  ///< Apply inbound delta

    // Full state import/export
    std::vector<uint8_t> exportFull() const;
    bool importFull(const std::vector<uint8_t>& data);

    std::map<std::string, CRDTNode> getIndex() const;

private:
    SovereignCRDTSync() = default;

    std::map<std::string, CRDTNode>  m_index;
    std::deque<std::vector<uint8_t>> m_deltaLog;
    mutable std::mutex               m_mutex;
    uint32_t                         m_localCounter = 0;
};

} // namespace RawrXD::Runtime
