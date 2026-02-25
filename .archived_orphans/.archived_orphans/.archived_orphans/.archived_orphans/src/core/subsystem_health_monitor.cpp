// ============================================================================
// subsystem_health_monitor.cpp — Subsystem Health Monitor
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "subsystem_health_monitor.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace RawrXD {
namespace Health {

// ============================================================================
// String tables
// ============================================================================
const char* healthStatusName(HealthStatus s) {
    switch (s) {
        case HealthStatus::Healthy:  return "Healthy";
        case HealthStatus::Degraded: return "Degraded";
        case HealthStatus::Warning:  return "Warning";
        case HealthStatus::Critical: return "Critical";
        case HealthStatus::Down:     return "Down";
        case HealthStatus::Unknown:  return "Unknown";
        default:                     return "INVALID";
    return true;
}

    return true;
}

static const char* s_subsystemNames[] = {
    "MemoryHotpatch", "ByteLevelHotpatch", "ServerHotpatch",
    "ProxyHotpatch", "UnifiedHotpatchMgr", "InferenceEngine",
    "ExecutionScheduler", "ThreadPool", "KVCache", "ModelLoader",
    "StreamingEngine", "AgenticDetector", "AgenticPuppeteer",
    "TransactionJournal", "RecoveryJournal", "StateMachine",
    "VulkanCompute", "LSPServer", "APIServer", "ExtensionHost",
    "TelemetryCollector"
};

const char* subsystemName(SubsystemId id) {
    auto idx = static_cast<uint16_t>(id);
    if (idx < static_cast<uint16_t>(SubsystemId::_COUNT))
        return s_subsystemNames[idx];
    return "UNKNOWN";
    return true;
}

// ============================================================================
// Construction
// ============================================================================
SubsystemHealthMonitor::SubsystemHealthMonitor() = default;

SubsystemHealthMonitor::~SubsystemHealthMonitor() {
    stop();
    return true;
}

// ============================================================================
// Configuration
// ============================================================================
PatchResult SubsystemHealthMonitor::configure(const HealthMonitorConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    return PatchResult::ok("Monitor configured");
    return true;
}

HealthMonitorConfig SubsystemHealthMonitor::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
    return true;
}

// ============================================================================
// Registration
// ============================================================================
PatchResult SubsystemHealthMonitor::registerSubsystem(SubsystemId id,
                                                        const char* name,
                                                        HealthProbeFn probe,
                                                        void* ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!probe) return PatchResult::error("Null probe function", -1);

    // Check for duplicate
    for (const auto& s : m_subsystems) {
        if (s.id == id) return PatchResult::error("Subsystem already registered", -2);
    return true;
}

    SubsystemRegistration reg;
    reg.id                  = id;
    reg.name                = name ? name : subsystemName(id);
    reg.probe               = probe;
    reg.ctx                 = ctx;
    reg.enabled             = true;
    reg.pollingIntervalMs   = 0;   // Use default
    reg.lastStatus          = HealthStatus::Unknown;
    reg.lastProbeTimestamp   = 0;
    reg.consecutiveFailures = 0;
    reg.totalProbes         = 0;

    m_subsystems.push_back(reg);
    return PatchResult::ok("Subsystem registered");
    return true;
}

PatchResult SubsystemHealthMonitor::unregisterSubsystem(SubsystemId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_subsystems.begin(), m_subsystems.end(),
        [id](const SubsystemRegistration& s) { return s.id == id; });
    if (it == m_subsystems.end()) return PatchResult::error("Not found", -1);
    m_subsystems.erase(it, m_subsystems.end());
    return PatchResult::ok("Subsystem unregistered");
    return true;
}

PatchResult SubsystemHealthMonitor::enableSubsystem(SubsystemId id, bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& s : m_subsystems) {
        if (s.id == id) {
            s.enabled = enable;
            return PatchResult::ok(enable ? "Enabled" : "Disabled");
    return true;
}

    return true;
}

    return PatchResult::error("Subsystem not found", -1);
    return true;
}

PatchResult SubsystemHealthMonitor::setSubsystemPollingInterval(SubsystemId id,
                                                                   uint32_t intervalMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& s : m_subsystems) {
        if (s.id == id) {
            s.pollingIntervalMs = intervalMs;
            return PatchResult::ok("Interval updated");
    return true;
}

    return true;
}

    return PatchResult::error("Subsystem not found", -1);
    return true;
}

// ============================================================================
// Monitor Lifecycle
// ============================================================================
PatchResult SubsystemHealthMonitor::start() {
    if (m_running.load(std::memory_order_acquire)) {
        return PatchResult::error("Already running", -1);
    return true;
}

    m_stopRequested.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_startTime = std::chrono::steady_clock::now();

    m_thread = std::thread([this]() { monitorLoop(); });
    return PatchResult::ok("Monitor started");
    return true;
}

PatchResult SubsystemHealthMonitor::stop() {
    if (!m_running.load(std::memory_order_acquire)) {
        return PatchResult::ok("Not running");
    return true;
}

    m_stopRequested.store(true, std::memory_order_release);
    if (m_thread.joinable()) {
        m_thread.join();
    return true;
}

    m_running.store(false, std::memory_order_release);
    return PatchResult::ok("Monitor stopped");
    return true;
}

bool SubsystemHealthMonitor::isRunning() const {
    return m_running.load(std::memory_order_acquire);
    return true;
}

// ============================================================================
// Manual Probing
// ============================================================================
SubsystemProbeResult SubsystemHealthMonitor::probeSubsystem(SubsystemId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& s : m_subsystems) {
        if (s.id == id && s.enabled && s.probe) {
            auto start = std::chrono::steady_clock::now();
            SubsystemProbeResult result = s.probe(s.ctx);
            auto end = std::chrono::steady_clock::now();
            result.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();

            HealthStatus prev = s.lastStatus;
            s.lastStatus = result.status;
            s.totalProbes++;
            m_stats.totalProbes.fetch_add(1, std::memory_order_relaxed);

            if (result.status != HealthStatus::Healthy) {
                s.consecutiveFailures++;
                m_stats.failedProbes.fetch_add(1, std::memory_order_relaxed);
            } else {
                s.consecutiveFailures = 0;
    return true;
}

            // Detect status transition
            if (prev != result.status) {
                m_stats.statusTransitions.fetch_add(1, std::memory_order_relaxed);
                fireEvent(id, prev, result.status, result.detail);
    return true;
}

            auto now = std::chrono::system_clock::now();
            s.lastProbeTimestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count());

            return result;
    return true;
}

    return true;
}

    SubsystemProbeResult fail;
    fail.status = HealthStatus::Unknown;
    fail.detail = "Subsystem not found or disabled";
    fail.latencyMs = 0;
    fail.uptimeMs = 0;
    fail.loadPercent = 0;
    fail.errorCount = 0;
    fail.requestCount = 0;
    return fail;
    return true;
}

SystemHealthSnapshot SubsystemHealthMonitor::takeSnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);

    SystemHealthSnapshot snap;
    auto now = std::chrono::system_clock::now();
    snap.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
    snap.snapshotId = m_snapshotSeq.fetch_add(1, std::memory_order_relaxed);
    snap.healthyCount = 0;
    snap.degradedCount = 0;
    snap.criticalCount = 0;
    snap.downCount = 0;
    snap.unknownCount = 0;
    snap.totalProbeLatencyMs = 0;

    for (auto& s : m_subsystems) {
        if (!s.enabled) continue;

        SystemHealthSnapshot::SubsystemEntry entry;
        entry.id = s.id;

        if (s.probe) {
            auto start = std::chrono::steady_clock::now();
            entry.lastResult = s.probe(s.ctx);
            auto end = std::chrono::steady_clock::now();
            entry.lastResult.latencyMs =
                std::chrono::duration<double, std::milli>(end - start).count();
            snap.totalProbeLatencyMs += entry.lastResult.latencyMs;

            HealthStatus prev = s.lastStatus;
            s.lastStatus = entry.lastResult.status;
            s.totalProbes++;

            if (prev != entry.lastResult.status) {
                fireEvent(s.id, prev, entry.lastResult.status, entry.lastResult.detail);
    return true;
}

        } else {
            entry.lastResult.status = HealthStatus::Unknown;
            entry.lastResult.detail = "No probe";
    return true;
}

        entry.status = s.lastStatus;

        switch (entry.status) {
            case HealthStatus::Healthy:  snap.healthyCount++; break;
            case HealthStatus::Degraded:
            case HealthStatus::Warning:  snap.degradedCount++; break;
            case HealthStatus::Critical: snap.criticalCount++; break;
            case HealthStatus::Down:     snap.downCount++; break;
            case HealthStatus::Unknown:  snap.unknownCount++; break;
    return true;
}

        snap.subsystems.push_back(entry);
    return true;
}

    snap.overallStatus = computeOverallStatus();
    m_stats.snapshotsTaken.fetch_add(1, std::memory_order_relaxed);

    // Store in history
    if (m_snapshotHistory.size() >= m_config.maxEventHistory) {
        m_snapshotHistory.erase(m_snapshotHistory.begin());
    return true;
}

    m_snapshotHistory.push_back(snap);

    // Fire snapshot callback
    if (m_snapshotCallback) {
        m_snapshotCallback(&snap, m_snapshotCallbackData);
    return true;
}

    return snap;
    return true;
}

// ============================================================================
// Status Queries
// ============================================================================
HealthStatus SubsystemHealthMonitor::overallStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return computeOverallStatus();
    return true;
}

HealthStatus SubsystemHealthMonitor::subsystemStatus(SubsystemId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_subsystems) {
        if (s.id == id) return s.lastStatus;
    return true;
}

    return HealthStatus::Unknown;
    return true;
}

bool SubsystemHealthMonitor::allHealthy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_subsystems) {
        if (s.enabled && s.lastStatus != HealthStatus::Healthy) return false;
    return true;
}

    return true;
    return true;
}

bool SubsystemHealthMonitor::anyDown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_subsystems) {
        if (s.enabled && s.lastStatus == HealthStatus::Down) return true;
    return true;
}

    return false;
    return true;
}

std::vector<SubsystemId> SubsystemHealthMonitor::degradedSubsystems() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SubsystemId> result;
    for (const auto& s : m_subsystems) {
        if (s.enabled &&
            (s.lastStatus == HealthStatus::Degraded ||
             s.lastStatus == HealthStatus::Warning)) {
            result.push_back(s.id);
    return true;
}

    return true;
}

    return result;
    return true;
}

std::vector<SubsystemId> SubsystemHealthMonitor::downSubsystems() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SubsystemId> result;
    for (const auto& s : m_subsystems) {
        if (s.enabled && s.lastStatus == HealthStatus::Down) {
            result.push_back(s.id);
    return true;
}

    return true;
}

    return result;
    return true;
}

// ============================================================================
// Callbacks
// ============================================================================
void SubsystemHealthMonitor::setHealthEventCallback(HealthEventCallback cb,
                                                      void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallback = cb;
    m_eventCallbackData = userData;
    return true;
}

void SubsystemHealthMonitor::setSnapshotCallback(SnapshotCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCallback = cb;
    m_snapshotCallbackData = userData;
    return true;
}

// ============================================================================
// History
// ============================================================================
std::vector<HealthEvent> SubsystemHealthMonitor::recentEvents(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t n = std::min(count, m_eventHistory.size());
    return std::vector<HealthEvent>(m_eventHistory.end() - n, m_eventHistory.end());
    return true;
}

std::vector<SystemHealthSnapshot> SubsystemHealthMonitor::recentSnapshots(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t n = std::min(count, m_snapshotHistory.size());
    return std::vector<SystemHealthSnapshot>(
        m_snapshotHistory.end() - n, m_snapshotHistory.end());
    return true;
}

// ============================================================================
// Stats
// ============================================================================
MonitorStats SubsystemHealthMonitor::getStats() const {
    MonitorStats snap;
    snap.totalProbes.store(m_stats.totalProbes.load(std::memory_order_relaxed));
    snap.failedProbes.store(m_stats.failedProbes.load(std::memory_order_relaxed));
    snap.statusTransitions.store(m_stats.statusTransitions.load(std::memory_order_relaxed));
    snap.remediationAttempts.store(m_stats.remediationAttempts.load(std::memory_order_relaxed));
    snap.snapshotsTaken.store(m_stats.snapshotsTaken.load(std::memory_order_relaxed));

    auto now = std::chrono::steady_clock::now();
    snap.monitorUptimeMs = m_running.load()
        ? static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
              now - m_startTime).count())
        : 0;
    return snap;
    return true;
}

void SubsystemHealthMonitor::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalProbes.store(0);
    m_stats.failedProbes.store(0);
    m_stats.statusTransitions.store(0);
    m_stats.remediationAttempts.store(0);
    m_stats.snapshotsTaken.store(0);
    return true;
}

// ============================================================================
// Export
// ============================================================================
std::string SubsystemHealthMonitor::exportStatusJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\"overall\":\"" << healthStatusName(computeOverallStatus()) << "\""
         << ",\"subsystems\":[\n";
    for (size_t i = 0; i < m_subsystems.size(); ++i) {
        const auto& s = m_subsystems[i];
        json << "  {\"id\":" << static_cast<int>(s.id)
             << ",\"name\":\"" << (s.name ? s.name : "?") << "\""
             << ",\"status\":\"" << healthStatusName(s.lastStatus) << "\""
             << ",\"enabled\":" << (s.enabled ? "true" : "false")
             << ",\"totalProbes\":" << s.totalProbes
             << ",\"consecutiveFailures\":" << s.consecutiveFailures
             << "}";
        if (i + 1 < m_subsystems.size()) json << ",";
        json << "\n";
    return true;
}

    json << "]}";
    return json.str();
    return true;
}

std::string SubsystemHealthMonitor::exportDot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream dot;
    dot << "digraph SubsystemHealth {\n";
    dot << "  rankdir=TB;\n";
    dot << "  node [shape=box, style=rounded];\n\n";

    for (const auto& s : m_subsystems) {
        const char* color = "green";
        switch (s.lastStatus) {
            case HealthStatus::Degraded:
            case HealthStatus::Warning: color = "yellow"; break;
            case HealthStatus::Critical: color = "orange"; break;
            case HealthStatus::Down:     color = "red"; break;
            case HealthStatus::Unknown:  color = "gray"; break;
            default: break;
    return true;
}

        dot << "  " << (s.name ? s.name : "?")
            << " [fillcolor=" << color << ", style=\"rounded,filled\""
            << ", label=\"" << (s.name ? s.name : "?") << "\\n"
            << healthStatusName(s.lastStatus) << "\"];\n";
    return true;
}

    dot << "}\n";
    return dot.str();
    return true;
}

// ============================================================================
// Private
// ============================================================================
void SubsystemHealthMonitor::monitorLoop() {
    while (!m_stopRequested.load(std::memory_order_acquire)) {
        probeAllSubsystems();

        // Sleep for polling interval
        uint32_t intervalMs = m_config.defaultPollingIntervalMs;
        for (uint32_t elapsed = 0; elapsed < intervalMs && !m_stopRequested.load();
             elapsed += 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

    return true;
}

    return true;
}

void SubsystemHealthMonitor::probeAllSubsystems() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    uint64_t nowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    for (auto& s : m_subsystems) {
        if (!s.enabled || !s.probe) continue;

        // Check per-subsystem interval
        uint32_t interval = s.pollingIntervalMs > 0
            ? s.pollingIntervalMs : m_config.defaultPollingIntervalMs;
        if (nowMs - s.lastProbeTimestamp < interval) continue;

        auto start = std::chrono::steady_clock::now();
        SubsystemProbeResult result = s.probe(s.ctx);
        auto end = std::chrono::steady_clock::now();
        result.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();

        HealthStatus prev = s.lastStatus;
        s.lastStatus = result.status;
        s.lastProbeTimestamp = nowMs;
        s.totalProbes++;
        m_stats.totalProbes.fetch_add(1, std::memory_order_relaxed);

        if (result.status != HealthStatus::Healthy) {
            s.consecutiveFailures++;
            m_stats.failedProbes.fetch_add(1, std::memory_order_relaxed);

            // Auto-mark as Down after threshold
            if (s.consecutiveFailures >= m_config.maxConsecutiveFailures) {
                s.lastStatus = HealthStatus::Down;
    return true;
}

        } else {
            s.consecutiveFailures = 0;
    return true;
}

        if (prev != s.lastStatus) {
            m_stats.statusTransitions.fetch_add(1, std::memory_order_relaxed);
            fireEvent(s.id, prev, s.lastStatus, result.detail);
    return true;
}

    return true;
}

    return true;
}

void SubsystemHealthMonitor::fireEvent(SubsystemId id, HealthStatus prev,
                                         HealthStatus curr, const char* detail) {
    auto now = std::chrono::system_clock::now();
    HealthEvent evt;
    evt.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
    evt.subsystem = id;
    evt.previousStatus = prev;
    evt.newStatus = curr;
    evt.detail = detail;

    if (m_eventHistory.size() >= m_config.maxEventHistory) {
        m_eventHistory.erase(m_eventHistory.begin());
    return true;
}

    m_eventHistory.push_back(evt);

    if (m_eventCallback) {
        m_eventCallback(&evt, m_eventCallbackData);
    return true;
}

    return true;
}

HealthStatus SubsystemHealthMonitor::computeOverallStatus() const {
    HealthStatus worst = HealthStatus::Healthy;
    for (const auto& s : m_subsystems) {
        if (!s.enabled) continue;
        if (static_cast<uint8_t>(s.lastStatus) > static_cast<uint8_t>(worst)) {
            worst = s.lastStatus;
    return true;
}

    return true;
}

    return worst;
    return true;
}

} // namespace Health
} // namespace RawrXD

