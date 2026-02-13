// ============================================================================
// replay_harness.hpp — Autonomous-Fix Replay Harness (Core Controller)
// ============================================================================
//
// The ReplayHarness is the top-level coordinator for:
//   1. RECORDING: Wrapping a live BulkFixOrchestrator run, capturing
//      pre/post repo snapshots + journal export as a sealed fixture.
//   2. REPLAY: Loading a fixture, injecting ReplayMockInference,
//      re-running the orchestrator, comparing via ReplayOracle.
//   3. BATCH: Running all fixtures as a CI gate (self_test_gate integration).
//
// What it tests:
//   - The full agentic pipeline: planner → orchestrator → failure detector →
//     puppeteer → hotpatch routing → verification
//   - Only inference is mocked; everything else runs live
//   - Determinism violations (sequence divergence) are test failures
//
// What it does NOT test:
//   - LLM quality (that's the model's job)
//   - Timing performance (metric-only, not a pass/fail gate)
//
// Pattern:  Structured results, no exceptions
// Threading: Recording is single-threaded. Replay is thread-safe.
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include "replay_fixture.hpp"
#include "replay_oracle.hpp"
#include "replay_mock_inference.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// ============================================================================
// GlobalStateReset — Ensures clean slate before every replay
// ============================================================================
// Before replay, ALL mutable global state must be cleared:
//   - Hotpatch presets
//   - Policy overrides
//   - Failure detector heuristics
//   - Token bias maps
//   - Correction counters
//   - Random seeds
// This prevents "works only once" bugs.
struct GlobalStateReset {
    static void resetAll();
    static void resetHotpatchManager();
    static void resetProxyHotpatcher();
    static void resetFailureDetector();
    static void resetOrchestrator();
};

// ============================================================================
// RecordingConfig — Configuration for fixture recording
// ============================================================================
struct RecordingConfig {
    std::string fixtureId;          // Unique fixture identifier
    std::string fixtureOutputDir;   // Where to write the fixture directory
    std::string repoPath;           // Path to the repo being fixed
    std::string description;        // Human-readable description
    std::string tag;                // Tag for manifest ("smoke", "full", etc.)

    // Strategy metadata (copied into the fixture)
    std::string strategyName;
    std::string strategyPromptTemplate;
};

// ============================================================================
// BatchResult — Aggregate results for CI gate
// ============================================================================
struct BatchResult {
    int         total       = 0;
    int         passed      = 0;
    int         failed      = 0;
    int         skipped     = 0;
    double      totalDurationMs = 0.0;

    struct Entry {
        std::string     fixtureId;
        OracleVerdict   verdict;
        double          durationMs;
    };
    std::vector<Entry> entries;

    bool allPassed() const { return failed == 0 && skipped == 0; }
};

// ============================================================================
// ReplayHarness — Main class
// ============================================================================
class ReplayHarness {
public:
    static ReplayHarness& instance();

    // ── Recording ──────────────────────────────────────────────────
    // Starts recording: snapshots the repo, begins journal session.
    // Call this BEFORE running BulkFixOrchestrator.
    bool startRecording(const RecordingConfig& config);

    // Stops recording: snapshots the post-fix repo, exports journal,
    // writes fixture.json, returns the completed fixture.
    // Call this AFTER BulkFixOrchestrator completes.
    ReplayFixture stopRecording(const std::vector<FixtureTarget>& targets);

    // Is a recording currently in progress?
    bool isRecording() const;

    // ── Replay ─────────────────────────────────────────────────────
    // Load a fixture and replay it. Returns Oracle judgment.
    // Performs full global state reset before execution.
    OracleVerdict replay(const ReplayFixture& fixture);
    OracleVerdict replay(const std::string& fixtureDir);

    // ── Batch Execution (CI Gate) ──────────────────────────────────
    // Run all enabled fixtures from the manifest.
    BatchResult runAll(const std::string& manifestPath = "");

    // Run only fixtures with a specific tag.
    BatchResult runTagged(const std::string& manifestPath,
                          const std::string& tag);

    // Self-test gate integration: returns true iff all pass.
    // Prints summary to stdout.
    bool runAsSelfTestGate(const std::string& manifestPath = "");

    // ── Configuration ──────────────────────────────────────────────
    // Base directory for fixtures (default: src/test_harness/fixtures/)
    void setFixtureBaseDir(const std::string& dir);
    std::string getFixtureBaseDir() const;

    // ── Diagnostics ────────────────────────────────────────────────
    struct Stats {
        int fixturesAvailable   = 0;
        int lastRunTotal        = 0;
        int lastRunPassed       = 0;
        int lastRunFailed       = 0;
        double lastRunDurationMs = 0.0;
        std::string lastRunTimestamp;
    };
    Stats getStats() const;

private:
    ReplayHarness();
    ~ReplayHarness() = default;
    ReplayHarness(const ReplayHarness&) = delete;
    ReplayHarness& operator=(const ReplayHarness&) = delete;

    // ── Recording State ────────────────────────────────────────────
    struct ActiveRecording {
        RecordingConfig config;
        std::string snapshotBeforePath;
        uint64_t    journalSessionId;
        std::chrono::steady_clock::time_point startTime;
    };
    std::unique_ptr<ActiveRecording> m_recording;

    // ── Replay Internals ───────────────────────────────────────────
    // Setup: copy snapshot_before to temp dir, reset global state
    bool setupReplayEnvironment(const ReplayFixture& fixture,
                                std::string& outTempDir);

    // Execute: run orchestrator with mock inference in temp dir
    OracleVerdict executeReplay(const ReplayFixture& fixture,
                                const std::string& tempDir);

    // Teardown: remove temp dir
    void teardownReplayEnvironment(const std::string& tempDir);

    // ── Snapshot ───────────────────────────────────────────────────
    // Copy directory tree for pre/post fix snapshots
    bool snapshotDirectory(const std::string& sourceDir,
                           const std::string& destDir);

    // ── State ──────────────────────────────────────────────────────
    std::string     m_fixtureBaseDir;
    Stats           m_stats;
    mutable std::mutex m_mutex;
};
