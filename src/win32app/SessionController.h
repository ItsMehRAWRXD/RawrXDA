#pragma once

#include "../engine/live_dag_stream.hpp"
#include "../engine/phase_trace_validator.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rawrxd::session {

enum class SessionPhase : uint8_t {
    Booting = 0,
    PreTouch = 1,
    Ready = 2,
    Stopped = 3,
};

struct SessionSnapshot {
    std::string session_id;
    std::string workspace_root;
    SessionPhase phase = SessionPhase::Booting;
    uint64_t uptime_ms = 0;
    uint64_t tick_count = 0;
    std::size_t dag_event_count = 0;
    std::size_t dag_node_count = 0;
    std::size_t dag_edge_count = 0;
    uint64_t dag_hash = 0;
    bool causal_integrity_ok = false;
    bool replay_snapshot_valid = false;
    bool trace_epoch_initialized = false;
    bool runtime_ready = false;
    bool live_dag_enabled = false;
    bool local_gguf_adapter_bound = false;
    std::string local_gguf_adapter_endpoint;
    bool local_gguf_backpressure_active = false;
    std::size_t local_gguf_backlog = 0;
    std::size_t queued_count = 0;
    uint64_t avg_wait_ms = 0;
    double flush_success_rate = 0.0;
    std::string drop_reason;
    std::string cancel_reason;
};

struct DeterminismGateResult {
    bool deterministic = false;
    bool causal_integrity_ok = false;
    bool crash_isolated = false;
    uint64_t hash = 0;
    std::string detail;
};

struct LocalGGUFTransportAdapter {
    std::string adapter_name;
    std::string endpoint;
    std::string model_path;
    bool supports_streaming = true;
    bool prefer_shared_memory = false;
    uint32_t timeout_ms = 30000;
    std::size_t backpressure_high_water = 16384;
    std::size_t backpressure_low_water = 12288;
};

struct MemoryOptimizationResult {
    bool working_set_lock_ok = false;
    bool prefetch_invoked = false;
    bool large_pages_available = false;
    std::size_t bytes_pretouched = 0;
    std::string detail;
};

struct SessionRunRequest {
    std::string system_prompt;
    std::string user_prompt;
    std::string model_hint;
    std::string signature_output_dir;
    std::string run_name;
};

struct SessionRunResult {
    bool success = false;
    std::string error;
    std::string provider;
    uint64_t dag_hash = 0;
    bool causal_integrity_ok = false;
    std::size_t trace_event_count = 0;
    std::string signature_json_path;
    std::string dag_bin_path;
    std::string hash_txt_path;
};

class SessionController {
  public:
    using DeferredDispatchFn = std::function<bool(const std::string&, std::string* error)>;

    SessionController() = default;
    ~SessionController();

    SessionController(const SessionController&) = delete;
    SessionController& operator=(const SessionController&) = delete;

    bool Start(const std::string& workspace_root);
    void Stop();

    void Tick();
    void SetLiveDagEnabled(bool enabled);
    void SetPhase(SessionPhase phase);
    SessionPhase GetPhase() const;
    void BeginModelPreTouch();
    void MarkPhaseReady();
    bool IsReadyForRendering() const;

    SessionSnapshot Snapshot() const;
    std::string BuildLiveDagAscii(std::size_t max_edges) const;

    bool ConfigureLocalGGUFTransport(const LocalGGUFTransportAdapter& adapter, std::string* error);
    LocalGGUFTransportAdapter GetLocalGGUFTransport() const;
    bool ShouldThrottleLocalGGUF(std::string* reason = nullptr, std::size_t* backlog = nullptr) const;

    void RequestCancel(const std::string& reason = "user_cancel");
    void ClearCancel();
    bool IsCancelRequested() const;

    MemoryOptimizationResult OptimizeModelStreamingMemory(void* base_address, std::size_t bytes) const;

    bool ExportReplayJsonl(const std::string& output_path,
                           std::size_t max_events,
                           uint64_t epoch_filter,
                           std::string* error) const;
    DeterminismGateResult RunDeterminismGate() const;
    SessionRunResult RunSession(const SessionRunRequest& request,
                                const std::function<bool(std::string* error)>& execute_fn);

    bool IsExecutionReady(std::string* reason) const;
    bool EnqueuePendingInference(const std::string& prompt, std::string* status);
    void SetDeferredDispatch(DeferredDispatchFn dispatch);

  private:
    static std::string BuildSessionId();

    struct PendingRequest {
        uint64_t id = 0;
        std::string prompt;
        uint64_t queued_at_ms = 0;
        uint64_t expires_at_ms = 0;
    };

    static constexpr std::size_t kMaxPendingRequests = 10;
    static constexpr uint64_t kPendingTtlMs = 30000;

    void runPendingDispatcher();
    void noteQueueDropLocked(const char* reason);

    mutable std::mutex mutex_;
    RawrXD::Replay::LiveDagStream live_dag_stream_;
    std::string session_id_;
    std::string workspace_root_;
    std::atomic<bool> started_{false};
    std::atomic<bool> live_dag_enabled_{false};
    std::atomic<bool> cancel_requested_{false};
    std::atomic<SessionPhase> phase_{SessionPhase::Booting};
    std::atomic<uint64_t> next_pending_id_{1};
    std::deque<PendingRequest> pending_requests_;
    DeferredDispatchFn deferred_dispatch_;
    std::condition_variable pending_cv_;
    std::thread pending_dispatcher_thread_;
    std::atomic<bool> pending_dispatcher_running_{false};

    uint64_t queue_wait_total_ms_ = 0;
    uint64_t queue_wait_samples_ = 0;
    uint64_t queue_flush_attempts_ = 0;
    uint64_t queue_flush_success_ = 0;
    std::string queue_last_drop_reason_ = "NONE";
    std::string cancel_reason_;

    LocalGGUFTransportAdapter local_transport_;
    uint64_t started_at_ms_ = 0;
    uint64_t tick_count_ = 0;
    std::size_t last_ui_ack_dag_event_count_ = 0;
};

SessionController* GetRuntimeSessionController();
const SessionController* GetRuntimeSessionControllerConst();

}  // namespace rawrxd::session
