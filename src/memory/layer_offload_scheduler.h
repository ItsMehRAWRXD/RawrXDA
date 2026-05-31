#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <functional>

namespace RawrXD::Memory {

enum class LayerResidence : uint8_t { VRAM = 0, SystemRAM, NVMe };

struct LayerDescriptor {
    uint32_t       layerIndex;
    LayerResidence residence;
    size_t         byteSize;
    uint64_t       lastAccessTick;
    uint32_t       accessCount;
};

struct OffloadAction {
    uint32_t       layerIndex;
    LayerResidence from;
    LayerResidence to;
};

// Dynamically schedules layer offload/promotion across VRAM↔RAM↔NVMe
// based on access frequency and a configurable VRAM budget.
class LayerOffloadScheduler {
public:
    explicit LayerOffloadScheduler(size_t vramBudgetBytes);

    void registerLayer(uint32_t idx, size_t bytes, LayerResidence initial = LayerResidence::VRAM);
    void touchLayer(uint32_t idx);                             // record access at current tick
    std::vector<OffloadAction> schedule();                     // returns actions to execute
    LayerResidence residenceOf(uint32_t idx) const;
    void setVramBudget(size_t bytes);
    size_t usedVram() const;

private:
    mutable std::mutex          m_mutex;
    size_t                      m_vramBudget;
    uint64_t                    m_tick{0};
    std::vector<LayerDescriptor> m_layers;

    LayerDescriptor* findLayer(uint32_t idx);
    const LayerDescriptor* findLayer(uint32_t idx) const;
    size_t calcVramInUse() const;
};

} // namespace RawrXD::Memory
