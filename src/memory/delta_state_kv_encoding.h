#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct DSKEDelta {
    uint64_t stateId = 0;
    std::vector<uint8_t> payload;
};

struct DSKEState {
    uint64_t stateId = 0;
    uint64_t checkpointId = 0;
    std::vector<uint8_t> checkpoint;
    std::deque<DSKEDelta> deltas;
};

class DeltaStateKVEncoding {
public:
    explicit DeltaStateKVEncoding(size_t maxDeltaChainLength = 8);

    void createCheckpoint(uint64_t stateId, const std::vector<uint8_t>& snapshot);
    bool appendDelta(uint64_t stateId, const std::vector<uint8_t>& deltaBytes);
    std::optional<std::vector<uint8_t>> reconstruct(uint64_t stateId) const;

private:
    mutable std::mutex m_mutex;
    size_t m_maxDeltaChainLength;
    std::unordered_map<uint64_t, DSKEState> m_states;
};

} // namespace RawrXD::Memory
