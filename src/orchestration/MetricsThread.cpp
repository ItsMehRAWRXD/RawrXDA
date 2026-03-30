#include "../../include/orchestration/MetricsThread.h"

#include <algorithm>
#include <chrono>

namespace rawrxd::orchestration {

MetricsThread::MetricsThread() = default;

MetricsThread::~MetricsThread() {
    Stop();
}

bool MetricsThread::Start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }

    m_thread = std::thread([this]() { WorkerLoop(); });
    return true;
}

void MetricsThread::Stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool MetricsThread::Submit(const RequestTimeline& timeline) noexcept {
    return m_queue.WaitPush(timeline, 128, 1);
}

OrchestrationAggregate MetricsThread::GetAggregateSnapshot() const {
    std::lock_guard<std::mutex> lock(m_guard);
    return m_aggregate;
}

std::vector<OrchestrationGapSample> MetricsThread::GetRecentSamples(std::size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_guard);
    if (maxCount == 0 || m_recent.empty()) {
        return {};
    }

    const std::size_t count = std::min(maxCount, m_recent.size());
    return std::vector<OrchestrationGapSample>(m_recent.end() - static_cast<std::ptrdiff_t>(count), m_recent.end());
}

void MetricsThread::WorkerLoop() {
    while (m_running.load(std::memory_order_acquire) || !m_queue.Empty()) {
        RequestTimeline timeline{};
        if (!m_queue.WaitPop(timeline, 128, 1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        ConsumeOne(timeline);
    }
}

void MetricsThread::ConsumeOne(const RequestTimeline& t) {
    OrchestrationGapSample s{};
    s.request_id = t.request_id;

    s.ttft_engine_us = SpanUs(t, StageStamp::RequestReceived, StageStamp::FirstTokenReady);
    if (s.ttft_engine_us <= 0.0) {
        s.ttft_engine_us = SpanUs(t, StageStamp::RequestReceived, StageStamp::RequestDone);
    }

    s.ttft_ui_us = SpanUs(t, StageStamp::RequestReceived, StageStamp::FirstTokenEmittedUi);
    if (s.ttft_ui_us <= 0.0) {
        s.ttft_ui_us = SpanUs(t, StageStamp::RequestReceived, StageStamp::RequestDone);
    }
    s.context_to_first_pulse_ready_us = SpanUs(t, StageStamp::FirstContextChunkReady, StageStamp::FirstPulseChunkReady);
    s.search_us = SpanUs(t, StageStamp::SearchStart, StageStamp::SearchDone);
    s.pulse_us = SpanUs(t, StageStamp::PulseStart, StageStamp::PulseDone);
    s.prefill_us = SpanUs(t, StageStamp::PrefillSubmitStart, StageStamp::PrefillDone);

    s.gap_us = s.ttft_engine_us - (s.search_us + s.pulse_us + s.prefill_us);
    if (s.gap_us < 0.0) {
        s.gap_us = 0.0;
    }
    s.gap_pct = (s.ttft_engine_us > 0.0) ? ((s.gap_us / s.ttft_engine_us) * 100.0) : 0.0;

    std::lock_guard<std::mutex> lock(m_guard);

    if (m_recent.size() >= kMaxRecentSamples) {
        m_recent.erase(m_recent.begin());
    }
    m_recent.push_back(s);

    std::vector<double> ttft;
    std::vector<double> gap;
    ttft.reserve(m_recent.size());
    gap.reserve(m_recent.size());
    for (const auto& sample : m_recent) {
        ttft.push_back(sample.ttft_engine_us);
        gap.push_back(sample.gap_us);
    }

    const auto avg = [](const std::vector<double>& v) {
        if (v.empty()) {
            return 0.0;
        }
        double sum = 0.0;
        for (double x : v) {
            sum += x;
        }
        return sum / static_cast<double>(v.size());
    };

    m_aggregate.total_requests += 1;
    m_aggregate.avg_ttft_engine_us = avg(ttft);
    m_aggregate.p95_ttft_engine_us = Percentile95(ttft);
    m_aggregate.avg_gap_us = avg(gap);
    m_aggregate.p95_gap_us = Percentile95(gap);
}

double MetricsThread::Percentile95(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const double rank = 0.95 * static_cast<double>(values.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(rank);
    const std::size_t hi = std::min(lo + 1, values.size() - 1);
    const double frac = rank - static_cast<double>(lo);
    return values[lo] * (1.0 - frac) + values[hi] * frac;
}

} // namespace rawrxd::orchestration
