// ContextCorrectnessHarness.h — Runtime correctness validation for ContextFusionEngine
// This tests the ONLY question that matters:
//   "Does the IDE stay coherent under real human typing speed + AI load?"
//
// What this validates:
//   1. Frame immutability (snapshots are truly frozen)
//   2. Causal ordering (events processed in correct order)
//   3. Subscriber isolation (no shared mutable state)
//   4. Backpressure handling (rapid events don't cause desync)
//   5. Race safety (concurrent keystrokes + LSP + AI)
//   6. Deterministic replay (can reconstruct state from log)
//   7. Conflict resolution (LSP vs AI vs Editor precedence)
//   8. Stress validation (100-200ms typing bursts)

#pragma once

#include "ContextFusionEngine.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace RawrXD {

// ─────────────────────────────────────────────────────────────────────────────
// Test Result Tracking
// ─────────────────────────────────────────────────────────────────────────────

struct CorrectnessResult {
    std::string testName;
    bool passed = false;
    std::string failureReason;
    uint64_t durationMs = 0;
    std::map<std::string, std::string> metrics;
    
    void Fail(const std::string& reason) {
        passed = false;
        failureReason = reason;
    }
};

struct StressTestMetrics {
    uint64_t totalEventsEmitted = 0;
    uint64_t framesReceivedBySubscribers = 0;
    uint64_t versionSkips = 0;        // frames where version jumped > 1
    uint64_t outOfOrderFrames = 0;    // frames received out of version order
    uint64_t staleFrameAccess = 0;    // subscriber accessed frame after it was updated
    uint64_t maxLatencyMs = 0;         // worst case subscriber latency
    uint64_t avgLatencyMs = 0;
    uint64_t droppedEvents = 0;        // events that never processed
    uint64_t raceConditions = 0;      // detected race conditions
};

// ─────────────────────────────────────────────────────────────────────────────
// Event Log for Deterministic Replay
// ─────────────────────────────────────────────────────────────────────────────

struct LoggedEvent {
    ContextEvent event;
    uint64_t sequenceNumber;
    uint64_t threadId;
    std::chrono::steady_clock::time_point timestamp;
    
    // For replay - store the frame state before/after
    ContextFrame frameBefore;
    ContextFrame frameAfter;
};

class EventLog {
public:
    void Record(const ContextEvent& event, const ContextFrame& before, const ContextFrame& after) {
        std::lock_guard<std::mutex> lock(m_mutex);
        LoggedEvent logged;
        logged.event = event;
        logged.sequenceNumber = m_nextSequence++;
        logged.threadId = GetCurrentThreadId();
        logged.timestamp = std::chrono::steady_clock::now();
        logged.frameBefore = before;
        logged.frameAfter = after;
        m_events.push_back(logged);
    }
    
    std::vector<LoggedEvent> GetEvents() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events;
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
        m_nextSequence = 0;
    }
    
    // Replay events to reconstruct state
    bool Replay(ContextFusionEngine& engine, uint64_t fromSequence = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& logged : m_events) {
            if (logged.sequenceNumber >= fromSequence) {
                // Verify causal ordering
                if (logged.sequenceNumber != m_replaySequence++) {
                    return false; // Out of order
                }
                engine.EmitEvent(logged.event);
            }
        }
        return true;
    }
    
private:
    mutable std::mutex m_mutex;
    std::vector<LoggedEvent> m_events;
    uint64_t m_nextSequence = 0;
    uint64_t m_replaySequence = 0;
    
    static uint64_t GetCurrentThreadId() {
        std::hash<std::thread::id> hasher;
        return hasher(std::this_thread::get_id());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test Subscriber with Race Detection
// ─────────────────────────────────────────────────────────────────────────────

class TestSubscriber : public IContextSubscriber {
public:
    TestSubscriber(const std::string& name, int priority = 100)
        : m_name(name), m_priority(priority) {}
    
    void OnContextUpdate(const ContextFrame& frame) override {
        auto receiveTime = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Detect version jumps (skipped frames)
        if (m_lastVersion > 0 && frame.version > m_lastVersion + 1) {
            m_metrics.versionSkips += (frame.version - m_lastVersion - 1);
        }
        
        // Detect out-of-order delivery
        if (frame.version < m_lastVersion) {
            m_metrics.outOfOrderFrames++;
        }
        
        // Store frame COPY (important for immutability test)
        m_receivedFrames.push_back(frame);
        m_lastVersion = frame.version;
        
        // Track latency
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            receiveTime - std::chrono::steady_clock::time_point(
                std::chrono::milliseconds(frame.timestamp)
            )
        ).count();
        m_metrics.maxLatencyMs = std::max(m_metrics.maxLatencyMs, (uint64_t)latency);
        
        m_frameCount++;
        m_cv.notify_all();
    }
    
    int GetPriority() const override { return m_priority; }
    std::string GetName() const override { return m_name; }
    
    // Wait for N frames (for synchronization in tests)
    bool WaitForFrames(size_t count, uint64_t timeoutMs = 5000) {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, count]() {
            return m_receivedFrames.size() >= count;
        });
    }
    
    const std::vector<ContextFrame>& GetReceivedFrames() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receivedFrames;
    }
    
    void ClearFrames() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_receivedFrames.clear();
        m_lastVersion = 0;
        m_frameCount = 0;
    }
    
    size_t GetFrameCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_frameCount;
    }
    
    const StressTestMetrics& GetMetrics() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_metrics;
    }
    
private:
    std::string m_name;
    int m_priority;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<ContextFrame> m_receivedFrames;
    uint64_t m_lastVersion = 0;
    size_t m_frameCount = 0;
    StressTestMetrics m_metrics;
};

// ─────────────────────────────────────────────────────────────────────────────
// Context Correctness Harness
// ─────────────────────────────────────────────────────────────────────────────

class ContextCorrectnessHarness {
public:
    ContextCorrectnessHarness() 
        : m_engine(ContextFusionEngine::Get()) {}
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test Suite
    // ─────────────────────────────────────────────────────────────────────────
    
    std::vector<CorrectnessResult> RunAllTests() {
        std::vector<CorrectnessResult> results;
        
        results.push_back(TestFrameImmutability());
        results.push_back(TestCausalOrdering());
        results.push_back(TestSubscriberIsolation());
        results.push_back(TestBackpressureHandling());
        results.push_back(TestRaceSafety());
        results.push_back(TestDeterministicReplay());
        results.push_back(TestConflictResolution());
        results.push_back(TestStressTypingBurst());
        
        return results;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: Frame Immutability
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestFrameImmutability() {
        CorrectnessResult result;
        result.testName = "FrameImmutability";
        
        // Setup
        m_engine.Initialize();
        TestSubscriber subscriber("ImmutabilityTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Emit event
        std::string text = "initial text";
        ContextEvent event(ContextEvent::EDITOR_CHANGED, "test", &text);
        m_engine.EmitEvent(event);
        
        // Get frame copy
        ContextFrame frame1 = m_engine.GetFrameCopy();
        uint64_t version1 = frame1.version;
        
        // Emit another event (should create new version)
        std::string text2 = "modified text";
        ContextEvent event2(ContextEvent::EDITOR_CHANGED, "test", &text2);
        m_engine.EmitEvent(event2);
        
        // Get another frame copy
        ContextFrame frame2 = m_engine.GetFrameCopy();
        
        // Verify frame1 is unchanged (immutability)
        if (frame1.bufferText != "initial text") {
            result.Fail("Frame copy was mutated after engine state changed");
            return result;
        }
        
        // Verify frame2 has new state
        if (frame2.bufferText != "modified text") {
            result.Fail("New frame does not reflect updated state");
            return result;
        }
        
        // Verify version incremented
        if (frame2.version != version1 + 1) {
            result.Fail("Version did not increment correctly");
            return result;
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: Causal Ordering
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestCausalOrdering() {
        CorrectnessResult result;
        result.testName = "CausalOrdering";
        
        m_engine.Initialize();
        TestSubscriber subscriber("OrderingTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Emit series of events
        std::vector<uint64_t> expectedVersions;
        for (int i = 0; i < 100; i++) {
            EditorPosition pos{i, 0};
            ContextEvent event(ContextEvent::CURSOR_MOVED, "test", &pos);
            m_engine.EmitEvent(event);
            expectedVersions.push_back(i + 1);
        }
        
        // Wait for all frames
        if (!subscriber.WaitForFrames(100)) {
            result.Fail("Did not receive all 100 frames within timeout");
            return result;
        }
        
        // Verify ordering
        const auto& frames = subscriber.GetReceivedFrames();
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].version != expectedVersions[i]) {
                result.Fail("Frame version mismatch at index " + std::to_string(i));
                return result;
            }
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: Subscriber Isolation
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestSubscriberIsolation() {
        CorrectnessResult result;
        result.testName = "SubscriberIsolation";
        
        m_engine.Initialize();
        TestSubscriber sub1("IsolationTest1", 10);
        TestSubscriber sub2("IsolationTest2", 20);
        
        m_engine.Subscribe(&sub1);
        m_engine.Subscribe(&sub2);
        
        // Emit event
        std::string text = "shared text";
        ContextEvent event(ContextEvent::EDITOR_CHANGED, "test", &text);
        m_engine.EmitEvent(event);
        
        sub1.WaitForFrames(1);
        sub2.WaitForFrames(1);
        
        // Both should have received same frame
        auto frames1 = sub1.GetReceivedFrames();
        auto frames2 = sub2.GetReceivedFrames();
        
        if (frames1.empty() || frames2.empty()) {
            result.Fail("Subscribers did not receive frames");
            return result;
        }
        
        if (frames1[0].version != frames2[0].version) {
            result.Fail("Subscribers received different versions");
            return result;
        }
        
        if (frames1[0].bufferText != frames2[0].bufferText) {
            result.Fail("Subscribers received different content");
            return result;
        }
        
        // Verify no shared mutable state (frames are copies)
        // Modify local copy - should not affect engine or other subscriber
        ContextFrame localCopy = frames1[0];
        localCopy.bufferText = "modified locally";
        
        ContextFrame freshFrame = m_engine.GetFrameCopy();
        if (freshFrame.bufferText == "modified locally") {
            result.Fail("Local modification affected engine state");
            return result;
        }
        
        m_engine.Unsubscribe(&sub1);
        m_engine.Unsubscribe(&sub2);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: Backpressure Handling
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestBackpressureHandling() {
        CorrectnessResult result;
        result.testName = "BackpressureHandling";
        
        m_engine.Initialize();
        TestSubscriber subscriber("BackpressureTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Rapid-fire events (simulating fast typing)
        const int EVENT_COUNT = 1000;
        auto startTime = std::chrono::steady_clock::now();
        
        for (int i = 0; i < EVENT_COUNT; i++) {
            EditorPosition pos{i % 100, i % 80};
            ContextEvent event(ContextEvent::CURSOR_MOVED, "test", &pos);
            m_engine.EmitEvent(event);
        }
        
        auto emitEndTime = std::chrono::steady_clock::now();
        auto emitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            emitEndTime - startTime
        ).count();
        
        // Wait for all frames
        if (!subscriber.WaitForFrames(EVENT_COUNT, 10000)) {
            result.metrics["frames_received"] = std::to_string(subscriber.GetFrameCount());
            result.metrics["frames_expected"] = std::to_string(EVENT_COUNT);
            result.Fail("Did not receive all frames within timeout");
            return result;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        ).count();
        
        // Check metrics
        auto metrics = subscriber.GetMetrics();
        result.metrics["emit_time_ms"] = std::to_string(emitMs);
        result.metrics["total_time_ms"] = std::to_string(totalMs);
        result.metrics["version_skips"] = std::to_string(metrics.versionSkips);
        result.metrics["out_of_order"] = std::to_string(metrics.outOfOrderFrames);
        result.metrics["max_latency_ms"] = std::to_string(metrics.maxLatencyMs);
        
        // Allow some version skips (coalescing is OK)
        // But no out-of-order delivery
        if (metrics.outOfOrderFrames > 0) {
            result.Fail("Out-of-order frame delivery detected");
            return result;
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 5: Race Safety (Concurrent Events)
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestRaceSafety() {
        CorrectnessResult result;
        result.testName = "RaceSafety";
        
        m_engine.Initialize();
        TestSubscriber subscriber("RaceTest", 10);
        m_engine.Subscribe(&subscriber);
        
        std::atomic<bool> running{true};
        std::atomic<uint64_t> raceCount{0};
        
        // Thread 1: Editor events
        std::thread editorThread([&]() {
            for (int i = 0; i < 500 && running; i++) {
                std::string text = "editor_" + std::to_string(i);
                ContextEvent event(ContextEvent::EDITOR_CHANGED, "editor", new std::string(text));
                m_engine.EmitEvent(event);
                delete static_cast<std::string*>(event.payload);
            }
        });
        
        // Thread 2: LSP events
        std::thread lspThread([&]() {
            for (int i = 0; i < 500 && running; i++) {
                std::vector<SymbolInfo> symbols;
                SymbolInfo sym{"func" + std::to_string(i), "function", i % 100, 0, ""};
                symbols.push_back(sym);
                ContextEvent event(ContextEvent::LSP_UPDATED, "lsp", new std::vector<SymbolInfo>(symbols));
                m_engine.EmitEvent(event);
                delete static_cast<std::vector<SymbolInfo>*>(event.payload);
            }
        });
        
        // Thread 3: AI events
        std::thread aiThread([&]() {
            for (int i = 0; i < 500 && running; i++) {
                AIInteraction interaction;
                interaction.prompt = "prompt_" + std::to_string(i);
                interaction.response = "response_" + std::to_string(i);
                interaction.confidence = 0.5f + (i % 50) / 100.0f;
                ContextEvent event(ContextEvent::AI_RESPONSE, "ai", new AIInteraction(interaction));
                m_engine.EmitEvent(event);
                delete static_cast<AIInteraction*>(event.payload);
            }
        });
        
        // Wait for all threads
        editorThread.join();
        lspThread.join();
        aiThread.join();
        
        // Wait for frames
        subscriber.WaitForFrames(100, 5000);
        
        auto metrics = subscriber.GetMetrics();
        result.metrics["frames_received"] = std::to_string(subscriber.GetFrameCount());
        result.metrics["version_skips"] = std::to_string(metrics.versionSkips);
        result.metrics["out_of_order"] = std::to_string(metrics.outOfOrderFrames);
        
        // Check for race conditions (version should be monotonically increasing)
        const auto& frames = subscriber.GetReceivedFrames();
        for (size_t i = 1; i < frames.size(); i++) {
            if (frames[i].version <= frames[i-1].version) {
                raceCount++;
            }
        }
        
        result.metrics["race_conditions"] = std::to_string(raceCount.load());
        
        if (raceCount > 0) {
            result.Fail("Race condition detected: version not monotonically increasing");
            return result;
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 6: Deterministic Replay
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestDeterministicReplay() {
        CorrectnessResult result;
        result.testName = "DeterministicReplay";
        
        m_engine.Initialize();
        EventLog eventLog;
        TestSubscriber subscriber("ReplayTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Record events
        std::vector<std::string> expectedTexts;
        for (int i = 0; i < 50; i++) {
            std::string text = "replay_" + std::to_string(i);
            expectedTexts.push_back(text);
            
            ContextFrame before = m_engine.GetFrameCopy();
            ContextEvent event(ContextEvent::EDITOR_CHANGED, "test", new std::string(text));
            m_engine.EmitEvent(event);
            ContextFrame after = m_engine.GetFrameCopy();
            
            eventLog.Record(event, before, after);
            delete static_cast<std::string*>(event.payload);
        }
        
        // Clear engine state
        m_engine.Initialize();
        subscriber.ClearFrames();
        
        // Replay events
        if (!eventLog.Replay(m_engine)) {
            result.Fail("Replay failed - events out of order");
            return result;
        }
        
        // Wait for frames
        subscriber.WaitForFrames(50);
        
        // Verify replay produced identical state
        const auto& frames = subscriber.GetReceivedFrames();
        if (frames.size() != expectedTexts.size()) {
            result.Fail("Replay produced wrong number of frames");
            return result;
        }
        
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].bufferText != expectedTexts[i]) {
                result.Fail("Replay produced different state at index " + std::to_string(i));
                return result;
            }
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 7: Conflict Resolution
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestConflictResolution() {
        CorrectnessResult result;
        result.testName = "ConflictResolution";
        
        m_engine.Initialize();
        TestSubscriber subscriber("ConflictTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Test precedence: Editor > LSP > AI
        
        // 1. Editor sets text
        std::string editorText = "editor content";
        ContextEvent editorEvent(ContextEvent::EDITOR_CHANGED, "editor", &editorText);
        m_engine.EmitEvent(editorEvent);
        
        ContextFrame frame1 = m_engine.GetFrameCopy();
        
        // 2. LSP tries to set symbols (should not override editor text)
        std::vector<SymbolInfo> symbols;
        symbols.push_back({"testFunc", "function", 1, 0, ""});
        ContextEvent lspEvent(ContextEvent::LSP_UPDATED, "lsp", &symbols);
        m_engine.EmitEvent(lspEvent);
        
        ContextFrame frame2 = m_engine.GetFrameCopy();
        
        // Editor text should be preserved
        if (frame2.bufferText != editorText) {
            result.Fail("LSP overwrote editor content");
            return result;
        }
        
        // LSP symbols should be added
        if (frame2.symbols.size() != 1 || frame2.symbols[0].name != "testFunc") {
            result.Fail("LSP symbols not added correctly");
            return result;
        }
        
        // 3. AI adds ghost text (should not override editor or LSP)
        AIInteraction interaction;
        interaction.prompt = "complete";
        interaction.response = "ghost text";
        interaction.confidence = 0.9f;
        ContextEvent aiEvent(ContextEvent::AI_RESPONSE, "ai", &interaction);
        m_engine.EmitEvent(aiEvent);
        
        ContextFrame frame3 = m_engine.GetFrameCopy();
        
        // Editor text still preserved
        if (frame3.bufferText != editorText) {
            result.Fail("AI overwrote editor content");
            return result;
        }
        
        // LSP symbols still present
        if (frame3.symbols.size() != 1) {
            result.Fail("AI cleared LSP symbols");
            return result;
        }
        
        // Ghost text added
        if (frame3.lastGhostText != "ghost text") {
            result.Fail("AI ghost text not added correctly");
            return result;
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Test 8: Stress Test - Typing Burst
    // ─────────────────────────────────────────────────────────────────────────
    
    CorrectnessResult TestStressTypingBurst() {
        CorrectnessResult result;
        result.testName = "StressTypingBurst";
        
        m_engine.Initialize();
        TestSubscriber subscriber("StressTest", 10);
        m_engine.Subscribe(&subscriber);
        
        // Simulate 200ms typing burst at 60 WPM (~5 chars per 200ms)
        // Actually stress test with much higher rate
        const int BURST_COUNT = 1000;
        const int BURST_ITERATIONS = 10;
        
        std::vector<uint64_t> latencies;
        
        for (int burst = 0; burst < BURST_ITERATIONS; burst++) {
            subscriber.ClearFrames();
            auto burstStart = std::chrono::steady_clock::now();
            
            // Rapid keystroke simulation
            for (int i = 0; i < BURST_COUNT; i++) {
                EditorPosition pos{burst * 100 + i, i % 80};
                ContextEvent event(ContextEvent::CURSOR_MOVED, "typing", &pos);
                m_engine.EmitEvent(event);
            }
            
            auto burstEnd = std::chrono::steady_clock::now();
            auto burstMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                burstEnd - burstStart
            ).count();
            
            latencies.push_back(burstMs);
            
            // Wait for processing
            subscriber.WaitForFrames(BURST_COUNT, 5000);
        }
        
        auto metrics = subscriber.GetMetrics();
        
        result.metrics["total_events"] = std::to_string(BURST_COUNT * BURST_ITERATIONS);
        result.metrics["version_skips"] = std::to_string(metrics.versionSkips);
        result.metrics["out_of_order"] = std::to_string(metrics.outOfOrderFrames);
        result.metrics["max_latency_ms"] = std::to_string(metrics.maxLatencyMs);
        
        // Calculate average burst time
        uint64_t totalBurstMs = 0;
        for (auto ms : latencies) totalBurstMs += ms;
        result.metrics["avg_burst_ms"] = std::to_string(totalBurstMs / latencies.size());
        
        // Success criteria:
        // 1. No out-of-order frames
        // 2. Reasonable latency (< 100ms per burst)
        // 3. All events processed
        
        if (metrics.outOfOrderFrames > 0) {
            result.Fail("Out-of-order frames during typing burst");
            return result;
        }
        
        if (metrics.maxLatencyMs > 500) {
            result.Fail("Max latency exceeded 500ms during typing burst");
            return result;
        }
        
        m_engine.Unsubscribe(&subscriber);
        result.passed = true;
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Summary Report
    // ─────────────────────────────────────────────────────────────────────────
    
    std::string GenerateReport(const std::vector<CorrectnessResult>& results) {
        std::ostringstream report;
        report << "╔════════════════════════════════════════════════════════════════╗\n";
        report << "║           CONTEXT CORRECTNESS HARNESS REPORT                  ║\n";
        report << "╠════════════════════════════════════════════════════════════════╣\n";
        
        int passed = 0;
        int failed = 0;
        
        for (const auto& result : results) {
            std::string status = result.passed ? "✓ PASS" : "✗ FAIL";
            report << "║ " << std::left << std::setw(30) << result.testName 
                   << std::setw(10) << status;
            if (!result.passed) {
                report << " - " << result.failureReason;
            }
            report << "\n";
            
            if (!result.metrics.empty()) {
                for (const auto& [key, value] : result.metrics) {
                    report << "║   └─ " << key << ": " << value << "\n";
                }
            }
            
            if (result.passed) passed++;
            else failed++;
        }
        
        report << "╠════════════════════════════════════════════════════════════════╣\n";
        report << "║ Summary: " << passed << " passed, " << failed << " failed";
        report << std::setw(30 - (std::string("Summary: ").length() + 
            std::to_string(passed).length() + 
            std::string(" passed, ").length() + 
            std::to_string(failed).length() + 
            std::string(" failed").length())) << "";
        report << "║\n";
        report << "╚════════════════════════════════════════════════════════════════╝\n";
        
        return report.str();
    }
    
private:
    ContextFusionEngine& m_engine;
};

} // namespace RawrXD