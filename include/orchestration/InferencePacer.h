#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "RequestTimeline.h"

namespace rawrxd::orchestration {

class MetricsThread;

enum class RequestState : std::uint8_t {
    Created = 0,
    SearchRunning,
    FirstContextReady,
    PulseRunning,
    PrefillRunning,
    DecodeRunning,
    FirstTokenEmitted,
    Completed,
    Cancelled,
    Failed
};

struct PacerConfig {
    std::uint32_t wait_timeout_ms = 1;
    bool pin_startup_thread_for_qpc_init = true;
};

class InferencePacer {
public:
    explicit InferencePacer(PacerConfig config = {});

    bool InitializeClockDomain() noexcept;
    void AttachMetrics(MetricsThread* metrics) noexcept;

    bool SubmitRequest(std::uint64_t requestId);
    void CancelRequest(std::uint64_t requestId) noexcept;

    void OnSearchStart(std::uint64_t requestId) noexcept;
    void OnFirstContextReady(std::uint64_t requestId, std::size_t contextBytes) noexcept;
    void OnSearchDone(std::uint64_t requestId) noexcept;

    void OnPulseStart(std::uint64_t requestId) noexcept;
    void OnFirstPulseReady(std::uint64_t requestId) noexcept;
    void OnPulseDone(std::uint64_t requestId) noexcept;

    void OnPrefillStart(std::uint64_t requestId) noexcept;
    void OnPrefillDone(std::uint64_t requestId) noexcept;
    void OnLanePrefillStart(std::uint64_t requestId, std::uint8_t laneId) noexcept;
    void OnLanePrefillDone(std::uint64_t requestId, std::uint8_t laneId) noexcept;
    void OnSwarmDivergence(std::uint64_t requestId, std::uint32_t tokenIndex, double disagreementScore) noexcept;

    void OnDecodeStart(std::uint64_t requestId) noexcept;
    void OnFirstTokenReady(std::uint64_t requestId) noexcept;
    void OnFirstTokenEmitted(std::uint64_t requestId) noexcept;

    void OnRequestDone(std::uint64_t requestId, std::size_t tokensGenerated, std::uint32_t errorCode = 0) noexcept;

    RequestState State() const noexcept;

private:
    void UpdateState(RequestState state) noexcept;

    static constexpr std::uint32_t kPrefillPrimaryStartFlag = 1u << 0;
    static constexpr std::uint32_t kPrefillVerifierStartFlag = 1u << 1;
    static constexpr std::uint32_t kPrefillPrimaryDoneFlag = 1u << 2;
    static constexpr std::uint32_t kPrefillVerifierDoneFlag = 1u << 3;
    static constexpr std::uint32_t kPrefillStartStampedFlag = 1u << 4;
    static constexpr std::uint32_t kPrefillDoneStampedFlag = 1u << 5;
    static constexpr std::uint32_t kSwarmDivergenceObservedFlag = 1u << 6;

private:
    PacerConfig m_config;
    std::atomic<RequestState> m_state{RequestState::Created};
    std::uint64_t m_qpcFrequency = 0;
    MetricsThread* m_metrics = nullptr;

    std::mutex m_requestsGuard;
    std::unordered_map<std::uint64_t, RequestTimeline> m_requests;
};

} // namespace rawrxd::orchestration
