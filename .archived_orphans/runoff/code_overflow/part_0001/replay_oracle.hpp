// ============================================================================
// replay_oracle.hpp — Expected-Outcome Validator for Replay Harness
// ============================================================================
//
// The Oracle compares actual replay output against expected snapshot_after/
// state. Produces an OracleVerdict: pass/fail with detailed metrics.
//
// Comparison modes:
//   - Exact:      byte-identical file comparison
//   - Structural: whitespace/comment-normalized comparison
//
// Determinism violations are test failures — not warnings.
// Timing is metric-only, never a correctness gate.
//
// Pattern:  Structured results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include "replay_fixture.hpp"
#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// FileDiff — Per-file comparison result
// ============================================================================
struct FileDiff {
    std::string relativePath;
    bool        matched;
    std::string diffDescription;    // Human-readable diff summary
    size_t      expectedSize;
    size_t      actualSize;
};

// ============================================================================
// OracleVerdict — Judgment result from replay comparison
// ============================================================================
struct OracleVerdict {
    bool        pass;
    float       successRate;        // Fraction of targets that matched expected
    int         matchedFiles;
    int         divergedFiles;
    int         extraFixes;         // Fixed but not in expected set
    int         missedFixes;        // Expected but not fixed
    double      replayDurationMs;
    double      originalDurationMs;
    std::string failureReason;      // Non-empty only on failure

    // Per-file details
    std::vector<FileDiff> fileDiffs;

    // Sequence integrity
    size_t      sequenceDeviations; // Number of prompt alignment failures
    bool        deterministicPass;  // True iff zero sequence deviations

    // Factory methods
    static OracleVerdict passed(float rate, int matched, double replayMs, double originalMs);
    static OracleVerdict failed(const std::string& reason);
};

// ============================================================================
// ReplayOracle — Diff engine + tolerance evaluator
// ============================================================================
class ReplayOracle {
public:
    ReplayOracle() = default;
    ~ReplayOracle() = default;

    // ── Main Judgment ──────────────────────────────────────────────
    // Compare actual output directory against expected snapshot_after/.
    // Applies tolerances from the fixture to determine pass/fail.
    OracleVerdict judge(const ReplayFixture& fixture,
                        const std::string& actualOutputDir,
                        double replayDurationMs,
                        size_t sequenceDeviations);

    // ── File Comparison ────────────────────────────────────────────
    // Byte-level exact comparison
    bool compareExact(const std::string& expectedPath,
                      const std::string& actualPath,
                      std::string& diffOut) const;

    // Structural comparison: normalize whitespace, line endings,
    // strip comments, then compare. Deterministic — no fuzzy matching.
    bool compareStructural(const std::string& expectedPath,
                          const std::string& actualPath,
                          std::string& diffOut) const;

    // Single file comparison dispatching on exact vs structural
    bool compareFile(const std::string& expectedPath,
                     const std::string& actualPath,
                     bool exactMatch,
                     std::string& diffOut) const;

private:
    // Normalize file content for structural comparison
    std::string normalizeContent(const std::string& content) const;

    // Collect all files recursively under a directory, returning relative paths
    std::vector<std::string> collectFiles(const std::string& dir) const;
};
