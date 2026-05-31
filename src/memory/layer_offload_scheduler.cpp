#include "layer_offload_scheduler.h"

#include <algorithm>

namespace RawrXD::Memory {

LayerOffloadScheduler::LayerOffloadScheduler(size_t vramBudgetBytes)
    : m_vramBudget(vramBudgetBytes) {}

void LayerOffloadScheduler::registerLayer(uint32_t idx, size_t bytes, LayerResidence initial) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (findLayer(idx) != nullptr) {
        return;
    }

    LayerDescriptor d{};
    d.layerIndex = idx;
    d.residence = initial;
    d.byteSize = bytes;
    d.lastAccessTick = ++m_tick;
    d.accessCount = 0;
    m_layers.push_back(d);
}

void LayerOffloadScheduler::touchLayer(uint32_t idx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (LayerDescriptor* d = findLayer(idx)) {
        d->lastAccessTick = ++m_tick;
        ++d->accessCount;
    }
}

std::vector<OffloadAction> LayerOffloadScheduler::schedule() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<OffloadAction> actions;
    size_t inUse = calcVramInUse();
    if (inUse <= m_vramBudget) {
        return actions;
    }

    // Coldest-first demotion policy when over VRAM budget.
    std::vector<LayerDescriptor*> vramLayers;
    vramLayers.reserve(m_layers.size());
    for (auto& l : m_layers) {
        if (l.residence == LayerResidence::VRAM) {
            vramLayers.push_back(&l);
        }
    }

    std::sort(vramLayers.begin(), vramLayers.end(), [](const LayerDescriptor* a, const LayerDescriptor* b) {
        if (a->accessCount != b->accessCount) {
            return a->accessCount < b->accessCount;
        }
        return a->lastAccessTick < b->lastAccessTick;
    });

    for (LayerDescriptor* l : vramLayers) {
        if (inUse <= m_vramBudget) {
            break;
        }

        const LayerResidence target = (l->byteSize > (32ull << 20))
            ? LayerResidence::NVMe
            : LayerResidence::SystemRAM;

        actions.push_back(OffloadAction{l->layerIndex, l->residence, target});
        l->residence = target;
        inUse -= l->byteSize;
    }

    return actions;
}

LayerResidence LayerOffloadScheduler::residenceOf(uint32_t idx) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (const LayerDescriptor* d = findLayer(idx)) {
        return d->residence;
    }
    return LayerResidence::SystemRAM;
}

void LayerOffloadScheduler::setVramBudget(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vramBudget = bytes;
}

size_t LayerOffloadScheduler::usedVram() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return calcVramInUse();
}

LayerDescriptor* LayerOffloadScheduler::findLayer(uint32_t idx) {
    auto it = std::find_if(m_layers.begin(), m_layers.end(), [idx](const LayerDescriptor& d) {
        return d.layerIndex == idx;
    });
    return it == m_layers.end() ? nullptr : &(*it);
}

const LayerDescriptor* LayerOffloadScheduler::findLayer(uint32_t idx) const {
    auto it = std::find_if(m_layers.begin(), m_layers.end(), [idx](const LayerDescriptor& d) {
        return d.layerIndex == idx;
    });
    return it == m_layers.end() ? nullptr : &(*it);
}

size_t LayerOffloadScheduler::calcVramInUse() const {
    size_t total = 0;
    for (const auto& l : m_layers) {
        if (l.residence == LayerResidence::VRAM) {
            total += l.byteSize;
        }
    }
    return total;
}

} // namespace RawrXD::Memory
