#pragma once
#include <stdint.h>
#include <atomic>

namespace RawrXD {

enum class ExecutionPhase : uint8_t {
    IDLE = 0,
    SPECULATIVE_DRAFT = 1,
    VULKAN_VERIFY = 2,
    MEMORY_RECLAIM = 3,
    HUD_RENDER = 4,
    IO_FLUSH = 5
};

struct TimelineEvent {
    uint64_t start_tsc;
    uint64_t end_tsc;
    ExecutionPhase phase;
    uint8_t thread_id;
    uint16_t metadata; // e.g. token index or pressure level
};

// Circular buffer for cycle-accurate tracing
constexpr int MAX_TIMELINE_EVENTS = 1024;

struct ExecutionTimeline {
    std::atomic<uint64_t> event_cursor{0};
    TimelineEvent events[MAX_TIMELINE_EVENTS];

    static ExecutionTimeline& Get() {
        static ExecutionTimeline instance;
        return instance;
    }

    inline void Record(ExecutionPhase phase, uint64_t start, uint64_t end, uint16_t meta = 0) {
        uint64_t idx = event_cursor.fetch_add(1, std::memory_order_relaxed) % MAX_TIMELINE_EVENTS;
        events[idx] = { start, end, phase, 0, meta };
    }
};

struct TimelineScope {
    ExecutionPhase p;
    uint64_t s;
    uint16_t m;
    TimelineScope(ExecutionPhase phase, uint16_t meta = 0) : p(phase), m(meta) {
        s = __rdtsc();
    }
    ~TimelineScope() {
        ExecutionTimeline::Get().Record(p, s, __rdtsc(), m);
    }
};

} // namespace RawrXD
