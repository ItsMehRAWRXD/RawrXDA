#include "thermal_aware_tensor_tiering"

namespace RawrXD::Memory {

uint64_t ThermalAwareTensorTiering::allocate(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t id = m_nextId++;
    TensorHeat t;
    t.tensorId = id;
    t.bytes = bytes;
    t.accessCount = 0;
    t.heat = 0;
    t.tier = TensorTier::PinnedRAM;
    m_tensors[id] = t;
    return id;
}

void ThermalAwareTensorTiering::touch(uint64_t tensorId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tensors.find(tensorId);
    if (it == m_tensors.end()) return;
    it->second.accessCount++;
    it->second.heat += it->second.bytes;
}

void ThermalAwareTensorTiering::decayAll(float factor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, t] : m_tensors) {
        t.heat = static_cast<uint64_t>(t.heat * factor);
    }
}

void ThermalAwareTensorTiering::free(uint64_t tensorId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tensors.erase(tensorId);
}

std::vector<uint64_t> ThermalAwareTensorTiering::rebalance() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint64_t> migrated;
    for (auto& [id, t] : m_tensors) {
        TensorTier target = computeTier(t.heat);
        if (target != t.tier) {
            t.tier = target;
            migrated.push_back(id);
        }
    }
    return migrated;
}

const TensorHeat* ThermalAwareTensorTiering::getTensor(uint64_t tensorId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tensors.find(tensorId);
    return it == m_tensors.end() ? nullptr : &it->second;
}

size_t ThermalAwareTensorTiering::totalBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& [id, t] : m_tensors) {
        total += t.bytes;
    }
    return total;
}

size_t ThermalAwareTensorTiering::vramBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vramBytes;
}

TensorTier ThermalAwareTensorTiering::computeTier(uint64_t heat) const {
    if (heat >= HOT_THRESHOLD) return TensorTier::VRAM;
    if (heat <= COLD_THRESHOLD) return TensorTier::LargePage;
    return TensorTier::PinnedRAM;
}

} // namespace RawrXD::Memory
