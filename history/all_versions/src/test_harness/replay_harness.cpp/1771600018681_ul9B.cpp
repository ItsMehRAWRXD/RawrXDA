// ============================================================================
// replay_harness.cpp — Autonomous-Fix Replay Harness Implementation
// ============================================================================
// Pattern:  Structured results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "replay_harness.hpp"
#include "replay_reporter.hpp"
#include "../core/deterministic_replay.h"
#include "../core/unified_hotpatch_manager.hpp"
#include "../core/proxy_hotpatcher.hpp"
#include "../agent/agentic_hotpatch_orchestrator.hpp"
#include "../agent/agentic_failure_detector.hpp"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cstring>

#include "logging/logger.h"
static Logger s_logger("replay_harness");

namespace fs = std::filesystem;

// ============================================================================
// GlobalStateReset — Clean slate before every replay
// ============================================================================

void GlobalStateReset::resetAll() {
    resetHotpatchManager();
    resetProxyHotpatcher();
    resetFailureDetector();
    resetOrchestrator();
}

void GlobalStateReset::resetHotpatchManager() {
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    // Clear any loaded presets
    // Note: clearAllPatches() clears memory + byte + server patches
    mgr.clearAllPatches();
}

void GlobalStateReset::resetProxyHotpatcher() {
    auto& proxy = ProxyHotpatcher::instance();
    proxy.clear_token_biases();
    proxy.clear_rewrite_rules();
    proxy.clear_termination_rules();
    proxy.clear_validators();
    proxy.resetStats();
}

void GlobalStateReset::resetFailureDetector() {
    // Reset statistics but preserve default patterns
    // (patterns are part of the engine config, not runtime state)
    AgenticFailureDetector detector;
    detector.resetStatistics();
}

void GlobalStateReset::resetOrchestrator() {
    auto& orch = AgenticHotpatchOrchestrator::instance();
    orch.resetStats();
    orch.loadDefaultPolicies();
}

// ============================================================================
// ReplayHarness — Singleton
// ============================================================================

ReplayHarness& ReplayHarness::instance() {
    static ReplayHarness inst;
    return inst;
}

ReplayHarness::ReplayHarness()
    : m_fixtureBaseDir("src/test_harness/fixtures") {
}

// ============================================================================
// Recording
// ============================================================================

bool ReplayHarness::startRecording(const RecordingConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_recording) {
        s_logger.error("[ReplayHarness] ERROR: Recording already in progress");
        return false;
    }

    // Create fixture output directory
    fs::create_directories(config.fixtureOutputDir);

    // Snapshot pre-fix repo state
    std::string snapshotBefore = (fs::path(config.fixtureOutputDir) / "snapshot_before").string();
    if (!snapshotDirectory(config.repoPath, snapshotBefore)) {
        s_logger.error("[ReplayHarness] ERROR: Failed to snapshot {}", config.repoPath);
        return false;
    }

    // Start journal session
    auto& journal = ReplayJournal::instance();
    uint64_t sessionId = journal.startSession("replay_recording_" + config.fixtureId);
    journal.startRecording();

    // Create active recording state
    m_recording = std::make_unique<ActiveRecording>();
    m_recording->config             = config;
    m_recording->snapshotBeforePath = snapshotBefore;
    m_recording->journalSessionId   = sessionId;
    m_recording->startTime          = std::chrono::steady_clock::now();

    s_logger.info("[ReplayHarness] Recording started: {}", config.fixtureId);
    return true;
}

ReplayFixture ReplayHarness::stopRecording(const std::vector<FixtureTarget>& targets) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ReplayFixture fixture;

    if (!m_recording) {
        s_logger.error( "[ReplayHarness] ERROR: No active recording\n";
        fixture.id = "__NO_RECORDING__";
        return fixture;
    }

    auto& config = m_recording->config;
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - m_recording->startTime).count();

    // Stop journal recording
    auto& journal = ReplayJournal::instance();
    journal.stopRecording();

    // Export journal to fixture directory
    std::string journalPath = (fs::path(config.fixtureOutputDir) / "journal.replay").string();
    journal.exportSession(m_recording->journalSessionId, journalPath);
    journal.endSession();

    // Snapshot post-fix repo state
    std::string snapshotAfter = (fs::path(config.fixtureOutputDir) / "snapshot_after").string();
    snapshotDirectory(config.repoPath, snapshotAfter);

    // Build fixture
    fixture.id                       = config.fixtureId;
    fixture.description              = config.description;
    fixture.snapshotBeforeDir        = m_recording->snapshotBeforePath;
    fixture.snapshotAfterDir         = snapshotAfter;
    fixture.journalPath              = journalPath;
    fixture.strategyName             = config.strategyName;
    fixture.strategyPromptTemplate   = config.strategyPromptTemplate;
    fixture.targets                  = targets;
    fixture.originalDurationMs       = static_cast<uint64_t>(durationMs);
    fixture.engineVersion            = REPLAY_ENGINE_VERSION;
    fixture.journalSchemaVersion     = REPLAY_JOURNAL_SCHEMA_VERSION;

    // Save fixture.json
    fixture.save(config.fixtureOutputDir);

    s_logger.info("[ReplayHarness] Recording stopped. Fixture saved to: ");

    m_recording.reset();
    return fixture;
}

bool ReplayHarness::isRecording() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_recording != nullptr;
}

// ============================================================================
// Replay
// ============================================================================

OracleVerdict ReplayHarness::replay(const ReplayFixture& fixture) {
    // Validate fixture structure
    if (!fixture.validateStructure()) {
        return OracleVerdict::failed("Fixture validation failed: missing files for '"
                                      + fixture.id + "'");
    }

    // Schema compatibility check — fail fast, never interpret old formats silently
    if (!fixture.isSchemaCompatible()) {
        return OracleVerdict::failed("Schema version mismatch: fixture has v"
            + std::to_string(fixture.journalSchemaVersion)
            + ", engine expects v" + std::to_string(REPLAY_JOURNAL_SCHEMA_VERSION)
            + ". Re-record required.");
    }

    // Setup temp environment
    std::string tempDir;
    if (!setupReplayEnvironment(fixture, tempDir)) {
        return OracleVerdict::failed("Failed to setup replay environment");
    }

    // Execute replay
    OracleVerdict verdict = executeReplay(fixture, tempDir);

    // Teardown
    teardownReplayEnvironment(tempDir);

    return verdict;
}

OracleVerdict ReplayHarness::replay(const std::string& fixtureDir) {
    ReplayFixture fixture = ReplayFixture::load(fixtureDir);
    if (fixture.id == "__LOAD_FAILED__") {
        return OracleVerdict::failed("Cannot load fixture from: " + fixtureDir);
    }
    return replay(fixture);
}

OracleVerdict ReplayHarness::executeReplay(const ReplayFixture& fixture,
                                            const std::string& tempDir) {
    auto start = std::chrono::steady_clock::now();

    // 1. Reset ALL global state
    GlobalStateReset::resetAll();

    // 2. Load mock inference from journal
    ReplayMockInference mock;
    if (!mock.loadJournal(fixture.journalPath)) {
        return OracleVerdict::failed("Failed to load journal: " + fixture.journalPath);
    }

    s_logger.info("[ReplayHarness] Loaded ");

    // 3. Run the orchestrator with mock inference
    //
    // The orchestrator calls into the inference backend. During replay,
    // we substitute the mock. The agentic pipeline runs live:
    //   - Failure detector analyzes each (mocked) response
    //   - Puppeteer corrects detected failures
    //   - Hotpatch orchestrator routes corrections to proper layer
    //   - Verification logic validates the result
    //
    // This tests the LOGIC of the agent, not the LUCK of the model.

    // Process targets through the mock
    // (In a full integration, BulkFixOrchestrator would be called here
    //  with the mock injected as the inference backend. For Phase 1,
    //  we validate the journal → mock → oracle pipeline directly.)

    for (const auto& target : fixture.targets) {
        // Build prompt from strategy template
        std::string prompt = fixture.strategyPromptTemplate;
        // Replace {{target}} with actual target path
        size_t pos = prompt.find("{{target}}");
        if (pos != std::string::npos) {
            prompt.replace(pos, 10, target.path);
        }
        pos = prompt.find("{{context}}");
        if (pos != std::string::npos) {
            prompt.replace(pos, 11, target.context);
        }

        // Get mock response
        MockInferenceResult result = mock.generate(prompt);

        if (!result.found) {
            s_logger.error("[ReplayHarness] WARNING: Mock exhausted at target '{}'", target.id);
            break;
        }

        if (!result.promptAligned) {
            s_logger.error("[ReplayHarness] DEVIATION: {}", result.mismatchDetail);
        }

        // In full integration: apply the response as a fix to tempDir
        // For Phase 1: the oracle compares pre-computed snapshot_after
    }

    auto end = std::chrono::steady_clock::now();
    double replayMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    // 4. Judge results via Oracle
    ReplayOracle oracle;
    OracleVerdict verdict = oracle.judge(
        fixture, tempDir, replayMs, mock.sequenceDeviations());

    return verdict;
}

bool ReplayHarness::setupReplayEnvironment(const ReplayFixture& fixture,
                                            std::string& outTempDir) {
    // Create isolated temp directory
    auto tempBase = fs::temp_directory_path() / ("rawrxd_replay_" + fixture.id);
    fs::create_directories(tempBase);
    outTempDir = tempBase.string();

    // Copy snapshot_before into temp dir
    return snapshotDirectory(fixture.snapshotBeforeDir, outTempDir);
}

void ReplayHarness::teardownReplayEnvironment(const std::string& tempDir) {
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    if (ec) {
        s_logger.error("[ReplayHarness] WARNING: Failed to clean up temp dir: {} ({})", tempDir, ec.message());
    }
}

bool ReplayHarness::snapshotDirectory(const std::string& sourceDir,
                                       const std::string& destDir) {
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) return false;

    fs::copy(sourceDir, destDir,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    return !ec;
}

// ============================================================================
// Batch Execution (CI Gate)
// ============================================================================

BatchResult ReplayHarness::runAll(const std::string& manifestPath) {
    std::string effectivePath = manifestPath.empty()
        ? (fs::path(m_fixtureBaseDir) / "fixture_manifest.json").string()
        : manifestPath;

    FixtureManifest manifest = FixtureManifest::load(effectivePath);
    auto enabledFixtures = manifest.getEnabled();

    BatchResult batch;
    auto batchStart = std::chrono::steady_clock::now();

    for (const auto& entry : enabledFixtures) {
        // Resolve fixture path relative to manifest directory
        fs::path manifestDir = fs::path(effectivePath).parent_path();
        std::string fixtureDir = (manifestDir / entry.path).string();

        s_logger.info("[ReplayHarness] Running fixture: {}", fixtureDir);

        auto entryStart = std::chrono::steady_clock::now();

        ReplayFixture fixture = ReplayFixture::load(fixtureDir);
        OracleVerdict verdict;

        if (fixture.id == "__LOAD_FAILED__") {
            verdict = OracleVerdict::failed("Cannot load fixture from: " + fixtureDir);
            batch.skipped++;
        } else {
            verdict = replay(fixture);
            if (verdict.pass) batch.passed++;
            else             batch.failed++;
        }

        auto entryEnd = std::chrono::steady_clock::now();
        double entryMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(entryEnd - entryStart).count());

        s_logger.info("[ReplayHarness] Result: {} ({} ms){}",
                  verdict.pass ? "PASS" : "FAIL",
                  static_cast<int>(entryMs),
                  verdict.pass ? "" : (" - " + verdict.failureReason));

        BatchResult::Entry e;
        e.fixtureId  = entry.id;
        e.verdict    = verdict;
        e.durationMs = entryMs;
        batch.entries.push_back(e);
        batch.total++;
    }

    auto batchEnd = std::chrono::steady_clock::now();
    batch.totalDurationMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(batchEnd - batchStart).count());

    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.fixturesAvailable = static_cast<int>(enabledFixtures.size());
        m_stats.lastRunTotal      = batch.total;
        m_stats.lastRunPassed     = batch.passed;
        m_stats.lastRunFailed     = batch.failed;
        m_stats.lastRunDurationMs = batch.totalDurationMs;
    }

    return batch;
}

BatchResult ReplayHarness::runTagged(const std::string& manifestPath,
                                      const std::string& tag) {
    std::string effectivePath = manifestPath.empty()
        ? (fs::path(m_fixtureBaseDir) / "fixture_manifest.json").string()
        : manifestPath;

    FixtureManifest manifest = FixtureManifest::load(effectivePath);
    auto taggedFixtures = manifest.getByTag(tag);

    BatchResult batch;
    auto batchStart = std::chrono::steady_clock::now();

    for (const auto& entry : taggedFixtures) {
        fs::path manifestDir = fs::path(effectivePath).parent_path();
        std::string fixtureDir = (manifestDir / entry.path).string();

        ReplayFixture fixture = ReplayFixture::load(fixtureDir);
        OracleVerdict verdict;

        if (fixture.id == "__LOAD_FAILED__") {
            verdict = OracleVerdict::failed("Cannot load fixture");
            batch.skipped++;
        } else {
            verdict = replay(fixture);
            if (verdict.pass) batch.passed++;
            else batch.failed++;
        }

        auto entryEnd = std::chrono::steady_clock::now();
        double entryMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                entryEnd - batchStart).count());

        BatchResult::Entry e;
        e.fixtureId  = entry.id;
        e.verdict    = verdict;
        e.durationMs = entryMs;
        batch.entries.push_back(e);
        batch.total++;
    }

    auto batchEnd = std::chrono::steady_clock::now();
    batch.totalDurationMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(batchEnd - batchStart).count());

    return batch;
}

bool ReplayHarness::runAsSelfTestGate(const std::string& manifestPath) {
    s_logger.info("\n");

    BatchResult batch = runAll(manifestPath);

    // Print summary
    ReplayReporter::printBatchSummary(batch);

    // Write JSON report
    ReplayReporter::writeBatchJson(batch, "replay_results.json");

    s_logger.info("\n");
    if (batch.allPassed()) {
        s_logger.info("REPLAY GATE: PASSED (");
    } else {
        s_logger.info("REPLAY GATE: FAILED (");
    }

    return batch.allPassed();
}

// ============================================================================
// Configuration
// ============================================================================

void ReplayHarness::setFixtureBaseDir(const std::string& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fixtureBaseDir = dir;
}

std::string ReplayHarness::getFixtureBaseDir() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fixtureBaseDir;
}

ReplayHarness::Stats ReplayHarness::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}
