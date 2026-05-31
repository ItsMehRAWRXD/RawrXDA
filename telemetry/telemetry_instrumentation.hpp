// telemetry_instrumentation.hpp
// Lightweight phase latency tracker for v1.0.0-gold baseline capture.
// Include in ExecutionScheduler and CompletionBridge for ETW-compatible logging.
// Qt-free, header-only, zero dependencies beyond <chrono> and <atomic>.

#pragma once
#include <chrono>
#include <atomic>
#include <cstdint>
#include <array>
#include <string_view>

namespace rawrxd::telemetry {

enum class Phase : uint8_t {
    PREFETCH_COMPLETION = 0,
    INFERENCE           = 1,
    COMMIT              = 2,
    COUNT               = 3
};

struct PhaseSample {
    double elapsed_ms{0.0};
    uint64_t timestamp_ns{0};
};

class PhaseLatencyTracker {
public:
    static constexpr size_t MAX_SAMPLES = 1024;

    void begin(Phase p) noexcept {
        m_start[static_cast<size_t>(p)].store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed
        );
    }

    void end(Phase p) noexcept {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto start = m_start[static_cast<size_t>(p)].load(std::memory_order_relaxed);
        double ms = static_cast<double>(now - start) / 1'000'000.0;

        size_t idx = m_head[static_cast<size_t>(p)].fetch_add(1, std::memory_order_relaxed) % MAX_SAMPLES;
        m_samples[static_cast<size_t>(p)][idx] = {ms, static_cast<uint64_t>(now)};
    }

    // Export to JSON-like buffer for telemetry harness consumption
    void export_samples(char* out, size_t out_len) const noexcept {
        // Minimal JSON serialization; caller provides large enough buffer
        int n = snprintf(out, out_len, R"({"phases":[)");
        for (size_t p = 0; p < static_cast<size_t>(Phase::COUNT) && n < static_cast<int>(out_len); ++p) {
            n += snprintf(out + n, out_len - n, "%s{\"id\":%zu,\"samples\":[", (p ? "," : ""));
            size_t head = m_head[p].load(std::memory_order_relaxed);
            for (size_t i = 0; i < std::min(head, MAX_SAMPLES) && n < static_cast<int>(out_len); ++i) {
                const auto& s = m_samples[p][i];
                n += snprintf(out + n, out_len - n, "%s%.3f", (i ? "," : ""), s.elapsed_ms);
            }
            n += snprintf(out + n, out_len - n, "]}");
        }
        snprintf(out + n, out_len - n, "]}");
    }

private:
    std::array<std::atomic<int64_t>, static_cast<size_t>(Phase::COUNT)> m_start{};
    std::array<std::atomic<size_t>, static_cast<size_t>(Phase::COUNT)> m_head{};
    std::array<std::array<PhaseSample, MAX_SAMPLES>, static_cast<size_t>(Phase::COUNT)> m_samples{};
};

// Global singleton for cross-component access without passing pointers everywhere
inline PhaseLatencyTracker& global_tracker() noexcept {
    static PhaseLatencyTracker instance;
    return instance;
}

} // namespace rawrxd::telemetry
