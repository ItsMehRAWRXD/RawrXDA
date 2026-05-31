#include "gradient_checkpoint_manager.h"

namespace RawrXD::Memory {

GradientCheckpointManager::GradientCheckpointManager(uint32_t totalLayers,
                                                     uint32_t checkpointInterval)
    : m_totalLayers(totalLayers), m_interval(checkpointInterval == 0 ? 1 : checkpointInterval) {
    m_segments.resize(m_totalLayers);
    for (uint32_t i = 0; i < m_totalLayers; ++i) {
        m_segments[i].layerIndex = i;
        m_segments[i].saved = shouldSave(i);
        m_segments[i].bytes = 0;
    }
}

bool GradientCheckpointManager::shouldSave(uint32_t layerIdx) const {
    if (layerIdx >= m_totalLayers) {
        return false;
    }
    return (layerIdx % m_interval) == 0 || layerIdx == (m_totalLayers - 1);
}

std::vector<uint32_t> GradientCheckpointManager::recomputePathTo(uint32_t targetLayer) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint32_t> path;
    if (targetLayer >= m_totalLayers) {
        return path;
    }

    int32_t start = static_cast<int32_t>(targetLayer);
    while (start > 0 && !m_segments[static_cast<size_t>(start)].saved) {
        --start;
    }

    for (uint32_t i = static_cast<uint32_t>(start + 1); i <= targetLayer; ++i) {
        if (!m_segments[i].saved) {
            path.push_back(i);
        }
    }
    return path;
}

void GradientCheckpointManager::recordActivation(uint32_t layerIdx, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (layerIdx >= m_segments.size()) {
        return;
    }
    m_segments[layerIdx].bytes = bytes;
    m_segments[layerIdx].saved = shouldSave(layerIdx);
}

size_t GradientCheckpointManager::savedMemoryBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& s : m_segments) {
        if (s.saved) {
            total += s.bytes;
        }
    }
    return total;
}

size_t GradientCheckpointManager::skippedMemoryBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& s : m_segments) {
        if (!s.saved) {
            total += s.bytes;
        }
    }
    return total;
}

void GradientCheckpointManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& s : m_segments) {
        s.saved = shouldSave(s.layerIndex);
        s.bytes = 0;
    }
}

} // namespace RawrXD::Memory
