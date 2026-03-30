#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rawrxd::orchestration {

enum class StageStamp : std::uint8_t {
    RequestReceived = 0,
    SearchStart,
    FirstContextChunkReady,
    SearchDone,
    PulseStart,
    FirstPulseChunkReady,
    PulseDone,
    PrefillSubmitStart,
    PrefillDone,
    DecodeStart,
    FirstTokenReady,
    FirstTokenEmittedUi,
    RequestDone,
    Count
};

inline constexpr std::size_t kStageCount = static_cast<std::size_t>(StageStamp::Count);

struct alignas(64) RequestTimeline {
    std::uint64_t request_id = 0;

    std::array<std::uint64_t, kStageCount> qpc_ticks{};
    std::array<std::uint64_t, kStageCount> tsc_cycles{};
    std::array<std::uint32_t, kStageCount> thread_id{};

    std::uint64_t qpc_frequency = 0;

    std::size_t context_bytes = 0;
    std::size_t tokens_generated = 0;
    std::uint32_t stall_count = 0;

    std::uint64_t stall_queue_full_us = 0;
    std::uint64_t stall_queue_empty_us = 0;
    std::uint64_t stall_backend_wait_us = 0;
    std::uint64_t stall_kv_miss_us = 0;

    std::uint32_t swarm_divergence_events = 0;
    std::uint32_t swarm_last_divergence_token = 0;
    double swarm_max_disagreement_score = 0.0;

    std::uint32_t error_code = 0;
    std::uint32_t flags = 0;
};

static_assert(std::is_standard_layout_v<RequestTimeline>);

inline std::uint64_t ReadQpc() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER now{};
    ::QueryPerformanceCounter(&now);
    return static_cast<std::uint64_t>(now.QuadPart);
#else
    return 0;
#endif
}

inline std::uint64_t ReadQpcFrequency() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER freq{};
    ::QueryPerformanceFrequency(&freq);
    return static_cast<std::uint64_t>(freq.QuadPart);
#else
    return 0;
#endif
}

inline std::uint64_t ReadTsc() noexcept {
#if defined(_MSC_VER) && defined(_M_X64)
    return __rdtsc();
#else
    return 0;
#endif
}

inline std::uint32_t ReadThreadId() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint32_t>(::GetCurrentThreadId());
#else
    return 0;
#endif
}

inline void Stamp(RequestTimeline& timeline, StageStamp stamp) noexcept {
    const std::size_t idx = static_cast<std::size_t>(stamp);
    timeline.qpc_ticks[idx] = ReadQpc();
    timeline.tsc_cycles[idx] = ReadTsc();
    timeline.thread_id[idx] = ReadThreadId();
}

inline double TicksToUs(std::uint64_t ticks, std::uint64_t frequency) noexcept {
    if (frequency == 0) {
        return 0.0;
    }
    return (static_cast<double>(ticks) * 1'000'000.0) / static_cast<double>(frequency);
}

inline double SpanUs(const RequestTimeline& t, StageStamp begin, StageStamp end) noexcept {
    const std::size_t b = static_cast<std::size_t>(begin);
    const std::size_t e = static_cast<std::size_t>(end);
    if (e >= kStageCount || b >= kStageCount || t.qpc_ticks[e] < t.qpc_ticks[b]) {
        return 0.0;
    }
    return TicksToUs(t.qpc_ticks[e] - t.qpc_ticks[b], t.qpc_frequency);
}

} // namespace rawrxd::orchestration
