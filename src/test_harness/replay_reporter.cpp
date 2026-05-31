// ============================================================================
// replay_reporter.cpp — Replay Results Formatting Implementation
// ============================================================================
// Pattern:  Static utility, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "replay_reporter.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>

using json = nlohmann::json;

// ============================================================================
// Stdout output
// ============================================================================

void ReplayReporter::printVerdict(const std::string& fixtureId,
                                  const OracleVerdict& verdict) {
    std::cout << "  " << passFailTag(verdict.pass) << " " << fixtureId;

    if (verdict.pass) {
        std::cout << " — " << formatPercent(verdict.successRate)
                  << " match, " << verdict.matchedFiles << " files"
                  << ", " << formatDuration(verdict.replayDurationMs);
    } else {
        std::cout << " — " << verdict.failureReason;
    }

    if (verdict.sequenceDeviations > 0) {
        std::cout << " [" << verdict.sequenceDeviations << " deviations]";
    }

    std::cout << "\n";
}

void ReplayReporter::printBatchSummary(const BatchResult& batch) {
    std::cout << "────────────────────────────────────────────────────\n";
    std::cout << "  Replay Harness Results\n";
    std::cout << "────────────────────────────────────────────────────\n";

    for (const auto& entry : batch.entries) {
        printVerdict(entry.fixtureId, entry.verdict);
    }

    std::cout << "────────────────────────────────────────────────────\n";
    std::cout << "  Total: " << batch.total
              << "  Passed: " << batch.passed
              << "  Failed: " << batch.failed
              << "  Skipped: " << batch.skipped << "\n";
    std::cout << "  Duration: " << formatDuration(batch.totalDurationMs) << "\n";
    std::cout << "────────────────────────────────────────────────────\n";
}

// ============================================================================
// JSON output
// ============================================================================

bool ReplayReporter::writeBatchJson(const BatchResult& batch,
                                     const std::string& outputPath) {
    std::string content = batchToJson(batch);
    std::ofstream out(outputPath);
    if (!out.is_open()) return false;
    out << content;
    return out.good();
}

std::string ReplayReporter::verdictToJson(const OracleVerdict& verdict) {
    json j;
    j["pass"]               = verdict.pass;
    j["successRate"]         = verdict.successRate;
    j["matchedFiles"]        = verdict.matchedFiles;
    j["divergedFiles"]       = verdict.divergedFiles;
    j["extraFixes"]          = verdict.extraFixes;
    j["missedFixes"]         = verdict.missedFixes;
    j["replayDurationMs"]    = verdict.replayDurationMs;
    j["originalDurationMs"]  = verdict.originalDurationMs;
    j["sequenceDeviations"]  = verdict.sequenceDeviations;
    j["deterministicPass"]   = verdict.deterministicPass;

    if (!verdict.failureReason.empty()) {
        j["failureReason"] = verdict.failureReason;
    }

    // File diffs
    json diffs = json::array();
    for (const auto& fd : verdict.fileDiffs) {
        json d;
        d["path"]        = fd.relativePath;
        d["matched"]     = fd.matched;
        d["expectedSize"] = fd.expectedSize;
        d["actualSize"]  = fd.actualSize;
        if (!fd.matched) {
            d["diff"] = fd.diffDescription;
        }
        diffs.push_back(d);
    }
    j["fileDiffs"] = diffs;

    return j.dump(2);
}

std::string ReplayReporter::batchToJson(const BatchResult& batch) {
    json j;
    j["total"]          = batch.total;
    j["passed"]         = batch.passed;
    j["failed"]         = batch.failed;
    j["skipped"]        = batch.skipped;
    j["totalDurationMs"] = batch.totalDurationMs;
    j["allPassed"]      = batch.allPassed();
    j["engineVersion"]  = REPLAY_ENGINE_VERSION;
    j["schemaVersion"]  = REPLAY_JOURNAL_SCHEMA_VERSION;

    json entries = json::array();
    for (const auto& entry : batch.entries) {
        json e;
        e["fixtureId"]  = entry.fixtureId;
        e["durationMs"]  = entry.durationMs;
        e["verdict"]     = json::parse(verdictToJson(entry.verdict));
        entries.push_back(e);
    }
    j["entries"] = entries;

    return j.dump(2);
}

// ============================================================================
// Formatting helpers
// ============================================================================

std::string ReplayReporter::formatDuration(double ms) {
    std::ostringstream oss;
    if (ms < 1000.0) {
        oss << std::fixed << std::setprecision(0) << ms << "ms";
    } else if (ms < 60000.0) {
        oss << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
    } else {
        double mins = ms / 60000.0;
        oss << std::fixed << std::setprecision(1) << mins << "m";
    }
    return oss.str();
}

std::string ReplayReporter::formatPercent(float rate) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (rate * 100.0f) << "%";
    return oss.str();
}

std::string ReplayReporter::passFailTag(bool pass) {
    return pass ? "[PASS]" : "[FAIL]";
}
