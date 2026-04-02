#include "gguf_swarm_plan_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../rawrxd_model_loader.h"

namespace RawrXD::Swarm
{
namespace
{

[[nodiscard]] int parseLeadingUInt(const std::string& name, std::size_t digitStart)
{
    if (digitStart >= name.size())
        return -1;
    if (!std::isdigit(static_cast<unsigned char>(name[digitStart])))
        return -1;
    int v = 0;
    for (std::size_t i = digitStart; i < name.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (!std::isdigit(c))
            break;
        if (v > 1'000'000)
            return -1;
        v = v * 10 + static_cast<int>(c - '0');
    }
    return v;
}

/// Returns layer bucket 0 .. n_layers-1 for block tensors, or n_layers for output head / final norm.
/// Returns -1 if unknown (ignored for plan).
[[nodiscard]] int classifyTensorLayer(const std::string& name, std::uint32_t n_layers)
{
    const std::uint32_t nl = n_layers;

    if (name.find("token_embd") != std::string::npos)
        return 0;
    if (name.find("tok_embd") != std::string::npos)
        return 0;
    if (name == "embeddings.weight")
        return 0;
    if (name.find("model.embed_tokens") != std::string::npos)
        return 0;

    if (name == "norm.weight")
        return static_cast<int>(nl);
    if (name.find("output_norm") != std::string::npos)
        return static_cast<int>(nl);
    if (name == "output.weight" || name == "output_norm.weight")
        return static_cast<int>(nl);
    if (name.size() >= 7 && name.compare(0, 7, "output.") == 0)
        return static_cast<int>(nl);
    if (name.find("lm_head") != std::string::npos)
        return static_cast<int>(nl);

    if (name.size() >= 4 && name.compare(0, 4, "blk.") == 0)
    {
        const int li = parseLeadingUInt(name, 4);
        if (li < 0 || static_cast<std::uint32_t>(li) >= nl)
            return -1;
        return li;
    }
    if (name.size() >= 7 && name.compare(0, 7, "layers.") == 0)
    {
        const int li = parseLeadingUInt(name, 7);
        if (li < 0 || static_cast<std::uint32_t>(li) >= nl)
            return -1;
        return li;
    }
    if (name.size() >= 13 && name.compare(0, 13, "model.layers.") == 0)
    {
        const int li = parseLeadingUInt(name, 13);
        if (li < 0 || static_cast<std::uint32_t>(li) >= nl)
            return -1;
        return li;
    }

    return -1;
}

/// If the tensor name is a per-expert MoE weight (llama.cpp `ffn_experts.N` or HF-style `mlp.experts.N`),
/// returns N. Otherwise nullopt (treat as static / shared for that layer).
[[nodiscard]] std::optional<std::uint32_t> tryParseMoEExpertOrdinal(const std::string& name)
{
    constexpr int kMaxExpertOrdinal = 65536;

    const char* markers[] = {"ffn_experts.", "mlp.experts."};
    for (const char* marker : markers)
    {
        const std::size_t p = name.find(marker);
        if (p == std::string::npos)
            continue;
        const std::size_t digitStart = p + std::strlen(marker);
        const int ei = parseLeadingUInt(name, digitStart);
        if (ei >= 0 && ei <= kMaxExpertOrdinal)
            return static_cast<std::uint32_t>(ei);
    }
    return std::nullopt;
}

struct LayerTensorSplit
{
    std::vector<TensorFileSpan> staticSpans;
    std::map<std::uint32_t, std::vector<TensorFileSpan>> expertSpans;
};

/// Cap on a **merged** run; exceeding it starts a new plan span (single tensors may still exceed this).
constexpr std::uint64_t kSoftMaxMergedSliceBytes = 512ull * 1024ull * 1024ull;

/// Sorted non-empty tensor spans → merged [start,end) ranges; clip to file size.
[[nodiscard]] std::vector<std::pair<std::uint64_t, std::uint64_t>> mergeCoalescedRanges(
    std::vector<TensorFileSpan> spans, std::uint64_t mergeGapBytes, std::uint64_t fileSz)
{
    if (spans.empty())
        return {};

    std::sort(spans.begin(), spans.end(),
              [](const TensorFileSpan& a, const TensorFileSpan& b)
              {
                  if (a.fileOffset != b.fileOffset)
                      return a.fileOffset < b.fileOffset;
                  return a.sizeBytes < b.sizeBytes;
              });

    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::uint64_t curStart = spans[0].fileOffset;
    std::uint64_t curEnd = spans[0].fileOffset + spans[0].sizeBytes;

    for (std::size_t i = 1; i < spans.size(); ++i)
    {
        const auto& t = spans[i];
        if (t.sizeBytes == 0)
            continue;
        const std::uint64_t tEnd = t.fileOffset + t.sizeBytes;
        if (tEnd < t.fileOffset)
            continue;

        const std::uint64_t runLen = curEnd - curStart;
        const std::uint64_t dynamicGap = std::min(mergeGapBytes, runLen >> 6);

        if (t.fileOffset <= curEnd + dynamicGap)
        {
            const std::uint64_t newEnd = std::max(curEnd, tEnd);
            if (newEnd - curStart > kSoftMaxMergedSliceBytes)
            {
                ranges.push_back({curStart, curEnd});
                curStart = t.fileOffset;
                curEnd = tEnd;
            }
            else
                curEnd = newEnd;
        }
        else
        {
            ranges.push_back({curStart, curEnd});
            curStart = t.fileOffset;
            curEnd = tEnd;
        }
    }
    ranges.push_back({curStart, curEnd});

    for (auto& pr : ranges)
    {
        if (pr.first >= fileSz)
        {
            pr.second = pr.first;
            continue;
        }
        pr.second = std::min(pr.second, fileSz);
    }

    ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
                                [](const std::pair<std::uint64_t, std::uint64_t>& p) { return p.second <= p.first; }),
                 ranges.end());

    return ranges;
}

void appendMergedSlices(std::vector<ModelSlice>& plan, std::uint32_t layerBucket,
                        const std::vector<std::pair<std::uint64_t, std::uint64_t>>& merged, std::uint32_t expertField)
{
    for (std::uint32_t ord = 0; ord < static_cast<std::uint32_t>(merged.size()); ++ord)
    {
        const std::uint64_t mn = merged[ord].first;
        const std::uint64_t mx = merged[ord].second;
        if (mx <= mn)
            continue;

        ModelSlice s{};
        s.id.modelIndex = 0;
        s.id.layerStart = layerBucket;
        s.id.layerEnd = layerBucket + 1u;
        s.id.expertIndex = expertField;
        s.id.planSpanOrdinal = ord;
        s.fileOffsetBytes = mn;
        s.byteSize = mx - mn;
        if (expertField == 0xFFFFFFFFu)
            s.debugName =
                "gguf L" + std::to_string(static_cast<int>(layerBucket)) + " static span" + std::to_string(ord);
        else
            s.debugName = "gguf L" + std::to_string(static_cast<int>(layerBucket)) + " expert" +
                          std::to_string(expertField) + " span" + std::to_string(ord);
        plan.push_back(std::move(s));
    }
}

}  // namespace

std::vector<ModelSlice> buildLayerSlicesFromGGUF(const RawrXDModelLoader& loader, std::uint32_t n_layers,
                                                 std::uint64_t mergeGapBytes)
{
    if (n_layers == 0)
        return {};

    const std::uint64_t fileSz = loader.GetFileSizeBytes();
    if (fileSz == 0)
        return {};

    const std::vector<TensorFileSpan> spans = loader.listTensorFileSpans();
    if (spans.empty())
        return {};

    const std::size_t numBuckets = static_cast<std::size_t>(n_layers) + 1u;
    std::vector<LayerTensorSplit> byLayer(numBuckets);

    for (const TensorFileSpan& sp : spans)
    {
        const int L = classifyTensorLayer(sp.name, n_layers);
        if (L < 0 || static_cast<std::size_t>(L) >= numBuckets)
            continue;
        if (sp.sizeBytes == 0)
            continue;
        const std::uint64_t end = sp.fileOffset + sp.sizeBytes;
        if (end < sp.fileOffset)
            continue;

        LayerTensorSplit& bucket = byLayer[static_cast<std::size_t>(L)];
        if (const std::optional<std::uint32_t> expertOrd = tryParseMoEExpertOrdinal(sp.name))
            bucket.expertSpans[*expertOrd].push_back(sp);
        else
            bucket.staticSpans.push_back(sp);
    }

    std::uint32_t blockLayersHit = 0;
    for (std::uint32_t L = 0; L < n_layers; ++L)
    {
        const LayerTensorSplit& b = byLayer[static_cast<std::size_t>(L)];
        if (!b.staticSpans.empty() || !b.expertSpans.empty())
            ++blockLayersHit;
    }

    const std::uint32_t minRequired = std::max(1u, n_layers / 4u);
    if (blockLayersHit < minRequired || blockLayersHit < n_layers)
        return {};

    std::vector<ModelSlice> plan;
    plan.reserve(static_cast<std::size_t>(n_layers) * 8u + 32u);

    for (std::uint32_t L = 0; L <= n_layers; ++L)
    {
        LayerTensorSplit& bucket = byLayer[static_cast<std::size_t>(L)];
        if (bucket.staticSpans.empty() && bucket.expertSpans.empty())
            continue;

        if (!bucket.staticSpans.empty())
        {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> merged =
                mergeCoalescedRanges(std::move(bucket.staticSpans), mergeGapBytes, fileSz);
            appendMergedSlices(plan, L, merged, 0xFFFFFFFFu);
        }

        for (auto& kv : bucket.expertSpans)
        {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> merged =
                mergeCoalescedRanges(std::move(kv.second), mergeGapBytes, fileSz);
            appendMergedSlices(plan, L, merged, kv.first);
        }
    }

    std::sort(plan.begin(), plan.end(),
              [](const ModelSlice& a, const ModelSlice& b)
              {
                  if (a.id.layerStart != b.id.layerStart)
                      return a.id.layerStart < b.id.layerStart;
                  const bool aStatic = (a.id.expertIndex == 0xFFFFFFFFu);
                  const bool bStatic = (b.id.expertIndex == 0xFFFFFFFFu);
                  if (aStatic != bStatic)
                      return aStatic;
                  if (!aStatic && !bStatic && a.id.expertIndex != b.id.expertIndex)
                      return a.id.expertIndex < b.id.expertIndex;
                  if (a.id.planSpanOrdinal != b.id.planSpanOrdinal)
                      return a.id.planSpanOrdinal < b.id.planSpanOrdinal;
                  return a.fileOffsetBytes < b.fileOffsetBytes;
              });

    return plan;
}

}  // namespace RawrXD::Swarm
