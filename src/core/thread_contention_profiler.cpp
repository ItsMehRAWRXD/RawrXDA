// ============================================================================
// thread_contention_profiler.cpp — Thread Contention Profiler
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "thread_contention_profiler.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <functional>

namespace RawrXD {
namespace Profiling {

// ============================================================================
// Thread ID hash helper
// ============================================================================
static size_t hashThreadId(std::thread::id tid) {
    return std::hash<std::thread::id>{}(tid);
}

// ============================================================================
// Timestamp helper
// ============================================================================
static uint64_t nowMicroseconds() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());
}

// ============================================================================
// ContentionProfiler — singleton
// ============================================================================
ContentionProfiler& ContentionProfiler::instance() {
    static ContentionProfiler s_instance;
    return s_instance;
}

ContentionProfiler::ContentionProfiler() {
    m_events.resize(8192);
}

// ============================================================================
// Configuration
// ============================================================================
PatchResult ContentionProfiler::configure(const ProfilerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    if (config.maxEventHistory != m_events.size()) {
        m_events.resize(config.maxEventHistory);
        m_eventHead = 0;
    }
    return PatchResult::ok("Profiler configured");
}

ProfilerConfig ContentionProfiler::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

PatchResult ContentionProfiler::enable(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.enabled = on;
    return PatchResult::ok(on ? "Profiler enabled" : "Profiler disabled");
}

bool ContentionProfiler::isEnabled() const {
    return m_config.enabled;
}

// ============================================================================
// Lock Registration
// ============================================================================
PatchResult ContentionProfiler::registerLock(const char* name, uint16_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!name) return PatchResult::error("Null lock name", -1);

    std::string key(name);
    if (m_lockStats.count(key)) return PatchResult::ok("Already registered");

    LockContentionStats stats;
    stats.lockName = name;
    stats.lockLevel = level;
    m_lockStats[key] = stats;

    return PatchResult::ok("Lock registered for profiling");
}

PatchResult ContentionProfiler::unregisterLock(const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_lockStats.find(std::string(name));
    if (it == m_lockStats.end()) return PatchResult::error("Not found", -1);
    m_lockStats.erase(it);
    return PatchResult::ok("Lock unregistered");
}

// ============================================================================
// Instrumentation Hooks
// ============================================================================
void ContentionProfiler::onLockAttempt(const char* lockName) {
    if (!m_config.enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto* stats = findLock(lockName);
    if (stats && m_config.trackWaiters) {
        uint64_t waiters = stats->currentWaiters.fetch_add(1, std::memory_order_relaxed) + 1;
        if (waiters > stats->maxConcurrentWaiters) {
            stats->maxConcurrentWaiters = waiters;
        }
    }
}

void ContentionProfiler::onLockAcquired(const char* lockName, double waitTimeUs) {
    if (!m_config.enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto* stats = findLock(lockName);
    if (!stats) return;

    stats->totalAcquisitions.fetch_add(1, std::memory_order_relaxed);
    if (m_config.trackWaiters) {
        stats->currentWaiters.fetch_sub(1, std::memory_order_relaxed);
    }

    bool contended = (waitTimeUs > m_config.contentionThresholdUs);
    if (contended) {
        stats->contentedAcquisitions.fetch_add(1, std::memory_order_relaxed);
        stats->totalWaitTimeUs += waitTimeUs;
        if (waitTimeUs > stats->maxWaitTimeUs) stats->maxWaitTimeUs = waitTimeUs;

        uint64_t total = stats->contentedAcquisitions.load(std::memory_order_relaxed);
        stats->avgWaitTimeUs = stats->totalWaitTimeUs / static_cast<double>(total);

        // Record event
        ContentionEvent evt;
        evt.eventId = m_nextEventId.fetch_add(1, std::memory_order_relaxed);
        evt.timestampUs = nowMicroseconds();
        evt.waitingThread = std::this_thread::get_id();
        evt.lockName = lockName;
        evt.lockLevel = stats->lockLevel;
        evt.waitTimeUs = waitTimeUs;
        evt.holdTimeUs = 0;
        evt.wasContended = true;
        recordEvent(evt);

        m_stats.contentionEvents.fetch_add(1, std::memory_order_relaxed);

        // Update thread profile
        auto* tp = findOrCreateThread(std::this_thread::get_id());
        if (tp) {
            tp->totalBlockedTimeUs += waitTimeUs;
            if (waitTimeUs > tp->maxSingleWaitUs) {
                tp->maxSingleWaitUs = waitTimeUs;
                tp->mostContestedLock = lockName;
                tp->mostContestedWaitUs = waitTimeUs;
            }
        }
    } else {
        stats->uncontentedAcquisitions.fetch_add(1, std::memory_order_relaxed);
    }

    m_stats.totalSampled.fetch_add(1, std::memory_order_relaxed);
    m_stats.totalBlockedTimeUs += waitTimeUs;
}

void ContentionProfiler::onLockReleased(const char* lockName, double holdTimeUs) {
    if (!m_config.enabled || !m_config.trackHoldTime) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto* stats = findLock(lockName);
    if (!stats) return;

    stats->totalHoldTimeUs += holdTimeUs;
    if (holdTimeUs > stats->maxHoldTimeUs) stats->maxHoldTimeUs = holdTimeUs;

    uint64_t total = stats->totalAcquisitions.load(std::memory_order_relaxed);
    if (total > 0) {
        stats->avgHoldTimeUs = stats->totalHoldTimeUs / static_cast<double>(total);
    }
}

// ============================================================================
// Queries — Per-Lock
// ============================================================================
LockContentionStats ContentionProfiler::getLockStats(const char* name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_lockStats.find(std::string(name));
    if (it != m_lockStats.end()) {
        // Return a snapshot (atomics are relaxed-read)
        return it->second;
    }
    LockContentionStats empty;
    empty.lockName = "NOT_FOUND";
    return empty;
}

std::vector<LockContentionStats> ContentionProfiler::allLockStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LockContentionStats> result;
    result.reserve(m_lockStats.size());
    for (const auto& [k, v] : m_lockStats) {
        result.push_back(v);
    }
    return result;
}

std::vector<LockContentionStats> ContentionProfiler::topContentedLocks(size_t n) const {
    auto all = allLockStats();
    std::sort(all.begin(), all.end(),
        [](const LockContentionStats& a, const LockContentionStats& b) {
            return a.contentedAcquisitions.load(std::memory_order_relaxed) >
                   b.contentedAcquisitions.load(std::memory_order_relaxed);
        });
    if (all.size() > n) all.resize(n);
    return all;
}

// ============================================================================
// Queries — Per-Thread
// ============================================================================
ThreadContentionProfile ContentionProfiler::getThreadProfile(std::thread::id tid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t h = hashThreadId(tid);
    auto it = m_threadProfiles.find(h);
    if (it != m_threadProfiles.end()) return it->second;
    ThreadContentionProfile empty;
    empty.threadId = tid;
    return empty;
}

std::vector<ThreadContentionProfile> ContentionProfiler::allThreadProfiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ThreadContentionProfile> result;
    result.reserve(m_threadProfiles.size());
    for (const auto& [k, v] : m_threadProfiles) {
        result.push_back(v);
    }
    return result;
}

// ============================================================================
// Heat Map
// ============================================================================
std::vector<ContentionHeatCell> ContentionProfiler::getHeatMap() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ContentionHeatCell> result;
    result.reserve(m_heatMap.size());
    for (const auto& [k, v] : m_heatMap) {
        result.push_back(v);
    }
    return result;
}

// ============================================================================
// Event History
// ============================================================================
std::vector<ContentionEvent> ContentionProfiler::recentEvents(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ContentionEvent> result;
    size_t total = std::min(count, m_events.size());
    for (size_t i = 0; i < total; ++i) {
        size_t idx = (m_eventHead + m_events.size() - total + i) % m_events.size();
        if (m_events[idx].eventId > 0) {
            result.push_back(m_events[idx]);
        }
    }
    return result;
}

// ============================================================================
// Stats
// ============================================================================
ProfilerStats ContentionProfiler::getStats() const {
    ProfilerStats snap;
    snap.totalSampled.store(m_stats.totalSampled.load(std::memory_order_relaxed));
    snap.contentionEvents.store(m_stats.contentionEvents.load(std::memory_order_relaxed));
    snap.droppedEvents.store(m_stats.droppedEvents.load(std::memory_order_relaxed));
    snap.totalBlockedTimeUs = m_stats.totalBlockedTimeUs;
    snap.trackedLocks = m_lockStats.size();
    snap.trackedThreads = m_threadProfiles.size();
    return snap;
}

void ContentionProfiler::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalSampled.store(0);
    m_stats.contentionEvents.store(0);
    m_stats.droppedEvents.store(0);
    m_stats.totalBlockedTimeUs = 0;
}

void ContentionProfiler::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lockStats.clear();
    m_threadProfiles.clear();
    m_heatMap.clear();
    std::fill(m_events.begin(), m_events.end(), ContentionEvent{});
    m_eventHead = 0;
    m_nextEventId.store(0);
    resetStats();
}

// ============================================================================
// Export
// ============================================================================
std::string ContentionProfiler::exportStatsJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\"locks\":[\n";
    size_t i = 0;
    for (const auto& [k, v] : m_lockStats) {
        json << "  {\"name\":\"" << k << "\""
             << ",\"total\":" << v.totalAcquisitions.load(std::memory_order_relaxed)
             << ",\"contended\":" << v.contentedAcquisitions.load(std::memory_order_relaxed)
             << ",\"avgWaitUs\":" << v.avgWaitTimeUs
             << ",\"maxWaitUs\":" << v.maxWaitTimeUs
             << ",\"avgHoldUs\":" << v.avgHoldTimeUs
             << ",\"maxHoldUs\":" << v.maxHoldTimeUs
             << ",\"maxWaiters\":" << v.maxConcurrentWaiters
             << "}";
        if (++i < m_lockStats.size()) json << ",";
        json << "\n";
    }
    json << "]}";
    return json.str();
}

std::string ContentionProfiler::exportHeatMapJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "[\n";
    size_t i = 0;
    for (const auto& [k, v] : m_heatMap) {
        json << "  {\"lockA\":\"" << (v.lockA ? v.lockA : "") << "\""
             << ",\"lockB\":\"" << (v.lockB ? v.lockB : "") << "\""
             << ",\"count\":" << v.contentionCount
             << ",\"totalUs\":" << v.totalContentionTimeUs
             << "}";
        if (++i < m_heatMap.size()) json << ",";
        json << "\n";
    }
    json << "]";
    return json.str();
}

std::string ContentionProfiler::exportSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;

    ss << "=== Thread Contention Profiler Summary ===\n\n";
    ss << "Total sampled:     " << m_stats.totalSampled.load() << "\n";
    ss << "Contention events: " << m_stats.contentionEvents.load() << "\n";
    ss << "Total blocked:     " << m_stats.totalBlockedTimeUs << " us\n";
    ss << "Tracked locks:     " << m_lockStats.size() << "\n";
    ss << "Tracked threads:   " << m_threadProfiles.size() << "\n\n";

    ss << "--- Top Contended Locks ---\n";
    // Collect into sortable vector
    std::vector<std::pair<std::string, uint64_t>> sorted;
    for (const auto& [k, v] : m_lockStats) {
        sorted.push_back({k, v.contentedAcquisitions.load(std::memory_order_relaxed)});
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(size_t(10), sorted.size()); ++i) {
        const auto& [name, count] = sorted[i];
        ss << "  " << name << ": " << count << " contended acquisitions\n";
    }

    return ss.str();
}

std::string ContentionProfiler::exportDot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream dot;
    dot << "digraph ContentionMap {\n";
    dot << "  rankdir=LR;\n";
    dot << "  node [shape=box, style=rounded];\n\n";

    for (const auto& [k, v] : m_lockStats) {
        uint64_t contended = v.contentedAcquisitions.load(std::memory_order_relaxed);
        const char* color = "green";
        if (contended > 1000) color = "red";
        else if (contended > 100) color = "orange";
        else if (contended > 10) color = "yellow";

        dot << "  \"" << k << "\" [fillcolor=" << color
            << ", style=\"rounded,filled\""
            << ", label=\"" << k << "\\ncontended=" << contended << "\"];\n";
    }

    for (const auto& [k, v] : m_heatMap) {
        dot << "  \"" << (v.lockA ? v.lockA : "?") << "\" -> \""
            << (v.lockB ? v.lockB : "?") << "\""
            << " [label=\"" << v.contentionCount << "\""
            << ", penwidth=" << std::max(1.0, v.contentionCount / 100.0)
            << "];\n";
    }

    dot << "}\n";
    return dot.str();
}

// ============================================================================
// Private
// ============================================================================
void ContentionProfiler::recordEvent(const ContentionEvent& event) {
    if (m_events.empty()) return;
    m_events[m_eventHead % m_events.size()] = event;
    m_eventHead = (m_eventHead + 1) % m_events.size();
}

LockContentionStats* ContentionProfiler::findLock(const char* name) {
    if (!name) return nullptr;
    auto it = m_lockStats.find(std::string(name));
    if (it != m_lockStats.end()) return &it->second;
    return nullptr;
}

ThreadContentionProfile* ContentionProfiler::findOrCreateThread(std::thread::id tid) {
    size_t h = hashThreadId(tid);
    auto it = m_threadProfiles.find(h);
    if (it != m_threadProfiles.end()) return &it->second;

    ThreadContentionProfile tp;
    tp.threadId = tid;
    m_threadProfiles[h] = tp;
    return &m_threadProfiles[h];
}

// ============================================================================
// ContentionMutex
// ============================================================================
ContentionMutex::ContentionMutex(const char* name, uint16_t level)
    : m_name(name), m_level(level) {
    auto& profiler = ContentionProfiler::instance();
    profiler.registerLock(name, level);
}

void ContentionMutex::lock() {
    auto& profiler = ContentionProfiler::instance();
    profiler.onLockAttempt(m_name);

    auto start = std::chrono::steady_clock::now();
    m_mutex.lock();
    auto end = std::chrono::steady_clock::now();

    double waitUs = std::chrono::duration<double, std::micro>(end - start).count();
    profiler.onLockAcquired(m_name, waitUs);

    m_acquireTime = end;
}

bool ContentionMutex::try_lock() {
    if (!m_mutex.try_lock()) return false;
    m_acquireTime = std::chrono::steady_clock::now();

    auto& profiler = ContentionProfiler::instance();
    profiler.onLockAcquired(m_name, 0.0);
    return true;
}

void ContentionMutex::unlock() {
    auto now = std::chrono::steady_clock::now();
    double holdUs = std::chrono::duration<double, std::micro>(now - m_acquireTime).count();

    auto& profiler = ContentionProfiler::instance();
    profiler.onLockReleased(m_name, holdUs);

    m_mutex.unlock();
}

} // namespace Profiling
} // namespace RawrXD
