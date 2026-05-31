#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <functional>

namespace RawrXD::Memory {

struct CheckpointSegment {
    uint32_t layerIndex;
    bool     saved;       // activation saved; else must recompute
    size_t   bytes;
};

// Gradient/activation checkpointing: saves only every N layers' activations.
// Layers in between are re-computed on backward pass to trade FLOPs for memory.
class GradientCheckpointManager {
public:
    // checkpointInterval — save every Nth layer; recompute the rest.
    explicit GradientCheckpointManager(uint32_t totalLayers,
                                        uint32_t checkpointInterval = 2);

    // Called at forward pass to decide whether to save activation for layerIdx.
    bool shouldSave(uint32_t layerIdx) const;

    // Called at backward pass; returns list of layers needing recomputation
    // before reaching targetLayer.
    std::vector<uint32_t> recomputePathTo(uint32_t targetLayer) const;

    // Register activation size for accounting.
    void recordActivation(uint32_t layerIdx, size_t bytes);

    size_t savedMemoryBytes() const;     // bytes currently held in checkpoints
    size_t skippedMemoryBytes() const;   // bytes saved by NOT storing
    void   reset();

private:
    uint32_t                       m_totalLayers;
    uint32_t                       m_interval;
    mutable std::mutex             m_mutex;
    std::vector<CheckpointSegment> m_segments;
};

} // namespace RawrXD::Memory
