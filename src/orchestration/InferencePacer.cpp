#include "../../include/orchestration/InferencePacer.h"

#include "../../include/orchestration/MetricsThread.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rawrxd::orchestration {

InferencePacer::InferencePacer(PacerConfig config) : m_config(config) {
}

bool InferencePacer::InitializeClockDomain() noexcept {
#if defined(_WIN32)
    HANDLE thread = ::GetCurrentThread();
    DWORD_PTR previousMask = 0;
    if (m_config.pin_startup_thread_for_qpc_init) {
        previousMask = ::SetThreadAffinityMask(thread, 1);
    }

    m_qpcFrequency = ReadQpcFrequency();

    if (m_config.pin_startup_thread_for_qpc_init && previousMask != 0) {
        ::SetThreadAffinityMask(thread, previousMask);
    }
#else
    m_qpcFrequency = ReadQpcFrequency();
#endif
    return m_qpcFrequency != 0;
}

void InferencePacer::AttachMetrics(MetricsThread* metrics) noexcept {
    m_metrics = metrics;
}

bool InferencePacer::SubmitRequest(std::uint64_t requestId) {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    RequestTimeline timeline{};
    timeline.request_id = requestId;
    timeline.qpc_frequency = m_qpcFrequency;
    Stamp(timeline, StageStamp::RequestReceived);
    m_requests[requestId] = timeline;
    UpdateState(RequestState::SearchRunning);
    return true;
}

void InferencePacer::CancelRequest(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    m_requests.erase(requestId);
    UpdateState(RequestState::Cancelled);
}

void InferencePacer::OnSearchStart(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::SearchStart);
    UpdateState(RequestState::SearchRunning);
}

void InferencePacer::OnFirstContextReady(std::uint64_t requestId, std::size_t contextBytes) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    it->second.context_bytes = contextBytes;
    Stamp(it->second, StageStamp::FirstContextChunkReady);
    UpdateState(RequestState::FirstContextReady);
}

void InferencePacer::OnSearchDone(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::SearchDone);
}

void InferencePacer::OnPulseStart(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::PulseStart);
    UpdateState(RequestState::PulseRunning);
}

void InferencePacer::OnFirstPulseReady(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::FirstPulseChunkReady);
}

void InferencePacer::OnPulseDone(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::PulseDone);
}

void InferencePacer::OnPrefillStart(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::PrefillSubmitStart);
    UpdateState(RequestState::PrefillRunning);
}

void InferencePacer::OnPrefillDone(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::PrefillDone);
}

void InferencePacer::OnLanePrefillStart(std::uint64_t requestId, std::uint8_t laneId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    auto& timeline = it->second;
    if (laneId == 0) {
        timeline.flags |= kPrefillPrimaryStartFlag;
    } else {
        timeline.flags |= kPrefillVerifierStartFlag;
    }

    if ((timeline.flags & kPrefillStartStampedFlag) == 0) {
        Stamp(timeline, StageStamp::PrefillSubmitStart);
        timeline.flags |= kPrefillStartStampedFlag;
    }

    UpdateState(RequestState::PrefillRunning);
}

void InferencePacer::OnLanePrefillDone(std::uint64_t requestId, std::uint8_t laneId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    auto& timeline = it->second;
    if (laneId == 0) {
        timeline.flags |= kPrefillPrimaryDoneFlag;
    } else {
        timeline.flags |= kPrefillVerifierDoneFlag;
    }

    const bool primaryDone = (timeline.flags & kPrefillPrimaryDoneFlag) != 0;
    const bool verifierDone = (timeline.flags & kPrefillVerifierDoneFlag) != 0;
    if (primaryDone && verifierDone && (timeline.flags & kPrefillDoneStampedFlag) == 0) {
        Stamp(timeline, StageStamp::PrefillDone);
        timeline.flags |= kPrefillDoneStampedFlag;
    }
}

void InferencePacer::OnSwarmDivergence(
    std::uint64_t requestId,
    std::uint32_t tokenIndex,
    double disagreementScore) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    auto& timeline = it->second;
    timeline.flags |= kSwarmDivergenceObservedFlag;
    timeline.swarm_divergence_events += 1;
    timeline.swarm_last_divergence_token = tokenIndex;
    if (disagreementScore > timeline.swarm_max_disagreement_score) {
        timeline.swarm_max_disagreement_score = disagreementScore;
    }
}

void InferencePacer::OnDecodeStart(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::DecodeStart);
    UpdateState(RequestState::DecodeRunning);
}

void InferencePacer::OnFirstTokenReady(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::FirstTokenReady);
}

void InferencePacer::OnFirstTokenEmitted(std::uint64_t requestId) noexcept {
    std::lock_guard<std::mutex> lock(m_requestsGuard);
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }
    Stamp(it->second, StageStamp::FirstTokenEmittedUi);
    UpdateState(RequestState::FirstTokenEmitted);
}

void InferencePacer::OnRequestDone(std::uint64_t requestId, std::size_t tokensGenerated, std::uint32_t errorCode) noexcept {
    RequestTimeline timeline{};
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_requestsGuard);
        auto it = m_requests.find(requestId);
        if (it != m_requests.end()) {
            it->second.tokens_generated = tokensGenerated;
            it->second.error_code = errorCode;
            Stamp(it->second, StageStamp::RequestDone);
            timeline = it->second;
            m_requests.erase(it);
            found = true;
        }
    }

    if (found && m_metrics != nullptr) {
        (void)m_metrics->Submit(timeline);
    }

    UpdateState(errorCode == 0 ? RequestState::Completed : RequestState::Failed);
}

RequestState InferencePacer::State() const noexcept {
    return m_state.load(std::memory_order_acquire);
}

void InferencePacer::UpdateState(RequestState state) noexcept {
    m_state.store(state, std::memory_order_release);
}

} // namespace rawrxd::orchestration
