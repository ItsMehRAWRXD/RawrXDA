#pragma once
/**
 * @file diff_engine.h
 * @brief Production-grade diff engine — Myers' algorithm with optimizations
 * Used by: OT collaboration, merge resolution, code review, AI suggestions
 * No dependencies on external diff libraries.
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD::Core::Diff {

// ============================================================================
// Types
// ============================================================================

enum class DiffOp {
    Equal,      // Lines are identical
    Insert,     // Added in new version
    Delete,     // Removed from old version
    Replace     // Modified (delete + insert combined)
};

struct DiffChunk {
    DiffOp op;
    size_t oldStart;   // 0-based line index in old text
    size_t oldCount;   // Number of lines in old text
    size_t newStart;   // 0-based line index in new text
    size_t newCount;   // Number of lines in new text
    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;

    bool isEmpty() const {
        return oldCount == 0 && newCount == 0;
    }
};

struct DiffResult {
    std::vector<DiffChunk> chunks;
    size_t oldLineCount = 0;
    size_t newLineCount = 0;
    size_t equalLines = 0;
    size_t insertedLines = 0;
    size_t deletedLines = 0;
    size_t modifiedLines = 0;
    double similarity = 0.0;  // 0.0 = completely different, 1.0 = identical

    bool hasChanges() const {
        return !chunks.empty() &&
               (insertedLines > 0 || deletedLines > 0 || modifiedLines > 0);
    }
};

// Unified diff format (like git diff)
struct UnifiedDiff {
    std::string oldPath;
    std::string newPath;
    std::string oldHash;
    std::string newHash;
    std::vector<std::string> hunks;  // Formatted hunk strings
};

// ============================================================================
// Configuration
// ============================================================================

struct DiffConfig {
    // Context lines around each change in unified diff output
    size_t contextLines = 3;

    // Maximum file size (lines) before switching to fast heuristic
    size_t maxLinesForFullDiff = 100000;

    // Timeout in milliseconds (0 = no timeout)
    uint32_t timeoutMs = 30000;

    // Whether to combine adjacent insert+delete into Replace
    bool combineReplace = true;

    // Whether to ignore whitespace-only changes
    bool ignoreWhitespace = false;

    // Whether to ignore line-ending differences (CRLF vs LF)
    bool ignoreLineEndings = true;

    // Similarity threshold for "near-match" detection (0.0-1.0)
    double similarityThreshold = 0.5;
};

// ============================================================================
// Diff Engine
// ============================================================================

class DiffEngine {
public:
    explicit DiffEngine(const DiffConfig& config = {});
    ~DiffEngine();

    // Main diff entry point
    DiffResult diff(
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines);

    // Diff strings (auto-split by lines)
    DiffResult diffStrings(const std::string& oldText, const std::string& newText);

    // Diff files
    DiffResult diffFiles(const std::string& oldPath, const std::string& newPath);

    // Generate unified diff output
    UnifiedDiff toUnifiedDiff(const DiffResult& result,
                               const std::string& oldPath = "a/file",
                               const std::string& newPath = "b/file");

    // Apply a diff to reconstruct the new text from old
    std::vector<std::string> applyDiff(
        const std::vector<std::string>& oldLines,
        const DiffResult& diff);

    // Invert a diff (swap old/new)
    DiffResult invert(const DiffResult& diff);

    // Compose two diffs: first then second
    DiffResult compose(const DiffResult& first, const DiffResult& second);

    // Line-level LCS (Longest Common Subsequence)
    std::vector<std::string> lcs(
        const std::vector<std::string>& a,
        const std::vector<std::string>& b);

    // Similarity score (0.0-1.0)
    double similarity(
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines);

    // Character-level diff within a line (for intra-line highlighting)
    struct CharDiff {
        DiffOp op;
        size_t oldPos;
        size_t newPos;
        size_t length;
    };
    std::vector<CharDiff> diffLineChars(
        const std::string& oldLine,
        const std::string& newLine);

private:
    DiffConfig m_config;

    // Myers' algorithm core
    DiffResult myersDiff(
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines);

    // Fast heuristic for very large files
    DiffResult heuristicDiff(
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines);

    // Normalize line for comparison
    std::string normalizeLine(const std::string& line) const;

    // Check if two lines are equal (respecting config)
    bool linesEqual(const std::string& a, const std::string& b) const;

    // Post-process: combine adjacent insert+delete into Replace
    std::vector<DiffChunk> combineReplacements(std::vector<DiffChunk> chunks);

    // Post-process: merge adjacent equal chunks
    std::vector<DiffChunk> mergeEqualChunks(std::vector<DiffChunk> chunks);

    // Build unified diff hunk
    std::vector<std::string> formatHunk(
        const DiffChunk& chunk,
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines,
        size_t contextLines);
};

// ============================================================================
// Free functions
// ============================================================================

// Quick diff — returns true if texts are identical
bool textsEqual(const std::string& a, const std::string& b,
                bool ignoreWhitespace = false);

// Split text into lines (handles CRLF, LF, CR)
std::vector<std::string> splitLines(const std::string& text);

// Join lines into text
std::string joinLines(const std::vector<std::string>& lines);

// Levenshtein distance for strings
size_t levenshteinDistance(const std::string& a, const std::string& b);

// Levenshtein distance for line arrays
size_t levenshteinDistanceLines(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b);

} // namespace RawrXD::Core::Diff
