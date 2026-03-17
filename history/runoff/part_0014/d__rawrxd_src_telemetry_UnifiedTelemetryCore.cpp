// ============================================================================
// UnifiedTelemetryCore.cpp — Implementation
// ============================================================================
// Bridges ASM counters, C++ metrics, agent transcripts, and session events
// into one coherent telemetry pipeline.  Pure C++20, no Qt.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "telemetry/UnifiedTelemetryCore.h"

// Link to pure MASM telemetry backend when available
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) && RAWRXD_LINK_TELEMETRY_KERNEL_ASM
#include "telemetry/RawrXD_Telemetry_MASM_Bridge.h"
#define UTC_MASM_AVAILABLE 1
#else
#define UTC_MASM_AVAILABLE 0
#endif

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Extern declarations for ASM globals (defined in assembly kernels)
// These are only available when the ASM objects are linked in.
// We use weak references / GetProcAddress fallback on Windows.
// ============================================================================
#ifdef _WIN32
// We'll read ASM counters via symbol resolution at runtime
// to avoid hard link-time dependency
static HMODULE g_selfModule = nullptr;

template<typename T>
static T* ResolveASMGlobal(const char* name) {
    if (!g_selfModule) {
        g_selfModule = GetModuleHandleA(nullptr);
        if (!g_selfModule) return nullptr;
    }
    return reinterpret_cast<T*>(GetProcAddress(g_selfModule, name));
}
#endif

// ============================================================================
// Singleton
// ============================================================================
UnifiedTelemetryCore& UnifiedTelemetryCore::Instance() {
    static UnifiedTelemetryCore instance;
    return instance;
}

UnifiedTelemetryCore::UnifiedTelemetryCore() = default;
UnifiedTelemetryCore::~UnifiedTelemetryCore() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void UnifiedTelemetryCore::Initialize(const std::string& logDir,
                                       TelemetryLevel minLevel)
{
    if (m_initialized.load()) return;

    m_logDir = logDir;
    m_minLevel = minLevel;

    // Ensure log directory exists
    if (!logDir.empty()) {
        fs::create_directories(logDir);
        m_logFilePath = logDir + "/telemetry.jsonl";
    }

    // Initialize previous ASM counters to zero
    m_prevASM = ASMCounterSet{};

    m_initialized.store(true);

    EmitSystemEvent("telemetry.init", "UnifiedTelemetryCore initialized");
}

void UnifiedTelemetryCore::Shutdown() {
    if (!m_initialized.load()) return;

    EmitSystemEvent("telemetry.shutdown", "UnifiedTelemetryCore shutting down");
    FlushToFile();

    std::lock_guard<std::mutex> lock(m_consumerMutex);
    m_consumers.clear();

    m_initialized.store(false);
}

// ============================================================================
// Primary Emit API
// ============================================================================
void UnifiedTelemetryCore::Emit(
    TelemetrySource source, TelemetryLevel level,
    const std::string& category, const std::string& message,
    double value, const std::string& unit,
    const std::map<std::string, std::string>& tags)
{
    if (level < m_minLevel) return;

    TelemetryEvent event;
    event.id = m_eventCounter.fetch_add(1, std::memory_order_relaxed);
    event.timestampMs = NowMs();
    event.source = source;
    event.level = level;
    event.category = category;
    event.message = message;
    event.valueNumeric = value;
    event.unit = unit;
    event.tags = tags;
    event.durationMs = -1;  // Point event

    EmitInternal(std::move(event));
}

void UnifiedTelemetryCore::EmitSpan(
    TelemetrySource source, TelemetryLevel level,
    const std::string& category, const std::string& message,
    int64_t durationMs,
    const std::map<std::string, std::string>& tags)
{
    if (level < m_minLevel) return;

    TelemetryEvent event;
    event.id = m_eventCounter.fetch_add(1, std::memory_order_relaxed);
    event.timestampMs = NowMs();
    event.source = source;
    event.level = level;
    event.category = category;
    event.message = message;
    event.valueNumeric = static_cast<double>(durationMs);
    event.unit = "ms";
    event.tags = tags;
    event.durationMs = durationMs;

    EmitInternal(std::move(event));
}

// ============================================================================
// Convenience Emitters
// ============================================================================
void UnifiedTelemetryCore::EmitInference(
    const std::string& model, int64_t latencyMs,
    int tokensIn, int tokensOut, bool success)
{
    std::map<std::string, std::string> tags = {
        {"model", model},
        {"tokens_in", std::to_string(tokensIn)},
        {"tokens_out", std::to_string(tokensOut)},
        {"success", success ? "true" : "false"}
    };

    EmitSpan(TelemetrySource::Inference,
             success ? TelemetryLevel::Info : TelemetryLevel::Warning,
             "inference.request", "LLM request to " + model,
             latencyMs, tags);

    // Update Prometheus metrics
    {
        std::lock_guard<std::mutex> lock(m_promMutex);
        auto& reqCount = m_prometheusMetrics["rawrxd_inference_requests_total"];
        reqCount.name = "rawrxd_inference_requests_total";
        reqCount.help = "Total inference requests";
        reqCount.type = MetricType::Counter;
        reqCount.value += 1.0;

        auto& latHist = m_prometheusMetrics["rawrxd_inference_latency_ms"];
        latHist.name = "rawrxd_inference_latency_ms";
        latHist.help = "Inference latency in milliseconds";
        latHist.type = MetricType::Gauge;
        latHist.value = static_cast<double>(latencyMs);

        auto& tokTotal = m_prometheusMetrics["rawrxd_tokens_total"];
        tokTotal.name = "rawrxd_tokens_total";
        tokTotal.help = "Total tokens processed";
        tokTotal.type = MetricType::Counter;
        tokTotal.value += static_cast<double>(tokensIn + tokensOut);
    }
}

void UnifiedTelemetryCore::EmitToolCall(
    const std::string& toolName, int64_t durationMs,
    bool success, const std::string& detail)
{
    std::map<std::string, std::string> tags = {
        {"tool", toolName},
        {"success", success ? "true" : "false"}
    };
    if (!detail.empty()) tags["detail"] = detail;

    EmitSpan(TelemetrySource::AgentLoop,
             success ? TelemetryLevel::Info : TelemetryLevel::Warning,
             "agent.tool_call", toolName + " invocation",
             durationMs, tags);
}

void UnifiedTelemetryCore::EmitAgentStep(
    int step, int maxSteps,
    const std::string& toolName,
    int64_t modelLatencyMs, int64_t toolLatencyMs)
{
    std::map<std::string, std::string> tags = {
        {"step", std::to_string(step)},
        {"max_steps", std::to_string(maxSteps)},
        {"tool", toolName},
        {"model_latency_ms", std::to_string(modelLatencyMs)},
        {"tool_latency_ms", std::to_string(toolLatencyMs)}
    };

    Emit(TelemetrySource::AgentLoop, TelemetryLevel::Info,
         "agent.step",
         "Step " + std::to_string(step) + "/" + std::to_string(maxSteps),
         static_cast<double>(step), "step", tags);
}

void UnifiedTelemetryCore::EmitHotpatch(
    const std::string& layer, const std::string& patchName,
    bool success, int64_t durationMs)
{
    std::map<std::string, std::string> tags = {
        {"layer", layer},
        {"patch", patchName},
        {"success", success ? "true" : "false"}
    };

    EmitSpan(TelemetrySource::Hotpatch,
             success ? TelemetryLevel::Info : TelemetryLevel::Error,
             "hotpatch." + layer, patchName,
             durationMs, tags);
}

void UnifiedTelemetryCore::EmitSystemEvent(
    const std::string& category, const std::string& message,
    TelemetryLevel level)
{
    Emit(TelemetrySource::System, level, category, message);
}

// ============================================================================
// ASM Counter Bridge
// ============================================================================
ASMCounterSet UnifiedTelemetryCore::ReadASMCounters() {
    ASMCounterSet counters{};
    ReadASMGlobals(counters);
    return counters;
}

void UnifiedTelemetryCore::PollASMCounters() {
    ASMCounterSet current{};
    ReadASMGlobals(current);

    // Compute deltas and emit events for non-zero changes
    auto emitDelta = [&](const char* name, uint64_t prev, uint64_t curr) {
        if (curr > prev) {
            uint64_t delta = curr - prev;
            Emit(TelemetrySource::ASM, TelemetryLevel::Debug,
                 std::string("asm.") + name,
                 std::string(name) + " delta",
                 static_cast<double>(delta), "count");
        }
    };

    emitDelta("gemm_calls",         m_prevASM.gemmCalls,         current.gemmCalls);
    emitDelta("gemv_calls",         m_prevASM.gemvCalls,         current.gemvCalls);
    emitDelta("gemm_flops",         m_prevASM.gemmFlops,         current.gemmFlops);
    emitDelta("q4_dequant_calls",   m_prevASM.q4DequantCalls,    current.q4DequantCalls);
    emitDelta("q8_dequant_calls",   m_prevASM.q8DequantCalls,    current.q8DequantCalls);
    emitDelta("req_intercept",      m_prevASM.reqInterceptCount, current.reqInterceptCount);
    emitDelta("resp_intercept",     m_prevASM.respInterceptCount,current.respInterceptCount);
    emitDelta("flash_attn_calls",   m_prevASM.flashAttnCalls,    current.flashAttnCalls);
    emitDelta("flash_attn_tiles",   m_prevASM.flashAttnTiles,    current.flashAttnTiles);
    emitDelta("patch_apply",        m_prevASM.patchApplyCount,   current.patchApplyCount);
    emitDelta("patch_revert",       m_prevASM.patchRevertCount,  current.patchRevertCount);
    emitDelta("cot_steps",          m_prevASM.cotStepsExecuted,  current.cotStepsExecuted);
    emitDelta("cot_arena_bytes",    m_prevASM.cotArenaBytes,     current.cotArenaBytes);

    m_prevASM = current;

    // Update Prometheus gauges
    {
        std::lock_guard<std::mutex> lock(m_promMutex);
        auto set = [&](const char* name, uint64_t val) {
            auto& m = m_prometheusMetrics[name];
            m.name = name;
            m.type = MetricType::Counter;
            m.value = static_cast<double>(val);
        };
        set("rawrxd_asm_gemm_calls_total", current.gemmCalls);
        set("rawrxd_asm_flash_attn_calls_total", current.flashAttnCalls);
        set("rawrxd_asm_patch_apply_total", current.patchApplyCount);
        set("rawrxd_asm_cot_steps_total", current.cotStepsExecuted);
    }
}

void UnifiedTelemetryCore::ReadASMGlobals(ASMCounterSet& out) {
#ifdef _WIN32
    // Read each exported global from the linked ASM modules
    // These are PUBLIC DQ globals in the .data sections of our .asm files
    auto read64 = [](const char* sym) -> uint64_t {
        auto* ptr = ResolveASMGlobal<volatile uint64_t>(sym);
        return ptr ? *ptr : 0;
    };

    out.gemmCalls         = read64("g_GemmCalls");
    out.gemvCalls         = read64("g_GemvCalls");
    out.gemmFlops         = read64("g_GemmFlops");
    out.q4DequantCalls    = read64("g_Q4DequantCalls");
    out.q8DequantCalls    = read64("g_Q8DequantCalls");
    out.reqInterceptCount = read64("g_ReqInterceptCount");
    out.respInterceptCount= read64("g_RespInterceptCount");
    out.flashAttnCalls    = read64("g_FlashAttnCalls");
    out.flashAttnTiles    = read64("g_FlashAttnTiles");
    out.patchApplyCount   = read64("g_PatchApplyCount");
    out.patchRevertCount  = read64("g_PatchRevertCount");
    out.cotStepsExecuted  = read64("g_CoT_StepsExecuted");
    out.cotArenaBytes     = read64("g_CoT_ArenaBytes");
#else
    // On non-Windows, ASM counters are not available
    (void)out;
#endif
}

// ============================================================================
// Event Retrieval
// ============================================================================
std::vector<TelemetryEvent> UnifiedTelemetryCore::GetRecentEvents(size_t count) const {
    std::lock_guard<std::mutex> lock(m_ringMutex);
    std::vector<TelemetryEvent> result;
    size_t start = (m_ring.size() > count) ? m_ring.size() - count : 0;
    for (size_t i = start; i < m_ring.size(); ++i) {
        result.push_back(m_ring[i]);
    }
    return result;
}

std::vector<TelemetryEvent> UnifiedTelemetryCore::GetEventsBySource(
    TelemetrySource source, size_t count) const
{
    std::lock_guard<std::mutex> lock(m_ringMutex);
    std::vector<TelemetryEvent> result;
    for (auto it = m_ring.rbegin(); it != m_ring.rend() && result.size() < count; ++it) {
        if (it->source == source) result.push_back(*it);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<TelemetryEvent> UnifiedTelemetryCore::GetEventsByCategory(
    const std::string& category, size_t count) const
{
    std::lock_guard<std::mutex> lock(m_ringMutex);
    std::vector<TelemetryEvent> result;
    for (auto it = m_ring.rbegin(); it != m_ring.rend() && result.size() < count; ++it) {
        if (it->category == category) result.push_back(*it);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

// ============================================================================
// Export — Prometheus text format
// ============================================================================
std::string UnifiedTelemetryCore::ExportPrometheus() const {
    std::lock_guard<std::mutex> lock(m_promMutex);
    std::ostringstream ss;

    for (const auto& [name, metric] : m_prometheusMetrics) {
        if (!metric.help.empty()) {
            ss << "# HELP " << metric.name << " " << metric.help << "\n";
        }
        switch (metric.type) {
            case MetricType::Counter:  ss << "# TYPE " << metric.name << " counter\n"; break;
            case MetricType::Gauge:    ss << "# TYPE " << metric.name << " gauge\n"; break;
            case MetricType::Histogram:ss << "# TYPE " << metric.name << " histogram\n"; break;
            case MetricType::Summary:  ss << "# TYPE " << metric.name << " summary\n"; break;
        }

        ss << metric.name;
        if (!metric.labels.empty()) {
            ss << "{";
            bool first = true;
            for (const auto& [k, v] : metric.labels) {
                if (!first) ss << ",";
                ss << k << "=\"" << v << "\"";
                first = false;
            }
            ss << "}";
        }
        ss << " " << std::fixed << std::setprecision(1) << metric.value << "\n";
    }

    return ss.str();
}

// ============================================================================
// Export — JSON
// ============================================================================
std::string UnifiedTelemetryCore::ExportJSON(size_t maxEvents) const {
    auto events = GetRecentEvents(maxEvents);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"engine\": \"UnifiedTelemetryCore\",\n";
    ss << "  \"version\": \"1.0.0\",\n";
    ss << "  \"total_events\": " << m_eventCounter.load() << ",\n";
    ss << "  \"events\": [\n";

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        ss << "    {";
        ss << "\"id\":" << e.id;
        ss << ",\"ts\":" << e.timestampMs;
        ss << ",\"level\":\"" << e.levelString() << "\"";
        ss << ",\"source\":\"" << e.sourceString() << "\"";
        ss << ",\"category\":\"" << e.category << "\"";
        ss << ",\"message\":\"" << e.message << "\"";
        if (e.valueNumeric != 0.0) {
            ss << ",\"value\":" << std::fixed << std::setprecision(2) << e.valueNumeric;
        }
        if (!e.unit.empty()) ss << ",\"unit\":\"" << e.unit << "\"";
        if (e.durationMs >= 0) ss << ",\"duration_ms\":" << e.durationMs;
        ss << "}";
        if (i + 1 < events.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}\n";
    return ss.str();
}

// ============================================================================
// Export — Summary text
// ============================================================================
std::string UnifiedTelemetryCore::ExportSummaryText() const {
    std::ostringstream ss;
    ss << "=== RawrXD Unified Telemetry Summary ===\n";
    ss << "Total events emitted: " << m_eventCounter.load() << "\n";
    ss << "Ring buffer size:     " << m_ring.size() << "/" << TELEMETRY_RING_SIZE << "\n\n";

    // Count by source
    std::map<std::string, size_t> sourceCounts;
    {
        std::lock_guard<std::mutex> lock(m_ringMutex);
        for (const auto& e : m_ring) {
            sourceCounts[e.sourceString()]++;
        }
    }
    ss << "Events by source (in ring):\n";
    for (const auto& [src, cnt] : sourceCounts) {
        ss << "  " << std::setw(14) << std::left << src << ": " << cnt << "\n";
    }

    // Prometheus metrics
    {
        std::lock_guard<std::mutex> lock(m_promMutex);
        ss << "\nPrometheus metrics: " << m_prometheusMetrics.size() << "\n";
        for (const auto& [name, m] : m_prometheusMetrics) {
            ss << "  " << name << " = " << std::fixed << std::setprecision(1)
               << m.value << "\n";
        }
    }

    return ss.str();
}

// ============================================================================
// Export — Flush to file
// ============================================================================
bool UnifiedTelemetryCore::FlushToFile() const {
    if (m_logFilePath.empty()) return false;

    std::ofstream file(m_logFilePath, std::ios::app);
    if (!file.is_open()) return false;

    std::lock_guard<std::mutex> lock(m_ringMutex);
    for (const auto& e : m_ring) {
        file << "{\"id\":" << e.id
             << ",\"ts\":" << e.timestampMs
             << ",\"level\":\"" << e.levelString() << "\""
             << ",\"source\":\"" << e.sourceString() << "\""
             << ",\"category\":\"" << e.category << "\""
             << ",\"msg\":\"" << e.message << "\"";
        if (e.valueNumeric != 0.0)
            file << ",\"val\":" << e.valueNumeric;
        if (e.durationMs >= 0)
            file << ",\"dur\":" << e.durationMs;
        file << "}\n";
    }

    return true;
}

// ============================================================================
// Consumer Management
// ============================================================================
uint64_t UnifiedTelemetryCore::RegisterConsumer(TelemetryConsumer consumer) {
    std::lock_guard<std::mutex> lock(m_consumerMutex);
    uint64_t id = m_nextConsumerId++;
    m_consumers[id] = std::move(consumer);
    return id;
}

void UnifiedTelemetryCore::UnregisterConsumer(uint64_t consumerId) {
    std::lock_guard<std::mutex> lock(m_consumerMutex);
    m_consumers.erase(consumerId);
}

// ============================================================================
// Internal: Route event to all sinks
// ============================================================================
void UnifiedTelemetryCore::EmitInternal(TelemetryEvent&& event) {
    // 1. Add to ring buffer
    {
        std::lock_guard<std::mutex> lock(m_ringMutex);
        if (m_ring.size() >= TELEMETRY_RING_SIZE) {
            m_ring.pop_front();
        }
        m_ring.push_back(event);
    }

    // 2. Write to log file (async would be better, but correctness first)
    WriteToLog(event);

    // 3. Notify registered consumers
    NotifyConsumers(event);
}

void UnifiedTelemetryCore::WriteToLog(const TelemetryEvent& event) {
    if (m_logFilePath.empty()) return;

    // Append single line (JSONL format)
    std::ofstream file(m_logFilePath, std::ios::app);
    if (!file.is_open()) return;

    file << "{\"id\":" << event.id
         << ",\"ts\":" << event.timestampMs
         << ",\"level\":\"" << event.levelString() << "\""
         << ",\"source\":\"" << event.sourceString() << "\""
         << ",\"category\":\"" << event.category << "\""
         << ",\"msg\":\"" << event.message << "\"}\n";
}

void UnifiedTelemetryCore::NotifyConsumers(const TelemetryEvent& event) {
    std::lock_guard<std::mutex> lock(m_consumerMutex);
    for (const auto& [id, consumer] : m_consumers) {
        consumer(event);
    }
}

// ============================================================================
// Utilities
// ============================================================================
uint64_t UnifiedTelemetryCore::NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

} // namespace Telemetry
} // namespace RawrXD
