#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "SovereignQueue.h"
#include "RequestTimeline.h"

namespace rawrxd::orchestration {

struct OrchestrationGapSample {
    std::uint64_t request_id = 0;
    double ttft_engine_us = 0.0;
    double ttft_ui_us = 0.0;
    double context_to_first_pulse_ready_us = 0.0;
    double search_us = 0.0;
    double pulse_us = 0.0;
    double prefill_us = 0.0;
    double gap_us = 0.0;
    double gap_pct = 0.0;
};

struct OrchestrationAggregate {
    std::uint64_t total_requests = 0;
    double avg_ttft_engine_us = 0.0;
    double p95_ttft_engine_us = 0.0;
    double avg_gap_us = 0.0;
    double p95_gap_us = 0.0;
};

class MetricsThread {
public:
    MetricsThread();
    ~MetricsThread();

    bool Start();
    void Stop();

    bool Submit(const RequestTimeline& timeline) noexcept;

    OrchestrationAggregate GetAggregateSnapshot() const;
    std::vector<OrchestrationGapSample> GetRecentSamples(std::size_t maxCount = 128) const;

private:
    void WorkerLoop();
    void ConsumeOne(const RequestTimeline& timeline);

    static double Percentile95(std::vector<double> values);

private:
    static constexpr std::size_t kQueueCapacity = 4096;
    static constexpr std::size_t kMaxRecentSamples = 4096;

    SovereignQueue<RequestTimeline, kQueueCapacity> m_queue;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    mutable std::mutex m_guard;
    std::vector<OrchestrationGapSample> m_recent;
    OrchestrationAggregate m_aggregate;
};

} // namespace rawrxd::orchestration
