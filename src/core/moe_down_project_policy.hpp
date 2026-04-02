// Runtime policy: looped vs grouped+cached MoE down-project (FP32 reference / future hot path).
// Work proxy: D * H * K (element product). Thresholds are tunable via RawrXDTransformer::Config.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace RawrXD
{
namespace MoEDownProject
{

enum class Path : std::uint8_t
{
    Looped = 0,
    GroupedCached = 1,
};

struct PolicyParams
{
    /// If workProduct(in,H,K) is below this, prefer looped (small shapes, cache-friendly experts).
    double workThreshold = 1e6;
    /// When work is large enough, require at least this expected reuse (tokens / pack generations) to pick
    /// grouped+cache.
    double minReuseForGrouped = 8.0;
};

[[nodiscard]] inline double workProduct(std::size_t inDim, std::size_t outDim, int numExperts) noexcept
{
    const std::int64_t k = static_cast<std::int64_t>(std::max(1, numExperts));
    return static_cast<double>(inDim) * static_cast<double>(outDim) * static_cast<double>(k);
}

/// \p expectedReuse: estimate from heatmap / residency / repeat_same_experts telemetry; use config default when
/// unknown.
[[nodiscard]] inline Path choosePath(std::size_t inDim, std::size_t outDim, int numExperts, double expectedReuse,
                                     const PolicyParams& p = {}) noexcept
{
    if (numExperts <= 0 || inDim == 0 || outDim == 0)
        return Path::Looped;
    const double w = workProduct(inDim, outDim, numExperts);
    if (w < p.workThreshold)
        return Path::Looped;
    if (expectedReuse >= p.minReuseForGrouped)
        return Path::GroupedCached;
    return Path::Looped;
}

}  // namespace MoEDownProject
}  // namespace RawrXD
