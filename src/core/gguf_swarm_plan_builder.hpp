#pragma once

#include <cstdint>
#include <vector>

#include "swarm_scheduler.hpp"

class RawrXDModelLoader;

namespace RawrXD::Swarm
{

/// Build file slices from the GGUF tensor table: tensors grouped by logical layer, sorted by offset,
/// then **coalesced** when the gap is ≤ min(\p mergeGapBytes, currentRun/64) (adaptive) and the merged
/// run stays ≤ 512 MiB (otherwise a new span starts). Larger file holes stay as separate spans.
///
/// **MoE (Mixtral / llama.cpp-style):** tensors whose names contain `ffn_experts.<i>` or
/// `mlp.experts.<i>` are classified as **expert** weights. They are **never** coalesced with
/// non-expert tensors, and each expert index is merged only with tensors for that same expert.
/// Resulting slices use `ModelSliceId::expertIndex` (0,1,…) for experts and `0xFFFFFFFF` for
/// static/shared weights (attention, norms, gating, embeddings, output).
///
/// Each span gets a distinct `ModelSliceId::planSpanOrdinal` within its `(layer, expertIndex)` group.
/// Returns empty if classification is too sparse — caller should fall back to a striped plan.
[[nodiscard]] std::vector<ModelSlice> buildLayerSlicesFromGGUF(const RawrXDModelLoader& loader, std::uint32_t n_layers,
                                                               std::uint64_t mergeGapBytes = 65536);

}  // namespace RawrXD::Swarm
