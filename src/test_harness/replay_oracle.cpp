// ============================================================================
// replay_oracle.cpp — Diff Engine + Tolerance Evaluator
// ============================================================================
// Pattern:  Structured results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "replay_oracle.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================================
// OracleVerdict factories
// ============================================================================

OracleVerdict OracleVerdict::passed(float rate, int matched,
                                    double replayMs, double originalMs) {
    OracleVerdict v;
    v.pass              = true;
    v.successRate       = rate;
    v.matchedFiles      = matched;
    v.divergedFiles     = 0;
    v.extraFixes        = 0;
    v.missedFixes       = 0;
    v.replayDurationMs  = replayMs;
    v.originalDurationMs = originalMs;
    v.sequenceDeviations = 0;
    v.deterministicPass  = true;
    return v;
}

OracleVerdict OracleVerdict::failed(const std::string& reason) {
    OracleVerdict v;
    v.pass              = false;
    v.successRate       = 0.0f;
    v.matchedFiles      = 0;
    v.divergedFiles     = 0;
    v.extraFixes        = 0;
    v.missedFixes       = 0;
    v.replayDurationMs  = 0.0;
    v.originalDurationMs = 0.0;
    v.failureReason     = reason;
    v.sequenceDeviations = 0;
    v.deterministicPass  = false;
    return v;
}

// ============================================================================
// ReplayOracle — Main judgment
// ============================================================================

OracleVerdict ReplayOracle::judge(const ReplayFixture& fixture,
                                  const std::string& actualOutputDir,
                                  double replayDurationMs,
                                  size_t sequenceDeviations) {
    OracleVerdict verdict;
    verdict.replayDurationMs  = replayDurationMs;
    verdict.originalDurationMs = static_cast<double>(fixture.originalDurationMs);
    verdict.sequenceDeviations = sequenceDeviations;
    verdict.deterministicPass  = (sequenceDeviations <= fixture.tolerances.maxSequenceDeviation);

    // Determinism is binary. If strict sequence lock and deviations > 0, fail immediately.
    if (!verdict.deterministicPass) {
        verdict.pass = false;
        verdict.failureReason = "Determinism violation: " +
            std::to_string(sequenceDeviations) + " sequence deviations "
            "(max allowed: " + std::to_string(fixture.tolerances.maxSequenceDeviation) + ")";
        return verdict;
    }

    // Collect expected files from snapshot_after
    auto expectedFiles = collectFiles(fixture.snapshotAfterDir);
    auto actualFiles   = collectFiles(actualOutputDir);

    int matched  = 0;
    int diverged = 0;
    int missed   = 0;
    int extra    = 0;

    // Check each expected file against actual
    for (const auto& relPath : expectedFiles) {
        std::string expectedFullPath = (fs::path(fixture.snapshotAfterDir) / relPath).string();
        std::string actualFullPath   = (fs::path(actualOutputDir) / relPath).string();

        FileDiff fd;
        fd.relativePath = relPath;
        fd.expectedSize = fs::exists(expectedFullPath) ? fs::file_size(expectedFullPath) : 0;

        if (!fs::exists(actualFullPath)) {
            fd.matched = false;
            fd.actualSize = 0;
            fd.diffDescription = "Missing in actual output";
            missed++;
            verdict.fileDiffs.push_back(fd);
            continue;
        }

        fd.actualSize = fs::file_size(actualFullPath);

        std::string diffDesc;
        bool match = compareFile(expectedFullPath, actualFullPath,
                                 fixture.tolerances.exactFileMatch, diffDesc);
        fd.matched = match;
        fd.diffDescription = diffDesc;
        verdict.fileDiffs.push_back(fd);

        if (match) {
            matched++;
        } else {
            diverged++;
        }
    }

    // Check for extra files in actual that weren't in expected
    for (const auto& relPath : actualFiles) {
        bool found = false;
        for (const auto& expPath : expectedFiles) {
            if (expPath == relPath) { found = true; break; }
        }
        if (!found) {
            extra++;
            if (!fixture.tolerances.allowExtraFixes) {
                FileDiff fd;
                fd.relativePath     = relPath;
                fd.matched          = false;
                fd.diffDescription  = "Unexpected extra file in actual output";
                fd.expectedSize     = 0;
                fd.actualSize       = fs::exists((fs::path(actualOutputDir) / relPath).string())
                                        ? fs::file_size((fs::path(actualOutputDir) / relPath).string())
                                        : 0;
                verdict.fileDiffs.push_back(fd);
            }
        }
    }

    int totalExpected = static_cast<int>(expectedFiles.size());
    verdict.matchedFiles  = matched;
    verdict.divergedFiles = diverged;
    verdict.missedFixes   = missed;
    verdict.extraFixes    = extra;
    verdict.successRate   = totalExpected > 0
                              ? static_cast<float>(matched) / static_cast<float>(totalExpected)
                              : 0.0f;

    // Apply tolerances
    if (verdict.successRate < fixture.tolerances.minSuccessRate) {
        verdict.pass = false;
        std::ostringstream oss;
        oss << "Success rate " << verdict.successRate
            << " below tolerance " << fixture.tolerances.minSuccessRate
            << " (" << matched << "/" << totalExpected << " files matched)";
        verdict.failureReason = oss.str();
    } else if (!fixture.tolerances.allowExtraFixes && extra > 0) {
        verdict.pass = false;
        verdict.failureReason = "Extra fixes not allowed: " +
            std::to_string(extra) + " unexpected files";
    } else {
        verdict.pass = true;
    }

    return verdict;
}

// ============================================================================
// File comparison
// ============================================================================

bool ReplayOracle::compareFile(const std::string& expectedPath,
                               const std::string& actualPath,
                               bool exactMatch,
                               std::string& diffOut) const {
    if (exactMatch) {
        return compareExact(expectedPath, actualPath, diffOut);
    } else {
        return compareStructural(expectedPath, actualPath, diffOut);
    }
}

bool ReplayOracle::compareExact(const std::string& expectedPath,
                                const std::string& actualPath,
                                std::string& diffOut) const {
    std::ifstream f1(expectedPath, std::ios::binary);
    std::ifstream f2(actualPath, std::ios::binary);
    if (!f1.is_open() || !f2.is_open()) {
        diffOut = "Cannot open one or both files";
        return false;
    }

    // Size check first (fast path)
    f1.seekg(0, std::ios::end);
    f2.seekg(0, std::ios::end);
    auto size1 = f1.tellg();
    auto size2 = f2.tellg();
    if (size1 != size2) {
        std::ostringstream oss;
        oss << "Size mismatch: expected " << size1 << " bytes, got " << size2;
        diffOut = oss.str();
        return false;
    }

    f1.seekg(0);
    f2.seekg(0);

    // 4KB chunk comparison
    constexpr size_t CHUNK = 4096;
    char buf1[CHUNK], buf2[CHUNK];
    size_t offset = 0;
    while (f1.good() && f2.good()) {
        f1.read(buf1, CHUNK);
        f2.read(buf2, CHUNK);
        auto read1 = f1.gcount();
        auto read2 = f2.gcount();
        if (read1 != read2) {
            diffOut = "Read length mismatch at offset " + std::to_string(offset);
            return false;
        }
        if (read1 == 0) break;
        if (std::memcmp(buf1, buf2, static_cast<size_t>(read1)) != 0) {
            // Find exact byte offset
            for (std::streamsize i = 0; i < read1; i++) {
                if (buf1[i] != buf2[i]) {
                    std::ostringstream oss;
                    oss << "Byte mismatch at offset " << (offset + i)
                        << ": expected 0x" << std::hex << (int)(unsigned char)buf1[i]
                        << " got 0x" << (int)(unsigned char)buf2[i];
                    diffOut = oss.str();
                    return false;
                }
            }
        }
        offset += static_cast<size_t>(read1);
    }

    return true;
}

bool ReplayOracle::compareStructural(const std::string& expectedPath,
                                     const std::string& actualPath,
                                     std::string& diffOut) const {
    auto readFile = [](const std::string& path) -> std::string {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    std::string expectedRaw = readFile(expectedPath);
    std::string actualRaw   = readFile(actualPath);

    if (expectedRaw.empty() && actualRaw.empty()) return true;
    if (expectedRaw.empty() || actualRaw.empty()) {
        diffOut = "One file is empty, the other is not";
        return false;
    }

    std::string expectedNorm = normalizeContent(expectedRaw);
    std::string actualNorm   = normalizeContent(actualRaw);

    if (expectedNorm == actualNorm) return true;

    // Find first divergence point for diagnostics
    size_t minLen = std::min(expectedNorm.size(), actualNorm.size());
    size_t divergeAt = 0;
    for (size_t i = 0; i < minLen; i++) {
        if (expectedNorm[i] != actualNorm[i]) {
            divergeAt = i;
            break;
        }
        divergeAt = i + 1;
    }

    std::ostringstream oss;
    oss << "Structural divergence at normalized offset " << divergeAt;

    // Show context around divergence (20 chars each side)
    size_t ctxStart = (divergeAt > 20) ? divergeAt - 20 : 0;
    size_t ctxEnd   = std::min(divergeAt + 20, std::min(expectedNorm.size(), actualNorm.size()));
    oss << "\n  expected: \"" << expectedNorm.substr(ctxStart, ctxEnd - ctxStart) << "\"";
    if (divergeAt < actualNorm.size()) {
        oss << "\n  actual:   \"" << actualNorm.substr(ctxStart, ctxEnd - ctxStart) << "\"";
    }

    diffOut = oss.str();
    return false;
}

// ============================================================================
// Content normalization for structural comparison
// ============================================================================

std::string ReplayOracle::normalizeContent(const std::string& content) const {
    std::string result;
    result.reserve(content.size());

    bool inLineComment  = false;
    bool inBlockComment = false;
    bool lastWasSpace   = false;

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];
        char next = (i + 1 < content.size()) ? content[i + 1] : '\0';

        // Skip carriage returns (normalize to \n only)
        if (c == '\r') continue;

        // Block comment tracking
        if (!inLineComment && !inBlockComment && c == '/' && next == '*') {
            inBlockComment = true;
            i++; // skip '*'
            continue;
        }
        if (inBlockComment) {
            if (c == '*' && next == '/') {
                inBlockComment = false;
                i++; // skip '/'
            }
            continue;
        }

        // Line comment tracking
        if (!inBlockComment && !inLineComment && c == '/' && next == '/') {
            inLineComment = true;
            i++; // skip second '/'
            continue;
        }
        if (inLineComment) {
            if (c == '\n') {
                inLineComment = false;
                // Treat newline as whitespace
                if (!lastWasSpace) {
                    result += ' ';
                    lastWasSpace = true;
                }
            }
            continue;
        }

        // Collapse whitespace
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!lastWasSpace) {
                result += ' ';
                lastWasSpace = true;
            }
            continue;
        }

        result += c;
        lastWasSpace = false;
    }

    // Trim trailing space
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    // Trim leading space
    size_t start = result.find_first_not_of(' ');
    if (start != std::string::npos && start > 0) {
        result = result.substr(start);
    }

    return result;
}

// ============================================================================
// Directory file collection
// ============================================================================

std::vector<std::string> ReplayOracle::collectFiles(const std::string& dir) const {
    std::vector<std::string> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) return files;

    fs::path base(dir);
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            fs::path rel = fs::relative(entry.path(), base);
            files.push_back(rel.generic_string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}
