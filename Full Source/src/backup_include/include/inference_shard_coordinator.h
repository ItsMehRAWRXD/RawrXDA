// ============================================================================
// inference_shard_coordinator.h — X+5 Multi-GPU inference sharding
// ============================================================================
// Phase X+5: Distribute transformer layers across multiple GPUs (e.g. RX 7800 XT
// + second GPU) for 120B+ model inference. This header defines the integration
// point; implementation can live in core/ or win32app/ and may use
// DirectML/D3D12 adapter enumeration and GGUF-DML bridge.
//
// First milestone: enumerate adapters and report "ready for layer split".
// See docs/PHASE_X5_DISTRIBUTED_SWARM.md.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD
{

struct InferenceAdapterInfo
{
    uint32_t index = 0;
    std::string description;
    uint64_t dedicatedBytes = 0;
    bool inUse = false;
};

// Enumerate GPUs suitable for inference (DXGI/D3D12 adapters).
// Returns count of adapters; adapterInfos filled up to maxRequested.
int EnumerateInferenceAdapters(std::vector<InferenceAdapterInfo>* adapterInfos, int maxRequested = 8);

// Returns true if at least two adapters are available (ready for layer split).
bool IsMultiGpuInferenceReady();

}  // namespace RawrXD
