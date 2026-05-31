#include "delta_state_kv_encoding.h"

#include <algorithm>

namespace RawrXD::Memory {

DeltaStateKVEncoding::DeltaStateKVEncoding(size_t maxDeltaChainLength)
    : m_maxDeltaChainLength(maxDeltaChainLength == 0 ? 1 : maxDeltaChainLength) {}

void DeltaStateKVEncoding::createCheckpoint(uint64_t stateId, const std::vector<uint8_t>& snapshot) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DSKEState s{};
    s.stateId = stateId;
    s.checkpointId = stateId;
    s.checkpoint = snapshot;
    m_states[stateId] = std::move(s);
}

bool DeltaStateKVEncoding::appendDelta(uint64_t stateId, const std::vector<uint8_t>& deltaBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_states.find(stateId);
    if (it == m_states.end()) {
        return false;
    }

    DSKEState& s = it->second;
    s.deltas.push_back(DSKEDelta{stateId, deltaBytes});

    // Bound reconstruction cost by adaptive checkpointing.
    if (s.deltas.size() > m_maxDeltaChainLength) {
        std::vector<uint8_t> merged = s.checkpoint;
        for (const auto& d : s.deltas) {
            if (merged.size() < d.payload.size()) {
                merged.resize(d.payload.size(), 0);
            }
            for (size_t i = 0; i < d.payload.size(); ++i) {
                merged[i] ^= d.payload[i];
            }
        }
        s.checkpoint = std::move(merged);
        s.checkpointId = stateId;
        s.deltas.clear();
    }

    return true;
}

std::optional<std::vector<uint8_t>> DeltaStateKVEncoding::reconstruct(uint64_t stateId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_states.find(stateId);
    if (it == m_states.end()) {
        return std::nullopt;
    }

    std::vector<uint8_t> out = it->second.checkpoint;
    for (const auto& d : it->second.deltas) {
        if (out.size() < d.payload.size()) {
            out.resize(d.payload.size(), 0);
        }
        for (size_t i = 0; i < d.payload.size(); ++i) {
            out[i] ^= d.payload[i];
        }
    }

    return out;
}

} // namespace RawrXD::Memory
