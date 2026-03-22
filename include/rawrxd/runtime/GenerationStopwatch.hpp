#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace RawrXD::Runtime {

/// Counts wall time only while the model is **generating tokens** (forward/token loop), not merely resident.
/// Distinct from `GenerationScope` in `model_runtime_gate.h` (gate + lanes); this aggregates cumulative ms.
class GenerationStopwatch {
public:
    static GenerationStopwatch& instance();

    void beginGeneration();
    void endGeneration();

    [[nodiscard]] bool generating() const {
        return m_generating.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t totalGenerationMs() const {
        return m_totalMs.load(std::memory_order_relaxed);
    }

private:
    GenerationStopwatch() = default;

    std::atomic<bool> m_generating{false};
    std::atomic<std::uint64_t> m_totalMs{0};
    std::chrono::steady_clock::time_point m_start{};
};

/// RAII: pairs with `GenerationStopwatch` only (not `ModelRuntimeGate::GenerationScope`).
class StopwatchGenerationScope {
public:
    StopwatchGenerationScope();
    ~StopwatchGenerationScope();

    StopwatchGenerationScope(const StopwatchGenerationScope&) = delete;
    StopwatchGenerationScope& operator=(const StopwatchGenerationScope&) = delete;

private:
    bool m_armed = true;
};

}  // namespace RawrXD::Runtime
