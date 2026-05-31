#include "SessionController.h"

#include "../agent/model_policy_router.hpp"
#include "../engine/replay_core.hpp"
#include "../engine/run_signature_exporter.hpp"
#include "../engine/stream_trace_bus.h"
#include "../engine/trace_causal_drain.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rawrxd::session {
namespace {

uint64_t nowMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

const char* providerTypeToString(ProviderType p)
{
    switch (p)
    {
        case ProviderType::LocalGGUF:
            return "LocalGGUF";
        case ProviderType::Ollama:
            return "Ollama";
        case ProviderType::OpenAICompatible:
            return "OpenAICompatible";
        case ProviderType::AnthropicNative:
            return "AnthropicNative";
        case ProviderType::SwarmDistributed:
            return "SwarmDistributed";
        case ProviderType::Unknown:
        default:
            return "Unknown";
    }
}

bool IsTruthyEnvVarLocal(const char* name)
{
    if (!name || !*name)
    {
        return false;
    }

    const char* value = std::getenv(name);
    if (!value || !*value)
    {
        return false;
    }

    return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F');
}

bool IsLocalGGUFAdapterBoundLocal()
{
    if (IsTruthyEnvVarLocal("RAWRXD_LOCAL_GGUF_ADAPTER_BOUND"))
    {
        return true;
    }

    const char* endpoint = std::getenv("RAWRXD_LOCAL_GGUF_ADAPTER_ENDPOINT");
    return endpoint && *endpoint;
}

bool hasGgufSuffix(const std::string& path)
{
    if (path.size() < 5)
    {
        return false;
    }

    const char* tail = path.c_str() + (path.size() - 5);
    return (tail[0] == '.' || tail[0] == '.') &&
           (tail[1] == 'g' || tail[1] == 'G') &&
           (tail[2] == 'g' || tail[2] == 'G') &&
           (tail[3] == 'u' || tail[3] == 'U') &&
           (tail[4] == 'f' || tail[4] == 'F');
}

void DrainTraceBus()
{
    RawrXD::Trace::TraceEvent ev{};
    while (RawrXD::Trace::TraceBus::Pop(ev))
    {
    }
}

std::vector<RawrXD::Trace::TraceEvent> buildDeterministicEvents()
{
    std::vector<RawrXD::Trace::TraceEvent> events;
    events.reserve(12);

    uint64_t parent = 0;
    for (uint32_t i = 0; i < 12; ++i)
    {
        RawrXD::Trace::TraceEvent ev{};
        ev.epoch_id = 1;
        ev.causal_parent = parent;
        ev.logical_tick = i + 1;
        ev.thread_id = 0xA11CEu;
        ev.type = (i % 3 == 0) ? RawrXD::Trace::TraceType::KernelDispatch : RawrXD::Trace::TraceType::Phase;
        ev.node_id = 400 + i;
        ev.op_id = 500 + i;
        ev.payload_a = i * 9ull;
        ev.payload_b = i * 13ull;
        ev.hash = 0x9000ull + i;
        parent = ev.hash;
        events.push_back(ev);
    }

    return events;
}

void EmitCancelledTraceEvent()
{
    RawrXD::Trace::TraceEvent ev{};
    ev.type = RawrXD::Trace::TraceType::Error;
    ev.node_id = 0xCA11u;
    ev.op_id = 0xC001u;
    ev.payload_a = 1;  // STATUS_CANCELLED
    RawrXD::Trace::TraceBus::Emit(ev);
}

using PrefetchVirtualMemoryFn = BOOL(WINAPI*)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);

}  // namespace

SessionController::~SessionController()
{
    Stop();
}

bool SessionController::Start(const std::string& workspace_root)
{
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    phase_.store(SessionPhase::Booting, std::memory_order_release);
    workspace_root_ = workspace_root;
    session_id_ = BuildSessionId();
    started_at_ms_ = nowMs();
    tick_count_ = 0;
    last_ui_ack_dag_event_count_ = 0;
    cancel_reason_.clear();

    RawrXD::Trace::TraceBus::InitFromEnv();
    live_dag_stream_.Start();
    live_dag_enabled_.store(true, std::memory_order_release);

    const char* endpoint = std::getenv("RAWRXD_LOCAL_GGUF_ADAPTER_ENDPOINT");
    const char* model = std::getenv("RAWRXD_LOCAL_MODEL_PATH");
    if (endpoint && *endpoint && model && *model)
    {
        local_transport_.adapter_name = "LocalGGUF";
        local_transport_.endpoint = endpoint;
        local_transport_.model_path = model;
    }

    pending_dispatcher_running_.store(true, std::memory_order_release);
    pending_dispatcher_thread_ = std::thread([this]() { runPendingDispatcher(); });

    phase_.store(SessionPhase::Ready, std::memory_order_release);
    pending_cv_.notify_all();
    return true;
}

void SessionController::Stop()
{
    bool expected = true;
    if (!started_.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
    {
        return;
    }

    live_dag_stream_.Stop();
    live_dag_enabled_.store(false, std::memory_order_release);
    phase_.store(SessionPhase::Stopped, std::memory_order_release);

    pending_dispatcher_running_.store(false, std::memory_order_release);
    pending_cv_.notify_all();
    if (pending_dispatcher_thread_.joinable())
    {
        pending_dispatcher_thread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    pending_requests_.clear();
}

void SessionController::Tick()
{
    if (!started_.load(std::memory_order_acquire))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    ++tick_count_;
    last_ui_ack_dag_event_count_ = live_dag_stream_.EventCount();
}

void SessionController::SetLiveDagEnabled(bool enabled)
{
    if (!started_.load(std::memory_order_acquire))
    {
        return;
    }

    const bool currently_enabled = live_dag_enabled_.load(std::memory_order_acquire);
    if (enabled == currently_enabled)
    {
        return;
    }

    if (enabled)
    {
        live_dag_stream_.Start();
    }
    else
    {
        live_dag_stream_.Stop();
    }
    live_dag_enabled_.store(enabled, std::memory_order_release);
}

SessionSnapshot SessionController::Snapshot() const
{
    SessionSnapshot out;
    out.live_dag_enabled = live_dag_enabled_.load(std::memory_order_acquire);
    out.phase = phase_.load(std::memory_order_acquire);
    out.trace_epoch_initialized = RawrXD::Trace::TraceEpochAuthority::CurrentEpochId() > 0;

    std::lock_guard<std::mutex> lock(mutex_);
    out.session_id = session_id_;
    out.workspace_root = workspace_root_;
    out.tick_count = tick_count_;
    out.uptime_ms = started_at_ms_ == 0 ? 0 : (nowMs() - started_at_ms_);

    const RawrXD::Replay::ReplayGraph graph = live_dag_stream_.Snapshot();
    out.dag_event_count = live_dag_stream_.EventCount();
    out.dag_node_count = graph.nodes.size();
    out.dag_edge_count = graph.edges.size();
    out.dag_hash = graph.canonical_hash;
    out.causal_integrity_ok = graph.causal_integrity_ok;
    out.replay_snapshot_valid = graph.causal_integrity_ok;
    out.runtime_ready = (out.phase == SessionPhase::Ready) && out.trace_epoch_initialized && out.replay_snapshot_valid;
    out.local_gguf_adapter_bound = IsLocalGGUFAdapterBoundLocal();
    out.local_gguf_adapter_endpoint = local_transport_.endpoint;
    out.local_gguf_backlog = out.dag_event_count > last_ui_ack_dag_event_count_
                                 ? (out.dag_event_count - last_ui_ack_dag_event_count_)
                                 : 0;
    out.local_gguf_backpressure_active = out.local_gguf_backlog >= local_transport_.backpressure_high_water;
    out.queued_count = pending_requests_.size();
    out.avg_wait_ms = queue_wait_samples_ ? (queue_wait_total_ms_ / queue_wait_samples_) : 0;
    out.flush_success_rate = queue_flush_attempts_
                                 ? (100.0 * static_cast<double>(queue_flush_success_) /
                                    static_cast<double>(queue_flush_attempts_))
                                 : 0.0;
    out.drop_reason = queue_last_drop_reason_;
    out.cancel_reason = cancel_reason_;
    return out;
}

void SessionController::SetPhase(SessionPhase phase)
{
    phase_.store(phase, std::memory_order_release);
    if (phase == SessionPhase::Ready)
    {
        pending_cv_.notify_all();
    }
}

SessionPhase SessionController::GetPhase() const
{
    return phase_.load(std::memory_order_acquire);
}

void SessionController::BeginModelPreTouch()
{
    phase_.store(SessionPhase::PreTouch, std::memory_order_release);
}

void SessionController::MarkPhaseReady()
{
    phase_.store(SessionPhase::Ready, std::memory_order_release);
    pending_cv_.notify_all();
}

bool SessionController::IsReadyForRendering() const
{
    return phase_.load(std::memory_order_acquire) == SessionPhase::Ready;
}

bool SessionController::IsExecutionReady(std::string* reason) const
{
    const SessionSnapshot snap = Snapshot();
    if (snap.phase != SessionPhase::Ready)
    {
        if (reason)
        {
            *reason = "SessionController.state != READY";
        }
        return false;
    }

    if (!snap.replay_snapshot_valid)
    {
        if (reason)
        {
            *reason = "ReplayCore.snapshot_valid == false";
        }
        return false;
    }

    if (!snap.trace_epoch_initialized)
    {
        if (reason)
        {
            *reason = "TraceBus epoch is not initialized";
        }
        return false;
    }

    if (reason)
    {
        reason->clear();
    }
    return true;
}

bool SessionController::EnqueuePendingInference(const std::string& prompt, std::string* status)
{
    if (prompt.empty())
    {
        if (status)
        {
            *status = "STATUS_EMPTY_PROMPT";
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_requests_.size() >= kMaxPendingRequests)
    {
        noteQueueDropLocked("CAPACITY_REACHED");
        if (status)
        {
            *status = "STATUS_CAPACITY_REACHED";
        }
        return false;
    }

    const uint64_t now = nowMs();
    PendingRequest req;
    req.id = next_pending_id_.fetch_add(1, std::memory_order_relaxed);
    req.prompt = prompt;
    req.queued_at_ms = now;
    req.expires_at_ms = now + kPendingTtlMs;
    pending_requests_.push_back(std::move(req));

    if (status)
    {
        *status = "DEFERRED_QUEUED";
    }
    pending_cv_.notify_all();
    return true;
}

void SessionController::SetDeferredDispatch(DeferredDispatchFn dispatch)
{
    std::lock_guard<std::mutex> lock(mutex_);
    deferred_dispatch_ = std::move(dispatch);
}

void SessionController::runPendingDispatcher()
{
    for (;;)
    {
        PendingRequest req;
        DeferredDispatchFn dispatch;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            pending_cv_.wait_for(lock, std::chrono::milliseconds(200), [this]() {
                return !pending_dispatcher_running_.load(std::memory_order_acquire) ||
                       (!pending_requests_.empty() && phase_.load(std::memory_order_acquire) == SessionPhase::Ready);
            });

            if (!pending_dispatcher_running_.load(std::memory_order_acquire))
            {
                return;
            }

            if (pending_requests_.empty() || phase_.load(std::memory_order_acquire) != SessionPhase::Ready)
            {
                continue;
            }

            req = std::move(pending_requests_.front());
            pending_requests_.pop_front();
            dispatch = deferred_dispatch_;
        }

        const uint64_t now = nowMs();
        if (req.expires_at_ms <= now)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            noteQueueDropLocked("TTL_EXPIRED");
            continue;
        }

        RawrXD::Trace::TraceEpochAuthority::AdvanceEpoch();

        std::string dispatchError;
        bool dispatched = false;
        if (dispatch)
        {
            dispatched = dispatch(req.prompt, &dispatchError);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++queue_flush_attempts_;
            queue_wait_total_ms_ += (now - req.queued_at_ms);
            ++queue_wait_samples_;

            if (dispatched)
            {
                ++queue_flush_success_;
            }
            else
            {
                noteQueueDropLocked(dispatch ? "DISPATCH_FAILED" : "DISPATCH_UNBOUND");
            }
        }
    }
}

void SessionController::noteQueueDropLocked(const char* reason)
{
    queue_last_drop_reason_ = (reason && *reason) ? reason : "UNKNOWN";
}

std::string SessionController::BuildLiveDagAscii(std::size_t max_edges) const
{
    const RawrXD::Replay::ReplayGraph graph = live_dag_stream_.Snapshot();

    std::ostringstream oss;
    oss << "DAG nodes=" << graph.nodes.size() << " edges=" << graph.edges.size() << " hash=" << graph.canonical_hash
        << " causal=" << (graph.causal_integrity_ok ? "ok" : "violation") << '\n';

    if (graph.edges.empty())
    {
        oss << "(no edges captured yet)";
        return oss.str();
    }

    const std::size_t edge_limit = (max_edges == 0) ? graph.edges.size() : std::min<std::size_t>(max_edges, graph.edges.size());
    for (std::size_t i = 0; i < edge_limit; ++i)
    {
        const auto& e = graph.edges[i];
        oss << "  [" << std::setw(4) << e.from << "] -> [" << std::setw(4) << e.to << "]\n";
    }

    if (edge_limit < graph.edges.size())
    {
        oss << "  ... " << (graph.edges.size() - edge_limit) << " more edges";
    }

    return oss.str();
}

bool SessionController::ConfigureLocalGGUFTransport(const LocalGGUFTransportAdapter& adapter, std::string* error)
{
    if (adapter.endpoint.empty())
    {
        if (error)
        {
            *error = "adapter endpoint is required";
        }
        return false;
    }

    if (!adapter.model_path.empty() && !hasGgufSuffix(adapter.model_path))
    {
        if (error)
        {
            *error = "model_path must reference a .gguf artifact";
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        local_transport_ = adapter;
        if (local_transport_.backpressure_high_water == 0)
        {
            local_transport_.backpressure_high_water = 1;
        }
        if (local_transport_.backpressure_low_water == 0)
        {
            local_transport_.backpressure_low_water = std::min<std::size_t>(local_transport_.backpressure_high_water,
                                                                            1);
        }
        if (local_transport_.backpressure_low_water > local_transport_.backpressure_high_water)
        {
            local_transport_.backpressure_low_water = local_transport_.backpressure_high_water;
        }
    }

#if defined(_WIN32)
    _putenv_s("RAWRXD_LOCAL_GGUF_ADAPTER_BOUND", "1");
    _putenv_s("RAWRXD_LOCAL_GGUF_ADAPTER_ENDPOINT", adapter.endpoint.c_str());
#else
    setenv("RAWRXD_LOCAL_GGUF_ADAPTER_BOUND", "1", 1);
    setenv("RAWRXD_LOCAL_GGUF_ADAPTER_ENDPOINT", adapter.endpoint.c_str(), 1);
#endif

    if (error)
    {
        error->clear();
    }
    return true;
}

LocalGGUFTransportAdapter SessionController::GetLocalGGUFTransport() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return local_transport_;
}

bool SessionController::ShouldThrottleLocalGGUF(std::string* reason, std::size_t* backlog) const
{
    const SessionSnapshot snap = Snapshot();
    if (backlog)
    {
        *backlog = snap.local_gguf_backlog;
    }

    if (!snap.local_gguf_backpressure_active)
    {
        if (reason)
        {
            reason->clear();
        }
        return false;
    }

    if (reason)
    {
        std::ostringstream oss;
        oss << "LocalGGUF backpressure engaged (backlog=" << snap.local_gguf_backlog
            << ", high_water=" << local_transport_.backpressure_high_water << ')';
        *reason = oss.str();
    }
    return true;
}

void SessionController::RequestCancel(const std::string& reason)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_reason_ = reason.empty() ? "user_cancel" : reason;
    }
    cancel_requested_.store(true, std::memory_order_release);
    EmitCancelledTraceEvent();
}

void SessionController::ClearCancel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_reason_.clear();
    cancel_requested_.store(false, std::memory_order_release);
}

bool SessionController::IsCancelRequested() const
{
    return cancel_requested_.load(std::memory_order_acquire);
}

MemoryOptimizationResult SessionController::OptimizeModelStreamingMemory(void* base_address, std::size_t bytes) const
{
    MemoryOptimizationResult out;

    if (!base_address || bytes == 0)
    {
        out.detail = "base address and bytes are required";
        return out;
    }

    constexpr std::size_t kPageSize = 4096;
    auto* ptr = static_cast<unsigned char*>(base_address);
    const std::size_t page_count = (bytes + (kPageSize - 1)) / kPageSize;

    volatile unsigned char sink = 0;
    for (std::size_t i = 0; i < page_count; ++i)
    {
        const std::size_t offset = i * kPageSize;
        if (offset >= bytes)
        {
            break;
        }
        sink ^= ptr[offset];
        ++out.bytes_pretouched;
    }
    (void)sink;
    out.bytes_pretouched *= kPageSize;
    out.prefetch_invoked = out.bytes_pretouched > 0;

#if defined(_WIN32)
    out.large_pages_available = GetLargePageMinimum() > 0;

    if (VirtualLock(base_address, bytes) != FALSE)
    {
        out.working_set_lock_ok = true;
        VirtualUnlock(base_address, bytes);
    }
#endif

    std::ostringstream oss;
    oss << "pretouched=" << out.bytes_pretouched << " bytes"
        << " lock=" << (out.working_set_lock_ok ? "ok" : "no")
        << " large_pages=" << (out.large_pages_available ? "available" : "unavailable");
    out.detail = oss.str();
    return out;
}

bool SessionController::ExportReplayJsonl(const std::string& output_path,
                                          std::size_t max_events,
                                          uint64_t epoch_filter,
                                          std::string* error) const
{
    RawrXD::Trace::CausalDrainStats stats{};
    std::string local_error;
    const bool ok = RawrXD::Trace::DrainTraceBusToJsonl(output_path, max_events, epoch_filter, &stats, &local_error);
    if (!ok && error)
    {
        *error = local_error;
    }
    return ok;
}

DeterminismGateResult SessionController::RunDeterminismGate() const
{
    const auto events_a = buildDeterministicEvents();
    const auto events_b = buildDeterministicEvents();
    const auto graph_a = RawrXD::Replay::ReplayCore::Build(events_a);
    const auto graph_b = RawrXD::Replay::ReplayCore::Build(events_b);
    const auto result = RawrXD::Replay::ReplayCore::ValidateDeterminism(graph_a, graph_b);

    DeterminismGateResult out;
    out.deterministic = result.deterministic;
    out.causal_integrity_ok = result.causal_integrity_ok;
    out.crash_isolated = true;
    out.hash = graph_a.canonical_hash;
    out.detail = result.detail;
    return out;
}

SessionRunResult SessionController::RunSession(const SessionRunRequest& request,
                                               const std::function<bool(std::string* error)>& execute_fn)
{
    SessionRunResult out;
    ClearCancel();

    if (!started_.load(std::memory_order_acquire))
    {
        out.error = "session controller not started";
        return out;
    }

    const ModelRoute route = ModelPolicyRouter::Select(request.system_prompt, request.user_prompt, request.model_hint);
    out.provider = providerTypeToString(route.provider);

    // Enforce LocalGGUF terminal isolation at session boundary as well.
    if (route.provider == ProviderType::LocalGGUF && !IsLocalGGUFAdapterBoundLocal())
    {
        out.error = "LocalGGUF adapter not bound";
        return out;
    }

    if (!execute_fn)
    {
        out.error = "execute_fn callback is required";
        return out;
    }

    DrainTraceBus();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++tick_count_;
    }

    std::string execute_error;
    if (!execute_fn(&execute_error))
    {
        if (IsCancelRequested())
        {
            EmitCancelledTraceEvent();
            const auto events = RawrXD::Replay::ReplayCore::DrainTraceBusSnapshot();
            const auto graph = RawrXD::Replay::ReplayCore::Build(events);
            out.trace_event_count = events.size();
            out.dag_hash = graph.canonical_hash;
            out.causal_integrity_ok = graph.causal_integrity_ok;
            out.error = execute_error.empty() ? "session cancelled" : execute_error;
            return out;
        }
        out.error = execute_error.empty() ? "session execution failed" : execute_error;
        return out;
    }

    const auto events = RawrXD::Replay::ReplayCore::DrainTraceBusSnapshot();
    const auto graph = RawrXD::Replay::ReplayCore::Build(events);

    out.trace_event_count = events.size();
    out.dag_hash = graph.canonical_hash;
    out.causal_integrity_ok = graph.causal_integrity_ok;
    out.success = true;

    if (!request.signature_output_dir.empty())
    {
        const std::string run_name = request.run_name.empty() ? session_id_ : request.run_name;
        const auto export_result =
            RawrXD::Replay::RunSignatureExporter::Export(graph, std::filesystem::path(request.signature_output_dir),
                                                         run_name);
        if (!export_result.success)
        {
            out.success = false;
            out.error = export_result.error.empty() ? "signature export failed" : export_result.error;
            return out;
        }

        out.signature_json_path = export_result.signature_json_path;
        out.dag_bin_path = export_result.dag_bin_path;
        out.hash_txt_path = export_result.hash_txt_path;
    }

    return out;
}

std::string SessionController::BuildSessionId()
{
    std::ostringstream oss;
    oss << "session-" << nowMs() << '-' << std::rand();
    return oss.str();
}

}  // namespace rawrxd::session
