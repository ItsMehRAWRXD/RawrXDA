// ============================================================================
// replay_fixture.hpp — Frozen Test Scenario for Autonomous-Fix Replay
// ============================================================================
//
// A ReplayFixture is a sealed, immutable test artifact containing:
//   - Pre-fix repo snapshot (snapshot_before/)
//   - Expected post-fix repo snapshot (snapshot_after/)
//   - Recorded ReplayJournal session (journal.replay)
//   - Strategy + targets + tolerances (fixture.json)
//
// Fixtures are production artifacts — every successful live autonomous-fix
// run can become a fixture. The test suite grows from reality, not imagination.
//
// Pattern:  Structured results, no exceptions
// Threading: Load/save are not thread-safe; caller must synchronize
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// JOURNAL SCHEMA VERSION — Bump on any ActionRecord format change
// ============================================================================
static constexpr uint32_t REPLAY_JOURNAL_SCHEMA_VERSION = 1;
static constexpr const char* REPLAY_ENGINE_VERSION = "7.4.0";

// ============================================================================
// Tolerances — Non-deterministic boundary configuration
// ============================================================================
struct ReplayTolerances {
    float   minSuccessRate      = 0.90f;    // 90% of targets must match expected
    bool    exactFileMatch      = false;    // true = byte-identical, false = structural
    bool    allowExtraFixes     = true;     // OK if agent fixes more than expected
    float   confidenceFloor     = 0.5f;     // Min confidence for a "real" action
    size_t  maxSequenceDeviation = 0;       // 0 = strict sequence lock
    // Timing is metric-only, never a correctness gate
};

// ============================================================================
// FixtureTarget — Lightweight target descriptor stored in fixture.json
// ============================================================================
struct FixtureTarget {
    std::string id;             // Unique target identifier
    std::string path;           // Relative file path within snapshot
    std::string context;        // Additional context (line range, symbol, etc.)
    std::string category;       // Grouping label for parallel dispatch
};

// ============================================================================
// FixtureManifestEntry — One entry in fixture_manifest.json
// ============================================================================
struct FixtureManifestEntry {
    std::string id;
    std::string path;           // Relative path to fixture directory
    std::string description;
    std::string tag;            // "smoke", "full", "enterprise", etc.
    bool        enabled = true;
};

// ============================================================================
// ReplayFixture — Complete frozen test scenario
// ============================================================================
struct ReplayFixture {
    // Identity
    std::string id;
    std::string description;

    // Paths (absolute, resolved at load time)
    std::string snapshotBeforeDir;
    std::string snapshotAfterDir;
    std::string journalPath;

    // Strategy descriptor (name + prompt template key fields)
    std::string strategyName;
    std::string strategyPromptTemplate;
    int         strategyMaxRetries      = 3;
    int         strategyMaxParallel     = 4;
    int         strategyPerTargetTimeoutMs = 60000;
    bool        strategyAutoVerify      = true;
    bool        strategySelfHeal        = true;

    // Targets
    std::vector<FixtureTarget> targets;

    // Tolerances
    ReplayTolerances tolerances;

    // Manifest metadata
    std::string createdDate;        // ISO 8601
    std::string engineVersion;      // RawrEngine version at recording time
    uint32_t    journalSchemaVersion = REPLAY_JOURNAL_SCHEMA_VERSION;
    uint64_t    originalDurationMs  = 0;

    // ── Serialization ──────────────────────────────────────────────
    // Writes fixture.json into the given directory
    bool save(const std::string& fixtureDir) const;

    // Loads fixture.json from the given directory and resolves paths
    static ReplayFixture load(const std::string& fixtureDir);

    // Validates that all referenced files/dirs exist
    bool validateStructure() const;

    // Schema compatibility check
    bool isSchemaCompatible() const;
};

// ============================================================================
// FixtureManifest — Index of all available fixtures
// ============================================================================
struct FixtureManifest {
    std::vector<FixtureManifestEntry> fixtures;

    static FixtureManifest load(const std::string& manifestPath);
    bool save(const std::string& manifestPath) const;

    std::vector<FixtureManifestEntry> getByTag(const std::string& tag) const;
    std::vector<FixtureManifestEntry> getEnabled() const;
};
