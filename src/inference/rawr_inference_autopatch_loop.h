#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace RawrXD::Inference {

struct TokenTelemetry {
    std::uint64_t tokenLatencyUs = 0;
    std::uint64_t committedRamBytes = 0;
    std::uint64_t committedVramBytes = 0;
    double cacheHitRate = 0.0;
    bool dispatchBound = false;
};

struct InferenceAutopatchConfig {
    std::size_t ringCapacity = 256;
    std::size_t tpsWindow = 64;
    std::size_t adaptEvery = 8;
    double panicTps = 2000.0;
    double targetTps = 4000.0;
    double headroomTps = 4400.0;
    double highPressure = 0.85;
};

enum class PatchAction {
    None = 0,
    EmergencyReset,
    EvictCold20,
    PrefetchDown,
    PrefetchUp,
    EnableKvCompression
};

struct PatchDecision {
    PatchAction action = PatchAction::None;
    std::uint32_t suggestedPrefetchDepth = 2;
    double rollingTps = 0.0;
    double rollingPressure = 0.0;
    bool cacheThrashing = false;
};

class InferenceAutopatchController {
  public:
    explicit InferenceAutopatchController(InferenceAutopatchConfig config = {});

    void onToken(const TokenTelemetry& token);
    bool shouldAdapt() const;
    PatchDecision adapt();

    std::uint64_t tokenCount() const { return tokenCount_; }
    std::uint32_t currentPrefetchDepth() const { return prefetchDepth_; }
    bool compressionEnabled() const { return compressionEnabled_; }

  private:
    static constexpr std::size_t kMaxRing = 256;

    struct Entry {
        std::uint64_t latencyUs;
        double pressure;
        double cacheHitRate;
        bool dispatchBound;
    };

    InferenceAutopatchConfig cfg_{};
    std::array<Entry, kMaxRing> ring_{};
    std::size_t writePos_ = 0;
    std::size_t validCount_ = 0;
    std::uint64_t tokenCount_ = 0;
    std::uint32_t prefetchDepth_ = 2;
    bool compressionEnabled_ = false;

    // Hysteresis / cooldown state
    std::uint64_t lastActionToken_ = 0;
    std::uint32_t cooldownTokens_ = 16; // minimum tokens between non-None actions
    PatchAction lastAction_ = PatchAction::None;

    [[nodiscard]] double computePressure(const TokenTelemetry& token) const;
    [[nodiscard]] double rollingTps(std::size_t window) const;
    [[nodiscard]] double rollingPressure(std::size_t window) const;
    [[nodiscard]] double rollingCacheHit(std::size_t window) const;
    [[nodiscard]] bool tpsDegrading(std::size_t window) const;

    [[nodiscard]] bool inCooldown() const;
    void recordAction(PatchAction action);
};

} // namespace RawrXD::Inference
