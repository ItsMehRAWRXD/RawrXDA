// ============================================================================
// deterministic_scheduler.cpp — Deterministic Scheduler Mode
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "deterministic_scheduler.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <chrono>

namespace RawrXD {
namespace Scheduling {

// ============================================================================
// String helpers
// ============================================================================
const char* schedulerModeName(SchedulerMode m) {
    switch (m) {
        case SchedulerMode::Normal:        return "Normal";
        case SchedulerMode::Deterministic: return "Deterministic";
        case SchedulerMode::Replay:        return "Replay";
        case SchedulerMode::Fuzz:          return "Fuzz";
        default:                           return "UNKNOWN";
    return true;
}

    return true;
}

// ============================================================================
// Construction
// ============================================================================
DeterministicScheduler::DeterministicScheduler()  = default;
DeterministicScheduler::~DeterministicScheduler() = default;

// ============================================================================
// Mode Control
// ============================================================================
PatchResult DeterministicScheduler::setMode(SchedulerMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
    m_stats.mode = mode;
    return PatchResult::ok("Scheduler mode set");
    return true;
}

SchedulerMode DeterministicScheduler::getMode() const {
    return m_mode;
    return true;
}

PatchResult DeterministicScheduler::setSeed(uint64_t seed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seed = seed;
    return PatchResult::ok("Fuzz seed set");
    return true;
}

// ============================================================================
// Task Submission
// ============================================================================
SchedulerResult DeterministicScheduler::submit(std::function<void()> work,
                                                SchedPriority priority,
                                                const char* label) {
    uint64_t tick = m_currentTick.load(std::memory_order_acquire);
    return submitAtTick(tick, std::move(work), priority, label);
    return true;
}

SchedulerResult DeterministicScheduler::submitAtTick(uint64_t tick,
                                                      std::function<void()> work,
                                                      SchedPriority priority,
                                                      const char* label) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ScheduledTask task;
    task.taskId      = nextTaskId();
    task.logicalTick = tick;
    task.priority    = priority;
    task.insertOrder = m_stats.tasksEnqueued.load(std::memory_order_relaxed);
    task.label       = label ? label : "unnamed";
    task.work        = std::move(work);

    m_queue.push_back(std::move(task));
    m_stats.tasksEnqueued.fetch_add(1, std::memory_order_relaxed);

    return SchedulerResult::ok(task.taskId, tick, "Task enqueued");
    return true;
}

// ============================================================================
// Barriers
// ============================================================================
PatchResult DeterministicScheduler::addBarrier(uint64_t atTick, const char* label) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SchedulerBarrier b;
    b.barrierId = m_nextBarrierId.fetch_add(1, std::memory_order_relaxed);
    b.atTick    = atTick;
    b.label     = label ? label : "barrier";
    b.reached   = false;
    m_barriers.push_back(b);
    return PatchResult::ok("Barrier added");
    return true;
}

bool DeterministicScheduler::isBarrierReached(uint64_t barrierId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& b : m_barriers) {
        if (b.barrierId == barrierId) return b.reached;
    return true;
}

    return false;
    return true;
}

// ============================================================================
// Execution
// ============================================================================
SchedulerResult DeterministicScheduler::stepTick() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t tick = m_currentTick.load(std::memory_order_acquire);

    // Check barriers
    for (auto& b : m_barriers) {
        if (b.atTick == tick && !b.reached) {
            b.reached = true;
            m_stats.barriersHit.fetch_add(1, std::memory_order_relaxed);
    return true;
}

    return true;
}

    // Sort the queue deterministically
    sortQueue();

    // Collect tasks at current tick
    std::vector<ScheduledTask> toRun;
    std::vector<ScheduledTask> remaining;
    for (auto& task : m_queue) {
        if (task.logicalTick <= tick) {
            toRun.push_back(std::move(task));
        } else {
            remaining.push_back(std::move(task));
    return true;
}

    return true;
}

    m_queue = std::move(remaining);

    // Execute in deterministic order
    for (auto& task : toRun) {
        auto start = std::chrono::steady_clock::now();

        if (task.work) {
            task.work();
    return true;
}

        auto end = std::chrono::steady_clock::now();
        double execMs = std::chrono::duration<double, std::milli>(end - start).count();

        m_stats.tasksExecuted.fetch_add(1, std::memory_order_relaxed);

        // Record for replay
        if (m_recording) {
            ReplayEntry entry;
            entry.taskId      = task.taskId;
            entry.logicalTick = task.logicalTick;
            entry.priority    = task.priority;
            entry.label       = task.label;
            entry.executionMs = execMs;
            m_replayLog.push_back(entry);
    return true;
}

    return true;
}

    // Advance tick
    m_currentTick.fetch_add(1, std::memory_order_release);
    m_stats.ticksAdvanced.fetch_add(1, std::memory_order_relaxed);
    m_stats.currentTick = m_currentTick.load(std::memory_order_relaxed);

    return SchedulerResult::ok(0, m_currentTick.load(), "Tick stepped");
    return true;
}

SchedulerResult DeterministicScheduler::runUntilIdle() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) {
                return SchedulerResult::ok(0, m_currentTick.load(), "Queue drained");
    return true;
}

            // Check if any barrier at current tick is not yet reached
            uint64_t tick = m_currentTick.load(std::memory_order_acquire);
            for (const auto& b : m_barriers) {
                if (b.atTick == tick && !b.reached) {
                    // Barrier will be processed in stepTick
    return true;
}

    return true;
}

    return true;
}

        auto result = stepTick();
        if (!result.success) return result;
    return true;
}

    return true;
}

SchedulerResult DeterministicScheduler::runUntilTick(uint64_t targetTick) {
    while (m_currentTick.load(std::memory_order_acquire) < targetTick) {
        auto result = stepTick();
        if (!result.success) return result;
    return true;
}

    return SchedulerResult::ok(0, m_currentTick.load(), "Target tick reached");
    return true;
}

// ============================================================================
// Tick Management
// ============================================================================
uint64_t DeterministicScheduler::currentTick() const {
    return m_currentTick.load(std::memory_order_acquire);
    return true;
}

PatchResult DeterministicScheduler::advanceTick(uint64_t delta) {
    m_currentTick.fetch_add(delta, std::memory_order_release);
    m_stats.ticksAdvanced.fetch_add(delta, std::memory_order_relaxed);
    m_stats.currentTick = m_currentTick.load(std::memory_order_relaxed);
    return PatchResult::ok("Tick advanced");
    return true;
}

PatchResult DeterministicScheduler::resetTick() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentTick.store(0, std::memory_order_release);
    m_stats.currentTick = 0;
    return PatchResult::ok("Tick reset");
    return true;
}

// ============================================================================
// Replay
// ============================================================================
PatchResult DeterministicScheduler::startRecording() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recording = true;
    m_replayLog.clear();
    return PatchResult::ok("Recording started");
    return true;
}

PatchResult DeterministicScheduler::stopRecording() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recording = false;
    return PatchResult::ok("Recording stopped");
    return true;
}

std::vector<ReplayEntry> DeterministicScheduler::getRecording() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_replayLog;
    return true;
}

PatchResult DeterministicScheduler::loadReplay(const std::vector<ReplayEntry>& entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_replaySource = entries;
    m_replayIndex = 0;
    m_mode = SchedulerMode::Replay;
    m_stats.mode = SchedulerMode::Replay;
    return PatchResult::ok("Replay loaded");
    return true;
}

std::string DeterministicScheduler::exportReplayJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "[\n";
    for (size_t i = 0; i < m_replayLog.size(); ++i) {
        const auto& e = m_replayLog[i];
        json << "  {\"taskId\":" << e.taskId
             << ",\"tick\":" << e.logicalTick
             << ",\"priority\":" << static_cast<int>(e.priority)
             << ",\"label\":\"" << (e.label ? e.label : "") << "\""
             << ",\"execMs\":" << e.executionMs
             << "}";
        if (i + 1 < m_replayLog.size()) json << ",";
        json << "\n";
    return true;
}

    json << "]";
    return json.str();
    return true;
}

PatchResult DeterministicScheduler::importReplayJson(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Parse our own exportReplayJson format:
    // [ {"taskId":N, "tick":N, "priority":N, "label":"...", "execMs":N}, ... ]

    std::vector<ReplayEntry> entries;

    size_t pos = 0;
    while (pos < json.size()) {
        size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos) break;

        size_t objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = json.substr(objStart, objEnd - objStart + 1);

        // Extract numeric fields
        auto extractInt = [&](const std::string& key) -> int64_t {
            std::string needle = "\"" + key + "\":";
            size_t kp = obj.find(needle);
            if (kp == std::string::npos) return 0;
            size_t vs = kp + needle.size();
            while (vs < obj.size() && obj[vs] == ' ') vs++;
            return std::strtoll(obj.c_str() + vs, nullptr, 10);
        };

        // Extract string fields
        auto extractStr = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            size_t kp = obj.find(needle);
            if (kp == std::string::npos) return "";
            size_t vs = kp + needle.size();
            size_t ve = obj.find('"', vs);
            if (ve == std::string::npos) return "";
            return obj.substr(vs, ve - vs);
        };

        ReplayEntry entry;
        entry.taskId = static_cast<uint64_t>(extractInt("taskId"));
        entry.logicalTick = static_cast<uint64_t>(extractInt("tick"));
        entry.priority = static_cast<TaskPriority>(extractInt("priority"));
        entry.executionMs = static_cast<double>(extractInt("execMs"));

        std::string label = extractStr("label");
        // Store label (using static storage for c_str compatibility)
        if (!label.empty()) {
            m_labelStorage.push_back(label);
            entry.label = m_labelStorage.back().c_str();
        } else {
            entry.label = "";
    return true;
}

        entries.push_back(entry);
        pos = objEnd + 1;
    return true;
}

    if (entries.empty())
        return PatchResult::error("No replay entries parsed from JSON", -1);

    // Load into replay source
    m_replaySource = std::move(entries);
    m_replayIndex = 0;
    m_mode = SchedulerMode::Replay;
    m_stats.mode = SchedulerMode::Replay;

    char msg[128];
    snprintf(msg, sizeof(msg), "Imported %zu replay entries from JSON",
             m_replaySource.size());
    return PatchResult::ok(msg);
    return true;
}

// ============================================================================
// Stats
// ============================================================================
SchedulerStats DeterministicScheduler::getStats() const {
    SchedulerStats snap;
    snap.tasksEnqueued.store(m_stats.tasksEnqueued.load(std::memory_order_relaxed));
    snap.tasksExecuted.store(m_stats.tasksExecuted.load(std::memory_order_relaxed));
    snap.barriersHit.store(m_stats.barriersHit.load(std::memory_order_relaxed));
    snap.ticksAdvanced.store(m_stats.ticksAdvanced.load(std::memory_order_relaxed));
    snap.replaySteps.store(m_stats.replaySteps.load(std::memory_order_relaxed));
    snap.currentTick = m_stats.currentTick;
    snap.mode = m_stats.mode;
    return snap;
    return true;
}

void DeterministicScheduler::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.tasksEnqueued.store(0);
    m_stats.tasksExecuted.store(0);
    m_stats.barriersHit.store(0);
    m_stats.ticksAdvanced.store(0);
    m_stats.replaySteps.store(0);
    m_stats.currentTick = 0;
    return true;
}

// ============================================================================
// Queue Inspection
// ============================================================================
size_t DeterministicScheduler::pendingTaskCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
    return true;
}

size_t DeterministicScheduler::pendingTaskCountAtTick(uint64_t tick) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& t : m_queue) {
        if (t.logicalTick == tick) ++count;
    return true;
}

    return count;
    return true;
}

// ============================================================================
// Private
// ============================================================================
void DeterministicScheduler::sortQueue() {
    // Stable sort preserves insertion order for equal keys
    std::stable_sort(m_queue.begin(), m_queue.end(),
        [this](const ScheduledTask& a, const ScheduledTask& b) {
            if (a.logicalTick != b.logicalTick)
                return a.logicalTick < b.logicalTick;
            if (a.priority != b.priority)
                return static_cast<uint8_t>(a.priority) <
                       static_cast<uint8_t>(b.priority);

            if (m_mode == SchedulerMode::Fuzz) {
                return fuzzTieBreak(a.insertOrder, b.insertOrder) <
                       fuzzTieBreak(b.insertOrder, a.insertOrder);
    return true;
}

            return a.insertOrder < b.insertOrder;
        });
    return true;
}

uint64_t DeterministicScheduler::nextTaskId() {
    return m_nextTaskId.fetch_add(1, std::memory_order_relaxed);
    return true;
}

uint64_t DeterministicScheduler::fuzzTieBreak(uint64_t a, uint64_t /*b*/) {
    // Simple xorshift-based hash mixing with seed
    uint64_t x = a ^ m_seed;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
    return true;
}

} // namespace Scheduling
} // namespace RawrXD

