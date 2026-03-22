#pragma once

// ============================================================================
// model_runtime_gate.h — Generation stopwatch + 4-lane runtime budget (core)
// ============================================================================
// Tracks "model resident" vs "actively generating" and enforces an optional
// concurrent cap on core subsystems: Parse, Execute, Render, Memory.
// Extension lanes are reserved for explicitly loaded modules (separate bitmask).
// Set RAWRXD_STRICT_RUNTIME_LANES=1 to fail operations when the budget is full.
// ============================================================================

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace RawrXD::Runtime {

enum class SubsystemLane : std::uint32_t {
    Parse = 0,
    Execute = 1,
    Render = 2,
    Memory = 3,
    /// Module-loaded work (does not consume the four core slots by default)
    Extension = 4,
};

class ModelRuntimeGate {
public:
    static ModelRuntimeGate& instance();

    /// Model bytes resident in memory (mmap or heap); not equivalent to generating.
    void notifyModelResident(std::string_view modelKey, std::uint64_t bytes);
    void notifyModelUnloaded(std::string_view modelKey);

    bool isGenerating() const { return m_generating.load(std::memory_order_acquire); }
    std::uint64_t generationCount() const { return m_generationCount.load(std::memory_order_acquire); }

    /// Last completed generation wall time (ms). 0 if none finished yet.
    std::uint64_t lastGenerationDurationMs() const { return m_lastGenMs.load(std::memory_order_acquire); }

    bool strictLaneBudget() const;

    /// Returns true if lane acquired (or fail-open when not strict / extension lane).
    bool tryAcquireLane(SubsystemLane lane);
    void releaseLane(SubsystemLane lane);

    void beginGeneration(std::string_view backend, std::string_view modelKey);
    void endGeneration();

private:
    ModelRuntimeGate() = default;

    static constexpr std::uint32_t kCoreLaneMask = 0x0Fu; // bits 0..3
    static constexpr std::uint32_t kExtensionBit = 1u << 4;

    static std::uint32_t laneBit(SubsystemLane lane);

    std::atomic<bool> m_generating{false};
    std::atomic<std::uint64_t> m_generationCount{0};
    std::atomic<std::uint64_t> m_lastGenMs{0};

    std::chrono::steady_clock::time_point m_genStart{};
    std::mutex m_genMutex;

    std::atomic<std::uint32_t> m_laneMask{0};

    std::mutex m_residentMutex;
    std::string m_residentKey;
    std::uint64_t m_residentBytes{0};
};

/// RAII: beginGeneration / endGeneration
class GenerationScope {
public:
    GenerationScope(std::string_view backend, std::string_view modelKey);
    ~GenerationScope();

    GenerationScope(const GenerationScope&) = delete;
    GenerationScope& operator=(const GenerationScope&) = delete;

private:
    bool m_active{false};
};

/// RAII: tryAcquireLane / releaseLane
class LaneGuard {
public:
    explicit LaneGuard(SubsystemLane lane);
    ~LaneGuard();

    LaneGuard(const LaneGuard&) = delete;
    LaneGuard& operator=(const LaneGuard&) = delete;

    /// True if this scope may run (lane acquired, or fail-open when not strict).
    bool allowed() const { return m_allowed; }
    /// True only when a core/extension bit was actually taken (must release).
    bool ownsLane() const { return m_ownsLane; }

private:
    SubsystemLane m_lane;
    bool m_ownsLane{false};
    bool m_allowed{false};
};

} // namespace RawrXD::Runtime
