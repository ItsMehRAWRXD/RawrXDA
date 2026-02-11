// ============================================================================
// UnifiedTelemetryCore.h — Single Entry Point for All Telemetry
// ============================================================================
// Bridges the four fragmented telemetry layers into one coherent system:
//
//   Layer 1: ASM Counters    — lock-free atomics in .data segments
//   Layer 2: C++ Metrics     — AIMetricsCollector (non-Qt, thread-safe)
//   Layer 3: Agent Telemetry — AgentTranscript step-level recording
//   Layer 4: Session Events  — AISession checkpoint/event log
//
// This core provides:
//   - One `emit()` call that routes to all relevant layers
//   - Unified metric retrieval for dashboards
//   - Non-Qt implementation (pure C++20 + Win32 for ASM bridge)
//   - Prometheus text-format export without Qt dependencies
//   - JSON export with all layers merged
//   - Ring-buffer event stream for real-time consumers
//
// Architecture:
//   ┌───────────────────────────────────────────────────────┐
//   │              UnifiedTelemetryCore (singleton)         │
//   │                                                       │
//   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────┐ │
//   │  │ ASM      │ │ C++      │ │ Agent    │ │ Session │ │
//   │  │ Counters │ │ Metrics  │ │ Transcript│ │ Events  │ │
//   │  │ Bridge   │ │ Collector│ │ Logger   │ │ Logger  │ │
//   │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬────┘ │
//   │       │             │            │             │      │
//   │       └─────────────┴────────────┴─────────────┘      │
//   │                       │                               │
//   │              ┌────────▼────────┐                      │
//   │              │  Event Ring     │                      │
//   │              │  Buffer (8192)  │                      │
//   │              └────────┬────────┘                      │
//   │                       │                               │
//   │       ┌───────────────┼───────────────┐               │
//   │       ▼               ▼               ▼               │
//   │  Prometheus      JSON Export     File Logger          │
//   │  /metrics        /telemetry      telemetry.jsonl      │
//   └───────────────────────────────────────────────────────┘
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <cstdint>

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Telemetry event severity levels
// ============================================================================
enum class TelemetryLevel {
    Trace   = 0,
    Debug   = 1,
    Info    = 2,
    Warning = 3,
    Error   = 4,
    Fatal   = 5
};

// ============================================================================
// Telemetry event categories (which layer generated this)
// ============================================================================
enum class TelemetrySource {
    ASM,            // From assembly kernel counters
    CppMetrics,     // From AIMetricsCollector
    AgentLoop,      // From BoundedAgentLoop / AgentTranscript
    Session,        // From AISession events
    Hotpatch,       // From 3-layer hotpatch system
    Inference,      // From inference engine
    Network,        // From HTTP/WebSocket layers
    System          // OS-level (memory, threads, etc.)
};

// ============================================================================
// Unified telemetry event — the common currency across all layers
// ============================================================================
struct TelemetryEvent {
    uint64_t        id              = 0;
    uint64_t        timestampMs     = 0;
    TelemetryLevel  level           = TelemetryLevel::Info;
    TelemetrySource source          = TelemetrySource::System;
    std::string     category;           // e.g., "inference.latency", "agent.tool_call"
    std::string     message;            // Human-readable description
    double          valueNumeric    = 0.0;
    std::string     valueString;
    std::string     unit;               // "ms", "bytes", "tokens", "count"

    // Structured metadata
    std::map<std::string, std::string> tags;

    // Timing for span-style events
    int64_t         durationMs      = -1; // -1 = point event, >=0 = span

    const char* levelString() const {
        switch (level) {
            case TelemetryLevel::Trace:   return "TRACE";
            case TelemetryLevel::Debug:   return "DEBUG";
            case TelemetryLevel::Info:    return "INFO";
            case TelemetryLevel::Warning: return "WARN";
            case TelemetryLevel::Error:   return "ERROR";
            case TelemetryLevel::Fatal:   return "FATAL";
            default: return "UNKNOWN";
        }
    }

    const char* sourceString() const {
        switch (source) {
            case TelemetrySource::ASM:        return "asm";
            case TelemetrySource::CppMetrics: return "cpp_metrics";
            case TelemetrySource::AgentLoop:  return "agent_loop";
            case TelemetrySource::Session:    return "session";
            case TelemetrySource::Hotpatch:   return "hotpatch";
            case TelemetrySource::Inference:  return "inference";
            case TelemetrySource::Network:    return "network";
            case TelemetrySource::System:     return "system";
            default: return "unknown";
        }
    }
};

// ============================================================================
// ASM counter bridge — reads exported globals from assembly kernels
// ============================================================================
struct ASMCounterSet {
    // Inference counters (from inference_core.asm)
    uint64_t gemmCalls          = 0;
    uint64_t gemvCalls          = 0;
    uint64_t gemmFlops          = 0;

    // Quantization counters (from quant_avx2.asm)
    uint64_t q4DequantCalls     = 0;
    uint64_t q8DequantCalls     = 0;

    // Server counters (from request_patch.asm)
    uint64_t reqInterceptCount  = 0;
    uint64_t respInterceptCount = 0;

    // Flash attention (from FlashAttention_AVX512.asm)
    uint64_t flashAttnCalls     = 0;
    uint64_t flashAttnTiles     = 0;

    // Hotpatch counters (from memory_patch.asm)
    uint64_t patchApplyCount    = 0;
    uint64_t patchRevertCount   = 0;

    // CoT engine (from rawrxd_cot_phase39.asm)
    uint64_t cotStepsExecuted   = 0;
    uint64_t cotArenaBytes      = 0;
};

// ============================================================================
// Prometheus-compatible metric types
// ============================================================================
enum class MetricType {
    Counter,    // Monotonically increasing
    Gauge,      // Arbitrary value
    Histogram,  // Distribution (p50/p95/p99)
    Summary     // Quantile summary
};

struct PrometheusMetric {
    std::string     name;
    std::string     help;
    MetricType      type    = MetricType::Counter;
    double          value   = 0.0;
    std::map<std::string, std::string> labels;
};

// ============================================================================
// Telemetry consumer callback — for real-time streaming
// ============================================================================
using TelemetryConsumer = std::function<void(const TelemetryEvent& event)>;

// ============================================================================
// Ring buffer configuration
// ============================================================================
constexpr size_t TELEMETRY_RING_SIZE = 8192;

// ============================================================================
// UnifiedTelemetryCore — The single telemetry entry point
// ============================================================================
class UnifiedTelemetryCore {
public:
    // ---- Singleton (no Qt, pure C++) ----
    static UnifiedTelemetryCore& Instance();

    // ---- Lifecycle ----
    void Initialize(const std::string& logDir = "./logs",
                    TelemetryLevel minLevel = TelemetryLevel::Info);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // ---- Primary emit API ----
    // Point event (no duration)
    void Emit(TelemetrySource source, TelemetryLevel level,
              const std::string& category, const std::string& message,
              double value = 0.0, const std::string& unit = "",
              const std::map<std::string, std::string>& tags = {});

    // Span event (has duration)
    void EmitSpan(TelemetrySource source, TelemetryLevel level,
                  const std::string& category, const std::string& message,
                  int64_t durationMs,
                  const std::map<std::string, std::string>& tags = {});

    // ---- Convenience emitters by source ----
    void EmitInference(const std::string& model, int64_t latencyMs,
                       int tokensIn, int tokensOut, bool success);

    void EmitToolCall(const std::string& toolName, int64_t durationMs,
                      bool success, const std::string& detail = "");

    void EmitAgentStep(int step, int maxSteps,
                       const std::string& toolName,
                       int64_t modelLatencyMs, int64_t toolLatencyMs);

    void EmitHotpatch(const std::string& layer, const std::string& patchName,
                      bool success, int64_t durationMs = 0);

    void EmitSystemEvent(const std::string& category,
                         const std::string& message,
                         TelemetryLevel level = TelemetryLevel::Info);

    // ---- ASM counter bridge ----
    ASMCounterSet ReadASMCounters();
    void PollASMCounters();     // Reads + emits delta events

    // ---- Event retrieval ----
    std::vector<TelemetryEvent> GetRecentEvents(size_t count = 100) const;
    std::vector<TelemetryEvent> GetEventsBySource(TelemetrySource source,
                                                   size_t count = 100) const;
    std::vector<TelemetryEvent> GetEventsByCategory(const std::string& category,
                                                     size_t count = 100) const;
    uint64_t GetTotalEventCount() const { return m_eventCounter.load(); }

    // ---- Export ----
    std::string ExportPrometheus() const;
    std::string ExportJSON(size_t maxEvents = 1000) const;
    std::string ExportSummaryText() const;
    bool FlushToFile() const;

    // ---- Consumer registration ----
    uint64_t RegisterConsumer(TelemetryConsumer consumer);
    void UnregisterConsumer(uint64_t consumerId);

    // ---- Configuration ----
    void SetMinLevel(TelemetryLevel level) { m_minLevel = level; }
    TelemetryLevel GetMinLevel() const { return m_minLevel; }

private:
    UnifiedTelemetryCore();
    ~UnifiedTelemetryCore();

    // Non-copyable
    UnifiedTelemetryCore(const UnifiedTelemetryCore&) = delete;
    UnifiedTelemetryCore& operator=(const UnifiedTelemetryCore&) = delete;

    // ---- Internal ----
    void EmitInternal(TelemetryEvent&& event);
    void WriteToLog(const TelemetryEvent& event);
    void NotifyConsumers(const TelemetryEvent& event);

    // ---- ASM counter reading (platform-specific) ----
    void ReadASMGlobals(ASMCounterSet& out);

    static uint64_t NowMs();

    // ---- State ----
    std::atomic<bool>               m_initialized{false};
    std::atomic<uint64_t>           m_eventCounter{0};
    TelemetryLevel                  m_minLevel = TelemetryLevel::Info;
    std::string                     m_logDir;
    std::string                     m_logFilePath;

    // Ring buffer (fixed-size, overwrite-oldest)
    mutable std::mutex              m_ringMutex;
    std::deque<TelemetryEvent>      m_ring;

    // Consumers
    mutable std::mutex              m_consumerMutex;
    std::map<uint64_t, TelemetryConsumer> m_consumers;
    uint64_t                        m_nextConsumerId = 1;

    // Prometheus metrics cache
    mutable std::mutex              m_promMutex;
    std::map<std::string, PrometheusMetric> m_prometheusMetrics;

    // Previous ASM counters (for delta computation)
    ASMCounterSet                   m_prevASM;
};

// ============================================================================
// Scoped span helper — RAII timing for function/block instrumentation
// ============================================================================
class TelemetrySpan {
public:
    TelemetrySpan(TelemetrySource source, const std::string& category,
                  const std::string& message,
                  const std::map<std::string, std::string>& tags = {})
        : m_source(source), m_category(category),
          m_message(message), m_tags(tags),
          m_startMs(NowMs()), m_committed(false) {}

    ~TelemetrySpan() {
        if (!m_committed) Commit();
    }

    void Commit() {
        if (m_committed) return;
        m_committed = true;
        int64_t duration = static_cast<int64_t>(NowMs() - m_startMs);
        UnifiedTelemetryCore::Instance().EmitSpan(
            m_source, TelemetryLevel::Info,
            m_category, m_message, duration, m_tags);
    }

    void AddTag(const std::string& key, const std::string& value) {
        m_tags[key] = value;
    }

private:
    static uint64_t NowMs() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count());
    }

    TelemetrySource m_source;
    std::string     m_category;
    std::string     m_message;
    std::map<std::string, std::string> m_tags;
    uint64_t        m_startMs;
    bool            m_committed;
};

// ============================================================================
// Convenience macros
// ============================================================================
#define TELEMETRY_SPAN(source, category, message) \
    RawrXD::Telemetry::TelemetrySpan _tspan##__LINE__( \
        RawrXD::Telemetry::TelemetrySource::source, category, message)

#define TELEMETRY_EMIT(source, level, category, message) \
    RawrXD::Telemetry::UnifiedTelemetryCore::Instance().Emit( \
        RawrXD::Telemetry::TelemetrySource::source, \
        RawrXD::Telemetry::TelemetryLevel::level, \
        category, message)

} // namespace Telemetry
} // namespace RawrXD
