#include "rawr_inference_autopatch_loop.h"

#include <algorithm>

namespace RawrXD::Inference {

InferenceAutopatchController::InferenceAutopatchController(InferenceAutopatchConfig config) : cfg_(config)
{
    cfg_.ringCapacity = std::min<std::size_t>(cfg_.ringCapacity, kMaxRing);
    cfg_.tpsWindow = std::max<std::size_t>(8, std::min<std::size_t>(cfg_.tpsWindow, cfg_.ringCapacity));
    cfg_.adaptEvery = std::max<std::size_t>(1, cfg_.adaptEvery);
}

void InferenceAutopatchController::onToken(const TokenTelemetry& token)
{
    const Entry entry{
        std::max<std::uint64_t>(1, token.tokenLatencyUs),
        computePressure(token),
        token.cacheHitRate,
        token.dispatchBound,
    };

    ring_[writePos_] = entry;
    writePos_ = (writePos_ + 1) % cfg_.ringCapacity;
    validCount_ = std::min<std::size_t>(validCount_ + 1, cfg_.ringCapacity);
    ++tokenCount_;
}

bool InferenceAutopatchController::shouldAdapt() const
{
    if (validCount_ < cfg_.adaptEvery) {
        return false;
    }
    return (tokenCount_ % cfg_.adaptEvery) == 0;
}

bool InferenceAutopatchController::inCooldown() const
{
    if (lastAction_ == PatchAction::None)
        return false;
    return (tokenCount_ - lastActionToken_) < cooldownTokens_;
}

void InferenceAutopatchController::recordAction(PatchAction action)
{
    lastAction_ = action;
    lastActionToken_ = tokenCount_;
}

PatchDecision InferenceAutopatchController::adapt()
{
    PatchDecision out{};
    out.rollingTps = rollingTps(cfg_.tpsWindow);
    out.rollingPressure = rollingPressure(cfg_.tpsWindow);

    const double hit = rollingCacheHit(cfg_.tpsWindow);
    const bool degrading = tpsDegrading(cfg_.tpsWindow);
    const bool cacheThrash = hit > 0.0 && hit < 0.50;
    out.cacheThrashing = cacheThrash;

    // Hierarchical signal prioritization (not parallel evaluation)
    // Tier 1: Panic — always immediate, bypasses cooldown
    if (out.rollingTps < cfg_.panicTps) {
        prefetchDepth_ = 1;
        compressionEnabled_ = true;
        out.action = PatchAction::EmergencyReset;
        out.suggestedPrefetchDepth = prefetchDepth_;
        recordAction(out.action);
        return out;
    }

    // Tier 2: Cooldown gate — suppress oscillation for non-critical actions
    if (inCooldown()) {
        out.action = PatchAction::None;
        out.suggestedPrefetchDepth = prefetchDepth_;
        return out;
    }

    // Tier 3: Pressure-dominant path (memory is the bottleneck)
    if (out.rollingPressure > cfg_.highPressure) {
        if (out.rollingTps < cfg_.targetTps && degrading) {
            compressionEnabled_ = true;
            out.action = PatchAction::EvictCold20;
            recordAction(out.action);
            out.suggestedPrefetchDepth = prefetchDepth_;
            return out;
        }
    }

    // Tier 4: Cache-dominant path (thrashing, not bandwidth)
    if (cacheThrash) {
        if (out.rollingTps < cfg_.targetTps && degrading) {
            prefetchDepth_ = std::max<std::uint32_t>(1, prefetchDepth_ - 1);
            out.action = PatchAction::PrefetchDown;
            recordAction(out.action);
            out.suggestedPrefetchDepth = prefetchDepth_;
            return out;
        }
    }

    // Tier 5: Dispatch-dominant path (GPU starved, increase overlap)
    if (out.rollingTps < cfg_.targetTps && degrading) {
        prefetchDepth_ = std::min<std::uint32_t>(8, prefetchDepth_ + 2);
        out.action = PatchAction::PrefetchUp;
        recordAction(out.action);
        out.suggestedPrefetchDepth = prefetchDepth_;
        return out;
    }

    // Tier 6: Headroom optimization (safe to increase)
    if (out.rollingTps > cfg_.headroomTps && !degrading) {
        prefetchDepth_ = std::min<std::uint32_t>(8, prefetchDepth_ + 1);
        out.action = PatchAction::PrefetchUp;
        recordAction(out.action);
        out.suggestedPrefetchDepth = prefetchDepth_;
        return out;
    }

    out.action = PatchAction::None;
    out.suggestedPrefetchDepth = prefetchDepth_;
    return out;
}

double InferenceAutopatchController::computePressure(const TokenTelemetry& token) const
{
    // Approximate memory pressure in [0, +inf) from committed usage against 64G RAM + 16G VRAM reference.
    constexpr double kRamBudget = 64.0 * 1024.0 * 1024.0 * 1024.0;
    constexpr double kVramBudget = 16.0 * 1024.0 * 1024.0 * 1024.0;
    const double ram = static_cast<double>(token.committedRamBytes) / kRamBudget;
    const double vram = static_cast<double>(token.committedVramBytes) / kVramBudget;
    return std::max(0.0, (ram * 0.65) + (vram * 0.35));
}

double InferenceAutopatchController::rollingTps(std::size_t window) const
{
    if (validCount_ == 0) {
        return 0.0;
    }

    const std::size_t count = std::min(window, validCount_);
    std::uint64_t sumUs = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t idx = (writePos_ + cfg_.ringCapacity - 1 - i) % cfg_.ringCapacity;
        sumUs += ring_[idx].latencyUs;
    }

    if (sumUs == 0) {
        return 0.0;
    }
    return static_cast<double>(count) * 1'000'000.0 / static_cast<double>(sumUs);
}

double InferenceAutopatchController::rollingPressure(std::size_t window) const
{
    if (validCount_ == 0) {
        return 0.0;
    }

    const std::size_t count = std::min(window, validCount_);
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t idx = (writePos_ + cfg_.ringCapacity - 1 - i) % cfg_.ringCapacity;
        sum += ring_[idx].pressure;
    }
    return sum / static_cast<double>(count);
}

double InferenceAutopatchController::rollingCacheHit(std::size_t window) const
{
    if (validCount_ == 0) {
        return 0.0;
    }

    const std::size_t count = std::min(window, validCount_);
    double sum = 0.0;
    std::size_t samples = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t idx = (writePos_ + cfg_.ringCapacity - 1 - i) % cfg_.ringCapacity;
        const double hit = ring_[idx].cacheHitRate;
        if (hit >= 0.0 && hit <= 1.0) {
            sum += hit;
            ++samples;
        }
    }

    if (samples == 0) {
        return 0.0;
    }
    return sum / static_cast<double>(samples);
}

bool InferenceAutopatchController::tpsDegrading(std::size_t window) const
{
    if (validCount_ < (window * 2)) {
        return false;
    }

    const std::size_t half = window / 2;
    if (half == 0) {
        return false;
    }

    std::uint64_t newerUs = 0;
    std::uint64_t olderUs = 0;

    for (std::size_t i = 0; i < half; ++i) {
        const std::size_t nIdx = (writePos_ + cfg_.ringCapacity - 1 - i) % cfg_.ringCapacity;
        const std::size_t oIdx = (writePos_ + cfg_.ringCapacity - 1 - (i + half)) % cfg_.ringCapacity;
        newerUs += ring_[nIdx].latencyUs;
        olderUs += ring_[oIdx].latencyUs;
    }

    if (newerUs == 0 || olderUs == 0) {
        return false;
    }

    const double newerTps = static_cast<double>(half) * 1'000'000.0 / static_cast<double>(newerUs);
    const double olderTps = static_cast<double>(half) * 1'000'000.0 / static_cast<double>(olderUs);
    return newerTps < (olderTps * 0.96);
}

} // namespace RawrXD::Inference
