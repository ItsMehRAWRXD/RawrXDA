// ============================================================================
// replay_reporter.hpp — Replay Results Formatter
// ============================================================================
//
// Formats BatchResult and OracleVerdict for:
//   - stdout (human-readable, CI log output)
//   - JSON (machine-readable, stored with build artifacts)
//   - Exit code (for self_test_gate integration)
//
// Pattern:  Static utility class, no state, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include "replay_harness.hpp"
#include "replay_oracle.hpp"
#include <string>

// ============================================================================
// ReplayReporter — Static formatting utilities
// ============================================================================
class ReplayReporter {
public:
    // ── Stdout (CI log) ────────────────────────────────────────────
    // Print a single verdict to stdout
    static void printVerdict(const std::string& fixtureId,
                             const OracleVerdict& verdict);

    // Print batch summary table to stdout
    static void printBatchSummary(const BatchResult& batch);

    // ── JSON ───────────────────────────────────────────────────────
    // Write batch results to JSON file
    static bool writeBatchJson(const BatchResult& batch,
                               const std::string& outputPath);

    // Serialize a single verdict to JSON string
    static std::string verdictToJson(const OracleVerdict& verdict);

    // Serialize batch to JSON string
    static std::string batchToJson(const BatchResult& batch);

    // ── Formatting Helpers ─────────────────────────────────────────
    static std::string formatDuration(double ms);
    static std::string formatPercent(float rate);
    static std::string passFailTag(bool pass);
};
